#include "client/weapon_parse.h"

#include "client/baseentity.h"
#include "rtech/rstdlib.h"
#include "tier0/callbacks.h"

static FileWeaponInfo_Client* (*s_GetFileWeaponInfoFromHandle)(WEAPON_FILE_INFO_HANDLE);
static FileWeaponInfo_Client* (*s_GetFileWeaponInfoFromName)(const char*);
static void (*s_ParseFileWeaponInfo)(FileWeaponInfo_Client*, KeyValues*, const char*);
static bool (*s_ReadWeaponDataFromFileForSlot)(IFileSystem*, const char*, WEAPON_FILE_INFO_HANDLE*, const unsigned char*);
static KeyValues* (*s_ReadEncryptedKVFile)(IFileSystem*, const char*, const unsigned char*);
static WeaponString_t (*s_AllocWeaponString)(FileWeaponInfo_Client*, const char*);
static bool (*s_GetIndexForModName)(const char*, const FileWeaponInfo_Client*, unsigned int*);
static bool (*s_CalcWeaponMods)(unsigned int, const FileWeaponInfo_Client*, WeaponModValues*, bool, unsigned int);
static KeyValues* (*s_ParseWeaponMod)(KeyValues*, FileWeaponInfo_Client*, const char*, WeaponMod*);
static bool (*s_InsertWeaponModAssemblyItem)(WeaponModEntry_t*, const WeaponModParseTableEntry*, WeaponModAssemblyItem_t*);
static int (*s_PrecacheImpactEffectTable)(const char*);
static int (*s_PrecacheParticleSystemByName)(const char*);
static int (*s_PrecacheModel)(const char*);
static void (*s_PrecacheScriptSound)(C_BaseEntity*, const char*);

static RHashMapString* g_WeaponModParseEntriesByName;
static const WeaponModParseTableEntry* g_WeaponModParseEntries;

FileWeaponInfo_Client* GetFileWeaponInfoFromHandle_Client(WEAPON_FILE_INFO_HANDLE handle)
{
    return s_GetFileWeaponInfoFromHandle(handle);
}

FileWeaponInfo_Client* GetFileWeaponInfoFromName_Client(const char* name)
{
    return s_GetFileWeaponInfoFromName(name);
}

WEAPON_FILE_INFO_HANDLE LookupWeaponInfoSlot_Client(const char* name)
{
    const FileWeaponInfo_Client* info = GetFileWeaponInfoFromName_Client(name);
    return info ? info->infoHandle : INVALID_WEAPON_INFO_HANDLE;
}

void FileWeaponInfo_Client::Parse(KeyValues* data, const char* weaponName)
{
    s_ParseFileWeaponInfo(this, data, weaponName);
}

bool ReadWeaponDataFromFileForSlot_Client(IFileSystem* filesystem, const char* weaponName, WEAPON_FILE_INFO_HANDLE* handle,
                                          const unsigned char* iceKey)
{
    return s_ReadWeaponDataFromFileForSlot(filesystem, weaponName, handle, iceKey);
}

KeyValues* ReadEncryptedKVFile_Client(IFileSystem* filesystem, const char* filenameWithoutExtension, const unsigned char* iceKey)
{
    return s_ReadEncryptedKVFile(filesystem, filenameWithoutExtension, iceKey);
}

WeaponString_t AllocWeaponString_Client(FileWeaponInfo_Client* info, const char* string)
{
    return s_AllocWeaponString(info, string);
}

bool GetIndexForModName_Client(const char* modName, const FileWeaponInfo_Client* info, unsigned int* index)
{
    return s_GetIndexForModName(modName, info, index);
}

bool CalcWeaponMods_Client(unsigned int bitfield, const FileWeaponInfo_Client* info, WeaponModValues* values, bool singlePlayer,
                           unsigned int overrideMods)
{
    return s_CalcWeaponMods(bitfield, info, values, singlePlayer, overrideMods);
}

