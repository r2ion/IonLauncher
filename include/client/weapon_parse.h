#pragma once

#include "../weapon_parse.h"

#include <cstddef>
#include <cstdint>

class C_BaseEntity;
class CHudTexture;
class IFileSystem;
class KeyValues;
struct HSCRIPT__;
struct UiAsset;

struct WeaponUi_s
{
    WeaponString_t attachJoint;
    WeaponString_t attachMesh;
    unsigned int attachMeshHash;
    unsigned short firstArg;
    unsigned short argCount;
    const UiAsset* ui;
};

struct WeaponUiArg_s
{
    unsigned short source;
    WeaponString_t uiArgName;
    unsigned short modEntryType;
};

struct CrosshairUiArgData_s
{
    unsigned short firstArg;
    unsigned short argCount;
};

struct FileWeaponInfo_Client
{
    void Parse(KeyValues* data, const char* weaponName);
    const char* GetString(WeaponString_t string) const
    {
        return stringPool + string;
    }

    WEAPON_FILE_INFO_HANDLE infoHandle; // 0x0000
    bool bParsedScript;
    bool bLoadedHudElements;
    bool bPrecached;
    bool bCallbacksInitialized;
    char szClassName[MAX_WEAPON_STRING];
    WeaponString_t projectileModel;
    WeaponString_t droppedModel;
    WeaponString_t scriptCBNames[37];
    HSCRIPT__* scriptCB[37];
    WeaponString_t projectileScriptCBNames[5];
    HSCRIPT__* projectileScriptCB[5];
    WeaponString_t projectileTrailAttachment;
    WeaponString_t weaponClass;
    int iRumbleEffect;
    float fireAnimRate;
    int weaponType;
    bool alwaysShow;
    bool statsRecord;
    float addOwnerVelocityFrac;
    bool playOffhandChargingAnim;
    bool playOffhandStartEndAnim;
    bool playOffhandFireAnim;
    bool isTossWeapon;
    bool allowEmptyClick;
    bool emptyReloadOnly;
    bool noAmmoUsedOnPrimaryAttack;
    bool projectileAdjustToGunBarrel;
    bool projectileAdjustToHand;
    bool entityColorFromCharge;
    bool m_bBuiltRightHanded;
    bool m_bAllowFlipping;
    int ownerMuzzleIndex;
    int iFlags;
    WeaponString_t aiAddon;
    WeaponString_t pickupHoldPrompt;
    WeaponString_t pickupPressPrompt;
    int weaponDamageType;
    float entityColorFromADSFactor;
    float smart_ammo_screen_min_x;
    float smart_ammo_screen_max_x;
    float smart_ammo_screen_min_y;
    float smart_ammo_screen_max_y;
    bool smart_ammo_search_projectiles;
    bool smart_ammo_titans_block_los;
    float smart_ammo_own_projectile_lock_grace;
    const char* smart_ammo_titan_lock_point[14];
    unsigned int smart_ammo_titan_lock_point_num;
    bool smartAmmoNPCUse;
    bool zoomEffects;
    bool netOptimize;
    WeaponSway_Spec sway;
    float impactSoundRadius;
    float scriptedProjectileMaxTimeStep;
    WeaponString_t soundTriggerPull;
    WeaponString_t soundTriggerRelease;
    WeaponString_t soundZoomIn;
    WeaponString_t soundZoomOut;
    float viewmodelShake_forward;
    float viewmodelShake_up;
    float viewmodelShake_right;
    float viewPunchMultiplier;
    bool disableTempViewmodelHack;
    bool offHandKeepPrimaryInHand;
    bool offHandHolsterPrimary;
    bool special3pAttackAnim;
    bool special3pAttackAnimAfterCharge;
    bool gestureAttackAnim;
    bool showGrenadeIndicator;
    bool grenadeShowIndicatorToOwner;
    bool hudGrappleIndicator;
    WeaponString_t bodygroupNames[10];
    WeaponString_t adsScopeBodygroupName;
    WeaponString_t clipBodygroupName;
    int clipBodygroupIndexShown;
    int clipBodygroupIndexHidden;
    bool clipBodygroupShowForMilestone[4];
    char stringPool[MAX_WEAPON_STRING_POOL]; // 0x066C
    unsigned int stringPoolUsed;
    WeaponModValues modValueDefaults;                    // 0x1270
    WeaponMod mods[MAX_WEAPON_MODS];                     // 0x1F10
    WeaponModEntry_t modEntries[MAX_WEAPON_MOD_ENTRIES]; // 0x1FD0
    unsigned int modsCount;                              // 0x2C50
    unsigned int modEntryCount;
    WeaponMod spBaseMod;
    bool spBaseModDefined;
    WeaponMod mpBaseMod;
    bool mpBaseModDefined;
    KeyValues* m_pKV; // 0x2C68
    int iSpriteCount;
    CHudTexture* iconInactive;
    CHudTexture* iconCrosshair;
    CHudTexture* iconZoomedCrosshair;
    unsigned int uiArgCount;
    WeaponUi_s uis[8];        // 0x2C98
    WeaponUiArg_s uiArgs[32]; // 0x2D58
    CrosshairUiArgData_s defaultCrosshairArgs;
    CrosshairUiArgData_s extraCrosshairArgs[4];
    const UiAsset* crosshairUi[4]; // 0x2E30
    float crosshairSpread[4];
};

FileWeaponInfo_Client* GetFileWeaponInfoFromHandle_Client(WEAPON_FILE_INFO_HANDLE handle);
FileWeaponInfo_Client* GetFileWeaponInfoFromName_Client(const char* name);
WEAPON_FILE_INFO_HANDLE LookupWeaponInfoSlot_Client(const char* name);
bool ReadWeaponDataFromFileForSlot_Client(IFileSystem* filesystem, const char* weaponName, WEAPON_FILE_INFO_HANDLE* handle,
                                          const unsigned char* iceKey = nullptr);
KeyValues* ReadEncryptedKVFile_Client(IFileSystem* filesystem, const char* filenameWithoutExtension, const unsigned char* iceKey);
WeaponString_t AllocWeaponString_Client(FileWeaponInfo_Client* info, const char* string);
bool GetIndexForModName_Client(const char* modName, const FileWeaponInfo_Client* info, unsigned int* index);
bool CalcWeaponMods_Client(unsigned int bitfield, const FileWeaponInfo_Client* info, WeaponModValues* values, bool singlePlayer,
                           unsigned int overrideMods = 0);

const WeaponModParseTableEntry* FindWeaponModParseTableEntry_Client(const char* fieldName);
const WeaponModParseTableEntry* GetWeaponModParseTableEntry_Client(WeaponModEntryType entryType);
KeyValues* ParseWeaponMod_Client(KeyValues* section, FileWeaponInfo_Client* info, const char* weaponName, WeaponMod* mod);
bool InsertWeaponModAssemblyItem_Client(WeaponModEntry_t* entry, const WeaponModParseTableEntry* parseEntry, WeaponModAssemblyItem_t* item);
void PrecacheWeaponModAsset_Client(const WeaponModParseTableEntry& parseEntry, const char* assetName);
int PrecacheWeaponModModel_Client(const char* modelName);
void PrecacheWeaponModSound_Client(C_BaseEntity* owner, const char* soundName);

static_assert(sizeof(FileWeaponInfo_Client) == 0x2E60);
