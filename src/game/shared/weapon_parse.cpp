#include "game/shared/weapon_parse.h"

#include "engine/IVModelInfo.h"
#include "engine/server/server.h"
#include "game/client/c_baseentity.h"
#include "game/client/c_player.h"
#include "game/server/baseentity.h"
#include "game/client/r1/c_weapon_x.h"
#include "game/server/r1/weapon_x.h"
#include "game/shared/weapon_mod_data.h"
#include "modsystem/modmanager.h"
#include "rtech/rstdlib.h"
#include "tier0/callbacks.h"
#include "util/utils.h"

#include <bitset>
#include <mutex>

DECLARE_MODULE(ClientWeaponParseHooks)
DECLARE_MODULE(ServerWeaponParseHooks)

const WeaponCallbackInfo* g_scriptCBInfo;

constexpr const char* SUPPORTED_SPECIAL_WEAPON_MOD_FIELDS[] = {
    "fire_mode",
    "aimassist_adspull_weaponclass",
    "damage_flags",
    "explosion_damage_flags",
    "ammo_suck_behavior",
    "damage_falloff_type",
    "viewkick_spring",
    "smart_ammo_hud_type",
    "smart_ammo_hud_lock_style",
    "smart_ammo_weapon_type",
    "smart_ammo_lock_type",
};

template <typename WeaponInfo> static WeaponInfo* (*GetFileWeaponInfoFromHandle)(WEAPON_FILE_INFO_HANDLE);
template <typename WeaponInfo> static KeyValues* (*ParseSingleMod)(KeyValues*, WeaponInfo*, const char*, WeaponMod*);
template <typename WeaponInfo>
bool (*AddEntryToAssembly_GeneralPriority)(WeaponModEntry_t*, const WeaponModParseTableEntry*, WeaponModAssemblyItem_t*);
template <typename WeaponInfo> static int (*PrecacheImpactEffectTable)(const char*);
template <typename WeaponInfo> static int (*PrecacheParticleSystemByName)(const char*);
template <typename WeaponInfo> static RHashMapString* s_MPEDict;
template <typename WeaponInfo> static const WeaponModParseTableEntry* ModParseEntries;

FileWeaponInfo_Client* (*GetFileWeaponInfoFromName_Client)(const char*);
void (*ParseFileWeaponInfo)(FileWeaponInfo_Client*, KeyValues*, const char*);
bool (*ReadWeaponDataFromFileForSlot_Client)(IFileSystem*, const char*, WEAPON_FILE_INFO_HANDLE*, const unsigned char*);
KeyValues* (*ReadEncryptedKVFile)(IFileSystem*, const char*, const unsigned char*);
WeaponString_t (*AllocWeaponString_Client)(FileWeaponInfo_Client*, const char*);
bool (*GetIndexForModName_Client)(const char*, const FileWeaponInfo_Client*, unsigned int*);
bool (*CalcWeaponModsForGameType)(unsigned int, const FileWeaponInfo_Client*, WeaponModValues*, bool, unsigned int);
IVModelInfoClient** clientmodelinfo;
template <typename WeaponInfo> static std::bitset<MAX_WEAPON_MOD_PARSE_ENTRIES> g_SupportedSpecialModEntries;

template <typename WeaponInfo> WeaponInfo* WeaponParse<WeaponInfo>::GetFileWeaponInfoFromHandle(const WEAPON_FILE_INFO_HANDLE handle)
{
    return ::GetFileWeaponInfoFromHandle<WeaponInfo>(handle);
}

template <typename WeaponInfo> const WeaponModParseTableEntry* WeaponParse<WeaponInfo>::FindModParseEntry(const char* fieldName)
{
    const auto* entryType = static_cast<const std::uint32_t*>(s_MPEDict<WeaponInfo>->Find(fieldName));
    return entryType && *entryType != WMET_INVALID && *entryType < MAX_WEAPON_MOD_PARSE_ENTRIES ? &ModParseEntries<WeaponInfo>[*entryType] : nullptr;
}

template <typename WeaponInfo> const WeaponModParseTableEntry* WeaponParse<WeaponInfo>::GetModParseEntry(const WeaponModEntryType entryType)
{
    const auto index = static_cast<std::uint16_t>(entryType);
    return index < MAX_WEAPON_MOD_PARSE_ENTRIES ? &ModParseEntries<WeaponInfo>[index] : nullptr;
}

