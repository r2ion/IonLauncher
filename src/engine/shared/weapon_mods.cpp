#include "weapon_mods.h"

#include "tier1/keyvalues.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <deque>
#include <limits>
#include <vector>

DECLARE_MODULE(WeaponModHooks)

constexpr std::uint32_t RetailCodeLimit = 200;
constexpr std::uint32_t MaxModGroupCount = 31;
constexpr std::size_t StoredModGroupCount = 32;
constexpr std::size_t MaxEncodedEntryCount = static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1;

static CWeaponModHandler<ClientWeaponInfo_t> g_ClientWeaponMods;
static CWeaponModHandler<ServerWeaponInfo_t> g_ServerWeaponMods;
std::uint16_t WeaponModCodeEntry_t::GetStringOffset() const
{
    std::uint16_t offset;
    std::memcpy(&offset, m_Value, sizeof(offset));
    return offset;
}

class CScopedKeyValuesChunk
{
  public:
    CScopedKeyValuesChunk(const char* pName, KeyValues* pFirst, KeyValues* pLast) : m_Chunk(pName), m_pLast(pLast), m_pNext(pLast->m_pPeer)
    {
        m_Chunk.m_pSub = pFirst;
        m_pLast->m_pPeer = nullptr;
    }

    ~CScopedKeyValuesChunk()
    {
        m_pLast->m_pPeer = m_pNext;
        m_Chunk.m_pSub = nullptr;
    }

    KeyValues* GetChunk()
    {
        return &m_Chunk;
    }

    KeyValues* GetNext() const
    {
        return m_pNext;
    }

  private:
    KeyValues m_Chunk;
    KeyValues* m_pLast;
    KeyValues* m_pNext;
};

template <typename WeaponInfo> class CScopedWeaponModAssembly
{
  public:
    CScopedWeaponModAssembly(CWeaponModHandler<WeaponInfo>& handler, WeaponInfo* pWeaponInfo, std::vector<WeaponModCodeEntry_t>& entries)
        : m_Handler(handler), m_pWeaponInfo(pWeaponInfo), m_Entries(entries), m_pPrevious(s_pActive)
    {
        s_pActive = this;
    }

    ~CScopedWeaponModAssembly()
    {
        s_pActive = m_pPrevious;
    }

    static CScopedWeaponModAssembly* GetActive()
    {
        return s_pActive;
    }

    std::uintptr_t ApplyEntry(WeaponModCodeEntry_t* pEmbeddedEntry, std::uintptr_t setBaseValue, std::uintptr_t remove)
    {
        WeaponModCodeEntry_t* pEntry = ResolveEntry(pEmbeddedEntry);
        if (!pEntry)
            return 0;

        auto& item = m_AssemblyItems.emplace_back();
        item.m_SetBaseValue = static_cast<std::uint8_t>(setBaseValue);
        item.m_Remove = static_cast<std::uint8_t>(remove);

        const WeaponFieldDescriptor_t& descriptor = m_Handler.m_pFieldDescriptors[pEntry->m_FieldIndex];
        if (!IsSupportedField(pEntry->m_FieldIndex, descriptor.m_Type))
            return 0;

        return m_Handler.m_pInsertAssemblyItem(pEntry, &descriptor, &item);
    }

  private:
    static bool IsSupportedField(std::uint16_t fieldIndex, std::uint8_t fieldType)
    {
        if (fieldType > 0 && fieldType <= 7)
            return true;

        if (fieldType != 8)
            return false;

        constexpr std::uint64_t SupportedLowFields = 0x3000100000000010;
        return fieldIndex == 102 || (fieldIndex <= 61 && (SupportedLowFields & (std::uint64_t{1} << fieldIndex))) || fieldIndex == 135 ||
               fieldIndex == 268 || (fieldIndex >= 538 && fieldIndex <= 541);
    }

    WeaponModCodeEntry_t* ResolveEntry(WeaponModCodeEntry_t* pEmbeddedEntry)
    {
        const std::uintptr_t storageAddress = reinterpret_cast<std::uintptr_t>(m_pWeaponInfo->m_WeaponMods.m_CodeEntries);
        const std::uintptr_t entryAddress = reinterpret_cast<std::uintptr_t>(pEmbeddedEntry);
        if (entryAddress < storageAddress)
            return pEmbeddedEntry;

        const std::uintptr_t offset = entryAddress - storageAddress;
        if (offset >= MaxEncodedEntryCount * sizeof(WeaponModCodeEntry_t) || offset % sizeof(WeaponModCodeEntry_t) != 0)
            return pEmbeddedEntry;

        const std::size_t index = offset / sizeof(WeaponModCodeEntry_t);
        if (index >= m_Entries.size())
            return nullptr;

        return &m_Entries[index];
    }

    static thread_local CScopedWeaponModAssembly* s_pActive;
    CWeaponModHandler<WeaponInfo>& m_Handler;
    WeaponInfo* m_pWeaponInfo;
    std::vector<WeaponModCodeEntry_t>& m_Entries;
    std::deque<WeaponModAssemblyItem_t> m_AssemblyItems;
    CScopedWeaponModAssembly* m_pPrevious;
};

