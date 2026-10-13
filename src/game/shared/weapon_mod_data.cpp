#include "game/shared/weapon_mod_data.h"

#include "game/shared/r1/c_weapon_x.h"
#include "game/shared/r1/weapon_x.h"
#include "tier1/keyvalues.h"
#include "util/utils.h"
#include "vscript/languages/squirrel_re/squirrel.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <deque>
#include <vector>

CWeaponModHandler<FileWeaponInfo_Client> g_ClientWeaponMods;
CWeaponModHandler<FileWeaponInfo_Server> g_ServerWeaponMods;

template <typename Value> Value ReadWeaponModValue(const WeaponModValues& values, std::uint16_t offset)
{
    Value result{};
    std::memcpy(&result, reinterpret_cast<const std::byte*>(&values) + offset, sizeof(result));
    return result;
}

ScriptDataType_t WeaponDescriptorScriptType(const WeaponModValueType descriptorType)
{
    switch (descriptorType)
    {
    case WeaponModValueType::Integer:
        return FIELD_INTEGER;
    case WeaponModValueType::Float:
        return FIELD_FLOAT;
    case WeaponModValueType::Boolean:
        return FIELD_BOOLEAN;
    case WeaponModValueType::Vector:
        return FIELD_VECTOR;
    default:
        return FIELD_VOID;
    }
}

bool IsWeaponFieldValueCompatible(const ScriptDataType_t fieldType, const ScriptDataType_t valueType)
{
    switch (fieldType)
    {
    case FIELD_BOOLEAN:
        return valueType == FIELD_BOOLEAN;
    case FIELD_INTEGER:
        return valueType == FIELD_INTEGER;
    case FIELD_FLOAT:
        return valueType == FIELD_FLOAT || valueType == FIELD_INTEGER;
    case FIELD_VECTOR:
        return valueType == FIELD_VECTOR;
    default:
        return false;
    }
}

template <typename WeaponInfo>
bool CWeaponModHandler<WeaponInfo>::WeaponFieldValueToData(const ScriptVariant_t& value, const ScriptDataType_t fieldType,
                                                           std::array<std::byte, sizeof(WeaponModEntry_t::value)>& output, std::size_t& outputSize)
{
    if (!IsWeaponFieldValueCompatible(fieldType, value.GetType()))
        return false;

    switch (fieldType)
    {
    case FIELD_BOOLEAN:
    {
        const bool typedValue = static_cast<bool>(value);
        outputSize = sizeof(typedValue);
        std::memcpy(output.data(), &typedValue, outputSize);
        return true;
    }
    case FIELD_INTEGER:
    {
        const int typedValue = static_cast<int>(value);
        outputSize = sizeof(typedValue);
        std::memcpy(output.data(), &typedValue, outputSize);
        return true;
    }
    case FIELD_FLOAT:
    {
        const float typedValue = value.GetType() == FIELD_FLOAT ? static_cast<float>(value) : static_cast<float>(static_cast<int>(value));
        outputSize = sizeof(typedValue);
        std::memcpy(output.data(), &typedValue, outputSize);
        return true;
    }
    case FIELD_VECTOR:
    {
        const Vector3D& typedValue = static_cast<const Vector3D&>(value);
        outputSize = sizeof(typedValue);
        std::memcpy(output.data(), &typedValue, outputSize);
        return true;
    }
    default:
        return false;
    }
}

template <typename WeaponInfo> static thread_local WeaponInfo* s_ActiveAssemblyWeaponInfo = nullptr;
template <typename WeaponInfo> static thread_local std::vector<WeaponModEntry_t>* s_ActiveAssemblyEntries = nullptr;
template <typename WeaponInfo> static thread_local std::deque<WeaponModAssemblyItem_t>* s_ActiveAssemblyItems = nullptr;

template <typename WeaponInfo> static bool IsSupportedWeaponModField(const WeaponModEntryType entryType, const WeaponModValueType valueType)
{
    if (valueType > WeaponModValueType::Invalid && valueType <= WeaponModValueType::WeaponString)
        return true;
    return valueType == WeaponModValueType::Special && WeaponParse<WeaponInfo>::IsSupportedSpecialModEntry(entryType);
}

