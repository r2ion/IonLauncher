#include "weapon_mods.h"

#include "client/weaponx.h"
#include "engine/cdll_int.h"
#include "server/weaponx.h"
#include "tier0/frametask.h"
#include "tier1/keyvalues.h"
#include "util/utils.h"
#include "vscript/languages/squirrel_re/squirrel.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <deque>
#include <vector>
DECLARE_MODULE(WeaponModHooks)

CWeaponModHandler<FileWeaponInfo_Client> g_ClientWeaponMods;
CWeaponModHandler<FileWeaponInfo_Server> g_ServerWeaponMods;

const WeaponModParseTableEntry* FindWeaponModParseEntry(const FileWeaponInfo_Client*, const char* fieldName)
{
    return FindWeaponModParseTableEntry_Client(fieldName);
}

const WeaponModParseTableEntry* FindWeaponModParseEntry(const FileWeaponInfo_Server*, const char* fieldName)
{
    return FindWeaponModParseTableEntry_Server(fieldName);
}

const WeaponModParseTableEntry* GetWeaponModParseEntry(const FileWeaponInfo_Client*, WeaponModEntryType entryType)
{
    return GetWeaponModParseTableEntry_Client(entryType);
}

const WeaponModParseTableEntry* GetWeaponModParseEntry(const FileWeaponInfo_Server*, WeaponModEntryType entryType)
{
    return GetWeaponModParseTableEntry_Server(entryType);
}

KeyValues* ParseWeaponMod(KeyValues* section, FileWeaponInfo_Client* info, const char* weaponName, WeaponMod* mod)
{
    return ParseWeaponMod_Client(section, info, weaponName, mod);
}

KeyValues* ParseWeaponMod(KeyValues* section, FileWeaponInfo_Server* info, const char* weaponName, WeaponMod* mod)
{
    return ParseWeaponMod_Server(section, info, weaponName, mod);
}

bool InsertWeaponModAssemblyItem(FileWeaponInfo_Client*, WeaponModEntry_t* entry, const WeaponModParseTableEntry* parseEntry,
                                 WeaponModAssemblyItem_t* item)
{
    return InsertWeaponModAssemblyItem_Client(entry, parseEntry, item);
}

bool InsertWeaponModAssemblyItem(FileWeaponInfo_Server*, WeaponModEntry_t* entry, const WeaponModParseTableEntry* parseEntry,
                                 WeaponModAssemblyItem_t* item)
{
    return InsertWeaponModAssemblyItem_Server(entry, parseEntry, item);
}

void PrecacheWeaponModAsset(FileWeaponInfo_Client*, const WeaponModParseTableEntry& parseEntry, const char* assetName)
{
    PrecacheWeaponModAsset_Client(parseEntry, assetName);
}

void PrecacheWeaponModAsset(FileWeaponInfo_Server*, const WeaponModParseTableEntry& parseEntry, const char* assetName)
{
    PrecacheWeaponModAsset_Server(parseEntry, assetName);
}

int PrecacheWeaponModModel(FileWeaponInfo_Client*, const char* modelName)
{
    return PrecacheWeaponModModel_Client(modelName);
}

int PrecacheWeaponModModel(FileWeaponInfo_Server*, const char* modelName)
{
    return PrecacheWeaponModModel_Server(modelName);
}

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