template <typename WeaponInfo> thread_local CScopedWeaponModAssembly<WeaponInfo>* CScopedWeaponModAssembly<WeaponInfo>::s_pActive = nullptr;

template <> void CWeaponModHandler<ClientWeaponInfo_t>::Initialize(const CModule& module)
{
    m_EntriesByWeapon.clear();
    m_pParseGroup = module.Offset(0x3D15D0).RCast<ParseWeaponModGroupFn<ClientWeaponInfo_t>>();
    m_pFieldDescriptors = module.Offset(0x942CA0).RCast<const WeaponFieldDescriptor_t*>();
    m_pPrecacheFlag4Asset = module.Offset(0x195CD0).RCast<PrecacheWeaponModAssetFn>();
    m_pPrecacheFlag8Asset = module.Offset(0x195F20).RCast<PrecacheWeaponModAssetFn>();
    m_pPrecacheString = module.Offset(0x3EDEB0).RCast<PrecacheWeaponModStringFn>();
    m_pGetWeaponInfo = module.Offset(0xBB4B0).RCast<GetWeaponModInfoFn<ClientWeaponInfo_t>>();
    m_pNotifyStringField = module.Offset(0x5B9A60).RCast<NotifyWeaponModStringFieldFn>();
    m_pInsertAssemblyItem = module.Offset(0x3C8F60).RCast<InsertWeaponModAssemblyItemFn>();
}

template <> void CWeaponModHandler<ServerWeaponInfo_t>::Initialize(const CModule& module)
{
    m_EntriesByWeapon.clear();
    m_pParseGroup = module.Offset(0x6CFDE0).RCast<ParseWeaponModGroupFn<ServerWeaponInfo_t>>();
    m_pFieldDescriptors = module.Offset(0x997DC0).RCast<const WeaponFieldDescriptor_t*>();
    m_pPrecacheFlag4Asset = module.Offset(0x159C00).RCast<PrecacheWeaponModAssetFn>();
    m_pPrecacheFlag8Asset = module.Offset(0x159E20).RCast<PrecacheWeaponModAssetFn>();
    m_pPrecacheString = module.Offset(0x429550).RCast<PrecacheWeaponModStringFn>();
    m_pGetWeaponInfo = module.Offset(0xF0CD0).RCast<GetWeaponModInfoFn<ServerWeaponInfo_t>>();
    m_pNotifyStringField = module.Offset(0x6A8C70).RCast<NotifyWeaponModStringFieldFn>();
    m_pInsertAssemblyItem = module.Offset(0x6C7690).RCast<InsertWeaponModAssemblyItemFn>();
}

template <typename WeaponInfo> void CWeaponModHandler<WeaponInfo>::InitializeWeaponInfo(WeaponInfo* pWeaponInfo)
{
    m_EntriesByWeapon.erase(pWeaponInfo);
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
    for (KeyValues* pGroup = pMods ? pMods->GetFirstTrueSubKey() : nullptr; pGroup && groupCount < MaxModGroupCount;
         pGroup = pGroup->GetNextTrueSubKey(), ++groupCount)
    {
        count += CountChildren(pGroup);
    }

    return count;
}

