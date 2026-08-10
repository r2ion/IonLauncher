#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

class CModule;
class KeyValues;

struct WeaponModCodeEntry_t
{
    std::uint16_t m_FieldIndex;
    std::uint16_t m_HasValue;
    std::byte m_Value[12];

    std::uint16_t GetStringOffset() const;
};

struct WeaponModAssemblyItem_t
{
    WeaponModCodeEntry_t* m_pEntry = nullptr;
    WeaponModAssemblyItem_t* m_pNext = nullptr;
    std::byte m_Reserved0[8]{};
    std::uint8_t m_SetBaseValue = 0;
    std::uint8_t m_Remove = 0;
    std::byte m_Reserved1[6]{};
};

struct WeaponModGroup_t
{
    std::uint16_t m_Name;
    std::uint16_t m_FirstEntry;
    std::uint16_t m_EntryCount;
};

struct WeaponFieldDescriptor_t
{
    std::byte m_Reserved0[25];
    std::uint8_t m_Type;
    std::uint8_t m_Flags;
    std::byte m_Reserved1[3];
    std::uint16_t m_CompiledOffset;
};

struct WeaponInfoStringPool_t
{
    char m_Data[0xC00];
    std::uint32_t m_Used;

    const char* GetString(const std::uint16_t offset) const
    {
        return m_Data + offset;
    }
};

struct WeaponInfoCompiledData_t
{
    std::byte m_Data[0xCA0];

    template <typename T> T GetValue(const std::uint16_t offset) const
    {
        T value;
        std::memcpy(&value, m_Data + offset, sizeof(value));
        return value;
    }
};

struct WeaponModData_t
{
    WeaponModGroup_t m_Groups[32];
    WeaponModCodeEntry_t m_CodeEntries[200];
    std::uint32_t m_GroupCount;
    std::uint32_t m_CodeEntryCount;
    WeaponModGroup_t m_SinglePlayerBase;
    std::uint8_t m_HasSinglePlayerBase;
    std::byte m_Reserved0;
    WeaponModGroup_t m_MultiplayerBase;
    std::uint8_t m_HasMultiplayerBase;
    std::byte m_Reserved1;
};

struct ClientWeaponInfo_t
{
    std::byte m_Reserved0[0x66C];
    WeaponInfoStringPool_t m_StringPool;
    WeaponInfoCompiledData_t m_CompiledData;
    WeaponModData_t m_WeaponMods;
    std::byte m_Reserved1[0x1F8];
};

struct ServerWeaponInfo_t
{
    std::byte m_Reserved0[0x680];
    WeaponInfoStringPool_t m_StringPool;
    std::byte m_Reserved1[4];
    WeaponInfoCompiledData_t m_CompiledData;
    WeaponModData_t m_WeaponMods;
    std::byte m_Reserved2[0x28];
};

template <typename WeaponInfo> using ParseWeaponModGroupFn = std::uintptr_t (*)(KeyValues*, WeaponInfo*, const char*, WeaponModGroup_t*);

using PrecacheWeaponModAssetFn = void (*)(const char*);
using PrecacheWeaponModStringFn = std::uintptr_t (*)(const char*);

template <typename WeaponInfo> using GetWeaponModInfoFn = WeaponInfo* (*)(void*);

using NotifyWeaponModStringFieldFn = std::uintptr_t (*)(void*, const char*);
using InsertWeaponModAssemblyItemFn = std::uintptr_t (*)(WeaponModCodeEntry_t*, const WeaponFieldDescriptor_t*, WeaponModAssemblyItem_t*);
template <typename WeaponInfo> class CScopedWeaponModAssembly;