template <typename WeaponInfo> void CWeaponModHandler<WeaponInfo>::Reset()
{
    m_EntriesByWeapon.clear();
    m_FieldOverridesByValues.clear();
}

template <typename WeaponInfo> void CWeaponModHandler<WeaponInfo>::InitializeWeaponInfo(WeaponInfo* pWeaponInfo)
{
    m_EntriesByWeapon.erase(pWeaponInfo);
}

template <typename WeaponInfo> const char* CWeaponModHandler<WeaponInfo>::GetWeaponName(const WeaponInfo* weaponInfo) const
{
    return std::memchr(weaponInfo->szClassName, '\0', sizeof(weaponInfo->szClassName)) ? weaponInfo->szClassName : nullptr;
}

template <typename WeaponInfo>
bool CWeaponModHandler<WeaponInfo>::SetField(const WeaponInfo* weaponInfo, WeaponModValues* values, const char* fieldName, const SQObject& object,
                                             bool& valueSupported)
{
    assert(weaponInfo && values && fieldName && *fieldName);

    valueSupported = false;
    ScriptVariant_t value;
    switch (object._Type)
    {
    case OT_INTEGER:
        value = static_cast<int>(_integer(object));
        break;
    case OT_FLOAT:
        value = static_cast<float>(_float(object));
        break;
    case OT_BOOL:
        value = static_cast<bool>(_bool(object));
        break;
    case OT_VECTOR:
    {
        const SQFloat* vector = _vector(object);
        value = Vector3D{vector[0], vector[1], vector[2]};
        break;
    }
    default:
        return false;
    }
    valueSupported = true;

    const char* weaponName = GetWeaponName(weaponInfo);
    if (!weaponName || !*weaponName)
        return false;

    const WeaponModParseTableEntry* descriptor = WeaponParse<WeaponInfo>::FindModParseEntry(fieldName);
    if (!descriptor)
        return false;

    const ScriptDataType_t fieldType = WeaponDescriptorScriptType(descriptor->parseType);
    std::array<std::byte, sizeof(WeaponModEntry_t::value)> valueData{};
    std::size_t valueSize = 0;
    if (!WeaponFieldValueToData(value, fieldType, valueData, valueSize))
        return false;
    if (descriptor->structOffset > sizeof(WeaponModValues) || valueSize > sizeof(WeaponModValues) - descriptor->structOffset)
        return false;

    auto& overrides = m_FieldOverridesByValues[values];
    const FieldOverride fieldOverride{descriptor->structOffset, static_cast<std::uint16_t>(valueSize), valueData};
    const auto existing =
        std::find_if(overrides.begin(), overrides.end(), [&](const FieldOverride& field) { return field.offset == fieldOverride.offset; });
    if (existing == overrides.end())
        overrides.push_back(fieldOverride);
    else
        *existing = fieldOverride;

    std::memcpy(reinterpret_cast<std::byte*>(values) + descriptor->structOffset, valueData.data(), valueSize);
    return true;
}

template <typename WeaponInfo> void CWeaponModHandler<WeaponInfo>::ApplyFieldOverrides(WeaponModValues* values) const
{
    const auto overrides = m_FieldOverridesByValues.find(values);
    if (overrides == m_FieldOverridesByValues.end())
        return;

    for (const FieldOverride& field : overrides->second)
        std::memcpy(reinterpret_cast<std::byte*>(values) + field.offset, field.value.data(), field.size);
}

template <typename WeaponInfo> void CWeaponModHandler<WeaponInfo>::ClearFieldOverrides(WeaponModValues* values)
{
    m_FieldOverridesByValues.erase(values);
}

template <typename WeaponInfo> std::size_t CWeaponModHandler<WeaponInfo>::CountChildren(KeyValues* pSection)
{
    std::size_t count = 0;
    for (KeyValues* pChild = pSection ? pSection->GetFirstSubKey() : nullptr; pChild; pChild = pChild->GetNextKey())
        ++count;
    return count;
}