template <typename WeaponInfo>
bool CWeaponModHandler<WeaponInfo>::CanAppendEntries(const std::vector<WeaponModCodeEntry_t>& entries, std::size_t groupEntryCount)
{
    return groupEntryCount <= std::numeric_limits<std::uint16_t>::max() && groupEntryCount <= MaxEncodedEntryCount - entries.size();
}

template <typename WeaponInfo> std::uint32_t CWeaponModHandler<WeaponInfo>::GetCodeCount(const WeaponInfo* pWeaponInfo) const
{
    return pWeaponInfo->m_WeaponMods.m_CodeEntryCount;
}

template <typename WeaponInfo> void CWeaponModHandler<WeaponInfo>::SetCodeCount(WeaponInfo* pWeaponInfo, std::uint32_t count) const
{
    pWeaponInfo->m_WeaponMods.m_CodeEntryCount = count;
}

template <typename WeaponInfo> std::uint32_t CWeaponModHandler<WeaponInfo>::GetModGroupCount(const WeaponInfo* pWeaponInfo) const
{
    return pWeaponInfo->m_WeaponMods.m_GroupCount;
}

template <typename WeaponInfo> const WeaponModGroup_t* CWeaponModHandler<WeaponInfo>::GetModGroups(const WeaponInfo* pWeaponInfo) const
{
    return pWeaponInfo->m_WeaponMods.m_Groups;
}