template <typename WeaponInfo> bool WeaponParse<WeaponInfo>::IsSupportedSpecialModEntry(const WeaponModEntryType entryType)
{
    const auto index = static_cast<std::uint16_t>(entryType);
    const auto& supportedEntries = g_SupportedSpecialModEntries<WeaponInfo>;
    return index < MAX_WEAPON_MOD_PARSE_ENTRIES && supportedEntries.test(index);
}

template <typename WeaponInfo>
KeyValues* WeaponParse<WeaponInfo>::ParseSingleMod(KeyValues* section, WeaponInfo* info, const char* weaponName, WeaponMod* mod)
{
    return ::ParseSingleMod<WeaponInfo>(section, info, weaponName, mod);
}

template <typename WeaponInfo>
bool WeaponParse<WeaponInfo>::AddEntryToAssemblyGeneralPriority(WeaponModEntry_t* entry, const WeaponModParseTableEntry* parseEntry,
                                                                WeaponModAssemblyItem_t* item)
{
    return AddEntryToAssembly_GeneralPriority<WeaponInfo>(entry, parseEntry, item);
}

template <typename WeaponInfo> void WeaponParse<WeaponInfo>::PrecacheModAsset(const WeaponModParseTableEntry& parseEntry, const char* assetName)
{
    if (parseEntry.parseFlags & WMPF_IMPACT_EFFECT_TABLE)
        PrecacheImpactEffectTable<WeaponInfo>(assetName);
    else if (parseEntry.parseFlags & WMPF_PARTICLE_SYSTEM)
        PrecacheParticleSystemByName<WeaponInfo>(assetName);
}

template <typename WeaponInfo> int WeaponParse<WeaponInfo>::PrecacheModel(const char* modelName)
{
    if constexpr (std::is_same_v<WeaponInfo, FileWeaponInfo_Client>)
        return (*clientmodelinfo)->GetModelIndex(modelName);
    else
        return CBaseEntity::PrecacheModel(modelName);
}

template <typename WeaponInfo> void WeaponParse<WeaponInfo>::PrecacheScriptSound(BaseEntity* owner, const char* soundName)
{
    owner->PrecacheScriptSound(soundName);
}

template <typename WeaponInfo> static void ResolveSupportedSpecialModEntries()
{
    auto& supportedEntries = g_SupportedSpecialModEntries<WeaponInfo>;
    supportedEntries.reset();
    for (const char* fieldName : SUPPORTED_SPECIAL_WEAPON_MOD_FIELDS)
    {
        const WeaponModParseTableEntry* entry = WeaponParse<WeaponInfo>::FindModParseEntry(fieldName);
        const auto entryType = entry ? static_cast<std::uint16_t>(entry->sanityCheckType) : 0;
        if (entry && entry->parseType == WeaponModValueType::Special && entryType < MAX_WEAPON_MOD_PARSE_ENTRIES)
            supportedEntries.set(entryType);
    }
}

template struct WeaponParse<FileWeaponInfo_Client>;
template struct WeaponParse<FileWeaponInfo_Server>;

template <typename WeaponInfo> static CWeaponModHandler<WeaponInfo>& GetWeaponModHandler()
{
    if constexpr (std::is_same_v<WeaponInfo, FileWeaponInfo_Client>)
        return g_ClientWeaponMods;
    else
    {
        static_assert(std::is_same_v<WeaponInfo, FileWeaponInfo_Server>);
        return g_ServerWeaponMods;
    }
}

template <typename WeaponInfo> static void ResetExtendedWeaponInfo(WeaponInfo* weaponInfo)
{
    if constexpr (std::is_same_v<WeaponInfo, FileWeaponInfo_Client>)
        C_WeaponXExtended::ResetWeaponInfo(weaponInfo);
    else
        CWeaponXExtended::ResetWeaponInfo(weaponInfo);
}

template <typename WeaponInfo> static void ParseExtendedWeaponInfo(WeaponInfo* weaponInfo, KeyValues* root, const char* weaponName)
{
    if constexpr (std::is_same_v<WeaponInfo, FileWeaponInfo_Client>)
        C_WeaponXExtended::ParseWeaponInfo(weaponInfo, root, weaponName);
    else
        CWeaponXExtended::ParseWeaponInfo(weaponInfo, root, weaponName);
}