template <typename WeaponInfo> std::size_t CWeaponModHandler<WeaponInfo>::CountExpectedEntries(KeyValues* pRoot)
{
    std::size_t count = CountChildren(pRoot->FindKey("SP_BASE", false));
    count += CountChildren(pRoot->FindKey("MP_BASE", false));

    KeyValues* pMods = pRoot->FindKey("Mods", false);
    std::uint32_t groupCount = 0;
    for (KeyValues* pGroup = pMods ? pMods->GetFirstTrueSubKey() : nullptr; pGroup && groupCount < MAX_WEAPON_MOD_GROUPS;
         pGroup = pGroup->GetNextTrueSubKey(), ++groupCount)
    {
        count += CountChildren(pGroup);
    }

    return count;
}

template <typename WeaponInfo>
bool CWeaponModHandler<WeaponInfo>::CanAppendEntries(const std::vector<WeaponModEntry_t>& entries, std::size_t groupEntryCount)
{
    return groupEntryCount < MAX_ENCODED_WEAPON_MOD_ENTRIES && groupEntryCount <= MAX_ENCODED_WEAPON_MOD_ENTRIES - entries.size();
}

template <typename WeaponInfo> std::uint32_t CWeaponModHandler<WeaponInfo>::GetCodeCount(const WeaponInfo* weaponInfo) const
{
    return weaponInfo->modEntryCount;
}

template <typename WeaponInfo> void CWeaponModHandler<WeaponInfo>::SetCodeCount(WeaponInfo* weaponInfo, std::uint32_t count) const
{
    weaponInfo->modEntryCount = count;
}

template <typename WeaponInfo> std::uint32_t CWeaponModHandler<WeaponInfo>::GetModGroupCount(const WeaponInfo* weaponInfo) const
{
    return weaponInfo->modsCount;
}

template <typename WeaponInfo> const WeaponMod* CWeaponModHandler<WeaponInfo>::GetModGroups(const WeaponInfo* weaponInfo) const
{
    return weaponInfo->mods;
}

template <typename WeaponInfo> void CWeaponModHandler<WeaponInfo>::PrepareWeaponParse(WeaponInfo* pWeaponInfo, KeyValues* pRoot)
{
    auto& entries = m_EntriesByWeapon[pWeaponInfo];
    entries.clear();

    const std::size_t expectedCount = (std::min)(CountExpectedEntries(pRoot), MAX_ENCODED_WEAPON_MOD_ENTRIES);
    if (entries.capacity() < expectedCount)
        entries.reserve(expectedCount);

    SetCodeCount(pWeaponInfo, 0);
}

template <typename WeaponInfo> void CWeaponModHandler<WeaponInfo>::FinishWeaponParse(WeaponInfo* pWeaponInfo)
{
    const auto iterator = m_EntriesByWeapon.find(pWeaponInfo);
    assert(iterator != m_EntriesByWeapon.end());
    SetCodeCount(pWeaponInfo, static_cast<std::uint32_t>(iterator->second.size()));
}

template <typename WeaponInfo> std::vector<WeaponModEntry_t>* CWeaponModHandler<WeaponInfo>::FindEntries(WeaponInfo* pWeaponInfo)
{
    const auto iterator = m_EntriesByWeapon.find(pWeaponInfo);
    return iterator != m_EntriesByWeapon.end() ? &iterator->second : nullptr;
}

template <typename WeaponInfo>
bool CWeaponModHandler<WeaponInfo>::GetGroupRange(const std::vector<WeaponModEntry_t>& entries, const WeaponMod& group, std::size_t& firstEntry,
                                                  std::size_t& entryCount) const
{
    firstEntry = group.firstEntry;
    entryCount = group.entryCount;
    return firstEntry <= entries.size() && entryCount <= entries.size() - firstEntry;
}

template <typename WeaponInfo>
std::uint32_t* CWeaponModHandler<WeaponInfo>::ParseWeaponInfo(WeaponInfo* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName,
                                                              ParseWeaponInfo_t original)
{
    PrepareWeaponParse(pWeaponInfo, pRoot);
    std::uint32_t* pResult = original(pWeaponInfo, pRoot, pWeaponName);
    FinishWeaponParse(pWeaponInfo);
    return pResult;
}