bool IsSupportedWeaponModField(const WeaponModEntryType entryType, const WeaponModValueType valueType)
{
    if (valueType > WeaponModValueType::Invalid && valueType <= WeaponModValueType::WeaponString)
        return true;

    if (valueType != WeaponModValueType::Special)
        return false;

    switch (static_cast<std::uint16_t>(entryType))
    {
    case WEAPON_MOD_ENTRY_FIRE_MODE:
    case WEAPON_MOD_ENTRY_AIMASSIST_ADSPULL_WEAPONCLASS:
    case WEAPON_MOD_ENTRY_DAMAGE_FLAGS:
    case WEAPON_MOD_ENTRY_EXPLOSION_DAMAGE_FLAGS:
    case WEAPON_MOD_ENTRY_AMMO_SUCK_BEHAVIOR:
    case WEAPON_MOD_ENTRY_DAMAGE_FALLOFF_TYPE:
    case WEAPON_MOD_ENTRY_VIEWKICK_SPRING:
    case WEAPON_MOD_ENTRY_SMART_AMMO_HUD_TYPE:
    case WEAPON_MOD_ENTRY_SMART_AMMO_HUD_LOCK_STYLE:
    case WEAPON_MOD_ENTRY_SMART_AMMO_WEAPON_TYPE:
    case WEAPON_MOD_ENTRY_SMART_AMMO_LOCK_TYPE:
        return true;
    default:
        return false;
    }
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

    const WeaponModParseTableEntry* descriptor = FindWeaponModParseEntry(weaponInfo, fieldName);
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

template bool CWeaponModHandler<FileWeaponInfo_Client>::SetField(const FileWeaponInfo_Client* weaponInfo, WeaponModValues* values,
                                                                 const char* fieldName, const SQObject& object, bool& valueSupported);
template bool CWeaponModHandler<FileWeaponInfo_Server>::SetField(const FileWeaponInfo_Server* weaponInfo, WeaponModValues* values,
                                                                 const char* fieldName, const SQObject& object, bool& valueSupported);

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
template <typename OriginalFn>
std::uint32_t* CWeaponModHandler<WeaponInfo>::ParseWeaponInfo(WeaponInfo* pWeaponInfo, KeyValues* pRoot, OriginalFn&& original)
{
    PrepareWeaponParse(pWeaponInfo, pRoot);
    std::uint32_t* pResult = original();
    FinishWeaponParse(pWeaponInfo);
    return pResult;
}

template <typename WeaponInfo>
template <typename OriginalFn>
std::uint32_t* CWeaponModHandler<WeaponInfo>::ParseGroups(WeaponInfo* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName,
                                                          WeaponMod* pOutputGroups, std::uint32_t* pOutputGroupCount, OriginalFn&& original)
{
    if (!FindEntries(pWeaponInfo))
        return original();

    std::memset(pOutputGroups, 0, sizeof(WeaponMod) * MAX_WEAPON_MODS);
    *pOutputGroupCount = 0;

    KeyValues* pMods = pRoot->FindKey("Mods", false);
    for (KeyValues* pGroup = pMods ? pMods->GetFirstTrueSubKey() : nullptr; pGroup && *pOutputGroupCount < MAX_WEAPON_MOD_GROUPS;
         pGroup = pGroup->GetNextTrueSubKey())
    {
        ParseWeaponMod(pGroup, pWeaponInfo, pWeaponName, &pOutputGroups[*pOutputGroupCount]);
        ++*pOutputGroupCount;
    }

    return pOutputGroupCount;
}

template <typename WeaponInfo>
template <typename OriginalFn>
KeyValues* CWeaponModHandler<WeaponInfo>::ParseGroup(KeyValues* pSection, WeaponInfo* pWeaponInfo, WeaponMod* pOutputGroup, OriginalFn&& original)
{
    auto* pEntries = FindEntries(pWeaponInfo);
    if (!pEntries)
        return original(pSection, pOutputGroup);

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
        KeyValues* result = original(pSection, pOutputGroup);
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
            original(&chunk, &scratchGroup);
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
template <typename OriginalFn>
std::uint8_t CWeaponModHandler<WeaponInfo>::Assemble(WeaponInfo* pWeaponInfo, OriginalFn&& original)
{
    auto* pEntries = FindEntries(pWeaponInfo);
    if (!pEntries)
        return original();

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
    return original();
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

    const WeaponModParseTableEntry* parseEntry = GetWeaponModParseEntry(weaponInfo, resolvedEntry->entryType);
    if (!parseEntry || !IsSupportedWeaponModField(resolvedEntry->entryType, parseEntry->parseType))
        return false;

    return InsertWeaponModAssemblyItem(weaponInfo, resolvedEntry, parseEntry, &item);
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

        const WeaponModParseTableEntry* parseEntry = GetWeaponModParseEntry(weaponInfo, entry.entryType);
        if (!parseEntry || parseEntry->parseType != WeaponModValueType::Asset)
            continue;

        const char* assetName = weaponInfo->GetString(entry.stringValue);
        if (*assetName)
            PrecacheWeaponModAsset(weaponInfo, *parseEntry, assetName);
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
        const WeaponModParseTableEntry* parseEntry = GetWeaponModParseEntry(weaponInfo, entry.entryType);
        if (!entry.HasValue() || !parseEntry || parseEntry->parseType != WeaponModValueType::WeaponString)
            continue;

        if (entry.stringValue != WEAPSTR_EMPTY)
            PrecacheWeaponModModel(weaponInfo, weaponInfo->GetString(entry.stringValue));
    }

    return static_cast<int>(entryCount);
}

template <typename WeaponInfo>
int CWeaponModHandler<WeaponInfo>::PrecacheAllClientStrings(WeaponInfo* weaponInfo, const std::vector<WeaponModEntry_t>& entries)
{
    int result = 0;
    for (std::uint16_t fieldIndex = 687; fieldIndex <= 689; ++fieldIndex)
    {
        const WeaponModParseTableEntry* parseEntry = GetWeaponModParseEntry(weaponInfo, static_cast<WeaponModEntryType>(fieldIndex));
        if (!parseEntry)
            continue;

        const WeaponString_t stringOffset = ReadWeaponModValue<WeaponString_t>(weaponInfo->modValueDefaults, parseEntry->structOffset);
        if (stringOffset != WEAPSTR_EMPTY)
            result = PrecacheWeaponModModel(weaponInfo, weaponInfo->GetString(stringOffset));
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
    const WeaponModParseTableEntry* parseEntry = GetWeaponModParseEntry(weaponInfo, entryType);
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
template <typename OriginalFn>
void CWeaponModHandler<WeaponInfo>::PrecacheAssets(WeaponInfo* weaponInfo, const WeaponMod& group, OriginalFn&& original)
{
    auto* entries = FindEntries(weaponInfo);
    if (!entries)
    {
        original();
        return;
    }

    PrecacheFlaggedAssets(weaponInfo, group, *entries);
}

template <typename WeaponInfo>
template <typename OriginalFn>
int CWeaponModHandler<WeaponInfo>::PrecacheStrings(WeaponInfo* weaponInfo, const WeaponMod& group, OriginalFn&& original)
{
    auto* entries = FindEntries(weaponInfo);
    return entries ? PrecacheStringEntries(weaponInfo, group, *entries) : original();
}

template <typename WeaponInfo>
template <typename OriginalFn>
void CWeaponModHandler<WeaponInfo>::PrecacheStringsNoResult(WeaponInfo* weaponInfo, const WeaponMod& group, OriginalFn&& original)
{
    auto* entries = FindEntries(weaponInfo);
    if (!entries)
    {
        original();
        return;
    }

    PrecacheStringEntries(weaponInfo, group, *entries);
}

template <typename WeaponInfo>
template <typename OriginalFn>
int CWeaponModHandler<WeaponInfo>::PrecacheAllStrings(WeaponInfo* weaponInfo, OriginalFn&& original)
{
    auto* entries = FindEntries(weaponInfo);
    return entries ? PrecacheAllClientStrings(weaponInfo, *entries) : original();
}

template <typename WeaponInfo>
template <typename NotifyFn, typename OriginalFn>
void CWeaponModHandler<WeaponInfo>::NotifyStringField(WeaponInfo* weaponInfo, WeaponModEntryType entryType, NotifyFn&& notify, OriginalFn&& original)
{
    auto* entries = FindEntries(weaponInfo);
    if (entries)
        NotifyStringFieldFromEntries(entryType, weaponInfo, *entries, notify);
    else
        original();
}

DECLARE_HOOK(C_WeaponX_GetMods, client.dll + 0x5A7FA0, [](auto& hook, C_WeaponX* pWeapon, HSQUIRRELVM sqvm) -> SQRESULT
{
    if (pWeapon->GetWeaponFileInfoHandle() != INVALID_WEAPON_INFO_HANDLE)
        return hook.Original(pWeapon, sqvm);

    g_pSquirrel[ScriptContext::CLIENT]->raiseerror(sqvm, "GetMods: invalid weapon-info handle (0xFFFF)");

    // crashes can happen due to weird server-side keyvalues bullshit, not sure if a disconnect is needed though
    g_TaskQueue.Dispatch([]() { g_pEngineClient->Disconnect("#DISCONNECT_OUT_OF_SYNC"); });

    return SQRESULT_ERROR;
});

DECLARE_HOOK(C_WeaponX_Destructor, client.dll + 0x59B760, [](auto& hook, C_WeaponX* pWeapon) -> std::uintptr_t
{
    g_ClientWeaponMods.ClearFieldOverrides(&pWeapon->m_modVars);
    return hook.Original(pWeapon);
});

DECLARE_HOOK(CWeaponX_Destructor, server.dll + 0x6813E0, [](auto& hook, CWeaponX* pWeapon) -> std::uintptr_t
{
    g_ServerWeaponMods.ClearFieldOverrides(&pWeapon->m_modVars);
    return hook.Original(pWeapon);
});

DECLARE_HOOK(InitializeWeaponInfo_Client, client.dll + 0x3CC990, [](auto& hook, FileWeaponInfo_Client* pWeaponInfo) -> std::uintptr_t
{
    g_ClientWeaponMods.InitializeWeaponInfo(pWeaponInfo);
    return hook.Original(pWeaponInfo);
});

DECLARE_HOOK(InitializeWeaponInfo_Server, server.dll + 0x6CB600, [](auto& hook, FileWeaponInfo_Server* pWeaponInfo) -> std::uintptr_t
{
    g_ServerWeaponMods.InitializeWeaponInfo(pWeaponInfo);
    return hook.Original(pWeaponInfo);
});

DECLARE_HOOK(ParseWeaponInfoFile_Client, client.dll + 0x3CFAC0,
             [](auto& hook, FileWeaponInfo_Client* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName) -> std::uint32_t*
{ return g_ClientWeaponMods.ParseWeaponInfo(pWeaponInfo, pRoot, [&]() { return hook.Original(pWeaponInfo, pRoot, pWeaponName); }); });

DECLARE_HOOK(ParseWeaponInfoFile_Server, server.dll + 0x6CE3F0,
             [](auto& hook, FileWeaponInfo_Server* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName) -> std::uint32_t*
{ return g_ServerWeaponMods.ParseWeaponInfo(pWeaponInfo, pRoot, [&]() { return hook.Original(pWeaponInfo, pRoot, pWeaponName); }); });

DECLARE_HOOK(ParseWeaponModGroups_Client, client.dll + 0x3D1020,
             [](auto& hook, FileWeaponInfo_Client* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName, WeaponMod* pOutputGroups,
                std::uint32_t* pOutputGroupCount) -> std::uint32_t*
{
    return g_ClientWeaponMods.ParseGroups(pWeaponInfo, pRoot, pWeaponName, pOutputGroups, pOutputGroupCount,
                                          [&]() { return hook.Original(pWeaponInfo, pRoot, pWeaponName, pOutputGroups, pOutputGroupCount); });
});

DECLARE_HOOK(ParseWeaponModGroups_Server, server.dll + 0x6CFA10,
             [](auto& hook, FileWeaponInfo_Server* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName, WeaponMod* pOutputGroups,
                std::uint32_t* pOutputGroupCount) -> std::uint32_t*
{
    return g_ServerWeaponMods.ParseGroups(pWeaponInfo, pRoot, pWeaponName, pOutputGroups, pOutputGroupCount,
                                          [&]() { return hook.Original(pWeaponInfo, pRoot, pWeaponName, pOutputGroups, pOutputGroupCount); });
});

DECLARE_HOOK(ParseWeaponModGroup_Client, client.dll + 0x3D15D0,
             [](auto& hook, KeyValues* section, FileWeaponInfo_Client* weaponInfo, const char* weaponName, WeaponMod* outputGroup) -> KeyValues*
{
    return g_ClientWeaponMods.ParseGroup(section, weaponInfo, outputGroup,
                                         [&](KeyValues* values, WeaponMod* group) { return hook.Original(values, weaponInfo, weaponName, group); });
});

DECLARE_HOOK(ParseWeaponModGroup_Server, server.dll + 0x6CFDE0,
             [](auto& hook, KeyValues* section, FileWeaponInfo_Server* weaponInfo, const char* weaponName, WeaponMod* outputGroup) -> KeyValues*
{
    return g_ServerWeaponMods.ParseGroup(section, weaponInfo, outputGroup,
                                         [&](KeyValues* values, WeaponMod* group) { return hook.Original(values, weaponInfo, weaponName, group); });
});

DECLARE_HOOK(AssembleWeaponMods_Client, client.dll + 0x3CA0B0,
             [](auto& hook, int enabledMods, FileWeaponInfo_Client* weaponInfo, WeaponModValues* output, bool singlePlayer,
                int removedMods) -> std::uint8_t
{
    const auto result =
        g_ClientWeaponMods.Assemble(weaponInfo, [&]() { return hook.Original(enabledMods, weaponInfo, output, singlePlayer, removedMods); });
    g_ClientWeaponMods.ApplyFieldOverrides(output);
    return result;
});

DECLARE_HOOK(AssembleWeaponMods_Server, server.dll + 0x6C8B80,
             [](auto& hook, int enabledMods, FileWeaponInfo_Server* weaponInfo, WeaponModValues* output, bool singlePlayer,
                int removedMods) -> std::uint8_t
{
    const auto result =
        g_ServerWeaponMods.Assemble(weaponInfo, [&]() { return hook.Original(enabledMods, weaponInfo, output, singlePlayer, removedMods); });
    g_ServerWeaponMods.ApplyFieldOverrides(output);
    return result;
});

DECLARE_HOOK(ApplyWeaponModEntry_Client, client.dll + 0x3C8EA0, [](auto& hook, WeaponModEntry_t* entry, bool setBaseValue, bool remove) -> bool
{
    if (!CWeaponModHandler<FileWeaponInfo_Client>::HasActiveAssembly())
        return hook.Original(entry, setBaseValue, remove);

    return CWeaponModHandler<FileWeaponInfo_Client>::ApplyActiveEntry(entry, setBaseValue, remove);
});

DECLARE_HOOK(ApplyWeaponModEntry_Server, server.dll + 0x6C75D0, [](auto& hook, WeaponModEntry_t* entry, bool setBaseValue, bool remove) -> bool
{
    if (!CWeaponModHandler<FileWeaponInfo_Server>::HasActiveAssembly())
        return hook.Original(entry, setBaseValue, remove);

    return CWeaponModHandler<FileWeaponInfo_Server>::ApplyActiveEntry(entry, setBaseValue, remove);
});

DECLARE_HOOK(PrecacheWeaponModAssets_Client, client.dll + 0x3D1ED0,
             [](auto& hook, FileWeaponInfo_Client* weaponInfo, const WeaponMod* group, float precacheValue) -> void
{ g_ClientWeaponMods.PrecacheAssets(weaponInfo, *group, [&]() { hook.Original(weaponInfo, group, precacheValue); }); });

DECLARE_HOOK(PrecacheWeaponModAssets_Server, server.dll + 0x6D0190, [](auto& hook, FileWeaponInfo_Server* weaponInfo, const WeaponMod* group) -> void
{ g_ServerWeaponMods.PrecacheAssets(weaponInfo, *group, [&]() { hook.Original(weaponInfo, group); }); });

DECLARE_HOOK(PrecacheWeaponModStrings_Client, client.dll + 0x3D23E0, [](auto& hook, FileWeaponInfo_Client* weaponInfo, const WeaponMod* group) -> int
{ return g_ClientWeaponMods.PrecacheStrings(weaponInfo, *group, [&]() { return hook.Original(weaponInfo, group); }); });

DECLARE_HOOK(PrecacheWeaponModStrings_Server, server.dll + 0x6D0680, [](auto& hook, FileWeaponInfo_Server* weaponInfo, const WeaponMod* group) -> void
{ g_ServerWeaponMods.PrecacheStringsNoResult(weaponInfo, *group, [&]() { hook.Original(weaponInfo, group); }); });

DECLARE_HOOK(PrecacheAllWeaponModStrings_Client, client.dll + 0x3D2480, [](auto& hook, FileWeaponInfo_Client* weaponInfo) -> int
{ return g_ClientWeaponMods.PrecacheAllStrings(weaponInfo, [&]() { return hook.Original(weaponInfo); }); });

DECLARE_HOOK(NotifyWeaponModStringField_Client, client.dll + 0x3D41E0, [](auto& hook, C_WeaponX* weapon, std::uint16_t fieldIndex) -> void
{
    auto* weaponInfo = const_cast<FileWeaponInfo_Client*>(&weapon->GetWpnData());
    g_ClientWeaponMods.NotifyStringField(weaponInfo, static_cast<WeaponModEntryType>(fieldIndex), [&](const char* soundName)
    { PrecacheWeaponModSound_Client(weapon, soundName); }, [&]() { hook.Original(weapon, fieldIndex); });
});

DECLARE_HOOK(NotifyWeaponModStringField_Server, server.dll + 0x6D1970, [](auto& hook, CWeaponX* weapon, std::uint16_t fieldIndex) -> void
{
    auto* weaponInfo = const_cast<FileWeaponInfo_Server*>(&weapon->GetWpnData());
    g_ServerWeaponMods.NotifyStringField(weaponInfo, static_cast<WeaponModEntryType>(fieldIndex), [&](const char* soundName)
    { PrecacheWeaponModSound_Server(weapon, soundName); }, [&]() { hook.Original(weapon, fieldIndex); });
});

ON_DLL_LOAD("server.dll", WeaponMods_Server, [](CModule)
{
    g_ServerWeaponMods.Reset();
    DISPATCH_MODULE(WeaponModHooks)
});

ON_DLL_LOAD("client.dll", WeaponMods_Client, [](CModule)
{
    g_ClientWeaponMods.Reset();
    DISPATCH_MODULE(WeaponModHooks)
});