template <typename WeaponInfo> class CWeaponModHandler
{
  public:
    void Initialize(const CModule& module);
    void InitializeWeaponInfo(WeaponInfo* pWeaponInfo);

    template <typename OriginalFn> std::uint32_t* ParseWeaponInfo(WeaponInfo* pWeaponInfo, KeyValues* pRoot, OriginalFn&& original);

    template <typename OriginalFn>
    std::uint32_t* ParseGroups(WeaponInfo* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName, WeaponModGroup_t* pOutputGroups,
                               std::uint32_t* pOutputGroupCount, OriginalFn&& original);

    template <typename OriginalFn>
    std::uintptr_t ParseGroup(KeyValues* pSection, WeaponInfo* pWeaponInfo, WeaponModGroup_t* pOutputGroup, OriginalFn&& original);

    template <typename OriginalFn> std::uint8_t Assemble(WeaponInfo* pWeaponInfo, OriginalFn&& original);

    static bool HasActiveAssembly();
    static std::uintptr_t ApplyActiveEntry(WeaponModCodeEntry_t* pEntry, std::uintptr_t setBaseValue, std::uintptr_t remove);

    template <typename OriginalFn> void PrecacheAssets(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, OriginalFn&& original);

    template <typename OriginalFn> std::uintptr_t PrecacheStrings(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, OriginalFn&& original);

    template <typename OriginalFn> void PrecacheStringsNoResult(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, OriginalFn&& original);

    template <typename OriginalFn> std::uintptr_t PrecacheAllStrings(WeaponInfo* pWeaponInfo, OriginalFn&& original);

    template <typename OriginalFn> std::uintptr_t NotifyStringField(void* pOwner, std::uint16_t fieldIndex, OriginalFn&& original);

  private:
    friend class CScopedWeaponModAssembly<WeaponInfo>;

    static std::size_t CountChildren(KeyValues* pSection);
    static std::size_t CountExpectedEntries(KeyValues* pRoot);
    static bool CanAppendEntries(const std::vector<WeaponModCodeEntry_t>& entries, std::size_t groupEntryCount);

    std::uint32_t GetCodeCount(const WeaponInfo* pWeaponInfo) const;
    void SetCodeCount(WeaponInfo* pWeaponInfo, std::uint32_t count) const;
    std::uint32_t GetModGroupCount(const WeaponInfo* pWeaponInfo) const;
    const WeaponModGroup_t* GetModGroups(const WeaponInfo* pWeaponInfo) const;
    void PrepareWeaponParse(WeaponInfo* pWeaponInfo, KeyValues* pRoot);
    void FinishWeaponParse(WeaponInfo* pWeaponInfo);
    std::vector<WeaponModCodeEntry_t>* FindEntries(WeaponInfo* pWeaponInfo);
    bool GetGroupRange(const std::vector<WeaponModCodeEntry_t>& entries, const WeaponModGroup_t& group, std::size_t& firstEntry,
                       std::size_t& entryCount) const;
    void PrecacheFlaggedAssets(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, const std::vector<WeaponModCodeEntry_t>& entries);
    std::uintptr_t PrecacheStringEntries(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, const std::vector<WeaponModCodeEntry_t>& entries);
    std::uintptr_t PrecacheAllClientStrings(WeaponInfo* pWeaponInfo, const std::vector<WeaponModCodeEntry_t>& entries);
    std::uintptr_t NotifyStringFieldFromEntries(void* pOwner, std::uint16_t fieldIndex, WeaponInfo* pWeaponInfo,
                                                const std::vector<WeaponModCodeEntry_t>& entries);

    ParseWeaponModGroupFn<WeaponInfo> m_pParseGroup = nullptr;
    const WeaponFieldDescriptor_t* m_pFieldDescriptors = nullptr;
    PrecacheWeaponModAssetFn m_pPrecacheFlag4Asset = nullptr;
    PrecacheWeaponModAssetFn m_pPrecacheFlag8Asset = nullptr;
    PrecacheWeaponModStringFn m_pPrecacheString = nullptr;
    GetWeaponModInfoFn<WeaponInfo> m_pGetWeaponInfo = nullptr;
    NotifyWeaponModStringFieldFn m_pNotifyStringField = nullptr;
    InsertWeaponModAssemblyItemFn m_pInsertAssemblyItem = nullptr;
    std::unordered_map<WeaponInfo*, std::vector<WeaponModCodeEntry_t>> m_EntriesByWeapon;
};

static_assert(sizeof(WeaponModCodeEntry_t) == 0x10);
static_assert(offsetof(WeaponModCodeEntry_t, m_FieldIndex) == 0x0);
static_assert(offsetof(WeaponModCodeEntry_t, m_HasValue) == 0x2);
static_assert(offsetof(WeaponModCodeEntry_t, m_Value) == 0x4);
static_assert(sizeof(WeaponModGroup_t) == 0x6);
static_assert(offsetof(WeaponModGroup_t, m_Name) == 0x0);
static_assert(offsetof(WeaponModGroup_t, m_FirstEntry) == 0x2);
static_assert(offsetof(WeaponModGroup_t, m_EntryCount) == 0x4);
static_assert(sizeof(WeaponModAssemblyItem_t) == 0x20);
static_assert(offsetof(WeaponModAssemblyItem_t, m_pEntry) == 0x0);
static_assert(offsetof(WeaponModAssemblyItem_t, m_pNext) == 0x8);
static_assert(offsetof(WeaponModAssemblyItem_t, m_SetBaseValue) == 0x18);
static_assert(offsetof(WeaponModAssemblyItem_t, m_Remove) == 0x19);
static_assert(sizeof(WeaponFieldDescriptor_t) == 0x20);
static_assert(offsetof(WeaponFieldDescriptor_t, m_Type) == 0x19);
static_assert(offsetof(WeaponFieldDescriptor_t, m_Flags) == 0x1A);
static_assert(offsetof(WeaponFieldDescriptor_t, m_CompiledOffset) == 0x1E);
static_assert(sizeof(WeaponInfoStringPool_t) == 0xC04);
static_assert(offsetof(WeaponInfoStringPool_t, m_Used) == 0xC00);
static_assert(sizeof(WeaponInfoCompiledData_t) == 0xCA0);
static_assert(sizeof(WeaponModData_t) == 0xD58);
static_assert(offsetof(WeaponModData_t, m_Groups) == 0x0);
static_assert(offsetof(WeaponModData_t, m_CodeEntries) == 0xC0);
static_assert(offsetof(WeaponModData_t, m_GroupCount) == 0xD40);
static_assert(offsetof(WeaponModData_t, m_CodeEntryCount) == 0xD44);
static_assert(offsetof(WeaponModData_t, m_SinglePlayerBase) == 0xD48);
static_assert(offsetof(WeaponModData_t, m_HasSinglePlayerBase) == 0xD4E);
static_assert(offsetof(WeaponModData_t, m_MultiplayerBase) == 0xD50);
static_assert(offsetof(WeaponModData_t, m_HasMultiplayerBase) == 0xD56);
static_assert(sizeof(ClientWeaponInfo_t) == 0x2E60);
static_assert(offsetof(ClientWeaponInfo_t, m_StringPool) == 0x66C);
static_assert(offsetof(ClientWeaponInfo_t, m_CompiledData) == 0x1270);
static_assert(offsetof(ClientWeaponInfo_t, m_WeaponMods) == 0x1F10);
static_assert(sizeof(ServerWeaponInfo_t) == 0x2CA8);
static_assert(offsetof(ServerWeaponInfo_t, m_StringPool) == 0x680);
static_assert(offsetof(ServerWeaponInfo_t, m_CompiledData) == 0x1288);
static_assert(offsetof(ServerWeaponInfo_t, m_WeaponMods) == 0x1F28);