template <typename WeaponInfo>
std::uint32_t* CWeaponModHandler<WeaponInfo>::ParseGroups(WeaponInfo* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName,
                                                          WeaponMod* pOutputGroups, std::uint32_t* pOutputGroupCount, ParseGroups_t original)
{
    if (!FindEntries(pWeaponInfo))
        return original(pWeaponInfo, pRoot, pWeaponName, pOutputGroups, pOutputGroupCount);

    std::memset(pOutputGroups, 0, sizeof(WeaponMod) * MAX_WEAPON_MODS);
    *pOutputGroupCount = 0;

    KeyValues* pMods = pRoot->FindKey("Mods", false);
    for (KeyValues* pGroup = pMods ? pMods->GetFirstTrueSubKey() : nullptr; pGroup && *pOutputGroupCount < MAX_WEAPON_MOD_GROUPS;
         pGroup = pGroup->GetNextTrueSubKey())
    {
        WeaponParse<WeaponInfo>::ParseSingleMod(pGroup, pWeaponInfo, pWeaponName, &pOutputGroups[*pOutputGroupCount]);
        ++*pOutputGroupCount;
    }

    return pOutputGroupCount;
}

template <typename WeaponInfo>
KeyValues* CWeaponModHandler<WeaponInfo>::ParseGroup(KeyValues* pSection, WeaponInfo* pWeaponInfo, const char* pWeaponName, WeaponMod* pOutputGroup,
                                                     ParseGroup_t original)
{
    auto* pEntries = FindEntries(pWeaponInfo);
    if (!pEntries)
        return original(pSection, pWeaponInfo, pWeaponName, pOutputGroup);

    const std::size_t childCount = CountChildren(pSection);
    if (!CanAppendEntries(*pEntries, childCount))
        return nullptr;

    if (pEntries->capacity() < pEntries->size() + childCount)
        pEntries->reserve(pEntries->size() + childCount);

    const std::size_t firstEntryIndex = pEntries->size();
    KeyValues* pFirst = pSection->GetFirstSubKey();
    if (!pFirst)
    {
        SetCodeCount(pWeaponInfo, 0);
        KeyValues* result = original(pSection, pWeaponInfo, pWeaponName, pOutputGroup);
        pOutputGroup->firstEntry = firstEntryIndex < MAX_ENCODED_WEAPON_MOD_ENTRIES ? static_cast<std::uint16_t>(firstEntryIndex) : 0;
        SetCodeCount(pWeaponInfo, static_cast<std::uint32_t>(pEntries->size()));
        return result;
    }

    WeaponString_t groupName = WEAPSTR_EMPTY;
    std::size_t parsedGroupEntryCount = 0;
    bool parsedFirstChunk = false;

    while (pFirst)
    {
        KeyValues* pLast = pFirst;
        std::uint32_t chunkNodeCount = 1;
        while (chunkNodeCount < MAX_WEAPON_MOD_ENTRIES && pLast->GetNextKey())
        {
            pLast = pLast->GetNextKey();
            ++chunkNodeCount;
        }

        WeaponMod scratchGroup{};
        SetCodeCount(pWeaponInfo, 0);
        KeyValues* const pNext = pLast->m_pPeer;
        {
            KeyValues chunk(pSection->GetName());
            chunk.m_pSub = pFirst;
            pLast->m_pPeer = nullptr;
            const ScopeGuard restoreChunk([&]
            {
                pLast->m_pPeer = pNext;
                chunk.m_pSub = nullptr;
            });
            original(&chunk, pWeaponInfo, pWeaponName, &scratchGroup);
        }

        const std::uint32_t parsedChunkEntryCount = GetCodeCount(pWeaponInfo);
        if (parsedChunkEntryCount > MAX_WEAPON_MOD_ENTRIES || scratchGroup.entryCount != parsedChunkEntryCount)
            return nullptr;

        if (!parsedFirstChunk)
        {
            groupName = scratchGroup.modName;
            parsedFirstChunk = true;
        }

        pEntries->insert(pEntries->end(), pWeaponInfo->modEntries, pWeaponInfo->modEntries + parsedChunkEntryCount);
        parsedGroupEntryCount += parsedChunkEntryCount;
        pFirst = pNext;
    }

    pOutputGroup->modName = groupName;
    pOutputGroup->firstEntry = static_cast<std::uint16_t>(firstEntryIndex);
    pOutputGroup->entryCount = static_cast<std::uint16_t>(parsedGroupEntryCount);
    SetCodeCount(pWeaponInfo, static_cast<std::uint32_t>(pEntries->size()));
    return nullptr;
}

