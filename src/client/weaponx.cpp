#include "client/weaponx.h"
#include "core/tier1.h"
#include "vscript/ivscript.h"

void (*s_EmitWeaponNpcSound)(C_WeaponX*, float, float);
void (*s_EmitWeaponNpcSound_DontUpdateLastFiredTime)(C_WeaponX*, float, float);
void (*s_ShowWeapon)(C_WeaponX*);
void (*s_SetViewmodelAmmoModelIndex_Script)(C_WeaponX*, int);
int (*s_GetWeaponChargeLevel)(const C_WeaponX*);
float (*s_GetChargeFraction)(const C_WeaponX*);
bool (*s_DeployWeapon)(C_WeaponX*, bool);
bool (*s_Holster)(C_WeaponX*);
bool (*s_FastHolster)(C_WeaponX*);
bool (*s_Lower)(C_WeaponX*);
bool (*s_Reload)(C_WeaponX*);
bool (*s_PrimaryAttack)(C_WeaponX*);
bool (*s_ChargeEnd_Internal)(C_WeaponX*, bool, bool);
bool (*s_HolsterInternal)(C_WeaponX*, bool);
bool (*s_RaiseInternal)(C_WeaponX*, bool, bool);
bool (*s_SustainedDischargeBegin)(C_WeaponX*);
ScriptStringOrNull* (*s_GetPrintName)(const C_BaseCombatWeapon*, ScriptStringOrNull*);
ScriptStringOrNull* (*s_GetWeaponDescription)(const C_BaseCombatWeapon*, ScriptStringOrNull*);
void (*s_Script_SetDroppedModel)(C_BaseCombatWeapon*, const char*);
int (*s_ScriptLookupWorldModelAttachment)(C_BaseCombatWeapon*, const char*);
int (*s_ScriptLookupViewModelAttachment)(C_BaseCombatWeapon*, const char*);

Vector3D* (*s_GetAttackPosition_Script)(C_WeaponX*, Vector3D*);
Vector3D* (*s_GetAttackDirection_Script)(C_WeaponX*, Vector3D*);
void (*s_AllowUse)(C_WeaponX*, bool);
void (*s_Script_EmitWeaponSound_1p3p)(C_WeaponX*, char const* const, char const* const);
void (*s_EmitWeaponSound_Script)(C_WeaponX*, char const*);
void (*s_StopWeaponSound_Script)(C_WeaponX*, char const* const);
bool (*s_IsNetOptimized)(const C_WeaponX*);
void (*s_StopWeaponEffect_Script)(C_WeaponX*, char const*, char const*);
void (*s_SetWeaponBurstFireCount_Script)(C_WeaponX*, int);
int (*s_GetWeaponBurstFireCount_Script)(C_WeaponX*);
void (*s_SetWeaponSkin)(C_WeaponX*, int);
void (*s_SetWeaponCamo)(C_WeaponX*, unsigned int);
void (*s_FireWeaponBullet_Script)(C_WeaponX*, Vector3D const&, Vector3D const&, int, int);
void (*s_FireWeaponBullet_Internal)(C_WeaponX*, const Vector3D&, const Vector3D&, int, int, bool, bool, bool, bool, bool, bool, bool);
C_BaseEntity* (*s_FireWeaponGrenade_Script)(C_WeaponX*, const Vector3D&, const Vector3D&, const Vector3D&, float, int, int, bool, bool, bool);
C_BaseEntity* (*s_FireWeaponMissile_Script)(C_WeaponX*, const Vector3D&, const Vector3D&, float, int, int, bool, bool);
int (*s_GetProjectilesPerShot_Script)(const C_WeaponX*);
int (*s_GetAmmoPerShot_Script)(const C_WeaponX*);
char const* (*s_GetAmmoDisplay_Script)(const C_WeaponX*);
int (*s_GetWeaponPrimaryClipCountMax_Script)(C_WeaponX*);
void (*s_SetWeaponPrimaryClipCount_Script)(C_WeaponX*, int);
void (*s_SetWeaponPrimaryClipCountAbsolute_Script)(C_WeaponX*, int);
void (*s_SetWeaponPrimaryClipCountNoRegenReset_Script)(C_WeaponX*, int);
bool (*s_IsWeaponRegenDraining_Script)(C_WeaponX*);
int (*s_GetWeaponPrimaryClipCount_Script)(C_WeaponX*);
void (*s_Script_RegenerateAmmoReset)(C_WeaponX*);
int (*s_Script_GetLifetimeShotsRemaining)(C_WeaponX*);
void (*s_Script_SetLifetimeShotsRemaining)(C_WeaponX*, int);
void (*s_Script_SetLifetimeShotsRemainingInfinite)(C_WeaponX*);
C_BaseEntity* (*s_GetWeaponUtilityEntity)(const C_WeaponX*);
bool (*s_IsWeaponAdsButtonPressed)(const C_WeaponX*);
int (*s_GetWeaponType)(const C_WeaponX*);
bool (*s_IsChargeWeapon)(const C_WeaponX*);
bool (*s_IsWeaponCharging_Script)(C_WeaponX*);
float (*s_GetWeaponChargeTime_Script)(C_WeaponX*);
float (*s_GetWeaponChargeTimeRemaining_Script)(C_WeaponX*);
float (*s_GetWeaponChargeFractionClamped)(const C_WeaponX*);
void (*s_Script_SetWeaponChargeFraction)(C_WeaponX*, float);
void (*s_Script_SetWeaponChargeFractionForced)(C_WeaponX*, float);
int (*s_GetWeaponChargeLevel_Script)(const C_WeaponX*);
int (*s_GetWeaponChargeLevelMax_Script)(const C_WeaponX*);
float (*s_GetChargeDuration)(const C_WeaponX*);
float (*s_GetWeaponReadyToFireProgress)(const C_WeaponX*);
int (*s_Script_GetWeaponChargeEnergyCost)(C_WeaponX*);
int (*s_Script_GetWeaponDefaultEnergyCost)(C_WeaponX*, int);
void (*s_Script_ResetWeaponToDefaultEnergyCost)(C_WeaponX*);
int (*s_Script_GetWeaponCurrentEnergyCost)(C_WeaponX*);
void (*s_Script_SetWeaponEnergyCost)(C_WeaponX*, int);
ScriptVariant_t* (*s_GetWeaponInfoFileKeyField)(C_WeaponX*, ScriptVariant_t*, char const*);
ScriptAsset* (*s_GetWeaponInfoFileKeyFieldAsset)(C_WeaponX*, ScriptAsset*, char const*);
bool (*s_IsWeaponDisabled_Script)(C_WeaponX*, C_BaseEntity*);
bool (*s_Script_DeployWeapon)(C_WeaponX*);
bool (*s_Script_DeployWeaponInstant)(C_WeaponX*);
bool (*s_Raise)(C_WeaponX*);
bool (*s_ShouldPredictProjectiles)(C_WeaponX*);
ScriptVariant_t* (*s_SmartAmmo_Script_GetStoredTargets)(C_WeaponX*, ScriptVariant_t*);
void (*s_SmartAmmo_Clear)(C_WeaponX*, bool, bool);
bool (*s_IsReadyToFire)(const C_WeaponX*);
bool (*s_IsBurstFireInProgress)(const C_WeaponX*);
int (*s_GetBurstFireShotsPending)(const C_WeaponX*);
float (*s_TimeUntilReadyToFire)(const C_WeaponX*);
char const* (*s_GetWeaponName)(const C_WeaponX*);
int (*s_GetDamageSourceID)(C_WeaponX*);
float (*s_GetMaxDamageFarDist)(C_WeaponX*);
void (*s_ForceRelease)(C_WeaponX*);
bool (*s_IsForceRelease)(const C_WeaponX*);
void (*s_AddMod_Script)(C_WeaponX*, char const* const);
void (*s_RemoveMod_Script)(C_WeaponX*, char const* const);
int (*s_GetMods_Script)(C_WeaponX*, SQVM*);
int (*s_SetMods_Script)(C_WeaponX*, SQVM*);
bool (*s_IsOffhandWeapon)(const C_WeaponX*);
float (*s_GetNextAttackAllowedTime_Script)(const C_WeaponX*);
float (*s_GetNextAttackAllowedTimeRaw_Script)(const C_WeaponX*);
void (*s_SetNextAttackAllowedTime_Script)(C_WeaponX*, float);
int (*s_GetRodeoDamage_Script)(const C_WeaponX*);
int (*s_GetShotCount_Script)(const C_WeaponX*);
char const* (*s_GetWeaponClass)(const C_WeaponX*);
bool (*s_IsInCooldown)(const C_WeaponX*);
bool (*s_IsCooldownPending)(const C_WeaponX*);
int (*s_GetWeaponDamageFlags)(const C_WeaponX*);
int (*s_GetWeaponExplosionDamageFlags)(const C_WeaponX*);
int (*s_Script_GetImpactTableIndex)(const C_WeaponX*);
bool (*s_Script_GetNPCMissFastPlayer)(const C_WeaponX*);
float (*s_Script_GetMeleeLungeTargetRange)(const C_WeaponX*);
float (*s_Script_GetMeleeLungeTargetAngle)(const C_WeaponX*);
bool (*s_Script_GetMeleeCanHitHumanSized)(const C_WeaponX*);
bool (*s_Script_GetMeleeCanHitTitans)(const C_WeaponX*);
int (*s_Script_GetDamageAmountForArmorType)(const C_WeaponX*, int);
float (*s_Script_GetMeleeAttackRange)(const C_WeaponX*);
float (*s_Script_GetMeleeAttackAngle)(const C_WeaponX*);
char const* (*s_Script_GetMeleeAnim3p)(const C_WeaponX*);
char const* (*s_Script_GetWeaponReadyMsg)(const C_WeaponX*);
char const* (*s_Script_GetWeaponReadyHint)(const C_WeaponX*);
float (*s_Script_GetGrenadeFuseTime)(const C_WeaponX*);
float (*s_Script_GetGrenadeIgnitionTime)(const C_WeaponX*);
bool (*s_Script_GetAllowHeadShots)(const C_WeaponX*);
int (*s_Script_GetCurrentAltFireIndex)(const C_WeaponX*);
float (*s_Script_GetWeaponZoomFOV)(C_WeaponX*);
int (*s_Script_GetReloadMilestoneIndex)(C_WeaponX*);
void (*s_Script_SetForcedADS)(C_WeaponX*);
void (*s_Script_ClearForcedADS)(C_WeaponX*);
int (*s_Script_GetForcedADS)(const C_WeaponX*);
int (*s_Script_GetInventoryIndex)(const C_WeaponX*);
int (*s_Script_GetChargeAnimIndex)(const C_WeaponX*);
void (*s_Script_SetChargeAnimIndex)(C_WeaponX*, int);
float (*s_GetWeaponDamageForce)(const C_WeaponX*);
float (*s_GetCoreDuration)(const C_WeaponX*);
bool (*s_IsSustainedDischargeWeapon)(const C_WeaponX*);
bool (*s_IsDischarging)(const C_WeaponX*);
float (*s_GetSustainedDischargeDuration)(const C_WeaponX*);
float (*s_GetSustainedDischargeRemainder)(const C_WeaponX*);
float (*s_GetSustainedDischargeFraction)(const C_WeaponX*);
float (*s_GetSustainedDischargePulseFrequency)(const C_WeaponX*);
void (*s_SetSustainedDischargeFractionForced)(C_WeaponX*, float);
bool (*s_IsSustainedLaserWeapon)(const C_WeaponX*);
void (*s_Script_DoMeleeHitConfirmation)(C_WeaponX*, float);
void (*s_Script_SetScriptTime0)(C_WeaponX*, float);
float (*s_Script_GetScriptTime0)(C_WeaponX*);
void (*s_Script_SetScriptFlags0)(C_WeaponX*, int);
int (*s_Script_GetScriptFlags0)(C_WeaponX*);
bool (*s_IsLoadoutPickup)(C_WeaponX*);
void (*s_HideWeapon)(C_WeaponX*);
C_BaseEntity* (*s_GetWeaponOwner_Script)(const C_WeaponX*);
bool (*s_HasSilencer)(const C_WeaponX*);
int (*s_PlayWeaponEffectAndReturnViewEffectHandle_Script)(C_WeaponX*, const char*, const char*, const char*);
void (*s_PlayWeaponEffectNoCull_Script)(C_WeaponX*, const char*, const char*, const char*);
void (*s_PlayWeaponEffect_Script)(C_WeaponX*, const char*, const char*, const char*);
C_BaseEntity* (*s_FireWeaponBolt_Script)(C_WeaponX*, const Vector3D&, const Vector3D&, float, int, int, bool, int);
int (*s_GetWeaponPrimaryAmmoCount_Script)(const C_WeaponX*);
void (*s_SetWeaponPrimaryAmmoCount_Script)(C_WeaponX*, int);
bool (*s_IsWeaponInAds_Script)(C_WeaponX*);
bool (*s_SmartAmmo_IsEnabled_Script)(C_WeaponX*);
void (*s_SmartAmmo_SetNewTargetTime)(C_WeaponX*);
float (*s_SmartAmmo_GetNewTargetTime)(C_WeaponX*);
float (*s_SmartAmmo_GetSearchAngle)(C_WeaponX*);
ScriptVariant_t* (*s_SmartAmmo_Script_GetTargets)(C_WeaponX*, ScriptVariant_t*);
void (*s_SmartAmmo_Script_SetTarget)(C_WeaponX*, C_BaseEntity*, float);
void (*s_SmartAmmo_Script_StoreTargets)(C_WeaponX*);
Vector3D* (*s_SmartAmmo_Script_GetFirePosition)(C_WeaponX*, Vector3D*, C_BaseEntity*, int);
void (*s_SmartAmmo_Script_TrackEntity)(C_WeaponX*, C_BaseEntity*, float);
void (*s_SmartAmmo_Script_UntrackEntity)(C_WeaponX*, C_BaseEntity*);
int (*s_SmartAmmo_Script_GetNumTrackersOnEntity)(C_WeaponX*, C_BaseEntity*);
bool (*s_SmartAmmo_IsVisibleTarget)(C_WeaponX*, C_BaseEntity*);
ScriptVariant_t* (*s_SmartAmmo_Script_GetTrackedEntities)(C_WeaponX*, ScriptVariant_t*);
bool (*s_HasMod_Script)(C_WeaponX*, const char*);
void (*s_SetModBitfield_Script)(C_WeaponX*, int);
int (*s_GetModBitfield_Script)(C_WeaponX*);
const char* (*s_GetSmartAmmoHudLockStyle_Script)(C_WeaponX*);
const char* (*s_GetSmartAmmoWeaponType_Script)(C_WeaponX*);
void (*s_SetAttackKickScale_Script)(C_WeaponX*, float);
void (*s_SetAttackKickRollScale_Script)(C_WeaponX*, float);
int (*s_Script_GetWeaponSettingInt)(C_WeaponX*, SQVM*);
int (*s_Script_GetWeaponSettingFloat)(C_WeaponX*, SQVM*);
int (*s_Script_GetWeaponSettingBool)(C_WeaponX*, SQVM*);
int (*s_Script_GetWeaponSettingVector)(C_WeaponX*, SQVM*);
int (*s_Script_GetWeaponSettingString)(C_WeaponX*, SQVM*);
int (*s_Script_GetWeaponSettingAsset)(C_WeaponX*, SQVM*);