template <typename WeaponInfo> static auto MakeInitFileWeaponInfoHook()
{
    return [](auto& hook, WeaponInfo* weaponInfo) -> std::uintptr_t
    {
        GetWeaponModHandler<WeaponInfo>().InitializeWeaponInfo(weaponInfo);
        ResetExtendedWeaponInfo(weaponInfo);
        return hook.Original(weaponInfo);
    };
}

template <typename WeaponInfo> static auto MakeParseFileWeaponInfoHook()
{
    return [](auto& hook, WeaponInfo* weaponInfo, KeyValues* root, const char* weaponName) -> std::uint32_t*
    {
        ParseExtendedWeaponInfo(weaponInfo, root, weaponName);
        return GetWeaponModHandler<WeaponInfo>().ParseWeaponInfo(weaponInfo, root, weaponName, hook.s_original);
    };
}

template <typename WeaponInfo> static auto MakeParseModsBlockHook()
{
    return [](auto& hook, WeaponInfo* weaponInfo, KeyValues* root, const char* weaponName, WeaponMod* outputGroups,
              std::uint32_t* outputGroupCount) -> std::uint32_t*
    { return GetWeaponModHandler<WeaponInfo>().ParseGroups(weaponInfo, root, weaponName, outputGroups, outputGroupCount, hook.s_original); };
}

template <typename WeaponInfo> static auto MakeParseSingleModHook()
{
    return [](auto& hook, KeyValues* section, WeaponInfo* weaponInfo, const char* weaponName, WeaponMod* outputGroup) -> KeyValues*
    { return GetWeaponModHandler<WeaponInfo>().ParseGroup(section, weaponInfo, weaponName, outputGroup, hook.s_original); };
}

template <typename WeaponInfo> static auto MakeCalcWeaponModsForGameTypeHook()
{
    return [](auto& hook, int enabledMods, WeaponInfo* weaponInfo, WeaponModValues* output, bool singlePlayer, int removedMods) -> std::uint8_t
    {
        auto& weaponMods = GetWeaponModHandler<WeaponInfo>();
        const auto result = weaponMods.Assemble(enabledMods, weaponInfo, output, singlePlayer, removedMods, hook.s_original);
        weaponMods.ApplyFieldOverrides(output);
        return result;
    };
}

template <typename WeaponInfo> static auto MakeAddEntryToAssemblyHook()
{
    return [](auto& hook, WeaponModEntry_t* entry, bool setBaseValue, bool remove) -> bool
    {
        if (!CWeaponModHandler<WeaponInfo>::HasActiveAssembly())
            return hook.Original(entry, setBaseValue, remove);
        return CWeaponModHandler<WeaponInfo>::ApplyActiveEntry(entry, setBaseValue, remove);
    };
}

template <typename WeaponInfo> static auto MakePrecacheWeaponModAssetsHook()
{
    if constexpr (std::is_same_v<WeaponInfo, FileWeaponInfo_Client>)
    {
        return [](auto& hook, WeaponInfo* weaponInfo, const WeaponMod* group, float precacheValue) -> void
        { GetWeaponModHandler<WeaponInfo>().PrecacheAssets(weaponInfo, *group, precacheValue, hook.s_original); };
    }
    else
    {
        return [](auto& hook, WeaponInfo* weaponInfo, const WeaponMod* group) -> void
        { GetWeaponModHandler<WeaponInfo>().PrecacheAssets(weaponInfo, *group, hook.s_original); };
    }
}

template <typename WeaponInfo> static auto MakePrecacheWeaponModStringsHook()
{
    if constexpr (std::is_same_v<WeaponInfo, FileWeaponInfo_Client>)
    {
        return [](auto& hook, WeaponInfo* weaponInfo, const WeaponMod* group) -> int
        { return GetWeaponModHandler<WeaponInfo>().PrecacheStrings(weaponInfo, *group, hook.s_original); };
    }
    else
    {
        return [](auto& hook, WeaponInfo* weaponInfo, const WeaponMod* group) -> void
        { GetWeaponModHandler<WeaponInfo>().PrecacheStringsNoResult(weaponInfo, *group, hook.s_original); };
    }
}

template <typename WeaponInfo> static auto MakeNotifyWeaponModStringFieldHook()
{
    using WeaponEntity = typename CWeaponModHandler<WeaponInfo>::WeaponEntity;
    return [](auto& hook, WeaponEntity* weapon, std::uint16_t fieldIndex) -> void
    { GetWeaponModHandler<WeaponInfo>().NotifyStringField(weapon, static_cast<WeaponModEntryType>(fieldIndex), hook.s_original); };
}

