#pragma once

#include "client/weapon_parse.h"
#include "server/weapon_parse.h"
#include "vscript/ivscript.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

class KeyValues;
struct SQObject;

template <typename WeaponInfo> class CWeaponModHandler
{
  public:
    void Reset();
    void InitializeWeaponInfo(WeaponInfo* weaponInfo);

    bool SetField(const WeaponInfo* weaponInfo, WeaponModValues* values, const char* fieldName, const SQObject& object, bool& valueSupported);
    void ApplyFieldOverrides(WeaponModValues* values) const;
    void ClearFieldOverrides(WeaponModValues* values);
    template <typename OriginalFn> std::uint32_t* ParseWeaponInfo(WeaponInfo* weaponInfo, KeyValues* root, OriginalFn&& original);

    template <typename OriginalFn>
    std::uint32_t* ParseGroups(WeaponInfo* weaponInfo, KeyValues* root, const char* weaponName, WeaponMod* outputGroups,
                               std::uint32_t* outputGroupCount, OriginalFn&& original);

    template <typename OriginalFn> KeyValues* ParseGroup(KeyValues* section, WeaponInfo* weaponInfo, WeaponMod* outputGroup, OriginalFn&& original);

    template <typename OriginalFn> std::uint8_t Assemble(WeaponInfo* weaponInfo, OriginalFn&& original);

    static bool HasActiveAssembly();
    static bool ApplyActiveEntry(WeaponModEntry_t* entry, bool setBaseValue, bool remove);

    template <typename OriginalFn> void PrecacheAssets(WeaponInfo* weaponInfo, const WeaponMod& group, OriginalFn&& original);

    template <typename OriginalFn> int PrecacheStrings(WeaponInfo* weaponInfo, const WeaponMod& group, OriginalFn&& original);

    template <typename OriginalFn> void PrecacheStringsNoResult(WeaponInfo* weaponInfo, const WeaponMod& group, OriginalFn&& original);

    template <typename OriginalFn> int PrecacheAllStrings(WeaponInfo* weaponInfo, OriginalFn&& original);

    template <typename NotifyFn, typename OriginalFn>
    void NotifyStringField(WeaponInfo* weaponInfo, WeaponModEntryType entryType, NotifyFn&& notify, OriginalFn&& original);

  private:
    static std::size_t CountChildren(KeyValues* section);
    static std::size_t CountExpectedEntries(KeyValues* root);
    static bool CanAppendEntries(const std::vector<WeaponModEntry_t>& entries, std::size_t groupEntryCount);
    static bool WeaponFieldValueToData(const ScriptVariant_t& value, ScriptDataType_t fieldType,
                                       std::array<std::byte, sizeof(WeaponModEntry_t::value)>& output, std::size_t& outputSize);

    std::uint32_t GetCodeCount(const WeaponInfo* weaponInfo) const;
    void SetCodeCount(WeaponInfo* weaponInfo, std::uint32_t count) const;
    std::uint32_t GetModGroupCount(const WeaponInfo* weaponInfo) const;
    const WeaponMod* GetModGroups(const WeaponInfo* weaponInfo) const;
    void PrepareWeaponParse(WeaponInfo* weaponInfo, KeyValues* root);
    const char* GetWeaponName(const WeaponInfo* weaponInfo) const;
    void FinishWeaponParse(WeaponInfo* weaponInfo);
    std::vector<WeaponModEntry_t>* FindEntries(WeaponInfo* weaponInfo);
    bool GetGroupRange(const std::vector<WeaponModEntry_t>& entries, const WeaponMod& group, std::size_t& firstEntry, std::size_t& entryCount) const;
    void PrecacheFlaggedAssets(WeaponInfo* weaponInfo, const WeaponMod& group, const std::vector<WeaponModEntry_t>& entries);
    int PrecacheStringEntries(WeaponInfo* weaponInfo, const WeaponMod& group, const std::vector<WeaponModEntry_t>& entries);
    int PrecacheAllClientStrings(WeaponInfo* weaponInfo, const std::vector<WeaponModEntry_t>& entries);

    template <typename NotifyFn>
    void NotifyStringFieldFromEntries(WeaponModEntryType entryType, WeaponInfo* weaponInfo, const std::vector<WeaponModEntry_t>& entries,
                                      NotifyFn&& notify);

    struct FieldOverride
    {
        std::uint16_t offset;
        std::uint16_t size;
        std::array<std::byte, sizeof(WeaponModEntry_t::value)> value;
    };

    std::unordered_map<WeaponInfo*, std::vector<WeaponModEntry_t>> m_EntriesByWeapon;
    std::unordered_map<WeaponModValues*, std::vector<FieldOverride>> m_FieldOverridesByValues;
};

extern CWeaponModHandler<FileWeaponInfo_Client> g_ClientWeaponMods;
extern CWeaponModHandler<FileWeaponInfo_Server> g_ServerWeaponMods;

extern template bool CWeaponModHandler<FileWeaponInfo_Client>::SetField(const FileWeaponInfo_Client* weaponInfo, WeaponModValues* values,
                                                                        const char* fieldName, const SQObject& object, bool& valueSupported);
extern template bool CWeaponModHandler<FileWeaponInfo_Server>::SetField(const FileWeaponInfo_Server* weaponInfo, WeaponModValues* values,
                                                                        const char* fieldName, const SQObject& object, bool& valueSupported);