template <typename WeaponInfo>
std::uint8_t CWeaponModHandler<WeaponInfo>::Assemble(int enabledMods, WeaponInfo* pWeaponInfo, WeaponModValues* output, bool singlePlayer,
                                                     int removedMods, Assemble_t original)
{
    auto* pEntries = FindEntries(pWeaponInfo);
    if (!pEntries)
        return original(enabledMods, pWeaponInfo, output, singlePlayer, removedMods);

    std::deque<WeaponModAssemblyItem_t> assemblyItems;
    WeaponInfo* const previousWeaponInfo = s_ActiveAssemblyWeaponInfo<WeaponInfo>;
    auto* const previousEntries = s_ActiveAssemblyEntries<WeaponInfo>;
    auto* const previousItems = s_ActiveAssemblyItems<WeaponInfo>;
    s_ActiveAssemblyWeaponInfo<WeaponInfo> = pWeaponInfo;
    s_ActiveAssemblyEntries<WeaponInfo> = pEntries;
    s_ActiveAssemblyItems<WeaponInfo> = &assemblyItems;
    const ScopeGuard restoreAssembly([previousWeaponInfo, previousEntries, previousItems]
    {
        s_ActiveAssemblyWeaponInfo<WeaponInfo> = previousWeaponInfo;
        s_ActiveAssemblyEntries<WeaponInfo> = previousEntries;
        s_ActiveAssemblyItems<WeaponInfo> = previousItems;
    });
    return original(enabledMods, pWeaponInfo, output, singlePlayer, removedMods);
}

template <typename WeaponInfo> bool CWeaponModHandler<WeaponInfo>::HasActiveAssembly()
{
    return s_ActiveAssemblyWeaponInfo<WeaponInfo> && s_ActiveAssemblyEntries<WeaponInfo> && s_ActiveAssemblyItems<WeaponInfo>;
}

template <typename WeaponInfo> bool CWeaponModHandler<WeaponInfo>::ApplyActiveEntry(WeaponModEntry_t* entry, bool setBaseValue, bool remove)
{
    WeaponInfo* const weaponInfo = s_ActiveAssemblyWeaponInfo<WeaponInfo>;
    auto* const entries = s_ActiveAssemblyEntries<WeaponInfo>;
    auto* const assemblyItems = s_ActiveAssemblyItems<WeaponInfo>;
    assert(weaponInfo && entries && assemblyItems);
    if (!entry)
        return false;

    WeaponModEntry_t* resolvedEntry = entry;
    const std::uintptr_t storageAddress = reinterpret_cast<std::uintptr_t>(weaponInfo->modEntries);
    const std::uintptr_t entryAddress = reinterpret_cast<std::uintptr_t>(entry);
    if (entryAddress >= storageAddress)
    {
        const std::uintptr_t offset = entryAddress - storageAddress;
        if (offset < MAX_ENCODED_WEAPON_MOD_ENTRIES * sizeof(WeaponModEntry_t) && offset % sizeof(WeaponModEntry_t) == 0)
        {
            const std::size_t index = offset / sizeof(WeaponModEntry_t);
            if (index >= entries->size())
                return false;
            resolvedEntry = &(*entries)[index];
        }
    }

    auto& item = assemblyItems->emplace_back();
    item.setBaseValue = setBaseValue;
    item.remove = remove;

    const WeaponModParseTableEntry* parseEntry = WeaponParse<WeaponInfo>::GetModParseEntry(resolvedEntry->entryType);
    if (!parseEntry || !IsSupportedWeaponModField<WeaponInfo>(resolvedEntry->entryType, parseEntry->parseType))
        return false;

    return WeaponParse<WeaponInfo>::AddEntryToAssemblyGeneralPriority(resolvedEntry, parseEntry, &item);
}