DECLARE_HOOK(UpdateWeaponUiValue, client.dll + 0x3D3710,
             [](auto& hook, float* output, const WeaponUiArg_s* source, C_WeaponX* weapon, C_Player* player, float spread)
{
    C_Player* const previousPlayer = C_Player::SetCrosshairQueryPlayer(source->source == 0x17 || source->source == 0x18 ? player : nullptr);
    const ScopeGuard restorePlayer([previousPlayer] { C_Player::SetCrosshairQueryPlayer(previousPlayer); });
    hook.Original(output, source, weapon, player, spread);
});

DECLARE_HOOK(InitFileWeaponInfo_Client, client.dll + 0x3CC990, MakeInitFileWeaponInfoHook<FileWeaponInfo_Client>());
DECLARE_HOOK(ParseFileWeaponInfo_Client, client.dll + 0x3CFAC0, MakeParseFileWeaponInfoHook<FileWeaponInfo_Client>());
DECLARE_HOOK(ParseModsBlock_Client, client.dll + 0x3D1020, MakeParseModsBlockHook<FileWeaponInfo_Client>());
DECLARE_HOOK(ParseSingleMod_Client, client.dll + 0x3D15D0, MakeParseSingleModHook<FileWeaponInfo_Client>());
DECLARE_HOOK(CalcWeaponModsForGameType_Client, client.dll + 0x3CA0B0, MakeCalcWeaponModsForGameTypeHook<FileWeaponInfo_Client>());
DECLARE_HOOK(AddEntryToAssembly_Client, client.dll + 0x3C8EA0, MakeAddEntryToAssemblyHook<FileWeaponInfo_Client>());
DECLARE_HOOK(PrecacheWeaponModAssets_Client, client.dll + 0x3D1ED0, MakePrecacheWeaponModAssetsHook<FileWeaponInfo_Client>());
DECLARE_HOOK(PrecacheWeaponModStrings_Client, client.dll + 0x3D23E0, MakePrecacheWeaponModStringsHook<FileWeaponInfo_Client>());
DECLARE_HOOK(PrecacheAllWeaponModStrings_Client, client.dll + 0x3D2480,
             [](auto& hook, FileWeaponInfo_Client* weaponInfo) -> int { return g_ClientWeaponMods.PrecacheAllStrings(weaponInfo, hook.s_original); });
DECLARE_HOOK(NotifyWeaponModStringField_Client, client.dll + 0x3D41E0, MakeNotifyWeaponModStringFieldHook<FileWeaponInfo_Client>());

DECLARE_HOOK(InitFileWeaponInfo_Server, server.dll + 0x6CB600, MakeInitFileWeaponInfoHook<FileWeaponInfo_Server>());
DECLARE_HOOK(ParseFileWeaponInfo_Server, server.dll + 0x6CE3F0, MakeParseFileWeaponInfoHook<FileWeaponInfo_Server>());
DECLARE_HOOK(ParseModsBlock_Server, server.dll + 0x6CFA10, MakeParseModsBlockHook<FileWeaponInfo_Server>());
DECLARE_HOOK(ParseSingleMod_Server, server.dll + 0x6CFDE0, MakeParseSingleModHook<FileWeaponInfo_Server>());
DECLARE_HOOK(CalcWeaponModsForGameType_Server, server.dll + 0x6C8B80, MakeCalcWeaponModsForGameTypeHook<FileWeaponInfo_Server>());
DECLARE_HOOK(AddEntryToAssembly_Server, server.dll + 0x6C75D0, MakeAddEntryToAssemblyHook<FileWeaponInfo_Server>());
DECLARE_HOOK(PrecacheWeaponModAssets_Server, server.dll + 0x6D0190, MakePrecacheWeaponModAssetsHook<FileWeaponInfo_Server>());
DECLARE_HOOK(PrecacheWeaponModStrings_Server, server.dll + 0x6D0680, MakePrecacheWeaponModStringsHook<FileWeaponInfo_Server>());
DECLARE_HOOK(NotifyWeaponModStringField_Server, server.dll + 0x6D1970, MakeNotifyWeaponModStringFieldHook<FileWeaponInfo_Server>());

using WeaponReparseCommand_t = void(__fastcall*)(const CCommand&);
static WeaponReparseCommand_t g_WeaponReparseServer;

