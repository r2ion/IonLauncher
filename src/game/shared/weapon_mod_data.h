#pragma once

#include "game/shared/weapon_parse.h"
#include "vscript/ivscript.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <vector>

class KeyValues;
struct SQObject;
class C_WeaponX;
class CWeaponX;
class CCommand;

void WeaponReparseServer(const CCommand& args);

template <typename WeaponInfo> class CWeaponModHandler
{
  public:
    void Reset();
    void InitializeWeaponInfo(WeaponInfo* weaponInfo);

    bool SetField(const WeaponInfo* weaponInfo, WeaponModValues* values, const char* fieldName, const SQObject& object, bool& valueSupported);
    void ApplyFieldOverrides(WeaponModValues* values) const;
    void ClearFieldOverrides(WeaponModValues* values);
    using ParseWeaponInfo_t = std::uint32_t* (*)(WeaponInfo*, KeyValues*, const char*);
    using ParseGroups_t = std::uint32_t* (*)(WeaponInfo*, KeyValues*, const char*, WeaponMod*, std::uint32_t*);
    using ParseGroup_t = KeyValues* (*)(KeyValues*, WeaponInfo*, const char*, WeaponMod*);
    using Assemble_t = std::uint8_t (*)(int, WeaponInfo*, WeaponModValues*, bool, int);
    using PrecacheAssetsClient_t = void (*)(WeaponInfo*, const WeaponMod*, float);
    using PrecacheAssetsServer_t = void (*)(WeaponInfo*, const WeaponMod*);
    using PrecacheStrings_t = int (*)(WeaponInfo*, const WeaponMod*);
    using PrecacheStringsNoResult_t = void (*)(WeaponInfo*, const WeaponMod*);
    using PrecacheAllStrings_t = int (*)(WeaponInfo*);
    using WeaponEntity = std::conditional_t<std::is_same_v<WeaponInfo, FileWeaponInfo_Client>, C_WeaponX, CWeaponX>;
    using NotifyStringField_t = void (*)(WeaponEntity*, std::uint16_t);

    std::uint32_t* ParseWeaponInfo(WeaponInfo* weaponInfo, KeyValues* root, const char* weaponName, ParseWeaponInfo_t original);
    std::uint32_t* ParseGroups(WeaponInfo* weaponInfo, KeyValues* root, const char* weaponName, WeaponMod* outputGroups,
                               std::uint32_t* outputGroupCount, ParseGroups_t original);
    KeyValues* ParseGroup(KeyValues* section, WeaponInfo* weaponInfo, const char* weaponName, WeaponMod* outputGroup, ParseGroup_t original);
    std::uint8_t Assemble(int enabledMods, WeaponInfo* weaponInfo, WeaponModValues* output, bool singlePlayer, int removedMods, Assemble_t original);

    static bool HasActiveAssembly();
    static bool ApplyActiveEntry(WeaponModEntry_t* entry, bool setBaseValue, bool remove);

    void PrecacheAssets(WeaponInfo* weaponInfo, const WeaponMod& group, float precacheValue, PrecacheAssetsClient_t original);
    void PrecacheAssets(WeaponInfo* weaponInfo, const WeaponMod& group, PrecacheAssetsServer_t original);
    int PrecacheStrings(WeaponInfo* weaponInfo, const WeaponMod& group, PrecacheStrings_t original);
    void PrecacheStringsNoResult(WeaponInfo* weaponInfo, const WeaponMod& group, PrecacheStringsNoResult_t original);
    int PrecacheAllStrings(WeaponInfo* weaponInfo, PrecacheAllStrings_t original);
    void NotifyStringField(WeaponEntity* weapon, WeaponModEntryType entryType, NotifyStringField_t original);

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

extern template class CWeaponModHandler<FileWeaponInfo_Client>;
extern template class CWeaponModHandler<FileWeaponInfo_Server>;