template <typename WeaponInfo>
void CWeaponModHandler<WeaponInfo>::PrecacheFlaggedAssets(WeaponInfo* weaponInfo, const WeaponMod& group,
                                                          const std::vector<WeaponModEntry_t>& entries)
{
    std::size_t firstEntry;
    std::size_t entryCount;
    if (!GetGroupRange(entries, group, firstEntry, entryCount))
        return;

    for (std::size_t index = firstEntry; index < firstEntry + entryCount; ++index)
    {
        const WeaponModEntry_t& entry = entries[index];
        if (!entry.HasValue())
            continue;

        const WeaponModParseTableEntry* parseEntry = WeaponParse<WeaponInfo>::GetModParseEntry(entry.entryType);
        if (!parseEntry || parseEntry->parseType != WeaponModValueType::Asset)
            continue;

        const char* assetName = weaponInfo->GetString(entry.stringValue);
        if (*assetName)
            WeaponParse<WeaponInfo>::PrecacheModAsset(*parseEntry, assetName);
    }
}

template <typename WeaponInfo>
int CWeaponModHandler<WeaponInfo>::PrecacheStringEntries(WeaponInfo* weaponInfo, const WeaponMod& group, const std::vector<WeaponModEntry_t>& entries)
{
    std::size_t firstEntry;
    std::size_t entryCount;
    if (!GetGroupRange(entries, group, firstEntry, entryCount))
        return 0;

    for (std::size_t index = firstEntry; index < firstEntry + entryCount; ++index)
    {
        const WeaponModEntry_t& entry = entries[index];
        const WeaponModParseTableEntry* parseEntry = WeaponParse<WeaponInfo>::GetModParseEntry(entry.entryType);
        if (!entry.HasValue() || !parseEntry || parseEntry->parseType != WeaponModValueType::WeaponString)
            continue;

        if (entry.stringValue != WEAPSTR_EMPTY)
            WeaponParse<WeaponInfo>::PrecacheModel(weaponInfo->GetString(entry.stringValue));
    }

    return static_cast<int>(entryCount);
}