void WeaponReparseServer(const CCommand& args)
{
    if (g_WeaponReparseServer && g_pServer && g_pServer->IsActive())
        g_WeaponReparseServer(args);
}

DECLARE_HOOK(ConCommand_weapon_reparse, client.dll + 0x3D4930, [](auto& hook, const CCommand& args) -> void
{
    g_pModManager->InvalidateKeyValuesPatches("scripts/weapons/");
    WeaponReparseServer(args);
    hook.Original(args);
});

DECLARE_HOOK(ConCommand_weapon_reparse_server, server.dll + 0x6D1B70, [](auto& hook, const CCommand& args) -> void
{
    g_pModManager->InvalidateKeyValuesPatches("scripts/weapons/");
    hook.Original(args);
});

static std::uint8_t* g_AimAssistAdspullClassesInitializedServer;

DECLARE_HOOK(AimAssistAdspullClassInit, server.dll + 0x6CC300, [](auto& hook) -> std::int64_t
{
    static std::mutex s_Mutex;

    const std::lock_guard lock(s_Mutex);
    if (*g_AimAssistAdspullClassesInitializedServer)
        return 0;

    return hook.Original();
});

ON_DLL_LOAD("client.dll", WeaponMods_Client, [](CModule)
{
    g_ClientWeaponMods.Reset();
    DISPATCH_HOOK(ClientWeaponParseHooks, InitFileWeaponInfo_Client)
    DISPATCH_HOOK(ClientWeaponParseHooks, ParseFileWeaponInfo_Client)
    DISPATCH_HOOK(ClientWeaponParseHooks, ParseModsBlock_Client)
    DISPATCH_HOOK(ClientWeaponParseHooks, ParseSingleMod_Client)
    DISPATCH_HOOK(ClientWeaponParseHooks, CalcWeaponModsForGameType_Client)
    DISPATCH_HOOK(ClientWeaponParseHooks, AddEntryToAssembly_Client)
    DISPATCH_HOOK(ClientWeaponParseHooks, PrecacheWeaponModAssets_Client)
    DISPATCH_HOOK(ClientWeaponParseHooks, PrecacheWeaponModStrings_Client)
    DISPATCH_HOOK(ClientWeaponParseHooks, PrecacheAllWeaponModStrings_Client)
    DISPATCH_HOOK(ClientWeaponParseHooks, NotifyWeaponModStringField_Client)
});

ON_DLL_LOAD("server.dll", WeaponMods_Server, [](CModule)
{
    g_ServerWeaponMods.Reset();
    DISPATCH_HOOK(ServerWeaponParseHooks, InitFileWeaponInfo_Server)
    DISPATCH_HOOK(ServerWeaponParseHooks, ParseFileWeaponInfo_Server)
    DISPATCH_HOOK(ServerWeaponParseHooks, ParseModsBlock_Server)
    DISPATCH_HOOK(ServerWeaponParseHooks, ParseSingleMod_Server)
    DISPATCH_HOOK(ServerWeaponParseHooks, CalcWeaponModsForGameType_Server)
    DISPATCH_HOOK(ServerWeaponParseHooks, AddEntryToAssembly_Server)
    DISPATCH_HOOK(ServerWeaponParseHooks, PrecacheWeaponModAssets_Server)
    DISPATCH_HOOK(ServerWeaponParseHooks, PrecacheWeaponModStrings_Server)
    DISPATCH_HOOK(ServerWeaponParseHooks, NotifyWeaponModStringField_Server)
});

ON_DLL_LOAD("client.dll", WeaponReparse_Client, [](CModule) { DISPATCH_HOOK(ClientWeaponParseHooks, ConCommand_weapon_reparse) });

ON_DLL_LOAD("server.dll", WeaponReparse_Server, [](CModule)
{
    DISPATCH_HOOK(ServerWeaponParseHooks, ConCommand_weapon_reparse_server)
    g_WeaponReparseServer = HookSys::GetOriginalFunction<WeaponReparseCommand_t>(HookSys::FindHook("ConCommand_weapon_reparse_server"));
});