Vector3D C_WeaponX::GetAttackPosition_Script()
{
    Vector3D result;
    s_GetAttackPosition_Script(this, &result);
    return result;
}

Vector3D C_WeaponX::GetAttackDirection_Script()
{
    Vector3D result;
    s_GetAttackDirection_Script(this, &result);
    return result;
}

void C_WeaponX::AllowUse(bool arg1)
{
    s_AllowUse(this, arg1);
}

void C_WeaponX::Script_EmitWeaponSound_1p3p(char const* const arg1, char const* const arg2)
{
    s_Script_EmitWeaponSound_1p3p(this, arg1, arg2);
}

void C_WeaponX::EmitWeaponSound_Script(char const* arg1)
{
    s_EmitWeaponSound_Script(this, arg1);
}

void C_WeaponX::StopWeaponSound_Script(char const* const arg1)
{
    s_StopWeaponSound_Script(this, arg1);
}

bool C_WeaponX::IsNetOptimized() const
{
    return s_IsNetOptimized(this);
}

void C_WeaponX::StopWeaponEffect_Script(char const* arg1, char const* arg2)
{
    s_StopWeaponEffect_Script(this, arg1, arg2);
}

void C_WeaponX::SetWeaponBurstFireCount_Script(int arg1)
{
    s_SetWeaponBurstFireCount_Script(this, arg1);
}

int C_WeaponX::GetWeaponBurstFireCount_Script()
{
    return s_GetWeaponBurstFireCount_Script(this);
}

void C_WeaponX::SetWeaponSkin(int arg1)
{
    s_SetWeaponSkin(this, arg1);
}

void C_WeaponX::SetWeaponCamo(unsigned int arg1)
{
    s_SetWeaponCamo(this, arg1);
}

void C_WeaponX::FireWeaponBullet_Script(Vector3D const& arg1, Vector3D const& arg2, int arg3, int arg4)
{
    s_FireWeaponBullet_Script(this, arg1, arg2, arg3, arg4);
}

