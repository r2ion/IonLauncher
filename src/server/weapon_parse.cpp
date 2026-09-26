#include <mutex>

#include "rtech/rstdlib.h"
#include "server/baseentity.h"
#include "server/weapon_parse.h"
#include "tier0/callbacks.h"

static FileWeaponInfo_Server* (*s_GetFileWeaponInfoFromHandle)(WEAPON_FILE_INFO_HANDLE);
static KeyValues* (*s_ParseWeaponMod)(KeyValues*, FileWeaponInfo_Server*, const char*, WeaponMod*);
static bool (*s_InsertWeaponModAssemblyItem)(WeaponModEntry_t*, const WeaponModParseTableEntry*, WeaponModAssemblyItem_t*);
static int (*s_PrecacheImpactEffectTable)(const char*);
static int (*s_PrecacheParticleSystemByName)(const char*);
static int (*s_PrecacheModel)(const char*);
static void (*s_PrecacheScriptSound)(CBaseEntity*, const char*);
static std::uint8_t* s_AimAssistAdspullClassesInitialized;

DECLARE_MODULE(ServerWeaponParseHooks)

static RHashMapString* g_WeaponModParseEntriesByName;
static const WeaponModParseTableEntry* g_WeaponModParseEntries;

FileWeaponInfo_Server* GetFileWeaponInfoFromHandle_Server(WEAPON_FILE_INFO_HANDLE handle)
{
    return s_GetFileWeaponInfoFromHandle(handle);
}

const WeaponModParseTableEntry* FindWeaponModParseTableEntry_Server(const char* fieldName)
{
    const auto* entryType = static_cast<const std::uint32_t*>(g_WeaponModParseEntriesByName->Find(fieldName));
    return entryType && *entryType != WMET_INVALID && *entryType < MAX_WEAPON_MOD_PARSE_ENTRIES ? &g_WeaponModParseEntries[*entryType] : nullptr;
}

const WeaponModParseTableEntry* GetWeaponModParseTableEntry_Server(WeaponModEntryType entryType)
{
    const auto index = static_cast<std::uint16_t>(entryType);
    return index < MAX_WEAPON_MOD_PARSE_ENTRIES ? &g_WeaponModParseEntries[index] : nullptr;
}

KeyValues* ParseWeaponMod_Server(KeyValues* section, FileWeaponInfo_Server* info, const char* weaponName, WeaponMod* mod)
{
    return s_ParseWeaponMod(section, info, weaponName, mod);
}

bool InsertWeaponModAssemblyItem_Server(WeaponModEntry_t* entry, const WeaponModParseTableEntry* parseEntry, WeaponModAssemblyItem_t* item)
{
    return s_InsertWeaponModAssemblyItem(entry, parseEntry, item);
}

void PrecacheWeaponModAsset_Server(const WeaponModParseTableEntry& parseEntry, const char* assetName)
{
    if (parseEntry.parseFlags & WMPF_IMPACT_EFFECT_TABLE)
        s_PrecacheImpactEffectTable(assetName);
    else if (parseEntry.parseFlags & WMPF_PARTICLE_SYSTEM)
        s_PrecacheParticleSystemByName(assetName);
}

int PrecacheWeaponModModel_Server(const char* modelName)
{
    return s_PrecacheModel(modelName);
}

void PrecacheWeaponModSound_Server(CBaseEntity* owner, const char* soundName)
{
    s_PrecacheScriptSound(owner, soundName);
}

DECLARE_HOOK(AimAssistAdspullClassInit, server.dll + 0x6CC300, [](auto& hook) -> std::int64_t
{
    static std::mutex s_Mutex;

    const std::lock_guard lock(s_Mutex);
    if (*s_AimAssistAdspullClassesInitialized)
        return 0;

    return hook.Original();
})

ON_DLL_LOAD("server.dll", ServerWeaponParseSdk, [](CModule module)
{
    s_GetFileWeaponInfoFromHandle = module.Offset(0x6CA0A0).RCast<decltype(s_GetFileWeaponInfoFromHandle)>();
    s_ParseWeaponMod = module.Offset(0x6CFDE0).RCast<decltype(s_ParseWeaponMod)>();
    s_InsertWeaponModAssemblyItem = module.Offset(0x6C7690).RCast<decltype(s_InsertWeaponModAssemblyItem)>();
    s_PrecacheImpactEffectTable = module.Offset(0x159C00).RCast<decltype(s_PrecacheImpactEffectTable)>();
    s_PrecacheParticleSystemByName = module.Offset(0x159E20).RCast<decltype(s_PrecacheParticleSystemByName)>();
    s_PrecacheModel = module.Offset(0x429550).RCast<decltype(s_PrecacheModel)>();
    s_PrecacheScriptSound = module.Offset(0x6A8C70).RCast<decltype(s_PrecacheScriptSound)>();
    s_AimAssistAdspullClassesInitialized = module.Offset(0x160B477).RCast<std::uint8_t*>();

    g_WeaponModParseEntriesByName = module.Offset(0x1615BD0).RCast<RHashMapString*>();
    g_WeaponModParseEntries = module.Offset(0x997DC0).RCast<const WeaponModParseTableEntry*>();

    DISPATCH_MODULE(ServerWeaponParseHooks);
});
