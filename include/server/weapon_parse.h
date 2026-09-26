#pragma once

#include "../weapon_parse.h"

#include <cstddef>
#include <cstdint>

class CBaseEntity;
class KeyValues;

struct FileWeaponInfo_Server
{
    const char* GetString(WeaponString_t string) const
    {
        return stringPool + string;
    }

    WEAPON_FILE_INFO_HANDLE infoHandle; // 0x0000
    bool bParsedScript;
    bool bLoadedHudElements;
    bool bPrecached;
    bool bCallbacksInitialized;
    char szClassName[MAX_WEAPON_STRING]; // 0x0006
    std::byte reserved0[0x62A];
    char stringPool[MAX_WEAPON_STRING_POOL]; // 0x0680
    std::uint32_t stringPoolUsed;
    std::byte reserved1[4];
    WeaponModValues modValueDefaults;                    // 0x1288
    WeaponMod mods[MAX_WEAPON_MODS];                     // 0x1F28
    WeaponModEntry_t modEntries[MAX_WEAPON_MOD_ENTRIES]; // 0x1FE8
    std::uint32_t modsCount;                             // 0x2C68
    std::uint32_t modEntryCount;
    WeaponMod spBaseMod;
    bool spBaseModDefined;
    std::byte reserved2;
    WeaponMod mpBaseMod;
    bool mpBaseModDefined;
    std::byte reserved3;
    std::byte reserved4[0x28];
};

FileWeaponInfo_Server* GetFileWeaponInfoFromHandle_Server(WEAPON_FILE_INFO_HANDLE handle);

const WeaponModParseTableEntry* FindWeaponModParseTableEntry_Server(const char* fieldName);
const WeaponModParseTableEntry* GetWeaponModParseTableEntry_Server(WeaponModEntryType entryType);
KeyValues* ParseWeaponMod_Server(KeyValues* section, FileWeaponInfo_Server* info, const char* weaponName, WeaponMod* mod);
bool InsertWeaponModAssemblyItem_Server(WeaponModEntry_t* entry, const WeaponModParseTableEntry* parseEntry, WeaponModAssemblyItem_t* item);
void PrecacheWeaponModAsset_Server(const WeaponModParseTableEntry& parseEntry, const char* assetName);
int PrecacheWeaponModModel_Server(const char* modelName);
void PrecacheWeaponModSound_Server(CBaseEntity* owner, const char* soundName);

static_assert(sizeof(FileWeaponInfo_Server) == 0x2CA8);