ON_DLL_LOAD_CLIENT("client.dll", ClientWeaponParseSdk, [](CModule module)
{
    GetFileWeaponInfoFromHandle<FileWeaponInfo_Client> =
        module.Offset(0x3CB030).RCast<decltype(GetFileWeaponInfoFromHandle<FileWeaponInfo_Client>)>();
    ParseSingleMod<FileWeaponInfo_Client> = module.Offset(0x3D15D0).RCast<decltype(ParseSingleMod<FileWeaponInfo_Client>)>();
    AddEntryToAssembly_GeneralPriority<FileWeaponInfo_Client> =
        module.Offset(0x3C8F60).RCast<decltype(AddEntryToAssembly_GeneralPriority<FileWeaponInfo_Client>)>();
    PrecacheImpactEffectTable<FileWeaponInfo_Client> = module.Offset(0x195CD0).RCast<decltype(PrecacheImpactEffectTable<FileWeaponInfo_Client>)>();
    PrecacheParticleSystemByName<FileWeaponInfo_Client> =
        module.Offset(0x195F20).RCast<decltype(PrecacheParticleSystemByName<FileWeaponInfo_Client>)>();
    s_MPEDict<FileWeaponInfo_Client> = module.Offset(0x23F9920).RCast<RHashMapString*>();
    ModParseEntries<FileWeaponInfo_Client> = module.Offset(0x942CA0).RCast<const WeaponModParseTableEntry*>();
    ResolveSupportedSpecialModEntries<FileWeaponInfo_Client>();

    GetFileWeaponInfoFromName_Client = module.Offset(0x3CB050).RCast<decltype(GetFileWeaponInfoFromName_Client)>();
    ParseFileWeaponInfo = module.Offset(0x3CFAC0).RCast<decltype(ParseFileWeaponInfo)>();
    ReadWeaponDataFromFileForSlot_Client = module.Offset(0x3D2950).RCast<decltype(ReadWeaponDataFromFileForSlot_Client)>();
    ReadEncryptedKVFile = module.Offset(0x3D2710).RCast<decltype(ReadEncryptedKVFile)>();
    AllocWeaponString_Client = module.Offset(0x3C9030).RCast<decltype(AllocWeaponString_Client)>();
    GetIndexForModName_Client = module.Offset(0x3CB1C0).RCast<decltype(GetIndexForModName_Client)>();
    CalcWeaponModsForGameType = module.Offset(0x3CA0B0).RCast<decltype(CalcWeaponModsForGameType)>();
    clientmodelinfo = module.Offset(0xC3D968).RCast<IVModelInfoClient**>();
    g_scriptCBInfo = module.Offset(0x997B30).RCast<const WeaponCallbackInfo*>();

    DISPATCH_HOOK(ClientWeaponParseHooks, UpdateWeaponUiValue)
});

ON_DLL_LOAD("server.dll", ServerWeaponParseSdk, [](CModule module)
{
    GetFileWeaponInfoFromHandle<FileWeaponInfo_Server> =
        module.Offset(0x6CA0A0).RCast<decltype(GetFileWeaponInfoFromHandle<FileWeaponInfo_Server>)>();
    ParseSingleMod<FileWeaponInfo_Server> = module.Offset(0x6CFDE0).RCast<decltype(ParseSingleMod<FileWeaponInfo_Server>)>();
    AddEntryToAssembly_GeneralPriority<FileWeaponInfo_Server> =
        module.Offset(0x6C7690).RCast<decltype(AddEntryToAssembly_GeneralPriority<FileWeaponInfo_Server>)>();
    PrecacheImpactEffectTable<FileWeaponInfo_Server> = module.Offset(0x159C00).RCast<decltype(PrecacheImpactEffectTable<FileWeaponInfo_Server>)>();
    PrecacheParticleSystemByName<FileWeaponInfo_Server> =
        module.Offset(0x159E20).RCast<decltype(PrecacheParticleSystemByName<FileWeaponInfo_Server>)>();
    s_MPEDict<FileWeaponInfo_Server> = module.Offset(0x1615BD0).RCast<RHashMapString*>();
    ModParseEntries<FileWeaponInfo_Server> = module.Offset(0x997DC0).RCast<const WeaponModParseTableEntry*>();
    ResolveSupportedSpecialModEntries<FileWeaponInfo_Server>();

    g_scriptCBInfo = module.Offset(0x98D7D0).RCast<const WeaponCallbackInfo*>();
    g_AimAssistAdspullClassesInitializedServer = module.Offset(0x160B477).RCast<std::uint8_t*>();
    DISPATCH_HOOK(ServerWeaponParseHooks, AimAssistAdspullClassInit)
});