template <typename WeaponInfo>
int CWeaponModHandler<WeaponInfo>::PrecacheAllClientStrings(WeaponInfo* weaponInfo, const std::vector<WeaponModEntry_t>& entries)
{
    int result = 0;
    for (std::uint16_t fieldIndex = 687; fieldIndex <= 689; ++fieldIndex)
    {
        const WeaponModParseTableEntry* parseEntry = WeaponParse<WeaponInfo>::GetModParseEntry(static_cast<WeaponModEntryType>(fieldIndex));
        if (!parseEntry)
            continue;

        const WeaponString_t stringOffset = ReadWeaponModValue<WeaponString_t>(weaponInfo->modValueDefaults, parseEntry->structOffset);
        if (stringOffset != WEAPSTR_EMPTY)
            result = WeaponParse<WeaponInfo>::PrecacheModel(weaponInfo->GetString(stringOffset));
    }

    if (weaponInfo->spBaseModDefined)
        result = PrecacheStringEntries(weaponInfo, weaponInfo->spBaseMod, entries);

    if (weaponInfo->mpBaseModDefined)
        result = PrecacheStringEntries(weaponInfo, weaponInfo->mpBaseMod, entries);

    const WeaponMod* groups = GetModGroups(weaponInfo);
    const std::uint32_t groupCount = GetModGroupCount(weaponInfo);
    for (std::uint32_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
        result = PrecacheStringEntries(weaponInfo, groups[groupIndex], entries);

    return result;
}

template <typename WeaponInfo>
template <typename NotifyFn>
void CWeaponModHandler<WeaponInfo>::NotifyStringFieldFromEntries(WeaponModEntryType entryType, WeaponInfo* weaponInfo,
                                                                 const std::vector<WeaponModEntry_t>& entries, NotifyFn&& notify)
{
    const WeaponModParseTableEntry* parseEntry = WeaponParse<WeaponInfo>::GetModParseEntry(entryType);
    if (!parseEntry)
        return;

    const char* defaultValue = ReadWeaponModValue<const char*>(weaponInfo->modValueDefaults, parseEntry->structOffset);
    if (defaultValue && *defaultValue)
        notify(defaultValue);

    const WeaponMod* groups = GetModGroups(weaponInfo);
    const std::uint32_t groupCount = GetModGroupCount(weaponInfo);
    for (std::uint32_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
    {
        std::size_t firstEntry;
        std::size_t entryCount;
        if (!GetGroupRange(entries, groups[groupIndex], firstEntry, entryCount))
            continue;

        for (std::size_t index = firstEntry; index < firstEntry + entryCount; ++index)
        {
            const WeaponModEntry_t& entry = entries[index];
            if (entry.entryType != entryType)
                continue;

            const char* value = weaponInfo->GetString(entry.stringValue);
            if (*value)
                notify(value);
        }
    }
}

template <typename WeaponInfo>
void CWeaponModHandler<WeaponInfo>::PrecacheAssets(WeaponInfo* weaponInfo, const WeaponMod& group, float precacheValue,
                                                   PrecacheAssetsClient_t original)
{
    auto* entries = FindEntries(weaponInfo);
    if (!entries)
    {
        original(weaponInfo, &group, precacheValue);
        return;
    }

    PrecacheFlaggedAssets(weaponInfo, group, *entries);
}

template <typename WeaponInfo>
void CWeaponModHandler<WeaponInfo>::PrecacheAssets(WeaponInfo* weaponInfo, const WeaponMod& group, PrecacheAssetsServer_t original)
{
    auto* entries = FindEntries(weaponInfo);
    if (!entries)
    {
        original(weaponInfo, &group);
        return;
    }

    PrecacheFlaggedAssets(weaponInfo, group, *entries);
}

template <typename WeaponInfo>
int CWeaponModHandler<WeaponInfo>::PrecacheStrings(WeaponInfo* weaponInfo, const WeaponMod& group, PrecacheStrings_t original)
{
    auto* entries = FindEntries(weaponInfo);
    return entries ? PrecacheStringEntries(weaponInfo, group, *entries) : original(weaponInfo, &group);
}

template <typename WeaponInfo>
void CWeaponModHandler<WeaponInfo>::PrecacheStringsNoResult(WeaponInfo* weaponInfo, const WeaponMod& group, PrecacheStringsNoResult_t original)
{
    auto* entries = FindEntries(weaponInfo);
    if (!entries)
    {
        original(weaponInfo, &group);
        return;
    }

    PrecacheStringEntries(weaponInfo, group, *entries);
}

template <typename WeaponInfo> int CWeaponModHandler<WeaponInfo>::PrecacheAllStrings(WeaponInfo* weaponInfo, PrecacheAllStrings_t original)
{
    auto* entries = FindEntries(weaponInfo);
    return entries ? PrecacheAllClientStrings(weaponInfo, *entries) : original(weaponInfo);
}

template <typename WeaponInfo>
void CWeaponModHandler<WeaponInfo>::NotifyStringField(WeaponEntity* weapon, WeaponModEntryType entryType, NotifyStringField_t original)
{
    auto* weaponInfo = const_cast<WeaponInfo*>(&weapon->GetWpnData());
    auto* entries = FindEntries(weaponInfo);
    if (entries)
        NotifyStringFieldFromEntries(entryType, weaponInfo, *entries,
                                     [weapon](const char* soundName) { WeaponParse<WeaponInfo>::PrecacheScriptSound(weapon, soundName); });
    else
        original(weapon, static_cast<std::uint16_t>(entryType));
}

template class CWeaponModHandler<FileWeaponInfo_Client>;
template class CWeaponModHandler<FileWeaponInfo_Server>;
