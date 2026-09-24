#pragma once

#include "vscript/ivscript.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
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

using PrecacheWeaponModAssetFn = std::uintptr_t (*)(const char*);
using PrecacheClientWeaponModFlag4AssetFn = std::uintptr_t (*)(const char*, std::uintptr_t, float);
using PrecacheWeaponModStringFn = std::uintptr_t (*)(const char*);

template <typename WeaponInfo> using GetWeaponModInfoFn = WeaponInfo* (*)(void*);

using NotifyWeaponModStringFieldFn = void (*)(void*, const char*);
using InsertWeaponModAssemblyItemFn = std::uintptr_t (*)(WeaponModCodeEntry_t*, const WeaponFieldDescriptor_t*, WeaponModAssemblyItem_t*);
template <typename WeaponInfo> class CScopedWeaponModAssembly;

template <typename WeaponInfo> class CWeaponModHandler
{
  public:
    void Initialize(const CModule& module);
    void InitializeWeaponInfo(WeaponInfo* pWeaponInfo);

    bool SetRuntimeField(void* pWeapon, void* pRuntimeValues, const char* pFieldName, const ScriptVariant_t& value);
    template <typename OriginalFn> std::uint32_t* ParseWeaponInfo(WeaponInfo* pWeaponInfo, KeyValues* pRoot, OriginalFn&& original);

    template <typename OriginalFn>
    std::uint32_t* ParseGroups(WeaponInfo* pWeaponInfo, KeyValues* pRoot, const char* pWeaponName, WeaponModGroup_t* pOutputGroups,
                               std::uint32_t* pOutputGroupCount, OriginalFn&& original);

    template <typename OriginalFn>
    std::uintptr_t ParseGroup(KeyValues* pSection, WeaponInfo* pWeaponInfo, WeaponModGroup_t* pOutputGroup, OriginalFn&& original);

    template <typename OriginalFn> std::uint8_t Assemble(WeaponInfo* pWeaponInfo, OriginalFn&& original);

    static bool HasActiveAssembly();
    static std::uintptr_t ApplyActiveEntry(WeaponModCodeEntry_t* pEntry, std::uintptr_t setBaseValue, std::uintptr_t remove);

    template <typename OriginalFn>
    void PrecacheAssets(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, float precacheValue, OriginalFn&& original);

    template <typename OriginalFn> std::uintptr_t PrecacheStrings(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, OriginalFn&& original);

    template <typename OriginalFn> void PrecacheStringsNoResult(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, OriginalFn&& original);

    template <typename OriginalFn> std::uintptr_t PrecacheAllStrings(WeaponInfo* pWeaponInfo, OriginalFn&& original);

    template <typename OriginalFn> void NotifyStringField(void* pOwner, std::uint16_t fieldIndex, OriginalFn&& original);

  private:
    friend class CScopedWeaponModAssembly<WeaponInfo>;

    static std::size_t CountChildren(KeyValues* pSection);
    static std::size_t CountExpectedEntries(KeyValues* pRoot);
    static bool CanAppendEntries(const std::vector<WeaponModCodeEntry_t>& entries, std::size_t groupEntryCount);
    static bool WeaponFieldValueToString(const ScriptVariant_t& value, std::string& output);
    static bool WeaponFieldValueToData(const ScriptVariant_t& value, ScriptDataType_t fieldType,
                                       std::array<std::byte, sizeof(WeaponModCodeEntry_t::m_Value)>& output, std::size_t& outputSize);

    std::uint32_t GetCodeCount(const WeaponInfo* pWeaponInfo) const;
    void SetCodeCount(WeaponInfo* pWeaponInfo, std::uint32_t count) const;
    std::uint32_t GetModGroupCount(const WeaponInfo* pWeaponInfo) const;
    const WeaponModGroup_t* GetModGroups(const WeaponInfo* pWeaponInfo) const;
    void PrepareWeaponParse(WeaponInfo* pWeaponInfo, KeyValues* pRoot);
    const char* GetWeaponName(const WeaponInfo* pWeaponInfo) const;
    void FinishWeaponParse(WeaponInfo* pWeaponInfo);
    std::vector<WeaponModCodeEntry_t>* FindEntries(WeaponInfo* pWeaponInfo);
    bool GetGroupRange(const std::vector<WeaponModCodeEntry_t>& entries, const WeaponModGroup_t& group, std::size_t& firstEntry,
                       std::size_t& entryCount) const;
    void PrecacheFlaggedAssets(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, const std::vector<WeaponModCodeEntry_t>& entries,
                               float precacheValue);
    std::uintptr_t PrecacheStringEntries(WeaponInfo* pWeaponInfo, const WeaponModGroup_t& group, const std::vector<WeaponModCodeEntry_t>& entries);
    std::uintptr_t PrecacheAllClientStrings(WeaponInfo* pWeaponInfo, const std::vector<WeaponModCodeEntry_t>& entries);
    void NotifyStringFieldFromEntries(void* pOwner, std::uint16_t fieldIndex, WeaponInfo* pWeaponInfo,
                                      const std::vector<WeaponModCodeEntry_t>& entries);

    ParseWeaponModGroupFn<WeaponInfo> m_pParseGroup = nullptr;
    const char* m_pParseGroupHookName = nullptr;
    const WeaponFieldDescriptor_t* m_pFieldDescriptors = nullptr;
    PrecacheWeaponModAssetFn m_pPrecacheFlag4Asset = nullptr;
    PrecacheClientWeaponModFlag4AssetFn m_pPrecacheClientFlag4Asset = nullptr;
    PrecacheWeaponModAssetFn m_pPrecacheFlag8Asset = nullptr;
    PrecacheWeaponModStringFn m_pPrecacheString = nullptr;
    GetWeaponModInfoFn<WeaponInfo> m_pGetWeaponInfo = nullptr;
    NotifyWeaponModStringFieldFn m_pNotifyStringField = nullptr;
    InsertWeaponModAssemblyItemFn m_pInsertAssemblyItem = nullptr;
    std::unordered_map<WeaponInfo*, std::vector<WeaponModCodeEntry_t>> m_EntriesByWeapon;
};

extern CWeaponModHandler<ClientWeaponInfo_t> g_ClientWeaponMods;
extern CWeaponModHandler<ServerWeaponInfo_t> g_ServerWeaponMods;

extern template bool CWeaponModHandler<ClientWeaponInfo_t>::SetRuntimeField(void* pWeapon, void* pRuntimeValues, const char* pFieldName,
                                                                            const ScriptVariant_t& value);
extern template bool CWeaponModHandler<ServerWeaponInfo_t>::SetRuntimeField(void* pWeapon, void* pRuntimeValues, const char* pFieldName,
                                                                            const ScriptVariant_t& value);

static_assert(sizeof(WeaponModCodeEntry_t) == 0x10);
static_assert(sizeof(WeaponModGroup_t) == 0x6);
static_assert(sizeof(WeaponModAssemblyItem_t) == 0x20);
static_assert(sizeof(WeaponFieldDescriptor_t) == 0x20);
static_assert(sizeof(WeaponInfoStringPool_t) == 0xC04);
static_assert(sizeof(WeaponInfoCompiledData_t) == 0xCA0);
static_assert(sizeof(WeaponModData_t) == 0xD58);
static_assert(sizeof(ClientWeaponInfo_t) == 0x2E60);
static_assert(sizeof(ServerWeaponInfo_t) == 0x2CA8);