const WeaponModParseTableEntry* FindWeaponModParseTableEntry_Client(const char* fieldName)
{
    const auto* entryType = static_cast<const std::uint32_t*>(g_WeaponModParseEntriesByName->Find(fieldName));
    return entryType && *entryType != WMET_INVALID && *entryType < MAX_WEAPON_MOD_PARSE_ENTRIES ? &g_WeaponModParseEntries[*entryType] : nullptr;
}

const WeaponModParseTableEntry* GetWeaponModParseTableEntry_Client(WeaponModEntryType entryType)
{
    const auto index = static_cast<std::uint16_t>(entryType);
    return index < MAX_WEAPON_MOD_PARSE_ENTRIES ? &g_WeaponModParseEntries[index] : nullptr;
}

KeyValues* ParseWeaponMod_Client(KeyValues* section, FileWeaponInfo_Client* info, const char* weaponName, WeaponMod* mod)
{
    return s_ParseWeaponMod(section, info, weaponName, mod);
}

bool InsertWeaponModAssemblyItem_Client(WeaponModEntry_t* entry, const WeaponModParseTableEntry* parseEntry, WeaponModAssemblyItem_t* item)
{
    return s_InsertWeaponModAssemblyItem(entry, parseEntry, item);
}

void PrecacheWeaponModAsset_Client(const WeaponModParseTableEntry& parseEntry, const char* assetName)
{
    if (parseEntry.parseFlags & WMPF_IMPACT_EFFECT_TABLE)
        s_PrecacheImpactEffectTable(assetName);
    else if (parseEntry.parseFlags & WMPF_PARTICLE_SYSTEM)
        s_PrecacheParticleSystemByName(assetName);
}

int PrecacheWeaponModModel_Client(const char* modelName)
{
    return s_PrecacheModel(modelName);
}

void PrecacheWeaponModSound_Client(C_BaseEntity* owner, const char* soundName)
{
    s_PrecacheScriptSound(owner, soundName);
}

ON_DLL_LOAD_CLIENT("client.dll", ClientWeaponParseSdk, [](CModule module)
{
    s_GetFileWeaponInfoFromHandle = module.Offset(0x3CB030).RCast<decltype(s_GetFileWeaponInfoFromHandle)>();
    s_GetFileWeaponInfoFromName = module.Offset(0x3CB050).RCast<decltype(s_GetFileWeaponInfoFromName)>();
    s_ParseFileWeaponInfo = module.Offset(0x3CFAC0).RCast<decltype(s_ParseFileWeaponInfo)>();
    s_ReadWeaponDataFromFileForSlot = module.Offset(0x3D2950).RCast<decltype(s_ReadWeaponDataFromFileForSlot)>();
    s_ReadEncryptedKVFile = module.Offset(0x3D2710).RCast<decltype(s_ReadEncryptedKVFile)>();
    s_AllocWeaponString = module.Offset(0x3C9030).RCast<decltype(s_AllocWeaponString)>();
    s_GetIndexForModName = module.Offset(0x3CB1C0).RCast<decltype(s_GetIndexForModName)>();
    s_CalcWeaponMods = module.Offset(0x3CA0B0).RCast<decltype(s_CalcWeaponMods)>();
    s_ParseWeaponMod = module.Offset(0x3D15D0).RCast<decltype(s_ParseWeaponMod)>();
    s_InsertWeaponModAssemblyItem = module.Offset(0x3C8F60).RCast<decltype(s_InsertWeaponModAssemblyItem)>();
    s_PrecacheImpactEffectTable = module.Offset(0x195CD0).RCast<decltype(s_PrecacheImpactEffectTable)>();
    s_PrecacheParticleSystemByName = module.Offset(0x195F20).RCast<decltype(s_PrecacheParticleSystemByName)>();
    s_PrecacheModel = module.Offset(0x3EDEB0).RCast<decltype(s_PrecacheModel)>();
    s_PrecacheScriptSound = module.Offset(0x5B9A60).RCast<decltype(s_PrecacheScriptSound)>();

    g_WeaponModParseEntriesByName = module.Offset(0x23F9920).RCast<RHashMapString*>();
    g_WeaponModParseEntries = module.Offset(0x942CA0).RCast<const WeaponModParseTableEntry*>();
});