template <typename WeaponInfo> void CWeaponModHandler<WeaponInfo>::PrepareWeaponParse(WeaponInfo* pWeaponInfo, KeyValues* pRoot)
{
    auto& entries = m_EntriesByWeapon[pWeaponInfo];
    entries.clear();

    const std::size_t expectedCount = (std::min)(CountExpectedEntries(pRoot), MaxEncodedEntryCount);
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

template <typename WeaponInfo> std::vector<WeaponModCodeEntry_t>* CWeaponModHandler<WeaponInfo>::FindEntries(WeaponInfo* pWeaponInfo)
{
    const auto iterator = m_EntriesByWeapon.find(pWeaponInfo);
    return iterator != m_EntriesByWeapon.end() ? &iterator->second : nullptr;
}

template <typename WeaponInfo>
bool CWeaponModHandler<WeaponInfo>::GetGroupRange(const std::vector<WeaponModCodeEntry_t>& entries, const WeaponModGroup_t& group,
                                                  std::size_t& firstEntry, std::size_t& entryCount) const
{
    firstEntry = group.m_FirstEntry;
    entryCount = group.m_EntryCount;
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
                                                          WeaponModGroup_t* pOutputGroups, std::uint32_t* pOutputGroupCount, OriginalFn&& original)
{
    if (!FindEntries(pWeaponInfo))
        return original();

    std::memset(pOutputGroups, 0, sizeof(WeaponModGroup_t) * StoredModGroupCount);
    *pOutputGroupCount = 0;

    KeyValues* pMods = pRoot->FindKey("Mods", false);
    for (KeyValues* pGroup = pMods ? pMods->GetFirstTrueSubKey() : nullptr; pGroup && *pOutputGroupCount < MaxModGroupCount;
         pGroup = pGroup->GetNextTrueSubKey())
    {
        m_pParseGroup(pGroup, pWeaponInfo, pWeaponName, &pOutputGroups[*pOutputGroupCount]);
        ++*pOutputGroupCount;
    }

    return pOutputGroupCount;
}

template <typename WeaponInfo>
template <typename OriginalFn>
std::uintptr_t CWeaponModHandler<WeaponInfo>::ParseGroup(KeyValues* pSection, WeaponInfo* pWeaponInfo, WeaponModGroup_t* pOutputGroup,
                                                         OriginalFn&& original)
{
    auto* pEntries = FindEntries(pWeaponInfo);
    if (!pEntries)
        return original(pSection, pOutputGroup);

    const std::size_t childCount = CountChildren(pSection);
    if (!CanAppendEntries(*pEntries, childCount))
        return 0;

    if (pEntries->capacity() < pEntries->size() + childCount)
        pEntries->reserve(pEntries->size() + childCount);

    const std::size_t firstEntryIndex = pEntries->size();
    KeyValues* pFirst = pSection->GetFirstSubKey();
    if (!pFirst)
    {
        SetCodeCount(pWeaponInfo, 0);
        const std::uintptr_t result = original(pSection, pOutputGroup);
        pOutputGroup->m_FirstEntry = firstEntryIndex < MaxEncodedEntryCount ? static_cast<std::uint16_t>(firstEntryIndex) : 0;
        SetCodeCount(pWeaponInfo, static_cast<std::uint32_t>(pEntries->size()));
        return result;
    }

    std::uint16_t groupName = 0;
    std::size_t parsedGroupEntryCount = 0;
    bool parsedFirstChunk = false;

    while (pFirst)
    {
        KeyValues* pLast = pFirst;
        std::uint32_t chunkNodeCount = 1;
        while (chunkNodeCount < RetailCodeLimit && pLast->GetNextKey())
        {
            pLast = pLast->GetNextKey();
            ++chunkNodeCount;
        }

        WeaponModGroup_t scratchGroup{};
        KeyValues* pNext;
        SetCodeCount(pWeaponInfo, 0);
        {
            CScopedKeyValuesChunk chunk(pSection->GetName(), pFirst, pLast);
            original(chunk.GetChunk(), &scratchGroup);
            pNext = chunk.GetNext();
        }

        const std::uint32_t parsedChunkEntryCount = GetCodeCount(pWeaponInfo);
        if (parsedChunkEntryCount > RetailCodeLimit || scratchGroup.m_EntryCount != parsedChunkEntryCount)
            return 0;

        if (!parsedFirstChunk)
        {
            groupName = scratchGroup.m_Name;
            parsedFirstChunk = true;
        }

        const auto* pScratchEntries = pWeaponInfo->m_WeaponMods.m_CodeEntries;
        pEntries->insert(pEntries->end(), pScratchEntries, pScratchEntries + parsedChunkEntryCount);
        parsedGroupEntryCount += parsedChunkEntryCount;
        pFirst = pNext;
    }

    pOutputGroup->m_Name = groupName;
    pOutputGroup->m_FirstEntry = static_cast<std::uint16_t>(firstEntryIndex);
    pOutputGroup->m_EntryCount = static_cast<std::uint16_t>(parsedGroupEntryCount);
    SetCodeCount(pWeaponInfo, static_cast<std::uint32_t>(pEntries->size()));
    return 0;
}

template <typename WeaponInfo>
template <typename OriginalFn>
std::uint8_t CWeaponModHandler<WeaponInfo>::Assemble(WeaponInfo* pWeaponInfo, OriginalFn&& original)
{
    auto* pEntries = FindEntries(pWeaponInfo);
    if (!pEntries)
        return original();

    CScopedWeaponModAssembly<WeaponInfo> activeAssembly(*this, pWeaponInfo, *pEntries);
    return original();
}

template <typename WeaponInfo> bool CWeaponModHandler<WeaponInfo>::HasActiveAssembly()
{
    return CScopedWeaponModAssembly<WeaponInfo>::GetActive() != nullptr;
}

template <typename WeaponInfo>
std::uintptr_t CWeaponModHandler<WeaponInfo>::ApplyActiveEntry(WeaponModCodeEntry_t* pEntry, std::uintptr_t setBaseValue, std::uintptr_t remove)
{
    CScopedWeaponModAssembly<WeaponInfo>* pAssembly = CScopedWeaponModAssembly<WeaponInfo>::GetActive();
    assert(pAssembly);
    return pAssembly->ApplyEntry(pEntry, setBaseValue, remove);
}

template <typename WeaponInfo>
void CWeaponModHandler<WeaponInfo>::PrecacheFlaggedAssets(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group,
                                                          const std::vector<WeaponModCodeEntry_t>& entries)
{
    std::size_t firstEntry;
    std::size_t entryCount;
    if (!GetGroupRange(entries, group, firstEntry, entryCount))
        return;

    for (std::size_t index = firstEntry; index < firstEntry + entryCount; ++index)
    {
        const auto& entry = entries[index];
        if (!entry.m_HasValue)
            continue;

        const auto& descriptor = m_pFieldDescriptors[entry.m_FieldIndex];
        if (descriptor.m_Type != 5)
            continue;

        const char* pValue = pWeaponInfo->m_StringPool.GetString(entry.GetStringOffset());
        if (!*pValue)
            continue;

        if (descriptor.m_Flags & 4)
            m_pPrecacheFlag4Asset(pValue);
        else if (descriptor.m_Flags & 8)
            m_pPrecacheFlag8Asset(pValue);
    }
}

template <typename WeaponInfo>
std::uintptr_t CWeaponModHandler<WeaponInfo>::PrecacheStringEntries(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group,
                                                                    const std::vector<WeaponModCodeEntry_t>& entries)
{
    std::size_t firstEntry;
    std::size_t entryCount;
    if (!GetGroupRange(entries, group, firstEntry, entryCount))
        return 0;

    for (std::size_t index = firstEntry; index < firstEntry + entryCount; ++index)
    {
        const auto& entry = entries[index];
        if (!entry.m_HasValue || m_pFieldDescriptors[entry.m_FieldIndex].m_Type != 7)
            continue;

        const std::uint16_t stringOffset = entry.GetStringOffset();
        if (stringOffset)
            m_pPrecacheString(pWeaponInfo->m_StringPool.GetString(stringOffset));
    }

    return entryCount;
}

template <typename WeaponInfo>
std::uintptr_t CWeaponModHandler<WeaponInfo>::PrecacheAllClientStrings(WeaponInfo* pWeaponInfo, const std::vector<WeaponModCodeEntry_t>& entries)
{
    std::uintptr_t result = 0;
    for (std::uint16_t fieldIndex = 687; fieldIndex <= 689; ++fieldIndex)
    {
        const std::uint16_t stringOffset =
            pWeaponInfo->m_CompiledData.template GetValue<std::uint16_t>(m_pFieldDescriptors[fieldIndex].m_CompiledOffset);
        if (stringOffset)
            result = m_pPrecacheString(pWeaponInfo->m_StringPool.GetString(stringOffset));
    }

    if (pWeaponInfo->m_WeaponMods.m_HasSinglePlayerBase)
        result = PrecacheStringEntries(pWeaponInfo, pWeaponInfo->m_WeaponMods.m_SinglePlayerBase, entries);

    if (pWeaponInfo->m_WeaponMods.m_HasMultiplayerBase)
        result = PrecacheStringEntries(pWeaponInfo, pWeaponInfo->m_WeaponMods.m_MultiplayerBase, entries);

    const WeaponModGroup_t* pGroups = GetModGroups(pWeaponInfo);
    const std::uint32_t groupCount = GetModGroupCount(pWeaponInfo);
    for (std::uint32_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
        result = PrecacheStringEntries(pWeaponInfo, pGroups[groupIndex], entries);

    return result;
}

template <typename WeaponInfo>
std::uintptr_t CWeaponModHandler<WeaponInfo>::NotifyStringFieldFromEntries(void* pOwner, std::uint16_t fieldIndex, WeaponInfo* pWeaponInfo,
                                                                           const std::vector<WeaponModCodeEntry_t>& entries)
{
    std::uintptr_t result = 0;
    const auto& descriptor = m_pFieldDescriptors[fieldIndex];
    const char* pDefaultValue = pWeaponInfo->m_CompiledData.template GetValue<const char*>(descriptor.m_CompiledOffset);
    if (pDefaultValue && *pDefaultValue)
        result = m_pNotifyStringField(pOwner);

    const WeaponModGroup_t* pGroups = GetModGroups(pWeaponInfo);
    const std::uint32_t groupCount = GetModGroupCount(pWeaponInfo);
    for (std::uint32_t groupIndex = 0; groupIndex < groupCount; ++groupIndex)
    {
        std::size_t firstEntry;
        std::size_t entryCount;
        if (!GetGroupRange(entries, pGroups[groupIndex], firstEntry, entryCount))
            continue;

        for (std::size_t index = firstEntry; index < firstEntry + entryCount; ++index)
        {
            const auto& entry = entries[index];
            if (entry.m_FieldIndex == fieldIndex && *pWeaponInfo->m_StringPool.GetString(entry.GetStringOffset()))
                result = m_pNotifyStringField(pOwner);
        }
    }

    return groupCount ? groupCount : result;
}

template <typename WeaponInfo>
template <typename OriginalFn>
void CWeaponModHandler<WeaponInfo>::PrecacheAssets(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, OriginalFn&& original)
{
    auto* pEntries = FindEntries(pWeaponInfo);
    if (!pEntries)
    {
        original();
        return;
    }

    PrecacheFlaggedAssets(pWeaponInfo, group, *pEntries);
}

template <typename WeaponInfo>
template <typename OriginalFn>
std::uintptr_t CWeaponModHandler<WeaponInfo>::PrecacheStrings(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, OriginalFn&& original)
{
    auto* pEntries = FindEntries(pWeaponInfo);
    return pEntries ? PrecacheStringEntries(pWeaponInfo, group, *pEntries) : original();
}

template <typename WeaponInfo>
template <typename OriginalFn>
void CWeaponModHandler<WeaponInfo>::PrecacheStringsNoResult(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, OriginalFn&& original)
{
    auto* pEntries = FindEntries(pWeaponInfo);
    if (!pEntries)
    {
        original();
        return;
    }

    PrecacheStringEntries(pWeaponInfo, group, *pEntries);
}

template <typename WeaponInfo>
template <typename OriginalFn>
std::uintptr_t CWeaponModHandler<WeaponInfo>::PrecacheAllStrings(WeaponInfo* pWeaponInfo, OriginalFn&& original)
{
    auto* pEntries = FindEntries(pWeaponInfo);
    return pEntries ? PrecacheAllClientStrings(pWeaponInfo, *pEntries) : original();
}

template <typename WeaponInfo>
template <typename OriginalFn>
std::uintptr_t CWeaponModHandler<WeaponInfo>::NotifyStringField(void* pOwner, std::uint16_t fieldIndex, OriginalFn&& original)
{
    WeaponInfo* pWeaponInfo = m_pGetWeaponInfo(pOwner);
    auto* pEntries = FindEntries(pWeaponInfo);
    return pEntries ? NotifyStringFieldFromEntries(pOwner, fieldIndex, pWeaponInfo, *pEntries) : original();
}

DECLARE_HOOK(InitializeWeaponInfo_Client, client.dll + 0x3CC990, [](auto& hook, ClientWeaponInfo_t* pWeaponInfo) -> std::uintptr_t
{
    g_ClientWeaponMods.InitializeWeaponInfo(pWeaponInfo);
    return hook.Original(pWeaponInfo);
});

DECLARE_HOOK(InitializeWeaponInfo_Server, server.dll + 0x6CB600, [](auto& hook, ServerWeaponInfo_t* pWeaponInfo) -> std::uintptr_t
{
    g_ServerWeaponMods.InitializeWeaponInfo(pWeaponInfo);
    return hook.Original(pWeaponInfo);
});

DECLARE_HOOK(ParseWeaponInfoFile_Client, client.dll + 0x3CFAC0,
             [](auto& hook, ClientWeaponInfo_t* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName) -> std::uint32_t*
{ return g_ClientWeaponMods.ParseWeaponInfo(pWeaponInfo, pRoot, [&]() { return hook.Original(pWeaponInfo, pRoot, pWeaponName); }); });

DECLARE_HOOK(ParseWeaponInfoFile_Server, server.dll + 0x6CE3F0,
             [](auto& hook, ServerWeaponInfo_t* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName) -> std::uint32_t*
{ return g_ServerWeaponMods.ParseWeaponInfo(pWeaponInfo, pRoot, [&]() { return hook.Original(pWeaponInfo, pRoot, pWeaponName); }); });

DECLARE_HOOK(ParseWeaponModGroups_Client, client.dll + 0x3D1020,
             [](auto& hook, ClientWeaponInfo_t* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName, WeaponModGroup_t* pOutputGroups,
                std::uint32_t* pOutputGroupCount) -> std::uint32_t*
{
    return g_ClientWeaponMods.ParseGroups(pWeaponInfo, pRoot, pWeaponName, pOutputGroups, pOutputGroupCount,
                                          [&]() { return hook.Original(pWeaponInfo, pRoot, pWeaponName, pOutputGroups, pOutputGroupCount); });
});

DECLARE_HOOK(ParseWeaponModGroups_Server, server.dll + 0x6CFA10,
             [](auto& hook, ServerWeaponInfo_t* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName, WeaponModGroup_t* pOutputGroups,
                std::uint32_t* pOutputGroupCount) -> std::uint32_t*
{
    return g_ServerWeaponMods.ParseGroups(pWeaponInfo, pRoot, pWeaponName, pOutputGroups, pOutputGroupCount,
                                          [&]() { return hook.Original(pWeaponInfo, pRoot, pWeaponName, pOutputGroups, pOutputGroupCount); });
});

DECLARE_HOOK(ParseWeaponModGroup_Client, client.dll + 0x3D15D0,
             [](auto& hook, KeyValues* pSection, ClientWeaponInfo_t* pWeaponInfo, const char* pWeaponName,
                WeaponModGroup_t* pOutputGroup) -> std::uintptr_t
{
    return g_ClientWeaponMods.ParseGroup(pSection, pWeaponInfo, pOutputGroup, [&](KeyValues* pValues, WeaponModGroup_t* pGroup)
    { return hook.Original(pValues, pWeaponInfo, pWeaponName, pGroup); });
});

DECLARE_HOOK(ParseWeaponModGroup_Server, server.dll + 0x6CFDE0,
             [](auto& hook, KeyValues* pSection, ServerWeaponInfo_t* pWeaponInfo, const char* pWeaponName,
                WeaponModGroup_t* pOutputGroup) -> std::uintptr_t
{
    return g_ServerWeaponMods.ParseGroup(pSection, pWeaponInfo, pOutputGroup, [&](KeyValues* pValues, WeaponModGroup_t* pGroup)
    { return hook.Original(pValues, pWeaponInfo, pWeaponName, pGroup); });
});

DECLARE_HOOK(AssembleWeaponMods_Client, client.dll + 0x3CA0B0,
             [](auto& hook, int enabledMods, ClientWeaponInfo_t* pWeaponInfo, void* pOutput, std::uint8_t singlePlayer,
                int removedMods) -> std::uint8_t
{ return g_ClientWeaponMods.Assemble(pWeaponInfo, [&]() { return hook.Original(enabledMods, pWeaponInfo, pOutput, singlePlayer, removedMods); }); });

DECLARE_HOOK(AssembleWeaponMods_Server, server.dll + 0x6C8B80,
             [](auto& hook, int enabledMods, ServerWeaponInfo_t* pWeaponInfo, void* pOutput, std::uint8_t singlePlayer,
                int removedMods) -> std::uint8_t
{ return g_ServerWeaponMods.Assemble(pWeaponInfo, [&]() { return hook.Original(enabledMods, pWeaponInfo, pOutput, singlePlayer, removedMods); }); });

DECLARE_HOOK(ApplyWeaponModEntry_Client, client.dll + 0x3C8EA0,
             [](auto& hook, WeaponModCodeEntry_t* pEntry, std::uintptr_t setBaseValue, std::uintptr_t remove) -> std::uintptr_t
{
    if (!CWeaponModHandler<ClientWeaponInfo_t>::HasActiveAssembly())
        return hook.Original(pEntry, setBaseValue, remove);

    return CWeaponModHandler<ClientWeaponInfo_t>::ApplyActiveEntry(pEntry, setBaseValue, remove);
});

DECLARE_HOOK(ApplyWeaponModEntry_Server, server.dll + 0x6C75D0,
             [](auto& hook, WeaponModCodeEntry_t* pEntry, std::uintptr_t setBaseValue, std::uintptr_t remove) -> std::uintptr_t
{
    if (!CWeaponModHandler<ServerWeaponInfo_t>::HasActiveAssembly())
        return hook.Original(pEntry, setBaseValue, remove);

    return CWeaponModHandler<ServerWeaponInfo_t>::ApplyActiveEntry(pEntry, setBaseValue, remove);
});

DECLARE_HOOK(PrecacheWeaponModAssets_Client, client.dll + 0x3D1ED0,
             [](auto& hook, ClientWeaponInfo_t* pWeaponInfo, const WeaponModGroup_t* pGroup) -> void
{ g_ClientWeaponMods.PrecacheAssets(pWeaponInfo, *pGroup, [&]() { hook.Original(pWeaponInfo, pGroup); }); });

DECLARE_HOOK(PrecacheWeaponModAssets_Server, server.dll + 0x6D0190,
             [](auto& hook, ServerWeaponInfo_t* pWeaponInfo, const WeaponModGroup_t* pGroup) -> void
{ g_ServerWeaponMods.PrecacheAssets(pWeaponInfo, *pGroup, [&]() { hook.Original(pWeaponInfo, pGroup); }); });

DECLARE_HOOK(PrecacheWeaponModStrings_Client, client.dll + 0x3D23E0,
             [](auto& hook, ClientWeaponInfo_t* pWeaponInfo, const WeaponModGroup_t* pGroup) -> std::uintptr_t
{ return g_ClientWeaponMods.PrecacheStrings(pWeaponInfo, *pGroup, [&]() { return hook.Original(pWeaponInfo, pGroup); }); });

DECLARE_HOOK(PrecacheWeaponModStrings_Server, server.dll + 0x6D0680,
             [](auto& hook, ServerWeaponInfo_t* pWeaponInfo, const WeaponModGroup_t* pGroup) -> void
{ g_ServerWeaponMods.PrecacheStringsNoResult(pWeaponInfo, *pGroup, [&]() { hook.Original(pWeaponInfo, pGroup); }); });

DECLARE_HOOK(PrecacheAllWeaponModStrings_Client, client.dll + 0x3D2480, [](auto& hook, ClientWeaponInfo_t* pWeaponInfo) -> std::uintptr_t
{ return g_ClientWeaponMods.PrecacheAllStrings(pWeaponInfo, [&]() { return hook.Original(pWeaponInfo); }); });

DECLARE_HOOK(NotifyWeaponModStringField_Client, client.dll + 0x3D41E0, [](auto& hook, void* pOwner, std::uint16_t fieldIndex) -> std::uintptr_t
{ return g_ClientWeaponMods.NotifyStringField(pOwner, fieldIndex, [&]() { return hook.Original(pOwner, fieldIndex); }); });

DECLARE_HOOK(NotifyWeaponModStringField_Server, server.dll + 0x6D1970, [](auto& hook, void* pOwner, std::uint16_t fieldIndex) -> std::uintptr_t
{ return g_ServerWeaponMods.NotifyStringField(pOwner, fieldIndex, [&]() { return hook.Original(pOwner, fieldIndex); }); });

ON_DLL_LOAD("server.dll", WeaponMods_Server, [](CModule module)
{
    g_ServerWeaponMods.Initialize(module);
    DISPATCH_MODULE(WeaponModHooks)
});

ON_DLL_LOAD("client.dll", WeaponMods_Client, [](CModule module)
{
    g_ClientWeaponMods.Initialize(module);
    DISPATCH_MODULE(WeaponModHooks)
});