void C_WeaponX::FireWeaponBullet_Internal(const Vector3D& arg1, const Vector3D& arg2, int arg3, int arg4, bool arg5, bool arg6, bool arg7, bool arg8,
                                          bool arg9, bool arg10, bool arg11)
{
    s_FireWeaponBullet_Internal(this, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
}

C_BaseEntity* C_WeaponX::FireWeaponGrenade_Script(const Vector3D& arg1, const Vector3D& arg2, const Vector3D& arg3, float arg4, int arg5, int arg6,
                                                  bool arg7, bool arg8, bool arg9)
{
    return s_FireWeaponGrenade_Script(this, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}

C_BaseEntity* C_WeaponX::FireWeaponMissile_Script(const Vector3D& arg1, const Vector3D& arg2, float arg3, int arg4, int arg5, bool arg6, bool arg7)
{
    return s_FireWeaponMissile_Script(this, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

int C_WeaponX::GetProjectilesPerShot_Script() const
{
    return s_GetProjectilesPerShot_Script(this);
}

int C_WeaponX::GetAmmoPerShot_Script() const
{
    return s_GetAmmoPerShot_Script(this);
}

char const* C_WeaponX::GetAmmoDisplay_Script() const
{
    return s_GetAmmoDisplay_Script(this);
}

int C_WeaponX::GetWeaponPrimaryClipCountMax_Script()
{
    return s_GetWeaponPrimaryClipCountMax_Script(this);
}

void C_WeaponX::SetWeaponPrimaryClipCount_Script(int arg1)
{
    s_SetWeaponPrimaryClipCount_Script(this, arg1);
}

void C_WeaponX::SetWeaponPrimaryClipCountAbsolute_Script(int arg1)
{
    s_SetWeaponPrimaryClipCountAbsolute_Script(this, arg1);
}

void C_WeaponX::SetWeaponPrimaryClipCountNoRegenReset_Script(int arg1)
{
    s_SetWeaponPrimaryClipCountNoRegenReset_Script(this, arg1);
}

bool C_WeaponX::IsWeaponRegenDraining_Script()
{
    return s_IsWeaponRegenDraining_Script(this);
}

int C_WeaponX::GetWeaponPrimaryClipCount_Script()
{
    return s_GetWeaponPrimaryClipCount_Script(this);
}

void C_WeaponX::Script_RegenerateAmmoReset()
{
    s_Script_RegenerateAmmoReset(this);
}

int C_WeaponX::Script_GetLifetimeShotsRemaining()
{
    return s_Script_GetLifetimeShotsRemaining(this);
}

void C_WeaponX::Script_SetLifetimeShotsRemaining(int arg1)
{
    s_Script_SetLifetimeShotsRemaining(this, arg1);
}

void C_WeaponX::Script_SetLifetimeShotsRemainingInfinite()
{
    s_Script_SetLifetimeShotsRemainingInfinite(this);
}

C_BaseEntity* C_WeaponX::GetWeaponUtilityEntity() const
{
    return s_GetWeaponUtilityEntity(this);
}

bool C_WeaponX::IsWeaponAdsButtonPressed() const
{
    return s_IsWeaponAdsButtonPressed(this);
}

int C_WeaponX::GetWeaponType() const
{
    return s_GetWeaponType(this);
}

bool C_WeaponX::IsChargeWeapon() const
{
    return s_IsChargeWeapon(this);
}

bool C_WeaponX::IsWeaponCharging_Script()
{
    return s_IsWeaponCharging_Script(this);
}

float C_WeaponX::GetWeaponChargeTime_Script()
{
    return s_GetWeaponChargeTime_Script(this);
}

float C_WeaponX::GetWeaponChargeTimeRemaining_Script()
{
    return s_GetWeaponChargeTimeRemaining_Script(this);
}

float C_WeaponX::GetWeaponChargeFractionClamped() const
{
    return s_GetWeaponChargeFractionClamped(this);
}

void C_WeaponX::Script_SetWeaponChargeFraction(float arg1)
{
    s_Script_SetWeaponChargeFraction(this, arg1);
}

void C_WeaponX::Script_SetWeaponChargeFractionForced(float arg1)
{
    s_Script_SetWeaponChargeFractionForced(this, arg1);
}

int C_WeaponX::GetWeaponChargeLevel_Script() const
{
    return s_GetWeaponChargeLevel_Script(this);
}

int C_WeaponX::GetWeaponChargeLevelMax_Script() const
{
    return s_GetWeaponChargeLevelMax_Script(this);
}

float C_WeaponX::GetChargeDuration() const
{
    return s_GetChargeDuration(this);
}

float C_WeaponX::GetWeaponReadyToFireProgress() const
{
    return s_GetWeaponReadyToFireProgress(this);
}

int C_WeaponX::Script_GetWeaponChargeEnergyCost()
{
    return s_Script_GetWeaponChargeEnergyCost(this);
}

int C_WeaponX::Script_GetWeaponDefaultEnergyCost(int arg1)
{
    return s_Script_GetWeaponDefaultEnergyCost(this, arg1);
}

void C_WeaponX::Script_ResetWeaponToDefaultEnergyCost()
{
    s_Script_ResetWeaponToDefaultEnergyCost(this);
}

int C_WeaponX::Script_GetWeaponCurrentEnergyCost()
{
    return s_Script_GetWeaponCurrentEnergyCost(this);
}

void C_WeaponX::Script_SetWeaponEnergyCost(int arg1)
{
    s_Script_SetWeaponEnergyCost(this, arg1);
}

ScriptVariant_t C_WeaponX::GetWeaponInfoFileKeyField(char const* arg1)
{
    ScriptVariant_t result;
    s_GetWeaponInfoFileKeyField(this, &result, arg1);
    return result;
}

ScriptAsset C_WeaponX::GetWeaponInfoFileKeyFieldAsset(char const* arg1)
{
    ScriptAsset result;
    s_GetWeaponInfoFileKeyFieldAsset(this, &result, arg1);
    return result;
}

bool C_WeaponX::IsWeaponDisabled_Script(C_BaseEntity* arg1)
{
    return s_IsWeaponDisabled_Script(this, arg1);
}

bool C_WeaponX::Script_DeployWeapon()
{
    return s_Script_DeployWeapon(this);
}

bool C_WeaponX::Script_DeployWeaponInstant()
{
    return s_Script_DeployWeaponInstant(this);
}

bool C_WeaponX::Raise()
{
    return s_Raise(this);
}

bool C_WeaponX::ShouldPredictProjectiles()
{
    return s_ShouldPredictProjectiles(this);
}

ScriptVariant_t C_WeaponX::SmartAmmo_Script_GetStoredTargets()
{
    ScriptVariant_t result;
    s_SmartAmmo_Script_GetStoredTargets(this, &result);
    return result;
}

void C_WeaponX::SmartAmmo_Clear(bool arg1, bool arg2)
{
    s_SmartAmmo_Clear(this, arg1, arg2);
}

bool C_WeaponX::IsReadyToFire() const
{
    return s_IsReadyToFire(this);
}

bool C_WeaponX::IsBurstFireInProgress() const
{
    return s_IsBurstFireInProgress(this);
}

int C_WeaponX::GetBurstFireShotsPending() const
{
    return s_GetBurstFireShotsPending(this);
}

float C_WeaponX::TimeUntilReadyToFire() const
{
    return s_TimeUntilReadyToFire(this);
}

char const* C_WeaponX::GetWeaponName() const
{
    return s_GetWeaponName(this);
}

int C_WeaponX::GetDamageSourceID()
{
    return s_GetDamageSourceID(this);
}

float C_WeaponX::GetMaxDamageFarDist()
{
    return s_GetMaxDamageFarDist(this);
}

void C_WeaponX::ForceRelease()
{
    s_ForceRelease(this);
}

bool C_WeaponX::IsForceRelease() const
{
    return s_IsForceRelease(this);
}

void C_WeaponX::AddMod_Script(char const* const arg1)
{
    s_AddMod_Script(this, arg1);
}

void C_WeaponX::RemoveMod_Script(char const* const arg1)
{
    s_RemoveMod_Script(this, arg1);
}

int C_WeaponX::GetMods_Script(SQVM* arg1)
{
    return s_GetMods_Script(this, arg1);
}

int C_WeaponX::SetMods_Script(SQVM* arg1)
{
    return s_SetMods_Script(this, arg1);
}

bool C_WeaponX::IsOffhandWeapon() const
{
    return s_IsOffhandWeapon(this);
}

float C_WeaponX::GetNextAttackAllowedTime_Script() const
{
    return s_GetNextAttackAllowedTime_Script(this);
}

float C_WeaponX::GetNextAttackAllowedTimeRaw_Script() const
{
    return s_GetNextAttackAllowedTimeRaw_Script(this);
}

void C_WeaponX::SetNextAttackAllowedTime_Script(float arg1)
{
    s_SetNextAttackAllowedTime_Script(this, arg1);
}

int C_WeaponX::GetRodeoDamage_Script() const
{
    return s_GetRodeoDamage_Script(this);
}

int C_WeaponX::GetShotCount_Script() const
{
    return s_GetShotCount_Script(this);
}

char const* C_WeaponX::GetWeaponClass() const
{
    return s_GetWeaponClass(this);
}

bool C_WeaponX::IsInCooldown() const
{
    return s_IsInCooldown(this);
}

bool C_WeaponX::IsCooldownPending() const
{
    return s_IsCooldownPending(this);
}

int C_WeaponX::GetWeaponDamageFlags() const
{
    return s_GetWeaponDamageFlags(this);
}

int C_WeaponX::GetWeaponExplosionDamageFlags() const
{
    return s_GetWeaponExplosionDamageFlags(this);
}

int C_WeaponX::Script_GetImpactTableIndex() const
{
    return s_Script_GetImpactTableIndex(this);
}

bool C_WeaponX::Script_GetNPCMissFastPlayer() const
{
    return s_Script_GetNPCMissFastPlayer(this);
}

float C_WeaponX::Script_GetMeleeLungeTargetRange() const
{
    return s_Script_GetMeleeLungeTargetRange(this);
}

float C_WeaponX::Script_GetMeleeLungeTargetAngle() const
{
    return s_Script_GetMeleeLungeTargetAngle(this);
}

bool C_WeaponX::Script_GetMeleeCanHitHumanSized() const
{
    return s_Script_GetMeleeCanHitHumanSized(this);
}

bool C_WeaponX::Script_GetMeleeCanHitTitans() const
{
    return s_Script_GetMeleeCanHitTitans(this);
}

int C_WeaponX::Script_GetDamageAmountForArmorType(int arg1) const
{
    return s_Script_GetDamageAmountForArmorType(this, arg1);
}

float C_WeaponX::Script_GetMeleeAttackRange() const
{
    return s_Script_GetMeleeAttackRange(this);
}

float C_WeaponX::Script_GetMeleeAttackAngle() const
{
    return s_Script_GetMeleeAttackAngle(this);
}

char const* C_WeaponX::Script_GetMeleeAnim3p() const
{
    return s_Script_GetMeleeAnim3p(this);
}

char const* C_WeaponX::Script_GetWeaponReadyMsg() const
{
    return s_Script_GetWeaponReadyMsg(this);
}

char const* C_WeaponX::Script_GetWeaponReadyHint() const
{
    return s_Script_GetWeaponReadyHint(this);
}

float C_WeaponX::Script_GetGrenadeFuseTime() const
{
    return s_Script_GetGrenadeFuseTime(this);
}

float C_WeaponX::Script_GetGrenadeIgnitionTime() const
{
    return s_Script_GetGrenadeIgnitionTime(this);
}

bool C_WeaponX::Script_GetAllowHeadShots() const
{
    return s_Script_GetAllowHeadShots(this);
}

int C_WeaponX::Script_GetCurrentAltFireIndex() const
{
    return s_Script_GetCurrentAltFireIndex(this);
}

float C_WeaponX::Script_GetWeaponZoomFOV()
{
    return s_Script_GetWeaponZoomFOV(this);
}

int C_WeaponX::Script_GetReloadMilestoneIndex()
{
    return s_Script_GetReloadMilestoneIndex(this);
}

void C_WeaponX::Script_SetForcedADS()
{
    s_Script_SetForcedADS(this);
}

void C_WeaponX::Script_ClearForcedADS()
{
    s_Script_ClearForcedADS(this);
}

int C_WeaponX::Script_GetForcedADS() const
{
    return s_Script_GetForcedADS(this);
}

int C_WeaponX::Script_GetInventoryIndex() const
{
    return s_Script_GetInventoryIndex(this);
}

int C_WeaponX::Script_GetChargeAnimIndex() const
{
    return s_Script_GetChargeAnimIndex(this);
}

void C_WeaponX::Script_SetChargeAnimIndex(int arg1)
{
    s_Script_SetChargeAnimIndex(this, arg1);
}

float C_WeaponX::GetWeaponDamageForce() const
{
    return s_GetWeaponDamageForce(this);
}

float C_WeaponX::GetCoreDuration() const
{
    return s_GetCoreDuration(this);
}

bool C_WeaponX::IsSustainedDischargeWeapon() const
{
    return s_IsSustainedDischargeWeapon(this);
}

bool C_WeaponX::IsDischarging() const
{
    return s_IsDischarging(this);
}

float C_WeaponX::GetSustainedDischargeDuration() const
{
    return s_GetSustainedDischargeDuration(this);
}

float C_WeaponX::GetSustainedDischargeRemainder() const
{
    return s_GetSustainedDischargeRemainder(this);
}

float C_WeaponX::GetSustainedDischargeFraction() const
{
    return s_GetSustainedDischargeFraction(this);
}

float C_WeaponX::GetSustainedDischargePulseFrequency() const
{
    return s_GetSustainedDischargePulseFrequency(this);
}

void C_WeaponX::SetSustainedDischargeFractionForced(float arg1)
{
    s_SetSustainedDischargeFractionForced(this, arg1);
}

bool C_WeaponX::IsSustainedLaserWeapon() const
{
    return s_IsSustainedLaserWeapon(this);
}

void C_WeaponX::Script_DoMeleeHitConfirmation(float arg1)
{
    s_Script_DoMeleeHitConfirmation(this, arg1);
}

void C_WeaponX::Script_SetScriptTime0(float arg1)
{
    s_Script_SetScriptTime0(this, arg1);
}

float C_WeaponX::Script_GetScriptTime0()
{
    return s_Script_GetScriptTime0(this);
}

void C_WeaponX::Script_SetScriptFlags0(int arg1)
{
    s_Script_SetScriptFlags0(this, arg1);
}

int C_WeaponX::Script_GetScriptFlags0()
{
    return s_Script_GetScriptFlags0(this);
}

bool C_WeaponX::IsLoadoutPickup()
{
    return s_IsLoadoutPickup(this);
}

void C_WeaponX::HideWeapon()
{
    s_HideWeapon(this);
}

C_BaseEntity* C_WeaponX::GetWeaponOwner_Script() const
{
    return s_GetWeaponOwner_Script(this);
}

bool C_WeaponX::HasSilencer() const
{
    return s_HasSilencer(this);
}

int C_WeaponX::PlayWeaponEffectAndReturnViewEffectHandle_Script(const char* arg1, const char* arg2, const char* arg3)
{
    return s_PlayWeaponEffectAndReturnViewEffectHandle_Script(this, arg1, arg2, arg3);
}

void C_WeaponX::PlayWeaponEffectNoCull_Script(const char* arg1, const char* arg2, const char* arg3)
{
    s_PlayWeaponEffectNoCull_Script(this, arg1, arg2, arg3);
}

void C_WeaponX::PlayWeaponEffect_Script(const char* arg1, const char* arg2, const char* arg3)
{
    s_PlayWeaponEffect_Script(this, arg1, arg2, arg3);
}

C_BaseEntity* C_WeaponX::FireWeaponBolt_Script(const Vector3D& arg1, const Vector3D& arg2, float arg3, int arg4, int arg5, bool arg6, int arg7)
{
    return s_FireWeaponBolt_Script(this, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

int C_WeaponX::GetWeaponPrimaryAmmoCount_Script() const
{
    return s_GetWeaponPrimaryAmmoCount_Script(this);
}

void C_WeaponX::SetWeaponPrimaryAmmoCount_Script(int arg1)
{
    s_SetWeaponPrimaryAmmoCount_Script(this, arg1);
}

bool C_WeaponX::IsWeaponInAds_Script()
{
    return s_IsWeaponInAds_Script(this);
}

bool C_WeaponX::SmartAmmo_IsEnabled_Script()
{
    return s_SmartAmmo_IsEnabled_Script(this);
}

void C_WeaponX::SmartAmmo_SetNewTargetTime()
{
    s_SmartAmmo_SetNewTargetTime(this);
}

float C_WeaponX::SmartAmmo_GetNewTargetTime()
{
    return s_SmartAmmo_GetNewTargetTime(this);
}

float C_WeaponX::SmartAmmo_GetSearchAngle()
{
    return s_SmartAmmo_GetSearchAngle(this);
}

ScriptVariant_t C_WeaponX::SmartAmmo_Script_GetTargets()
{
    ScriptVariant_t result;
    s_SmartAmmo_Script_GetTargets(this, &result);
    return result;
}

void C_WeaponX::SmartAmmo_Script_SetTarget(C_BaseEntity* arg1, float arg2)
{
    s_SmartAmmo_Script_SetTarget(this, arg1, arg2);
}

void C_WeaponX::SmartAmmo_Script_StoreTargets()
{
    s_SmartAmmo_Script_StoreTargets(this);
}

Vector3D C_WeaponX::SmartAmmo_Script_GetFirePosition(C_BaseEntity* arg1, int arg2)
{
    Vector3D result;
    s_SmartAmmo_Script_GetFirePosition(this, &result, arg1, arg2);
    return result;
}

void C_WeaponX::SmartAmmo_Script_TrackEntity(C_BaseEntity* arg1, float arg2)
{
    s_SmartAmmo_Script_TrackEntity(this, arg1, arg2);
}

void C_WeaponX::SmartAmmo_Script_UntrackEntity(C_BaseEntity* arg1)
{
    s_SmartAmmo_Script_UntrackEntity(this, arg1);
}

int C_WeaponX::SmartAmmo_Script_GetNumTrackersOnEntity(C_BaseEntity* arg1)
{
    return s_SmartAmmo_Script_GetNumTrackersOnEntity(this, arg1);
}

bool C_WeaponX::SmartAmmo_IsVisibleTarget(C_BaseEntity* arg1)
{
    return s_SmartAmmo_IsVisibleTarget(this, arg1);
}

ScriptVariant_t C_WeaponX::SmartAmmo_Script_GetTrackedEntities()
{
    ScriptVariant_t result;
    s_SmartAmmo_Script_GetTrackedEntities(this, &result);
    return result;
}

bool C_WeaponX::HasMod_Script(const char* arg1)
{
    return s_HasMod_Script(this, arg1);
}

void C_WeaponX::SetModBitfield_Script(int arg1)
{
    s_SetModBitfield_Script(this, arg1);
}

int C_WeaponX::GetModBitfield_Script()
{
    return s_GetModBitfield_Script(this);
}

const char* C_WeaponX::GetSmartAmmoHudLockStyle_Script()
{
    return s_GetSmartAmmoHudLockStyle_Script(this);
}

const char* C_WeaponX::GetSmartAmmoWeaponType_Script()
{
    return s_GetSmartAmmoWeaponType_Script(this);
}

void C_WeaponX::SetAttackKickScale_Script(float arg1)
{
    s_SetAttackKickScale_Script(this, arg1);
}

void C_WeaponX::SetAttackKickRollScale_Script(float arg1)
{
    s_SetAttackKickRollScale_Script(this, arg1);
}

int C_WeaponX::Script_GetWeaponSettingInt(SQVM* arg1)
{
    return s_Script_GetWeaponSettingInt(this, arg1);
}

int C_WeaponX::Script_GetWeaponSettingFloat(SQVM* arg1)
{
    return s_Script_GetWeaponSettingFloat(this, arg1);
}

int C_WeaponX::Script_GetWeaponSettingBool(SQVM* arg1)
{
    return s_Script_GetWeaponSettingBool(this, arg1);
}

int C_WeaponX::Script_GetWeaponSettingVector(SQVM* arg1)
{
    return s_Script_GetWeaponSettingVector(this, arg1);
}

int C_WeaponX::Script_GetWeaponSettingString(SQVM* arg1)
{
    return s_Script_GetWeaponSettingString(this, arg1);
}

int C_WeaponX::Script_GetWeaponSettingAsset(SQVM* arg1)
{
    return s_Script_GetWeaponSettingAsset(this, arg1);
}

FileWeaponInfo_t* (*s_GetFileWeaponInfoFromHandle)(WEAPON_FILE_INFO_HANDLE);
FileWeaponInfo_t* (*s_GetFileWeaponInfoFromName)(const char*);
void (*s_ParseFileWeaponInfo)(FileWeaponInfo_t*, KeyValues*, const char*);
bool (*s_ReadWeaponDataFromFileForSlot)(IFileSystem*, const char*, WEAPON_FILE_INFO_HANDLE*, const unsigned char*);
KeyValues* (*s_ReadEncryptedKVFile)(IFileSystem*, const char*, const unsigned char*);
WeaponString_t (*s_AllocWeaponString)(FileWeaponInfo_t*, const char*);
bool (*s_GetIndexForModName)(const char*, const FileWeaponInfo_t*, unsigned int*);
bool (*s_CalcWeaponMods)(unsigned int, const FileWeaponInfo_t*, WeaponModValues*, bool, unsigned int);
datamap_t* s_WeaponPlayerDataPredMap;
datamap_t* s_SmartAmmoPredMap;

FileWeaponInfo_t* GetFileWeaponInfoFromHandle(WEAPON_FILE_INFO_HANDLE handle)
{
    return s_GetFileWeaponInfoFromHandle(handle);
}

FileWeaponInfo_t* GetFileWeaponInfoFromName(const char* name)
{
    return s_GetFileWeaponInfoFromName(name);
}

WEAPON_FILE_INFO_HANDLE LookupWeaponInfoSlot(const char* name)
{
    const FileWeaponInfo_t* info = GetFileWeaponInfoFromName(name);
    return info ? info->infoHandle : GetInvalidWeaponInfoHandle();
}

void FileWeaponInfo_t::Parse(KeyValues* data, const char* weaponName)
{
    s_ParseFileWeaponInfo(this, data, weaponName);
}

bool ReadWeaponDataFromFileForSlot(IFileSystem* filesystem, const char* weaponName, WEAPON_FILE_INFO_HANDLE* handle, const unsigned char* iceKey)
{
    return s_ReadWeaponDataFromFileForSlot(filesystem, weaponName, handle, iceKey);
}

KeyValues* ReadEncryptedKVFile(IFileSystem* filesystem, const char* filenameWithoutExtension, const unsigned char* iceKey)
{
    return s_ReadEncryptedKVFile(filesystem, filenameWithoutExtension, iceKey);
}

WeaponString_t AllocWeaponString(FileWeaponInfo_t* info, const char* string)
{
    return s_AllocWeaponString(info, string);
}

bool GetIndexForModName(const char* modName, const FileWeaponInfo_t* info, unsigned int* index)
{
    return s_GetIndexForModName(modName, info, index);
}

bool CalcWeaponMods(unsigned int bitfield, const FileWeaponInfo_t* info, WeaponModValues* values, bool singlePlayer, unsigned int overrideMods)
{
    return s_CalcWeaponMods(bitfield, info, values, singlePlayer, overrideMods);
}

datamap_t* WeaponPlayerData_Client::GetPredDescMap()
{
    return s_WeaponPlayerDataPredMap;
}
const datamap_t* WeaponPlayerData_Client::GetPredDescMap() const
{
    return s_WeaponPlayerDataPredMap;
}
datamap_t* SmartAmmo_WeaponData_Client::GetPredDescMap()
{
    return s_SmartAmmoPredMap;
}
const datamap_t* SmartAmmo_WeaponData_Client::GetPredDescMap() const
{
    return s_SmartAmmoPredMap;
}

void C_WeaponX::EmitWeaponNpcSound(float soundRadius, float soundDuration)
{
    return s_EmitWeaponNpcSound(this, soundRadius, soundDuration);
}

void C_WeaponX::EmitWeaponNpcSound_DontUpdateLastFiredTime(float soundRadius, float soundDuration)
{
    return s_EmitWeaponNpcSound_DontUpdateLastFiredTime(this, soundRadius, soundDuration);
}

void C_WeaponX::ShowWeapon()
{
    return s_ShowWeapon(this);
}

void C_WeaponX::SetViewmodelAmmoModelIndex_Script(int modelIndex)
{
    return s_SetViewmodelAmmoModelIndex_Script(this, modelIndex);
}

int C_WeaponX::GetWeaponChargeLevel() const
{
    return s_GetWeaponChargeLevel(this);
}

float C_WeaponX::GetChargeFraction() const
{
    return s_GetChargeFraction(this);
}

bool C_WeaponX::DeployWeapon(bool forceDeploy)
{
    return s_DeployWeapon(this, forceDeploy);
}

bool C_WeaponX::Holster()
{
    return s_Holster(this);
}

bool C_WeaponX::FastHolster()
{
    return s_FastHolster(this);
}

bool C_WeaponX::Lower()
{
    return s_Lower(this);
}

bool C_WeaponX::Reload()
{
    return s_Reload(this);
}

bool C_WeaponX::PrimaryAttack()
{
    return s_PrimaryAttack(this);
}

bool C_WeaponX::ChargeEnd_Internal(bool allowAttack, bool updateState)
{
    return s_ChargeEnd_Internal(this, allowAttack, updateState);
}

bool C_WeaponX::HolsterInternal(bool fastHolster)
{
    return s_HolsterInternal(this, fastHolster);
}

bool C_WeaponX::RaiseInternal(bool fromSprint, bool arg2)
{
    return s_RaiseInternal(this, fromSprint, arg2);
}

bool C_WeaponX::SustainedDischargeBegin()
{
    return s_SustainedDischargeBegin(this);
}

ScriptStringOrNull C_BaseCombatWeapon::GetPrintName() const
{
    ScriptStringOrNull result;
    s_GetPrintName(this, &result);
    return result;
}

ScriptStringOrNull C_BaseCombatWeapon::GetWeaponDescription() const
{
    ScriptStringOrNull result;
    s_GetWeaponDescription(this, &result);
    return result;
}

void C_BaseCombatWeapon::Script_SetDroppedModel(const char* modelName)
{
    return s_Script_SetDroppedModel(this, modelName);
}

int C_BaseCombatWeapon::ScriptLookupWorldModelAttachment(const char* attachmentName)
{
    return s_ScriptLookupWorldModelAttachment(this, attachmentName);
}

int C_BaseCombatWeapon::ScriptLookupViewModelAttachment(const char* attachmentName)
{
    return s_ScriptLookupViewModelAttachment(this, attachmentName);
}

ON_DLL_LOAD_CLIENT("client.dll", WeaponSdkMethods, [](CModule module)
{
    s_EmitWeaponNpcSound = module.Offset(0x5A5300).RCast<decltype(s_EmitWeaponNpcSound)>();
    s_EmitWeaponNpcSound_DontUpdateLastFiredTime = module.Offset(0x5A5310).RCast<decltype(s_EmitWeaponNpcSound_DontUpdateLastFiredTime)>();
    s_ShowWeapon = module.Offset(0x5B8AB0).RCast<decltype(s_ShowWeapon)>();
    s_SetViewmodelAmmoModelIndex_Script = module.Offset(0x5B82A0).RCast<decltype(s_SetViewmodelAmmoModelIndex_Script)>();
    s_GetWeaponChargeLevel = module.Offset(0x5A9540).RCast<decltype(s_GetWeaponChargeLevel)>();
    s_GetChargeFraction = module.Offset(0x5A6D60).RCast<decltype(s_GetChargeFraction)>();
    s_DeployWeapon = module.Offset(0x5A42D0).RCast<decltype(s_DeployWeapon)>();
    s_Holster = module.Offset(0x5AB400).RCast<decltype(s_Holster)>();
    s_FastHolster = module.Offset(0x5A5A50).RCast<decltype(s_FastHolster)>();
    s_Lower = module.Offset(0x5B0FE0).RCast<decltype(s_Lower)>();
    s_Reload = module.Offset(0x5B6470).RCast<decltype(s_Reload)>();
    s_PrimaryAttack = module.Offset(0x5B48C0).RCast<decltype(s_PrimaryAttack)>();
    s_ChargeEnd_Internal = module.Offset(0x5A24F0).RCast<decltype(s_ChargeEnd_Internal)>();
    s_HolsterInternal = module.Offset(0x5AB410).RCast<decltype(s_HolsterInternal)>();
    s_RaiseInternal = module.Offset(0x5B5990).RCast<decltype(s_RaiseInternal)>();
    s_SustainedDischargeBegin = module.Offset(0x5B9C00).RCast<decltype(s_SustainedDischargeBegin)>();
    s_GetPrintName = module.Offset(0xBB180).RCast<decltype(s_GetPrintName)>();
    s_GetWeaponDescription = module.Offset(0xBB360).RCast<decltype(s_GetWeaponDescription)>();
    s_Script_SetDroppedModel = module.Offset(0xBD340).RCast<decltype(s_Script_SetDroppedModel)>();
    s_ScriptLookupWorldModelAttachment = module.Offset(0xBD260).RCast<decltype(s_ScriptLookupWorldModelAttachment)>();
    s_ScriptLookupViewModelAttachment = module.Offset(0xBD180).RCast<decltype(s_ScriptLookupViewModelAttachment)>();
    s_GetAttackPosition_Script = module.Offset(0x5A6C60).RCast<decltype(s_GetAttackPosition_Script)>();
    s_GetAttackDirection_Script = module.Offset(0x5A6BB0).RCast<decltype(s_GetAttackDirection_Script)>();
    s_AllowUse = module.Offset(0x59CA20).RCast<decltype(s_AllowUse)>();
    s_Script_EmitWeaponSound_1p3p = module.Offset(0x5B73C0).RCast<decltype(s_Script_EmitWeaponSound_1p3p)>();
    s_EmitWeaponSound_Script = module.Offset(0x5A54A0).RCast<decltype(s_EmitWeaponSound_Script)>();
    s_StopWeaponSound_Script = module.Offset(0x5B9B70).RCast<decltype(s_StopWeaponSound_Script)>();
    s_IsNetOptimized = module.Offset(0xBC210).RCast<decltype(s_IsNetOptimized)>();
    s_StopWeaponEffect_Script = module.Offset(0x5B9A00).RCast<decltype(s_StopWeaponEffect_Script)>();
    s_SetWeaponBurstFireCount_Script = module.Offset(0x5B8320).RCast<decltype(s_SetWeaponBurstFireCount_Script)>();
    s_GetWeaponBurstFireCount_Script = module.Offset(0x5A94B0).RCast<decltype(s_GetWeaponBurstFireCount_Script)>();
    s_SetWeaponSkin = module.Offset(0x5B8640).RCast<decltype(s_SetWeaponSkin)>();
    s_SetWeaponCamo = module.Offset(0x5B83B0).RCast<decltype(s_SetWeaponCamo)>();
    s_FireWeaponBullet_Script = module.Offset(0x5A6470).RCast<decltype(s_FireWeaponBullet_Script)>();
    s_FireWeaponBullet_Internal = module.Offset(0x5A6080).RCast<decltype(s_FireWeaponBullet_Internal)>();
    s_FireWeaponGrenade_Script = module.Offset(0x5A67E0).RCast<decltype(s_FireWeaponGrenade_Script)>();
    s_FireWeaponMissile_Script = module.Offset(0x5A6850).RCast<decltype(s_FireWeaponMissile_Script)>();
    s_GetProjectilesPerShot_Script = module.Offset(0x5A8980).RCast<decltype(s_GetProjectilesPerShot_Script)>();
    s_GetAmmoPerShot_Script = module.Offset(0x5A6BA0).RCast<decltype(s_GetAmmoPerShot_Script)>();
    s_GetAmmoDisplay_Script = module.Offset(0x5A6B70).RCast<decltype(s_GetAmmoDisplay_Script)>();
    s_GetWeaponPrimaryClipCountMax_Script = module.Offset(0x5A9D20).RCast<decltype(s_GetWeaponPrimaryClipCountMax_Script)>();
    s_SetWeaponPrimaryClipCount_Script = module.Offset(0x5B84D0).RCast<decltype(s_SetWeaponPrimaryClipCount_Script)>();
    s_SetWeaponPrimaryClipCountAbsolute_Script = module.Offset(0x5B84B0).RCast<decltype(s_SetWeaponPrimaryClipCountAbsolute_Script)>();
    s_SetWeaponPrimaryClipCountNoRegenReset_Script = module.Offset(0x5B84C0).RCast<decltype(s_SetWeaponPrimaryClipCountNoRegenReset_Script)>();
    s_IsWeaponRegenDraining_Script = module.Offset(0x5B0AA0).RCast<decltype(s_IsWeaponRegenDraining_Script)>();
    s_GetWeaponPrimaryClipCount_Script = module.Offset(0x5A9D30).RCast<decltype(s_GetWeaponPrimaryClipCount_Script)>();
    s_Script_RegenerateAmmoReset = module.Offset(0x5B76F0).RCast<decltype(s_Script_RegenerateAmmoReset)>();
    s_Script_GetLifetimeShotsRemaining = module.Offset(0x5B7510).RCast<decltype(s_Script_GetLifetimeShotsRemaining)>();
    s_Script_SetLifetimeShotsRemaining = module.Offset(0x5B7750).RCast<decltype(s_Script_SetLifetimeShotsRemaining)>();
    s_Script_SetLifetimeShotsRemainingInfinite = module.Offset(0x5B7770).RCast<decltype(s_Script_SetLifetimeShotsRemainingInfinite)>();
    s_GetWeaponUtilityEntity = module.Offset(0x5AA8D0).RCast<decltype(s_GetWeaponUtilityEntity)>();
    s_IsWeaponAdsButtonPressed = module.Offset(0x5B0920).RCast<decltype(s_IsWeaponAdsButtonPressed)>();
    s_GetWeaponType = module.Offset(0x5AA8B0).RCast<decltype(s_GetWeaponType)>();
    s_IsChargeWeapon = module.Offset(0x5B0590).RCast<decltype(s_IsChargeWeapon)>();
    s_IsWeaponCharging_Script = module.Offset(0x5B0980).RCast<decltype(s_IsWeaponCharging_Script)>();
    s_GetWeaponChargeTime_Script = module.Offset(0x5A96F0).RCast<decltype(s_GetWeaponChargeTime_Script)>();
    s_GetWeaponChargeTimeRemaining_Script = module.Offset(0x5A9630).RCast<decltype(s_GetWeaponChargeTimeRemaining_Script)>();
    s_GetWeaponChargeFractionClamped = module.Offset(0x5A94F0).RCast<decltype(s_GetWeaponChargeFractionClamped)>();
    s_Script_SetWeaponChargeFraction = module.Offset(0x5B77A0).RCast<decltype(s_Script_SetWeaponChargeFraction)>();
    s_Script_SetWeaponChargeFractionForced = module.Offset(0x5B7890).RCast<decltype(s_Script_SetWeaponChargeFractionForced)>();
    s_GetWeaponChargeLevel_Script = module.Offset(0x5A95F0).RCast<decltype(s_GetWeaponChargeLevel_Script)>();
    s_GetWeaponChargeLevelMax_Script = module.Offset(0x5A95E0).RCast<decltype(s_GetWeaponChargeLevelMax_Script)>();
    s_GetChargeDuration = module.Offset(0x5A6D50).RCast<decltype(s_GetChargeDuration)>();
    s_GetWeaponReadyToFireProgress = module.Offset(0x5A9EF0).RCast<decltype(s_GetWeaponReadyToFireProgress)>();
    s_Script_GetWeaponChargeEnergyCost = module.Offset(0x5B75D0).RCast<decltype(s_Script_GetWeaponChargeEnergyCost)>();
    s_Script_GetWeaponDefaultEnergyCost = module.Offset(0x5B75F0).RCast<decltype(s_Script_GetWeaponDefaultEnergyCost)>();
    s_Script_ResetWeaponToDefaultEnergyCost = module.Offset(0x5B7710).RCast<decltype(s_Script_ResetWeaponToDefaultEnergyCost)>();
    s_Script_GetWeaponCurrentEnergyCost = module.Offset(0x5B75E0).RCast<decltype(s_Script_GetWeaponCurrentEnergyCost)>();
    s_Script_SetWeaponEnergyCost = module.Offset(0x5B7910).RCast<decltype(s_Script_SetWeaponEnergyCost)>();
    s_GetWeaponInfoFileKeyField = module.Offset(0x5A9880).RCast<decltype(s_GetWeaponInfoFileKeyField)>();
    s_GetWeaponInfoFileKeyFieldAsset = module.Offset(0x5A98C0).RCast<decltype(s_GetWeaponInfoFileKeyFieldAsset)>();
    s_IsWeaponDisabled_Script = module.Offset(0x5B09C0).RCast<decltype(s_IsWeaponDisabled_Script)>();
    s_Script_DeployWeapon = module.Offset(0x5B7010).RCast<decltype(s_Script_DeployWeapon)>();
    s_Script_DeployWeaponInstant = module.Offset(0x5B7280).RCast<decltype(s_Script_DeployWeaponInstant)>();
    s_Raise = module.Offset(0x5B5960).RCast<decltype(s_Raise)>();
    s_ShouldPredictProjectiles = module.Offset(0x5B8890).RCast<decltype(s_ShouldPredictProjectiles)>();
    s_SmartAmmo_Script_GetStoredTargets = module.Offset(0x5B8F40).RCast<decltype(s_SmartAmmo_Script_GetStoredTargets)>();
    s_SmartAmmo_Clear = module.Offset(0x5B8B20).RCast<decltype(s_SmartAmmo_Clear)>();
    s_IsReadyToFire = module.Offset(0x4DADD0).RCast<decltype(s_IsReadyToFire)>();
    s_IsBurstFireInProgress = module.Offset(0x5B0540).RCast<decltype(s_IsBurstFireInProgress)>();
    s_GetBurstFireShotsPending = module.Offset(0x5A6D10).RCast<decltype(s_GetBurstFireShotsPending)>();
    s_TimeUntilReadyToFire = module.Offset(0x5BBCF0).RCast<decltype(s_TimeUntilReadyToFire)>();
    s_GetWeaponName = module.Offset(0x5A9940).RCast<decltype(s_GetWeaponName)>();
    s_GetDamageSourceID = module.Offset(0x5A72B0).RCast<decltype(s_GetDamageSourceID)>();
    s_GetMaxDamageFarDist = module.Offset(0x5A7E20).RCast<decltype(s_GetMaxDamageFarDist)>();
    s_ForceRelease = module.Offset(0x5A6A00).RCast<decltype(s_ForceRelease)>();
    s_IsForceRelease = module.Offset(0x5B0600).RCast<decltype(s_IsForceRelease)>();
    s_AddMod_Script = module.Offset(0x59C200).RCast<decltype(s_AddMod_Script)>();
    s_RemoveMod_Script = module.Offset(0x5B68D0).RCast<decltype(s_RemoveMod_Script)>();
    s_GetMods_Script = module.Offset(0x5A7FA0).RCast<decltype(s_GetMods_Script)>();
    s_SetMods_Script = module.Offset(0x5B7C30).RCast<decltype(s_SetMods_Script)>();
    s_IsOffhandWeapon = module.Offset(0xB3600).RCast<decltype(s_IsOffhandWeapon)>();
    s_GetNextAttackAllowedTime_Script = module.Offset(0x5A8150).RCast<decltype(s_GetNextAttackAllowedTime_Script)>();
    s_GetNextAttackAllowedTimeRaw_Script = module.Offset(0x5A8140).RCast<decltype(s_GetNextAttackAllowedTimeRaw_Script)>();
    s_SetNextAttackAllowedTime_Script = module.Offset(0x5B7DA0).RCast<decltype(s_SetNextAttackAllowedTime_Script)>();
    s_GetRodeoDamage_Script = module.Offset(0x5A8C50).RCast<decltype(s_GetRodeoDamage_Script)>();
    s_GetShotCount_Script = module.Offset(0x5A8EF0).RCast<decltype(s_GetShotCount_Script)>();
    s_GetWeaponClass = module.Offset(0x5A9740).RCast<decltype(s_GetWeaponClass)>();
    s_IsInCooldown = module.Offset(0x5B0610).RCast<decltype(s_IsInCooldown)>();
    s_IsCooldownPending = module.Offset(0x5B05B0).RCast<decltype(s_IsCooldownPending)>();
    s_GetWeaponDamageFlags = module.Offset(0x5A97A0).RCast<decltype(s_GetWeaponDamageFlags)>();
    s_GetWeaponExplosionDamageFlags = module.Offset(0x5A9800).RCast<decltype(s_GetWeaponExplosionDamageFlags)>();
    s_Script_GetImpactTableIndex = module.Offset(0x5B7490).RCast<decltype(s_Script_GetImpactTableIndex)>();
    s_Script_GetNPCMissFastPlayer = module.Offset(0x5B7590).RCast<decltype(s_Script_GetNPCMissFastPlayer)>();
    s_Script_GetMeleeLungeTargetRange = module.Offset(0x5B7580).RCast<decltype(s_Script_GetMeleeLungeTargetRange)>();
    s_Script_GetMeleeLungeTargetAngle = module.Offset(0x5B7570).RCast<decltype(s_Script_GetMeleeLungeTargetAngle)>();
    s_Script_GetMeleeCanHitHumanSized = module.Offset(0x5B7550).RCast<decltype(s_Script_GetMeleeCanHitHumanSized)>();
    s_Script_GetMeleeCanHitTitans = module.Offset(0x5B7560).RCast<decltype(s_Script_GetMeleeCanHitTitans)>();
    s_Script_GetDamageAmountForArmorType = module.Offset(0x5B7420).RCast<decltype(s_Script_GetDamageAmountForArmorType)>();
    s_Script_GetMeleeAttackRange = module.Offset(0x5B7540).RCast<decltype(s_Script_GetMeleeAttackRange)>();
    s_Script_GetMeleeAttackAngle = module.Offset(0x5B7530).RCast<decltype(s_Script_GetMeleeAttackAngle)>();
    s_Script_GetMeleeAnim3p = module.Offset(0x5B7520).RCast<decltype(s_Script_GetMeleeAnim3p)>();
    s_Script_GetWeaponReadyMsg = module.Offset(0x5B7610).RCast<decltype(s_Script_GetWeaponReadyMsg)>();
    s_Script_GetWeaponReadyHint = module.Offset(0x5B7600).RCast<decltype(s_Script_GetWeaponReadyHint)>();
    s_Script_GetGrenadeFuseTime = module.Offset(0x5B7470).RCast<decltype(s_Script_GetGrenadeFuseTime)>();
    s_Script_GetGrenadeIgnitionTime = module.Offset(0x5B7480).RCast<decltype(s_Script_GetGrenadeIgnitionTime)>();
    s_Script_GetAllowHeadShots = module.Offset(0x5B73F0).RCast<decltype(s_Script_GetAllowHeadShots)>();
    s_Script_GetCurrentAltFireIndex = module.Offset(0x5B7410).RCast<decltype(s_Script_GetCurrentAltFireIndex)>();
    s_Script_GetWeaponZoomFOV = module.Offset(0x5B76E0).RCast<decltype(s_Script_GetWeaponZoomFOV)>();
    s_Script_GetReloadMilestoneIndex = module.Offset(0x5B75A0).RCast<decltype(s_Script_GetReloadMilestoneIndex)>();
    s_Script_SetForcedADS = module.Offset(0x5B7740).RCast<decltype(s_Script_SetForcedADS)>();
    s_Script_ClearForcedADS = module.Offset(0x5B6FF0).RCast<decltype(s_Script_ClearForcedADS)>();
    s_Script_GetForcedADS = module.Offset(0x5B7460).RCast<decltype(s_Script_GetForcedADS)>();
    s_Script_GetInventoryIndex = module.Offset(0x5B74A0).RCast<decltype(s_Script_GetInventoryIndex)>();
    s_Script_GetChargeAnimIndex = module.Offset(0x5B7400).RCast<decltype(s_Script_GetChargeAnimIndex)>();
    s_Script_SetChargeAnimIndex = module.Offset(0x5B7720).RCast<decltype(s_Script_SetChargeAnimIndex)>();
    s_GetWeaponDamageForce = module.Offset(0x5A97B0).RCast<decltype(s_GetWeaponDamageForce)>();
    s_GetCoreDuration = module.Offset(0x5A71A0).RCast<decltype(s_GetCoreDuration)>();
    s_IsSustainedDischargeWeapon = module.Offset(0x5B08E0).RCast<decltype(s_IsSustainedDischargeWeapon)>();
    s_IsDischarging = module.Offset(0x5B05C0).RCast<decltype(s_IsDischarging)>();
    s_GetSustainedDischargeDuration = module.Offset(0x5A91F0).RCast<decltype(s_GetSustainedDischargeDuration)>();
    s_GetSustainedDischargeRemainder = module.Offset(0x5A9260).RCast<decltype(s_GetSustainedDischargeRemainder)>();
    s_GetSustainedDischargeFraction = module.Offset(0x5A9200).RCast<decltype(s_GetSustainedDischargeFraction)>();
    s_GetSustainedDischargePulseFrequency = module.Offset(0x5A9250).RCast<decltype(s_GetSustainedDischargePulseFrequency)>();
    s_SetSustainedDischargeFractionForced = module.Offset(0x5B7F60).RCast<decltype(s_SetSustainedDischargeFractionForced)>();
    s_IsSustainedLaserWeapon = module.Offset(0x5B0900).RCast<decltype(s_IsSustainedLaserWeapon)>();
    s_Script_DoMeleeHitConfirmation = module.Offset(0x5B7350).RCast<decltype(s_Script_DoMeleeHitConfirmation)>();
    s_Script_SetScriptTime0 = module.Offset(0x5B7790).RCast<decltype(s_Script_SetScriptTime0)>();
    s_Script_GetScriptTime0 = module.Offset(0x5B75C0).RCast<decltype(s_Script_GetScriptTime0)>();
    s_Script_SetScriptFlags0 = module.Offset(0x5B7780).RCast<decltype(s_Script_SetScriptFlags0)>();
    s_Script_GetScriptFlags0 = module.Offset(0x5B75B0).RCast<decltype(s_Script_GetScriptFlags0)>();
    s_IsLoadoutPickup = module.Offset(0x5B0630).RCast<decltype(s_IsLoadoutPickup)>();
    s_HideWeapon = module.Offset(0x5AB390).RCast<decltype(s_HideWeapon)>();
    s_GetWeaponOwner_Script = module.Offset(0x5A9C70).RCast<decltype(s_GetWeaponOwner_Script)>();
    s_HasSilencer = module.Offset(0x5AB350).RCast<decltype(s_HasSilencer)>();
    s_PlayWeaponEffectAndReturnViewEffectHandle_Script =
        module.Offset(0x5B31F0).RCast<decltype(s_PlayWeaponEffectAndReturnViewEffectHandle_Script)>();
    s_PlayWeaponEffectNoCull_Script = module.Offset(0x5B32A0).RCast<decltype(s_PlayWeaponEffectNoCull_Script)>();
    s_PlayWeaponEffect_Script = module.Offset(0x5B32C0).RCast<decltype(s_PlayWeaponEffect_Script)>();
    s_FireWeaponBolt_Script = module.Offset(0x5A5D10).RCast<decltype(s_FireWeaponBolt_Script)>();
    s_GetWeaponPrimaryAmmoCount_Script = module.Offset(0x5A9CB0).RCast<decltype(s_GetWeaponPrimaryAmmoCount_Script)>();
    s_SetWeaponPrimaryAmmoCount_Script = module.Offset(0x5B8420).RCast<decltype(s_SetWeaponPrimaryAmmoCount_Script)>();
    s_IsWeaponInAds_Script = module.Offset(0x5B0A30).RCast<decltype(s_IsWeaponInAds_Script)>();
    s_SmartAmmo_IsEnabled_Script = module.Offset(0x5B8D90).RCast<decltype(s_SmartAmmo_IsEnabled_Script)>();
    s_SmartAmmo_SetNewTargetTime = module.Offset(0x5B93A0).RCast<decltype(s_SmartAmmo_SetNewTargetTime)>();
    s_SmartAmmo_GetNewTargetTime = module.Offset(0x5B8D30).RCast<decltype(s_SmartAmmo_GetNewTargetTime)>();
    s_SmartAmmo_GetSearchAngle = module.Offset(0x5B8D80).RCast<decltype(s_SmartAmmo_GetSearchAngle)>();
    s_SmartAmmo_Script_GetTargets = module.Offset(0x5B90B0).RCast<decltype(s_SmartAmmo_Script_GetTargets)>();
    s_SmartAmmo_Script_SetTarget = module.Offset(0x5B9210).RCast<decltype(s_SmartAmmo_Script_SetTarget)>();
    s_SmartAmmo_Script_StoreTargets = module.Offset(0x5B9230).RCast<decltype(s_SmartAmmo_Script_StoreTargets)>();
    s_SmartAmmo_Script_GetFirePosition = module.Offset(0x5B8E90).RCast<decltype(s_SmartAmmo_Script_GetFirePosition)>();
    s_SmartAmmo_Script_TrackEntity = module.Offset(0x5B9360).RCast<decltype(s_SmartAmmo_Script_TrackEntity)>();
    s_SmartAmmo_Script_UntrackEntity = module.Offset(0x5B9380).RCast<decltype(s_SmartAmmo_Script_UntrackEntity)>();
    s_SmartAmmo_Script_GetNumTrackersOnEntity = module.Offset(0x5B8F10).RCast<decltype(s_SmartAmmo_Script_GetNumTrackersOnEntity)>();
    s_SmartAmmo_IsVisibleTarget = module.Offset(0x5B8DA0).RCast<decltype(s_SmartAmmo_IsVisibleTarget)>();
    s_SmartAmmo_Script_GetTrackedEntities = module.Offset(0x5B9110).RCast<decltype(s_SmartAmmo_Script_GetTrackedEntities)>();
    s_HasMod_Script = module.Offset(0x5AB2E0).RCast<decltype(s_HasMod_Script)>();
    s_SetModBitfield_Script = module.Offset(0x5B7B80).RCast<decltype(s_SetModBitfield_Script)>();
    s_GetModBitfield_Script = module.Offset(0x5A7EC0).RCast<decltype(s_GetModBitfield_Script)>();
    s_GetSmartAmmoHudLockStyle_Script = module.Offset(0x5A8F00).RCast<decltype(s_GetSmartAmmoHudLockStyle_Script)>();
    s_GetSmartAmmoWeaponType_Script = module.Offset(0x5A8F10).RCast<decltype(s_GetSmartAmmoWeaponType_Script)>();
    s_SetAttackKickScale_Script = module.Offset(0x5B7AC0).RCast<decltype(s_SetAttackKickScale_Script)>();
    s_SetAttackKickRollScale_Script = module.Offset(0x5B7AB0).RCast<decltype(s_SetAttackKickRollScale_Script)>();
    s_Script_GetWeaponSettingInt = module.Offset(0x5B7680).RCast<decltype(s_Script_GetWeaponSettingInt)>();
    s_Script_GetWeaponSettingFloat = module.Offset(0x5B7660).RCast<decltype(s_Script_GetWeaponSettingFloat)>();
    s_Script_GetWeaponSettingBool = module.Offset(0x5B7640).RCast<decltype(s_Script_GetWeaponSettingBool)>();
    s_Script_GetWeaponSettingVector = module.Offset(0x5B76C0).RCast<decltype(s_Script_GetWeaponSettingVector)>();
    s_Script_GetWeaponSettingString = module.Offset(0x5B76A0).RCast<decltype(s_Script_GetWeaponSettingString)>();
    s_Script_GetWeaponSettingAsset = module.Offset(0x5B7620).RCast<decltype(s_Script_GetWeaponSettingAsset)>();
    s_GetFileWeaponInfoFromHandle = module.Offset(0x3CB030).RCast<decltype(s_GetFileWeaponInfoFromHandle)>();
    s_GetFileWeaponInfoFromName = module.Offset(0x3CB050).RCast<decltype(s_GetFileWeaponInfoFromName)>();
    s_ParseFileWeaponInfo = module.Offset(0x3CFAC0).RCast<decltype(s_ParseFileWeaponInfo)>();
    s_ReadWeaponDataFromFileForSlot = module.Offset(0x3D2950).RCast<decltype(s_ReadWeaponDataFromFileForSlot)>();
    s_ReadEncryptedKVFile = module.Offset(0x3D2710).RCast<decltype(s_ReadEncryptedKVFile)>();
    s_AllocWeaponString = module.Offset(0x3C9030).RCast<decltype(s_AllocWeaponString)>();
    s_GetIndexForModName = module.Offset(0x3CB1C0).RCast<decltype(s_GetIndexForModName)>();
    s_CalcWeaponMods = module.Offset(0x3CA0B0).RCast<decltype(s_CalcWeaponMods)>();
    s_WeaponPlayerDataPredMap = module.Offset(0xB4A660).RCast<datamap_t*>();
    s_SmartAmmoPredMap = module.Offset(0xB492A0).RCast<datamap_t*>();
})
