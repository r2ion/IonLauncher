#include "game/client/r1/c_weapon_x.h"
#include "engine/client/cdll_engine_int.h"
#include "game/shared/weapon_mod_data.h"
#include "tier0/frametask.h"
#include "tier0/hooks.h"
#include "vscript/ivscript.h"
#include "vscript/languages/squirrel_re/squirrel.h"

#include <algorithm>
#include <array>
#include <mutex>

C_WeaponXExtended C_WeaponX::sm_WeaponsExtended[NUM_ENT_ENTRIES];

C_WeaponXExtended::C_WeaponXExtended()
{
    m_WeaponPrediction.Clear();
}

C_WeaponXExtended* C_WeaponX::GetWeaponExtended()
{
    const CBaseHandle handle = GetRefEHandle();
    const int index = handle.GetEntryIndex();
    if (index < 0 || index >= NUM_ENT_ENTRIES)
        return nullptr;
    C_WeaponXExtended& extended = sm_WeaponsExtended[index];
    if (extended.m_WeaponPrediction.weaponHandle != 0xFFFFFFFFu && extended.m_WeaponPrediction.weaponHandle != handle.ToInt())
        extended.Forget();
    return &extended;
}

C_WeaponXExtended* C_WeaponX::FindWeaponExtended(const std::uint32_t handle)
{
    const unsigned int index = handle & 0xFFFFu;
    if (index >= NUM_ENT_ENTRIES)
        return nullptr;
    C_WeaponXExtended& extended = sm_WeaponsExtended[index];
    return extended.m_WeaponPrediction.weaponHandle == handle ? &extended : nullptr;
}
DECLARE_MODULE(ClientWeaponInterpolation)
DECLARE_HOOK(C_WeaponX_GetMods, client.dll + 0x5A7FA0, [](auto& hook, C_WeaponX* pWeapon, HSQUIRRELVM sqvm) -> SQRESULT
{
    if (pWeapon->GetWeaponFileInfoHandle() != INVALID_WEAPON_INFO_HANDLE)
        return hook.Original(pWeapon, sqvm);

    g_pSquirrel[ScriptContext::CLIENT]->raiseerror(sqvm, "GetMods: invalid weapon-info handle (0xFFFF)");

    // crashes can happen due to weird server-side keyvalues bullshit, not sure if a disconnect is needed though
    g_TaskQueue.Dispatch([]() { g_pEngineClient->Disconnect("#DISCONNECT_OUT_OF_SYNC"); });

    return SQRESULT_ERROR;
});

DECLARE_HOOK(C_WeaponX_Destructor, client.dll + 0x59B760, [](auto& hook, C_WeaponX* pWeapon) -> std::uintptr_t
{
    if (C_WeaponXExtended* const extended = pWeapon->GetWeaponExtended())
        extended->Forget();
    g_ClientWeaponMods.ClearFieldOverrides(&pWeapon->m_modVars);
    return hook.Original(pWeapon);
});

ON_DLL_LOAD_RELIESON("client.dll", ClientWeaponModHooks, WeaponMods_Client, [](CModule)
{
    DISPATCH_HOOK(ClientWeaponInterpolation, C_WeaponX_GetMods)
    DISPATCH_HOOK(ClientWeaponInterpolation, C_WeaponX_Destructor)
});

int C_WeaponX::InterpolateFieldsInternal(float currentTime, const SingleSnapshotValues* secondSnapshot, float secondSnapshotTime)
{
    const int status = C_BaseAnimating::InterpolateFieldsInternal(currentTime, secondSnapshot, secondSnapshotTime);
    if (status)
    {
        const float fraction = GetInterpolationFraction(currentTime, secondSnapshotTime, secondSnapshot);
        const auto& first = m_lerpData.currentSnap->weaponData->smartAmmoFractions;
        const auto& second = m_lerpData.futureSnap->weaponData->smartAmmoFractions;
        auto& fractions = m_smartAmmo.currentFrameSmartAmmoFractions;
        const auto lerp = [fraction](float a, float b) { return (b - a) * fraction + a; };
        for (int i = 0; i < 8; ++i)
            fractions[i] = lerp(first[i], second[i]);
    }
    return status;
}

DECLARE_HOOK(C_WeaponX_InterpolateFieldsInternal, client.dll + 0x5B02E0,
             [](auto&, C_WeaponX* self, float currentTime, const SingleSnapshotValues* secondSnapshot, float secondSnapshotTime) -> int
{ return self->C_WeaponX::InterpolateFieldsInternal(currentTime, secondSnapshot, secondSnapshotTime); })

static int Script_SetWeaponInfoFileKeyField(HSQUIRRELVM sqvm)
{
    constexpr ScriptContext context = ScriptContext::CLIENT;
    C_WeaponX* pWeapon = nullptr;
    if (!g_pSquirrel[context]->getthisentity(sqvm, &pWeapon))
        return SQRESULT_ERROR;

    const char* pFieldName = g_pSquirrel[context]->getstring(sqvm, 1);
    if (!pWeapon || !pFieldName || !*pFieldName)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "SetWeaponInfoFileKeyField requires a valid weapon and non-empty key");
        return SQRESULT_ERROR;
    }

    bool valueSupported = false;
    const bool updated =
        g_ClientWeaponMods.SetField(&pWeapon->GetWpnData(), &pWeapon->m_modVars, pFieldName, sqvm->_stackOfCurrentFunction[2], valueSupported);
    if (!valueSupported)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "SetWeaponInfoFileKeyField only accepts int, float, bool, or vector values");
        return SQRESULT_ERROR;
    }

    g_pSquirrel[context]->pushbool(sqvm, updated);
    return SQRESULT_NOTNULL;
}

void (*C_WeaponX__EmitWeaponNpcSound)(C_WeaponX*, float, float);
void (*C_WeaponX__EmitWeaponNpcSound_DontUpdateLastFiredTime)(C_WeaponX*, float, float);
void (*C_WeaponX__ShowWeapon)(C_WeaponX*);
void (*C_WeaponX__SetViewmodelAmmoModelIndex_Script)(C_WeaponX*, int);
int (*C_WeaponX__GetWeaponChargeLevel)(const C_WeaponX*);
float (*C_WeaponX__GetChargeFraction)(const C_WeaponX*);
bool (*C_WeaponX__DeployWeapon)(C_WeaponX*, bool);
bool (*C_WeaponX__Holster)(C_WeaponX*);
bool (*C_WeaponX__FastHolster)(C_WeaponX*);
bool (*C_WeaponX__Lower)(C_WeaponX*);
bool (*C_WeaponX__Reload)(C_WeaponX*);
bool (*C_WeaponX__PrimaryAttack)(C_WeaponX*);
bool (*C_WeaponX__ChargeEnd_Internal)(C_WeaponX*, bool, bool);
bool (*C_WeaponX__HolsterInternal)(C_WeaponX*, bool);
bool (*C_WeaponX__RaiseInternal)(C_WeaponX*, bool, bool);
bool (*C_WeaponX__SustainedDischargeBegin)(C_WeaponX*);

Vector3D* (*C_WeaponX__GetAttackPosition_Script)(C_WeaponX*, Vector3D*);
Vector3D* (*C_WeaponX__GetAttackDirection_Script)(C_WeaponX*, Vector3D*);
void (*C_WeaponX__AllowUse)(C_WeaponX*, bool);
void (*C_WeaponX__Script_EmitWeaponSound_1p3p)(C_WeaponX*, char const* const, char const* const);
void (*C_WeaponX__EmitWeaponSound_Script)(C_WeaponX*, char const*);
void (*C_WeaponX__StopWeaponSound_Script)(C_WeaponX*, char const* const);
bool (*C_WeaponX__IsNetOptimized)(const C_WeaponX*);
void (*C_WeaponX__StopWeaponEffect_Script)(C_WeaponX*, char const*, char const*);
void (*C_WeaponX__SetWeaponBurstFireCount_Script)(C_WeaponX*, int);
int (*C_WeaponX__GetWeaponBurstFireCount_Script)(C_WeaponX*);
void (*C_WeaponX__SetWeaponSkin)(C_WeaponX*, int);
void (*C_WeaponX__SetWeaponCamo)(C_WeaponX*, unsigned int);
void (*C_WeaponX__FireWeaponBullet_Script)(C_WeaponX*, Vector3D const&, Vector3D const&, int, int);
void (*C_WeaponX__FireWeaponBullet_Internal)(C_WeaponX*, const Vector3D&, const Vector3D&, int, int, bool, bool, bool, bool, bool, bool, bool);
C_BaseEntity* (*C_WeaponX__FireWeaponGrenade_Script)(C_WeaponX*, const Vector3D&, const Vector3D&, const Vector3D&, float, int, int, bool, bool, bool);
C_BaseEntity* (*C_WeaponX__FireWeaponMissile_Script)(C_WeaponX*, const Vector3D&, const Vector3D&, float, int, int, bool, bool);
int (*C_WeaponX__GetProjectilesPerShot_Script)(const C_WeaponX*);
int (*C_WeaponX__GetAmmoPerShot_Script)(const C_WeaponX*);
char const* (*C_WeaponX__GetAmmoDisplay_Script)(const C_WeaponX*);
int (*C_WeaponX__GetWeaponPrimaryClipCountMax_Script)(C_WeaponX*);
void (*C_WeaponX__SetWeaponPrimaryClipCount_Script)(C_WeaponX*, int);
void (*C_WeaponX__SetWeaponPrimaryClipCountAbsolute_Script)(C_WeaponX*, int);
void (*C_WeaponX__SetWeaponPrimaryClipCountNoRegenReset_Script)(C_WeaponX*, int);
bool (*C_WeaponX__IsWeaponRegenDraining_Script)(C_WeaponX*);
int (*C_WeaponX__GetWeaponPrimaryClipCount_Script)(C_WeaponX*);
void (*C_WeaponX__Script_RegenerateAmmoReset)(C_WeaponX*);
int (*C_WeaponX__Script_GetLifetimeShotsRemaining)(C_WeaponX*);
void (*C_WeaponX__Script_SetLifetimeShotsRemaining)(C_WeaponX*, int);
void (*C_WeaponX__Script_SetLifetimeShotsRemainingInfinite)(C_WeaponX*);
C_BaseEntity* (*C_WeaponX__GetWeaponUtilityEntity)(const C_WeaponX*);
bool (*C_WeaponX__IsWeaponAdsButtonPressed)(const C_WeaponX*);
int (*C_WeaponX__GetWeaponType)(const C_WeaponX*);
bool (*C_WeaponX__IsChargeWeapon)(const C_WeaponX*);
bool (*C_WeaponX__IsWeaponCharging_Script)(C_WeaponX*);
float (*C_WeaponX__GetWeaponChargeTime_Script)(C_WeaponX*);
float (*C_WeaponX__GetWeaponChargeTimeRemaining_Script)(C_WeaponX*);
float (*C_WeaponX__GetWeaponChargeFractionClamped)(const C_WeaponX*);
void (*C_WeaponX__Script_SetWeaponChargeFraction)(C_WeaponX*, float);
void (*C_WeaponX__Script_SetWeaponChargeFractionForced)(C_WeaponX*, float);
int (*C_WeaponX__GetWeaponChargeLevel_Script)(const C_WeaponX*);
int (*C_WeaponX__GetWeaponChargeLevelMax_Script)(const C_WeaponX*);
float (*C_WeaponX__GetChargeDuration)(const C_WeaponX*);
float (*C_WeaponX__GetWeaponReadyToFireProgress)(const C_WeaponX*);
int (*C_WeaponX__Script_GetWeaponChargeEnergyCost)(C_WeaponX*);
int (*C_WeaponX__Script_GetWeaponDefaultEnergyCost)(C_WeaponX*, int);
void (*C_WeaponX__Script_ResetWeaponToDefaultEnergyCost)(C_WeaponX*);
int (*C_WeaponX__Script_GetWeaponCurrentEnergyCost)(C_WeaponX*);
void (*C_WeaponX__Script_SetWeaponEnergyCost)(C_WeaponX*, int);
ScriptVariant_t* (*C_WeaponX__GetWeaponInfoFileKeyField)(C_WeaponX*, ScriptVariant_t*, char const*);
ScriptAsset* (*C_WeaponX__GetWeaponInfoFileKeyFieldAsset)(C_WeaponX*, ScriptAsset*, char const*);
bool (*C_WeaponX__IsWeaponDisabled_Script)(C_WeaponX*, C_BaseEntity*);
bool (*C_WeaponX__Script_DeployWeapon)(C_WeaponX*);
bool (*C_WeaponX__Script_DeployWeaponInstant)(C_WeaponX*);
bool (*C_WeaponX__Raise)(C_WeaponX*);
bool (*C_WeaponX__ShouldPredictProjectiles)(C_WeaponX*);
ScriptVariant_t* (*C_WeaponX__SmartAmmo_Script_GetStoredTargets)(C_WeaponX*, ScriptVariant_t*);
void (*C_WeaponX__SmartAmmo_Clear)(C_WeaponX*, bool, bool);
bool (*C_WeaponX__IsReadyToFire)(const C_WeaponX*);
bool (*C_WeaponX__IsBurstFireInProgress)(const C_WeaponX*);
int (*C_WeaponX__GetBurstFireShotsPending)(const C_WeaponX*);
float (*C_WeaponX__TimeUntilReadyToFire)(const C_WeaponX*);
char const* (*C_WeaponX__GetWeaponName)(const C_WeaponX*);
int (*C_WeaponX__GetDamageSourceID)(C_WeaponX*);
float (*C_WeaponX__GetMaxDamageFarDist)(C_WeaponX*);
void (*C_WeaponX__ForceRelease)(C_WeaponX*);
bool (*C_WeaponX__IsForceRelease)(const C_WeaponX*);
void (*C_WeaponX__AddMod_Script)(C_WeaponX*, char const* const);
void (*C_WeaponX__RemoveMod_Script)(C_WeaponX*, char const* const);
int (*C_WeaponX__GetMods_Script)(C_WeaponX*, SQVM*);
int (*C_WeaponX__SetMods_Script)(C_WeaponX*, SQVM*);
bool (*C_WeaponX__IsOffhandWeapon)(const C_WeaponX*);
float (*C_WeaponX__GetNextAttackAllowedTime_Script)(const C_WeaponX*);
float (*C_WeaponX__GetNextAttackAllowedTimeRaw_Script)(const C_WeaponX*);
void (*C_WeaponX__SetNextAttackAllowedTime_Script)(C_WeaponX*, float);
int (*C_WeaponX__GetRodeoDamage_Script)(const C_WeaponX*);
int (*C_WeaponX__GetShotCount_Script)(const C_WeaponX*);
char const* (*C_WeaponX__GetWeaponClass)(const C_WeaponX*);
bool (*C_WeaponX__IsInCooldown)(const C_WeaponX*);
bool (*C_WeaponX__IsCooldownPending)(const C_WeaponX*);
int (*C_WeaponX__GetWeaponDamageFlags)(const C_WeaponX*);
int (*C_WeaponX__GetWeaponExplosionDamageFlags)(const C_WeaponX*);
int (*C_WeaponX__Script_GetImpactTableIndex)(const C_WeaponX*);
bool (*C_WeaponX__Script_GetNPCMissFastPlayer)(const C_WeaponX*);
float (*C_WeaponX__Script_GetMeleeLungeTargetRange)(const C_WeaponX*);
float (*C_WeaponX__Script_GetMeleeLungeTargetAngle)(const C_WeaponX*);
bool (*C_WeaponX__Script_GetMeleeCanHitHumanSized)(const C_WeaponX*);
bool (*C_WeaponX__Script_GetMeleeCanHitTitans)(const C_WeaponX*);
int (*C_WeaponX__Script_GetDamageAmountForArmorType)(const C_WeaponX*, int);
float (*C_WeaponX__Script_GetMeleeAttackRange)(const C_WeaponX*);
float (*C_WeaponX__Script_GetMeleeAttackAngle)(const C_WeaponX*);
char const* (*C_WeaponX__Script_GetMeleeAnim3p)(const C_WeaponX*);
char const* (*C_WeaponX__Script_GetWeaponReadyMsg)(const C_WeaponX*);
char const* (*C_WeaponX__Script_GetWeaponReadyHint)(const C_WeaponX*);
float (*C_WeaponX__Script_GetGrenadeFuseTime)(const C_WeaponX*);
float (*C_WeaponX__Script_GetGrenadeIgnitionTime)(const C_WeaponX*);
bool (*C_WeaponX__Script_GetAllowHeadShots)(const C_WeaponX*);
int (*C_WeaponX__Script_GetCurrentAltFireIndex)(const C_WeaponX*);
float (*C_WeaponX__Script_GetWeaponZoomFOV)(C_WeaponX*);
int (*C_WeaponX__Script_GetReloadMilestoneIndex)(C_WeaponX*);
void (*C_WeaponX__Script_SetForcedADS)(C_WeaponX*);
void (*C_WeaponX__Script_ClearForcedADS)(C_WeaponX*);
int (*C_WeaponX__Script_GetForcedADS)(const C_WeaponX*);
int (*C_WeaponX__Script_GetInventoryIndex)(const C_WeaponX*);
int (*C_WeaponX__Script_GetChargeAnimIndex)(const C_WeaponX*);
void (*C_WeaponX__Script_SetChargeAnimIndex)(C_WeaponX*, int);
float (*C_WeaponX__GetWeaponDamageForce)(const C_WeaponX*);
float (*C_WeaponX__GetCoreDuration)(const C_WeaponX*);
bool (*C_WeaponX__IsSustainedDischargeWeapon)(const C_WeaponX*);
bool (*C_WeaponX__IsDischarging)(const C_WeaponX*);
float (*C_WeaponX__GetSustainedDischargeDuration)(const C_WeaponX*);
float (*C_WeaponX__GetSustainedDischargeRemainder)(const C_WeaponX*);
float (*C_WeaponX__GetSustainedDischargeFraction)(const C_WeaponX*);
float (*C_WeaponX__GetSustainedDischargePulseFrequency)(const C_WeaponX*);
void (*C_WeaponX__SetSustainedDischargeFractionForced)(C_WeaponX*, float);
bool (*C_WeaponX__IsSustainedLaserWeapon)(const C_WeaponX*);
void (*C_WeaponX__Script_DoMeleeHitConfirmation)(C_WeaponX*, float);
void (*C_WeaponX__Script_SetScriptTime0)(C_WeaponX*, float);
float (*C_WeaponX__Script_GetScriptTime0)(C_WeaponX*);
void (*C_WeaponX__Script_SetScriptFlags0)(C_WeaponX*, int);
int (*C_WeaponX__Script_GetScriptFlags0)(C_WeaponX*);
bool (*C_WeaponX__IsLoadoutPickup)(C_WeaponX*);
void (*C_WeaponX__HideWeapon)(C_WeaponX*);
C_BaseEntity* (*C_WeaponX__GetWeaponOwner_Script)(const C_WeaponX*);
bool (*C_WeaponX__HasSilencer)(const C_WeaponX*);
int (*C_WeaponX__PlayWeaponEffectAndReturnViewEffectHandle_Script)(C_WeaponX*, const char*, const char*, const char*);
void (*C_WeaponX__PlayWeaponEffectNoCull_Script)(C_WeaponX*, const char*, const char*, const char*);
void (*C_WeaponX__PlayWeaponEffect_Script)(C_WeaponX*, const char*, const char*, const char*);
C_BaseEntity* (*C_WeaponX__FireWeaponBolt_Script)(C_WeaponX*, const Vector3D&, const Vector3D&, float, int, int, bool, int);
int (*C_WeaponX__GetWeaponPrimaryAmmoCount_Script)(const C_WeaponX*);
void (*C_WeaponX__SetWeaponPrimaryAmmoCount_Script)(C_WeaponX*, int);
bool (*C_WeaponX__IsWeaponInAds_Script)(C_WeaponX*);
bool (*C_WeaponX__SmartAmmo_IsEnabled_Script)(C_WeaponX*);
void (*C_WeaponX__SmartAmmo_SetNewTargetTime)(C_WeaponX*);
float (*C_WeaponX__SmartAmmo_GetNewTargetTime)(C_WeaponX*);
float (*C_WeaponX__SmartAmmo_GetSearchAngle)(C_WeaponX*);
ScriptVariant_t* (*C_WeaponX__SmartAmmo_Script_GetTargets)(C_WeaponX*, ScriptVariant_t*);
void (*C_WeaponX__SmartAmmo_Script_SetTarget)(C_WeaponX*, C_BaseEntity*, float);
void (*C_WeaponX__SmartAmmo_Script_StoreTargets)(C_WeaponX*);
Vector3D* (*C_WeaponX__SmartAmmo_Script_GetFirePosition)(C_WeaponX*, Vector3D*, C_BaseEntity*, int);
void (*C_WeaponX__SmartAmmo_Script_TrackEntity)(C_WeaponX*, C_BaseEntity*, float);
void (*C_WeaponX__SmartAmmo_Script_UntrackEntity)(C_WeaponX*, C_BaseEntity*);
int (*C_WeaponX__SmartAmmo_Script_GetNumTrackersOnEntity)(C_WeaponX*, C_BaseEntity*);
bool (*C_WeaponX__SmartAmmo_IsVisibleTarget)(C_WeaponX*, C_BaseEntity*);
ScriptVariant_t* (*C_WeaponX__SmartAmmo_Script_GetTrackedEntities)(C_WeaponX*, ScriptVariant_t*);
bool (*C_WeaponX__HasMod_Script)(C_WeaponX*, const char*);
void (*C_WeaponX__SetModBitfield_Script)(C_WeaponX*, int);
int (*C_WeaponX__GetModBitfield_Script)(C_WeaponX*);
const char* (*C_WeaponX__GetSmartAmmoHudLockStyle_Script)(C_WeaponX*);
const char* (*C_WeaponX__GetSmartAmmoWeaponType_Script)(C_WeaponX*);
void (*C_WeaponX__SetAttackKickScale_Script)(C_WeaponX*, float);
void (*C_WeaponX__SetAttackKickRollScale_Script)(C_WeaponX*, float);
int (*C_WeaponX__Script_GetWeaponSettingInt)(C_WeaponX*, SQVM*);
int (*C_WeaponX__Script_GetWeaponSettingFloat)(C_WeaponX*, SQVM*);
int (*C_WeaponX__Script_GetWeaponSettingBool)(C_WeaponX*, SQVM*);
int (*C_WeaponX__Script_GetWeaponSettingVector)(C_WeaponX*, SQVM*);
int (*C_WeaponX__Script_GetWeaponSettingString)(C_WeaponX*, SQVM*);
int (*C_WeaponX__Script_GetWeaponSettingAsset)(C_WeaponX*, SQVM*);

Vector3D C_WeaponX::GetAttackPosition_Script()
{
    Vector3D result;
    C_WeaponX__GetAttackPosition_Script(this, &result);
    return result;
}

Vector3D C_WeaponX::GetAttackDirection_Script()
{
    Vector3D result;
    C_WeaponX__GetAttackDirection_Script(this, &result);
    return result;
}

void C_WeaponX::AllowUse(bool arg1)
{
    C_WeaponX__AllowUse(this, arg1);
}

void C_WeaponX::Script_EmitWeaponSound_1p3p(char const* const arg1, char const* const arg2)
{
    C_WeaponX__Script_EmitWeaponSound_1p3p(this, arg1, arg2);
}

void C_WeaponX::EmitWeaponSound_Script(char const* arg1)
{
    C_WeaponX__EmitWeaponSound_Script(this, arg1);
}

void C_WeaponX::StopWeaponSound_Script(char const* const arg1)
{
    C_WeaponX__StopWeaponSound_Script(this, arg1);
}

bool C_WeaponX::IsNetOptimized() const
{
    return C_WeaponX__IsNetOptimized(this);
}

void C_WeaponX::StopWeaponEffect_Script(char const* arg1, char const* arg2)
{
    C_WeaponX__StopWeaponEffect_Script(this, arg1, arg2);
}

void C_WeaponX::SetWeaponBurstFireCount_Script(int arg1)
{
    C_WeaponX__SetWeaponBurstFireCount_Script(this, arg1);
}

int C_WeaponX::GetWeaponBurstFireCount_Script()
{
    return C_WeaponX__GetWeaponBurstFireCount_Script(this);
}

void C_WeaponX::SetWeaponSkin(int arg1)
{
    C_WeaponX__SetWeaponSkin(this, arg1);
}

void C_WeaponX::SetWeaponCamo(unsigned int arg1)
{
    C_WeaponX__SetWeaponCamo(this, arg1);
}

void C_WeaponX::FireWeaponBullet_Script(Vector3D const& arg1, Vector3D const& arg2, int arg3, int arg4)
{
    C_WeaponX__FireWeaponBullet_Script(this, arg1, arg2, arg3, arg4);
}

void C_WeaponX::FireWeaponBullet_Internal(const Vector3D& arg1, const Vector3D& arg2, int arg3, int arg4, bool arg5, bool arg6, bool arg7, bool arg8,
                                          bool arg9, bool arg10, bool arg11)
{
    C_WeaponX__FireWeaponBullet_Internal(this, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
}

C_BaseEntity* C_WeaponX::FireWeaponGrenade_Script(const Vector3D& arg1, const Vector3D& arg2, const Vector3D& arg3, float arg4, int arg5, int arg6,
                                                  bool arg7, bool arg8, bool arg9)
{
    return C_WeaponX__FireWeaponGrenade_Script(this, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}

C_BaseEntity* C_WeaponX::FireWeaponMissile_Script(const Vector3D& arg1, const Vector3D& arg2, float arg3, int arg4, int arg5, bool arg6, bool arg7)
{
    return C_WeaponX__FireWeaponMissile_Script(this, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

int C_WeaponX::GetProjectilesPerShot_Script() const
{
    return C_WeaponX__GetProjectilesPerShot_Script(this);
}

int C_WeaponX::GetAmmoPerShot_Script() const
{
    return C_WeaponX__GetAmmoPerShot_Script(this);
}

char const* C_WeaponX::GetAmmoDisplay_Script() const
{
    return C_WeaponX__GetAmmoDisplay_Script(this);
}

int C_WeaponX::GetWeaponPrimaryClipCountMax_Script()
{
    return C_WeaponX__GetWeaponPrimaryClipCountMax_Script(this);
}

void C_WeaponX::SetWeaponPrimaryClipCount_Script(int arg1)
{
    C_WeaponX__SetWeaponPrimaryClipCount_Script(this, arg1);
}

void C_WeaponX::SetWeaponPrimaryClipCountAbsolute_Script(int arg1)
{
    C_WeaponX__SetWeaponPrimaryClipCountAbsolute_Script(this, arg1);
}

void C_WeaponX::SetWeaponPrimaryClipCountNoRegenReset_Script(int arg1)
{
    C_WeaponX__SetWeaponPrimaryClipCountNoRegenReset_Script(this, arg1);
}

bool C_WeaponX::IsWeaponRegenDraining_Script()
{
    return C_WeaponX__IsWeaponRegenDraining_Script(this);
}

int C_WeaponX::GetWeaponPrimaryClipCount_Script()
{
    return C_WeaponX__GetWeaponPrimaryClipCount_Script(this);
}

void C_WeaponX::Script_RegenerateAmmoReset()
{
    C_WeaponX__Script_RegenerateAmmoReset(this);
}

int C_WeaponX::Script_GetLifetimeShotsRemaining()
{
    return C_WeaponX__Script_GetLifetimeShotsRemaining(this);
}

void C_WeaponX::Script_SetLifetimeShotsRemaining(int arg1)
{
    C_WeaponX__Script_SetLifetimeShotsRemaining(this, arg1);
}

void C_WeaponX::Script_SetLifetimeShotsRemainingInfinite()
{
    C_WeaponX__Script_SetLifetimeShotsRemainingInfinite(this);
}

C_BaseEntity* C_WeaponX::GetWeaponUtilityEntity() const
{
    return C_WeaponX__GetWeaponUtilityEntity(this);
}

bool C_WeaponX::IsWeaponAdsButtonPressed() const
{
    return C_WeaponX__IsWeaponAdsButtonPressed(this);
}

int C_WeaponX::GetWeaponType() const
{
    return C_WeaponX__GetWeaponType(this);
}

bool C_WeaponX::IsChargeWeapon() const
{
    return C_WeaponX__IsChargeWeapon(this);
}

bool C_WeaponX::IsWeaponCharging_Script()
{
    return C_WeaponX__IsWeaponCharging_Script(this);
}

float C_WeaponX::GetWeaponChargeTime_Script()
{
    return C_WeaponX__GetWeaponChargeTime_Script(this);
}

float C_WeaponX::GetWeaponChargeTimeRemaining_Script()
{
    return C_WeaponX__GetWeaponChargeTimeRemaining_Script(this);
}

float C_WeaponX::GetWeaponChargeFractionClamped() const
{
    return C_WeaponX__GetWeaponChargeFractionClamped(this);
}

void C_WeaponX::Script_SetWeaponChargeFraction(float arg1)
{
    C_WeaponX__Script_SetWeaponChargeFraction(this, arg1);
}

void C_WeaponX::Script_SetWeaponChargeFractionForced(float arg1)
{
    C_WeaponX__Script_SetWeaponChargeFractionForced(this, arg1);
}

int C_WeaponX::GetWeaponChargeLevel_Script() const
{
    return C_WeaponX__GetWeaponChargeLevel_Script(this);
}

int C_WeaponX::GetWeaponChargeLevelMax_Script() const
{
    return C_WeaponX__GetWeaponChargeLevelMax_Script(this);
}

float C_WeaponX::GetChargeDuration() const
{
    return C_WeaponX__GetChargeDuration(this);
}

float C_WeaponX::GetWeaponReadyToFireProgress() const
{
    return C_WeaponX__GetWeaponReadyToFireProgress(this);
}

int C_WeaponX::Script_GetWeaponChargeEnergyCost()
{
    return C_WeaponX__Script_GetWeaponChargeEnergyCost(this);
}

int C_WeaponX::Script_GetWeaponDefaultEnergyCost(int arg1)
{
    return C_WeaponX__Script_GetWeaponDefaultEnergyCost(this, arg1);
}

void C_WeaponX::Script_ResetWeaponToDefaultEnergyCost()
{
    C_WeaponX__Script_ResetWeaponToDefaultEnergyCost(this);
}

int C_WeaponX::Script_GetWeaponCurrentEnergyCost()
{
    return C_WeaponX__Script_GetWeaponCurrentEnergyCost(this);
}

void C_WeaponX::Script_SetWeaponEnergyCost(int arg1)
{
    C_WeaponX__Script_SetWeaponEnergyCost(this, arg1);
}

ScriptVariant_t C_WeaponX::GetWeaponInfoFileKeyField(char const* arg1)
{
    ScriptVariant_t result;
    C_WeaponX__GetWeaponInfoFileKeyField(this, &result, arg1);
    return result;
}

ScriptAsset C_WeaponX::GetWeaponInfoFileKeyFieldAsset(char const* arg1)
{
    ScriptAsset result;
    C_WeaponX__GetWeaponInfoFileKeyFieldAsset(this, &result, arg1);
    return result;
}

bool C_WeaponX::IsWeaponDisabled_Script(C_BaseEntity* arg1)
{
    return C_WeaponX__IsWeaponDisabled_Script(this, arg1);
}

bool C_WeaponX::Script_DeployWeapon()
{
    return C_WeaponX__Script_DeployWeapon(this);
}

bool C_WeaponX::Script_DeployWeaponInstant()
{
    return C_WeaponX__Script_DeployWeaponInstant(this);
}

bool C_WeaponX::Raise()
{
    return C_WeaponX__Raise(this);
}

bool C_WeaponX::ShouldPredictProjectiles()
{
    return C_WeaponX__ShouldPredictProjectiles(this);
}

ScriptVariant_t C_WeaponX::SmartAmmo_Script_GetStoredTargets()
{
    ScriptVariant_t result;
    C_WeaponX__SmartAmmo_Script_GetStoredTargets(this, &result);
    return result;
}

void C_WeaponX::SmartAmmo_Clear(bool arg1, bool arg2)
{
    C_WeaponX__SmartAmmo_Clear(this, arg1, arg2);
}

bool C_WeaponX::IsReadyToFire() const
{
    return C_WeaponX__IsReadyToFire(this);
}

bool C_WeaponX::IsBurstFireInProgress() const
{
    return C_WeaponX__IsBurstFireInProgress(this);
}

int C_WeaponX::GetBurstFireShotsPending() const
{
    return C_WeaponX__GetBurstFireShotsPending(this);
}

float C_WeaponX::TimeUntilReadyToFire() const
{
    return C_WeaponX__TimeUntilReadyToFire(this);
}

char const* C_WeaponX::GetWeaponName() const
{
    return C_WeaponX__GetWeaponName(this);
}

int C_WeaponX::GetDamageSourceID()
{
    return C_WeaponX__GetDamageSourceID(this);
}

float C_WeaponX::GetMaxDamageFarDist()
{
    return C_WeaponX__GetMaxDamageFarDist(this);
}

void C_WeaponX::ForceRelease()
{
    C_WeaponX__ForceRelease(this);
}

bool C_WeaponX::IsForceRelease() const
{
    return C_WeaponX__IsForceRelease(this);
}

void C_WeaponX::AddMod_Script(char const* const arg1)
{
    C_WeaponX__AddMod_Script(this, arg1);
}

void C_WeaponX::RemoveMod_Script(char const* const arg1)
{
    C_WeaponX__RemoveMod_Script(this, arg1);
}

int C_WeaponX::GetMods_Script(SQVM* arg1)
{
    return C_WeaponX__GetMods_Script(this, arg1);
}

int C_WeaponX::SetMods_Script(SQVM* arg1)
{
    return C_WeaponX__SetMods_Script(this, arg1);
}

bool C_WeaponX::IsOffhandWeapon() const
{
    return C_WeaponX__IsOffhandWeapon(this);
}

float C_WeaponX::GetNextAttackAllowedTime_Script() const
{
    return C_WeaponX__GetNextAttackAllowedTime_Script(this);
}

float C_WeaponX::GetNextAttackAllowedTimeRaw_Script() const
{
    return C_WeaponX__GetNextAttackAllowedTimeRaw_Script(this);
}

void C_WeaponX::SetNextAttackAllowedTime_Script(float arg1)
{
    C_WeaponX__SetNextAttackAllowedTime_Script(this, arg1);
}

int C_WeaponX::GetRodeoDamage_Script() const
{
    return C_WeaponX__GetRodeoDamage_Script(this);
}

int C_WeaponX::GetShotCount_Script() const
{
    return C_WeaponX__GetShotCount_Script(this);
}

char const* C_WeaponX::GetWeaponClass() const
{
    return C_WeaponX__GetWeaponClass(this);
}

bool C_WeaponX::IsInCooldown() const
{
    return C_WeaponX__IsInCooldown(this);
}

bool C_WeaponX::IsCooldownPending() const
{
    return C_WeaponX__IsCooldownPending(this);
}

int C_WeaponX::GetWeaponDamageFlags() const
{
    return C_WeaponX__GetWeaponDamageFlags(this);
}

int C_WeaponX::GetWeaponExplosionDamageFlags() const
{
    return C_WeaponX__GetWeaponExplosionDamageFlags(this);
}

int C_WeaponX::Script_GetImpactTableIndex() const
{
    return C_WeaponX__Script_GetImpactTableIndex(this);
}

bool C_WeaponX::Script_GetNPCMissFastPlayer() const
{
    return C_WeaponX__Script_GetNPCMissFastPlayer(this);
}

float C_WeaponX::Script_GetMeleeLungeTargetRange() const
{
    return C_WeaponX__Script_GetMeleeLungeTargetRange(this);
}

float C_WeaponX::Script_GetMeleeLungeTargetAngle() const
{
    return C_WeaponX__Script_GetMeleeLungeTargetAngle(this);
}

bool C_WeaponX::Script_GetMeleeCanHitHumanSized() const
{
    return C_WeaponX__Script_GetMeleeCanHitHumanSized(this);
}

bool C_WeaponX::Script_GetMeleeCanHitTitans() const
{
    return C_WeaponX__Script_GetMeleeCanHitTitans(this);
}

int C_WeaponX::Script_GetDamageAmountForArmorType(int arg1) const
{
    return C_WeaponX__Script_GetDamageAmountForArmorType(this, arg1);
}

float C_WeaponX::Script_GetMeleeAttackRange() const
{
    return C_WeaponX__Script_GetMeleeAttackRange(this);
}

float C_WeaponX::Script_GetMeleeAttackAngle() const
{
    return C_WeaponX__Script_GetMeleeAttackAngle(this);
}

char const* C_WeaponX::Script_GetMeleeAnim3p() const
{
    return C_WeaponX__Script_GetMeleeAnim3p(this);
}

char const* C_WeaponX::Script_GetWeaponReadyMsg() const
{
    return C_WeaponX__Script_GetWeaponReadyMsg(this);
}

char const* C_WeaponX::Script_GetWeaponReadyHint() const
{
    return C_WeaponX__Script_GetWeaponReadyHint(this);
}

float C_WeaponX::Script_GetGrenadeFuseTime() const
{
    return C_WeaponX__Script_GetGrenadeFuseTime(this);
}

float C_WeaponX::Script_GetGrenadeIgnitionTime() const
{
    return C_WeaponX__Script_GetGrenadeIgnitionTime(this);
}

bool C_WeaponX::Script_GetAllowHeadShots() const
{
    return C_WeaponX__Script_GetAllowHeadShots(this);
}

int C_WeaponX::Script_GetCurrentAltFireIndex() const
{
    return C_WeaponX__Script_GetCurrentAltFireIndex(this);
}

float C_WeaponX::Script_GetWeaponZoomFOV()
{
    return C_WeaponX__Script_GetWeaponZoomFOV(this);
}

int C_WeaponX::Script_GetReloadMilestoneIndex()
{
    return C_WeaponX__Script_GetReloadMilestoneIndex(this);
}

void C_WeaponX::Script_SetForcedADS()
{
    C_WeaponX__Script_SetForcedADS(this);
}

void C_WeaponX::Script_ClearForcedADS()
{
    C_WeaponX__Script_ClearForcedADS(this);
}

int C_WeaponX::Script_GetForcedADS() const
{
    return C_WeaponX__Script_GetForcedADS(this);
}

int C_WeaponX::Script_GetInventoryIndex() const
{
    return C_WeaponX__Script_GetInventoryIndex(this);
}

int C_WeaponX::Script_GetChargeAnimIndex() const
{
    return C_WeaponX__Script_GetChargeAnimIndex(this);
}

void C_WeaponX::Script_SetChargeAnimIndex(int arg1)
{
    C_WeaponX__Script_SetChargeAnimIndex(this, arg1);
}

float C_WeaponX::GetWeaponDamageForce() const
{
    return C_WeaponX__GetWeaponDamageForce(this);
}

float C_WeaponX::GetCoreDuration() const
{
    return C_WeaponX__GetCoreDuration(this);
}

bool C_WeaponX::IsSustainedDischargeWeapon() const
{
    return C_WeaponX__IsSustainedDischargeWeapon(this);
}

bool C_WeaponX::IsDischarging() const
{
    return C_WeaponX__IsDischarging(this);
}

float C_WeaponX::GetSustainedDischargeDuration() const
{
    return C_WeaponX__GetSustainedDischargeDuration(this);
}

float C_WeaponX::GetSustainedDischargeRemainder() const
{
    return C_WeaponX__GetSustainedDischargeRemainder(this);
}

float C_WeaponX::GetSustainedDischargeFraction() const
{
    return C_WeaponX__GetSustainedDischargeFraction(this);
}

float C_WeaponX::GetSustainedDischargePulseFrequency() const
{
    return C_WeaponX__GetSustainedDischargePulseFrequency(this);
}

void C_WeaponX::SetSustainedDischargeFractionForced(float arg1)
{
    C_WeaponX__SetSustainedDischargeFractionForced(this, arg1);
}

bool C_WeaponX::IsSustainedLaserWeapon() const
{
    return C_WeaponX__IsSustainedLaserWeapon(this);
}

void C_WeaponX::Script_DoMeleeHitConfirmation(float arg1)
{
    C_WeaponX__Script_DoMeleeHitConfirmation(this, arg1);
}

void C_WeaponX::Script_SetScriptTime0(float arg1)
{
    C_WeaponX__Script_SetScriptTime0(this, arg1);
}

float C_WeaponX::Script_GetScriptTime0()
{
    return C_WeaponX__Script_GetScriptTime0(this);
}

void C_WeaponX::Script_SetScriptFlags0(int arg1)
{
    C_WeaponX__Script_SetScriptFlags0(this, arg1);
}

int C_WeaponX::Script_GetScriptFlags0()
{
    return C_WeaponX__Script_GetScriptFlags0(this);
}

bool C_WeaponX::IsLoadoutPickup()
{
    return C_WeaponX__IsLoadoutPickup(this);
}

void C_WeaponX::HideWeapon()
{
    C_WeaponX__HideWeapon(this);
}

C_BaseEntity* C_WeaponX::GetWeaponOwner_Script() const
{
    return C_WeaponX__GetWeaponOwner_Script(this);
}

bool C_WeaponX::HasSilencer() const
{
    return C_WeaponX__HasSilencer(this);
}

int C_WeaponX::PlayWeaponEffectAndReturnViewEffectHandle_Script(const char* arg1, const char* arg2, const char* arg3)
{
    return C_WeaponX__PlayWeaponEffectAndReturnViewEffectHandle_Script(this, arg1, arg2, arg3);
}

void C_WeaponX::PlayWeaponEffectNoCull_Script(const char* arg1, const char* arg2, const char* arg3)
{
    C_WeaponX__PlayWeaponEffectNoCull_Script(this, arg1, arg2, arg3);
}

void C_WeaponX::PlayWeaponEffect_Script(const char* arg1, const char* arg2, const char* arg3)
{
    C_WeaponX__PlayWeaponEffect_Script(this, arg1, arg2, arg3);
}

C_BaseEntity* C_WeaponX::FireWeaponBolt_Script(const Vector3D& arg1, const Vector3D& arg2, float arg3, int arg4, int arg5, bool arg6, int arg7)
{
    return C_WeaponX__FireWeaponBolt_Script(this, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

int C_WeaponX::GetWeaponPrimaryAmmoCount_Script() const
{
    return C_WeaponX__GetWeaponPrimaryAmmoCount_Script(this);
}

void C_WeaponX::SetWeaponPrimaryAmmoCount_Script(int arg1)
{
    C_WeaponX__SetWeaponPrimaryAmmoCount_Script(this, arg1);
}

bool C_WeaponX::IsWeaponInAds_Script()
{
    return C_WeaponX__IsWeaponInAds_Script(this);
}

bool C_WeaponX::SmartAmmo_IsEnabled_Script()
{
    return C_WeaponX__SmartAmmo_IsEnabled_Script(this);
}

void C_WeaponX::SmartAmmo_SetNewTargetTime()
{
    C_WeaponX__SmartAmmo_SetNewTargetTime(this);
}

float C_WeaponX::SmartAmmo_GetNewTargetTime()
{
    return C_WeaponX__SmartAmmo_GetNewTargetTime(this);
}

float C_WeaponX::SmartAmmo_GetSearchAngle()
{
    return C_WeaponX__SmartAmmo_GetSearchAngle(this);
}

ScriptVariant_t C_WeaponX::SmartAmmo_Script_GetTargets()
{
    ScriptVariant_t result;
    C_WeaponX__SmartAmmo_Script_GetTargets(this, &result);
    return result;
}

void C_WeaponX::SmartAmmo_Script_SetTarget(C_BaseEntity* arg1, float arg2)
{
    C_WeaponX__SmartAmmo_Script_SetTarget(this, arg1, arg2);
}

void C_WeaponX::SmartAmmo_Script_StoreTargets()
{
    C_WeaponX__SmartAmmo_Script_StoreTargets(this);
}

Vector3D C_WeaponX::SmartAmmo_Script_GetFirePosition(C_BaseEntity* arg1, int arg2)
{
    Vector3D result;
    C_WeaponX__SmartAmmo_Script_GetFirePosition(this, &result, arg1, arg2);
    return result;
}

void C_WeaponX::SmartAmmo_Script_TrackEntity(C_BaseEntity* arg1, float arg2)
{
    C_WeaponX__SmartAmmo_Script_TrackEntity(this, arg1, arg2);
}

void C_WeaponX::SmartAmmo_Script_UntrackEntity(C_BaseEntity* arg1)
{
    C_WeaponX__SmartAmmo_Script_UntrackEntity(this, arg1);
}

int C_WeaponX::SmartAmmo_Script_GetNumTrackersOnEntity(C_BaseEntity* arg1)
{
    return C_WeaponX__SmartAmmo_Script_GetNumTrackersOnEntity(this, arg1);
}

bool C_WeaponX::SmartAmmo_IsVisibleTarget(C_BaseEntity* arg1)
{
    return C_WeaponX__SmartAmmo_IsVisibleTarget(this, arg1);
}

ScriptVariant_t C_WeaponX::SmartAmmo_Script_GetTrackedEntities()
{
    ScriptVariant_t result;
    C_WeaponX__SmartAmmo_Script_GetTrackedEntities(this, &result);
    return result;
}

bool C_WeaponX::HasMod_Script(const char* arg1)
{
    return C_WeaponX__HasMod_Script(this, arg1);
}

void C_WeaponX::SetModBitfield_Script(int arg1)
{
    C_WeaponX__SetModBitfield_Script(this, arg1);
}

int C_WeaponX::GetModBitfield_Script()
{
    return C_WeaponX__GetModBitfield_Script(this);
}

const char* C_WeaponX::GetSmartAmmoHudLockStyle_Script()
{
    return C_WeaponX__GetSmartAmmoHudLockStyle_Script(this);
}

const char* C_WeaponX::GetSmartAmmoWeaponType_Script()
{
    return C_WeaponX__GetSmartAmmoWeaponType_Script(this);
}

void C_WeaponX::SetAttackKickScale_Script(float arg1)
{
    C_WeaponX__SetAttackKickScale_Script(this, arg1);
}

void C_WeaponX::SetAttackKickRollScale_Script(float arg1)
{
    C_WeaponX__SetAttackKickRollScale_Script(this, arg1);
}

int C_WeaponX::Script_GetWeaponSettingInt(SQVM* arg1)
{
    return C_WeaponX__Script_GetWeaponSettingInt(this, arg1);
}

int C_WeaponX::Script_GetWeaponSettingFloat(SQVM* arg1)
{
    return C_WeaponX__Script_GetWeaponSettingFloat(this, arg1);
}

int C_WeaponX::Script_GetWeaponSettingBool(SQVM* arg1)
{
    return C_WeaponX__Script_GetWeaponSettingBool(this, arg1);
}

int C_WeaponX::Script_GetWeaponSettingVector(SQVM* arg1)
{
    return C_WeaponX__Script_GetWeaponSettingVector(this, arg1);
}

int C_WeaponX::Script_GetWeaponSettingString(SQVM* arg1)
{
    return C_WeaponX__Script_GetWeaponSettingString(this, arg1);
}

int C_WeaponX::Script_GetWeaponSettingAsset(SQVM* arg1)
{
    return C_WeaponX__Script_GetWeaponSettingAsset(this, arg1);
}

datamap_t* WeaponPlayerData_Client__m_PredMap;
datamap_t* SmartAmmo_WeaponData_Client__m_PredMap;

datamap_t* WeaponPlayerData_Client::GetPredDescMap()
{
    return WeaponPlayerData_Client__m_PredMap;
}
const datamap_t* WeaponPlayerData_Client::GetPredDescMapConst() const
{
    return WeaponPlayerData_Client__m_PredMap;
}
datamap_t* SmartAmmo_WeaponData_Client::GetPredDescMap()
{
    return SmartAmmo_WeaponData_Client__m_PredMap;
}
const datamap_t* SmartAmmo_WeaponData_Client::GetPredDescMapConst() const
{
    return SmartAmmo_WeaponData_Client__m_PredMap;
}

void C_WeaponX::EmitWeaponNpcSound(float soundRadius, float soundDuration)
{
    return C_WeaponX__EmitWeaponNpcSound(this, soundRadius, soundDuration);
}

void C_WeaponX::EmitWeaponNpcSound_DontUpdateLastFiredTime(float soundRadius, float soundDuration)
{
    return C_WeaponX__EmitWeaponNpcSound_DontUpdateLastFiredTime(this, soundRadius, soundDuration);
}

void C_WeaponX::ShowWeapon()
{
    return C_WeaponX__ShowWeapon(this);
}

void C_WeaponX::SetViewmodelAmmoModelIndex_Script(int modelIndex)
{
    return C_WeaponX__SetViewmodelAmmoModelIndex_Script(this, modelIndex);
}

int C_WeaponX::GetWeaponChargeLevel() const
{
    return C_WeaponX__GetWeaponChargeLevel(this);
}

float C_WeaponX::GetChargeFraction() const
{
    return C_WeaponX__GetChargeFraction(this);
}

bool C_WeaponX::DeployWeapon(bool forceDeploy)
{
    return C_WeaponX__DeployWeapon(this, forceDeploy);
}

bool C_WeaponX::Holster()
{
    return C_WeaponX__Holster(this);
}

bool C_WeaponX::FastHolster()
{
    return C_WeaponX__FastHolster(this);
}

bool C_WeaponX::Lower()
{
    return C_WeaponX__Lower(this);
}

bool C_WeaponX::Reload()
{
    return C_WeaponX__Reload(this);
}

bool C_WeaponX::PrimaryAttack()
{
    return C_WeaponX__PrimaryAttack(this);
}

bool C_WeaponX::ChargeEnd_Internal(bool allowAttack, bool updateState)
{
    return C_WeaponX__ChargeEnd_Internal(this, allowAttack, updateState);
}

bool C_WeaponX::HolsterInternal(bool fastHolster)
{
    return C_WeaponX__HolsterInternal(this, fastHolster);
}

bool C_WeaponX::RaiseInternal(bool fromSprint, bool arg2)
{
    return C_WeaponX__RaiseInternal(this, fromSprint, arg2);
}

bool C_WeaponX::SustainedDischargeBegin()
{
    return C_WeaponX__SustainedDischargeBegin(this);
}

ON_DLL_LOAD_CLIENT("client.dll", WeaponSdkMethods, [](CModule module)
{
    DISPATCH_HOOK(ClientWeaponInterpolation, C_WeaponX_InterpolateFieldsInternal)
    auto* pScriptDesc = module.Offset(0x2E34E50).RCast<ScriptClassDesc_t*>();
    auto& binding = pScriptDesc->m_NativeFunctionBindings[pScriptDesc->m_NativeFunctionBindings.AddToTail()];
    binding.Init("SetWeaponInfoFileKeyField", "Script_SetWeaponInfoFileKeyField",
                 "Sets a scalar or vector field on this client WeaponX instance at runtime.", "bool", "string key, var value", false,
                 &Script_SetWeaponInfoFileKeyField);
    RegisterWeaponPredictionScriptMethods(ScriptContext::CLIENT, pScriptDesc);

    C_WeaponX__EmitWeaponNpcSound = module.Offset(0x5A5300).RCast<decltype(C_WeaponX__EmitWeaponNpcSound)>();
    C_WeaponX__EmitWeaponNpcSound_DontUpdateLastFiredTime = module.Offset(0x5A5310).RCast<decltype(C_WeaponX__EmitWeaponNpcSound_DontUpdateLastFiredTime)>();
    C_WeaponX__ShowWeapon = module.Offset(0x5B8AB0).RCast<decltype(C_WeaponX__ShowWeapon)>();
    C_WeaponX__SetViewmodelAmmoModelIndex_Script = module.Offset(0x5B82A0).RCast<decltype(C_WeaponX__SetViewmodelAmmoModelIndex_Script)>();
    C_WeaponX__GetWeaponChargeLevel = module.Offset(0x5A9540).RCast<decltype(C_WeaponX__GetWeaponChargeLevel)>();
    C_WeaponX__GetChargeFraction = module.Offset(0x5A6D60).RCast<decltype(C_WeaponX__GetChargeFraction)>();
    C_WeaponX__DeployWeapon = module.Offset(0x5A42D0).RCast<decltype(C_WeaponX__DeployWeapon)>();
    C_WeaponX__Holster = module.Offset(0x5AB400).RCast<decltype(C_WeaponX__Holster)>();
    C_WeaponX__FastHolster = module.Offset(0x5A5A50).RCast<decltype(C_WeaponX__FastHolster)>();
    C_WeaponX__Lower = module.Offset(0x5B0FE0).RCast<decltype(C_WeaponX__Lower)>();
    C_WeaponX__Reload = module.Offset(0x5B6470).RCast<decltype(C_WeaponX__Reload)>();
    C_WeaponX__PrimaryAttack = module.Offset(0x5B48C0).RCast<decltype(C_WeaponX__PrimaryAttack)>();
    C_WeaponX__ChargeEnd_Internal = module.Offset(0x5A24F0).RCast<decltype(C_WeaponX__ChargeEnd_Internal)>();
    C_WeaponX__HolsterInternal = module.Offset(0x5AB410).RCast<decltype(C_WeaponX__HolsterInternal)>();
    C_WeaponX__RaiseInternal = module.Offset(0x5B5990).RCast<decltype(C_WeaponX__RaiseInternal)>();
    C_WeaponX__SustainedDischargeBegin = module.Offset(0x5B9C00).RCast<decltype(C_WeaponX__SustainedDischargeBegin)>();
    C_WeaponX__GetAttackPosition_Script = module.Offset(0x5A6C60).RCast<decltype(C_WeaponX__GetAttackPosition_Script)>();
    C_WeaponX__GetAttackDirection_Script = module.Offset(0x5A6BB0).RCast<decltype(C_WeaponX__GetAttackDirection_Script)>();
    C_WeaponX__AllowUse = module.Offset(0x59CA20).RCast<decltype(C_WeaponX__AllowUse)>();
    C_WeaponX__Script_EmitWeaponSound_1p3p = module.Offset(0x5B73C0).RCast<decltype(C_WeaponX__Script_EmitWeaponSound_1p3p)>();
    C_WeaponX__EmitWeaponSound_Script = module.Offset(0x5A54A0).RCast<decltype(C_WeaponX__EmitWeaponSound_Script)>();
    C_WeaponX__StopWeaponSound_Script = module.Offset(0x5B9B70).RCast<decltype(C_WeaponX__StopWeaponSound_Script)>();
    C_WeaponX__IsNetOptimized = module.Offset(0xBC210).RCast<decltype(C_WeaponX__IsNetOptimized)>();
    C_WeaponX__StopWeaponEffect_Script = module.Offset(0x5B9A00).RCast<decltype(C_WeaponX__StopWeaponEffect_Script)>();
    C_WeaponX__SetWeaponBurstFireCount_Script = module.Offset(0x5B8320).RCast<decltype(C_WeaponX__SetWeaponBurstFireCount_Script)>();
    C_WeaponX__GetWeaponBurstFireCount_Script = module.Offset(0x5A94B0).RCast<decltype(C_WeaponX__GetWeaponBurstFireCount_Script)>();
    C_WeaponX__SetWeaponSkin = module.Offset(0x5B8640).RCast<decltype(C_WeaponX__SetWeaponSkin)>();
    C_WeaponX__SetWeaponCamo = module.Offset(0x5B83B0).RCast<decltype(C_WeaponX__SetWeaponCamo)>();
    C_WeaponX__FireWeaponBullet_Script = module.Offset(0x5A6470).RCast<decltype(C_WeaponX__FireWeaponBullet_Script)>();
    C_WeaponX__FireWeaponBullet_Internal = module.Offset(0x5A6080).RCast<decltype(C_WeaponX__FireWeaponBullet_Internal)>();
    C_WeaponX__FireWeaponGrenade_Script = module.Offset(0x5A67E0).RCast<decltype(C_WeaponX__FireWeaponGrenade_Script)>();
    C_WeaponX__FireWeaponMissile_Script = module.Offset(0x5A6850).RCast<decltype(C_WeaponX__FireWeaponMissile_Script)>();
    C_WeaponX__GetProjectilesPerShot_Script = module.Offset(0x5A8980).RCast<decltype(C_WeaponX__GetProjectilesPerShot_Script)>();
    C_WeaponX__GetAmmoPerShot_Script = module.Offset(0x5A6BA0).RCast<decltype(C_WeaponX__GetAmmoPerShot_Script)>();
    C_WeaponX__GetAmmoDisplay_Script = module.Offset(0x5A6B70).RCast<decltype(C_WeaponX__GetAmmoDisplay_Script)>();
    C_WeaponX__GetWeaponPrimaryClipCountMax_Script = module.Offset(0x5A9D20).RCast<decltype(C_WeaponX__GetWeaponPrimaryClipCountMax_Script)>();
    C_WeaponX__SetWeaponPrimaryClipCount_Script = module.Offset(0x5B84D0).RCast<decltype(C_WeaponX__SetWeaponPrimaryClipCount_Script)>();
    C_WeaponX__SetWeaponPrimaryClipCountAbsolute_Script = module.Offset(0x5B84B0).RCast<decltype(C_WeaponX__SetWeaponPrimaryClipCountAbsolute_Script)>();
    C_WeaponX__SetWeaponPrimaryClipCountNoRegenReset_Script = module.Offset(0x5B84C0).RCast<decltype(C_WeaponX__SetWeaponPrimaryClipCountNoRegenReset_Script)>();
    C_WeaponX__IsWeaponRegenDraining_Script = module.Offset(0x5B0AA0).RCast<decltype(C_WeaponX__IsWeaponRegenDraining_Script)>();
    C_WeaponX__GetWeaponPrimaryClipCount_Script = module.Offset(0x5A9D30).RCast<decltype(C_WeaponX__GetWeaponPrimaryClipCount_Script)>();
    C_WeaponX__Script_RegenerateAmmoReset = module.Offset(0x5B76F0).RCast<decltype(C_WeaponX__Script_RegenerateAmmoReset)>();
    C_WeaponX__Script_GetLifetimeShotsRemaining = module.Offset(0x5B7510).RCast<decltype(C_WeaponX__Script_GetLifetimeShotsRemaining)>();
    C_WeaponX__Script_SetLifetimeShotsRemaining = module.Offset(0x5B7750).RCast<decltype(C_WeaponX__Script_SetLifetimeShotsRemaining)>();
    C_WeaponX__Script_SetLifetimeShotsRemainingInfinite = module.Offset(0x5B7770).RCast<decltype(C_WeaponX__Script_SetLifetimeShotsRemainingInfinite)>();
    C_WeaponX__GetWeaponUtilityEntity = module.Offset(0x5AA8D0).RCast<decltype(C_WeaponX__GetWeaponUtilityEntity)>();
    C_WeaponX__IsWeaponAdsButtonPressed = module.Offset(0x5B0920).RCast<decltype(C_WeaponX__IsWeaponAdsButtonPressed)>();
    C_WeaponX__GetWeaponType = module.Offset(0x5AA8B0).RCast<decltype(C_WeaponX__GetWeaponType)>();
    C_WeaponX__IsChargeWeapon = module.Offset(0x5B0590).RCast<decltype(C_WeaponX__IsChargeWeapon)>();
    C_WeaponX__IsWeaponCharging_Script = module.Offset(0x5B0980).RCast<decltype(C_WeaponX__IsWeaponCharging_Script)>();
    C_WeaponX__GetWeaponChargeTime_Script = module.Offset(0x5A96F0).RCast<decltype(C_WeaponX__GetWeaponChargeTime_Script)>();
    C_WeaponX__GetWeaponChargeTimeRemaining_Script = module.Offset(0x5A9630).RCast<decltype(C_WeaponX__GetWeaponChargeTimeRemaining_Script)>();
    C_WeaponX__GetWeaponChargeFractionClamped = module.Offset(0x5A94F0).RCast<decltype(C_WeaponX__GetWeaponChargeFractionClamped)>();
    C_WeaponX__Script_SetWeaponChargeFraction = module.Offset(0x5B77A0).RCast<decltype(C_WeaponX__Script_SetWeaponChargeFraction)>();
    C_WeaponX__Script_SetWeaponChargeFractionForced = module.Offset(0x5B7890).RCast<decltype(C_WeaponX__Script_SetWeaponChargeFractionForced)>();
    C_WeaponX__GetWeaponChargeLevel_Script = module.Offset(0x5A95F0).RCast<decltype(C_WeaponX__GetWeaponChargeLevel_Script)>();
    C_WeaponX__GetWeaponChargeLevelMax_Script = module.Offset(0x5A95E0).RCast<decltype(C_WeaponX__GetWeaponChargeLevelMax_Script)>();
    C_WeaponX__GetChargeDuration = module.Offset(0x5A6D50).RCast<decltype(C_WeaponX__GetChargeDuration)>();
    C_WeaponX__GetWeaponReadyToFireProgress = module.Offset(0x5A9EF0).RCast<decltype(C_WeaponX__GetWeaponReadyToFireProgress)>();
    C_WeaponX__Script_GetWeaponChargeEnergyCost = module.Offset(0x5B75D0).RCast<decltype(C_WeaponX__Script_GetWeaponChargeEnergyCost)>();
    C_WeaponX__Script_GetWeaponDefaultEnergyCost = module.Offset(0x5B75F0).RCast<decltype(C_WeaponX__Script_GetWeaponDefaultEnergyCost)>();
    C_WeaponX__Script_ResetWeaponToDefaultEnergyCost = module.Offset(0x5B7710).RCast<decltype(C_WeaponX__Script_ResetWeaponToDefaultEnergyCost)>();
    C_WeaponX__Script_GetWeaponCurrentEnergyCost = module.Offset(0x5B75E0).RCast<decltype(C_WeaponX__Script_GetWeaponCurrentEnergyCost)>();
    C_WeaponX__Script_SetWeaponEnergyCost = module.Offset(0x5B7910).RCast<decltype(C_WeaponX__Script_SetWeaponEnergyCost)>();
    C_WeaponX__GetWeaponInfoFileKeyField = module.Offset(0x5A9880).RCast<decltype(C_WeaponX__GetWeaponInfoFileKeyField)>();
    C_WeaponX__GetWeaponInfoFileKeyFieldAsset = module.Offset(0x5A98C0).RCast<decltype(C_WeaponX__GetWeaponInfoFileKeyFieldAsset)>();
    C_WeaponX__IsWeaponDisabled_Script = module.Offset(0x5B09C0).RCast<decltype(C_WeaponX__IsWeaponDisabled_Script)>();
    C_WeaponX__Script_DeployWeapon = module.Offset(0x5B7010).RCast<decltype(C_WeaponX__Script_DeployWeapon)>();
    C_WeaponX__Script_DeployWeaponInstant = module.Offset(0x5B7280).RCast<decltype(C_WeaponX__Script_DeployWeaponInstant)>();
    C_WeaponX__Raise = module.Offset(0x5B5960).RCast<decltype(C_WeaponX__Raise)>();
    C_WeaponX__ShouldPredictProjectiles = module.Offset(0x5B8890).RCast<decltype(C_WeaponX__ShouldPredictProjectiles)>();
    C_WeaponX__SmartAmmo_Script_GetStoredTargets = module.Offset(0x5B8F40).RCast<decltype(C_WeaponX__SmartAmmo_Script_GetStoredTargets)>();
    C_WeaponX__SmartAmmo_Clear = module.Offset(0x5B8B20).RCast<decltype(C_WeaponX__SmartAmmo_Clear)>();
    C_WeaponX__IsReadyToFire = module.Offset(0x4DADD0).RCast<decltype(C_WeaponX__IsReadyToFire)>();
    C_WeaponX__IsBurstFireInProgress = module.Offset(0x5B0540).RCast<decltype(C_WeaponX__IsBurstFireInProgress)>();
    C_WeaponX__GetBurstFireShotsPending = module.Offset(0x5A6D10).RCast<decltype(C_WeaponX__GetBurstFireShotsPending)>();
    C_WeaponX__TimeUntilReadyToFire = module.Offset(0x5BBCF0).RCast<decltype(C_WeaponX__TimeUntilReadyToFire)>();
    C_WeaponX__GetWeaponName = module.Offset(0x5A9940).RCast<decltype(C_WeaponX__GetWeaponName)>();
    C_WeaponX__GetDamageSourceID = module.Offset(0x5A72B0).RCast<decltype(C_WeaponX__GetDamageSourceID)>();
    C_WeaponX__GetMaxDamageFarDist = module.Offset(0x5A7E20).RCast<decltype(C_WeaponX__GetMaxDamageFarDist)>();
    C_WeaponX__ForceRelease = module.Offset(0x5A6A00).RCast<decltype(C_WeaponX__ForceRelease)>();
    C_WeaponX__IsForceRelease = module.Offset(0x5B0600).RCast<decltype(C_WeaponX__IsForceRelease)>();
    C_WeaponX__AddMod_Script = module.Offset(0x59C200).RCast<decltype(C_WeaponX__AddMod_Script)>();
    C_WeaponX__RemoveMod_Script = module.Offset(0x5B68D0).RCast<decltype(C_WeaponX__RemoveMod_Script)>();
    C_WeaponX__GetMods_Script = module.Offset(0x5A7FA0).RCast<decltype(C_WeaponX__GetMods_Script)>();
    C_WeaponX__SetMods_Script = module.Offset(0x5B7C30).RCast<decltype(C_WeaponX__SetMods_Script)>();
    C_WeaponX__IsOffhandWeapon = module.Offset(0xB3600).RCast<decltype(C_WeaponX__IsOffhandWeapon)>();
    C_WeaponX__GetNextAttackAllowedTime_Script = module.Offset(0x5A8150).RCast<decltype(C_WeaponX__GetNextAttackAllowedTime_Script)>();
    C_WeaponX__GetNextAttackAllowedTimeRaw_Script = module.Offset(0x5A8140).RCast<decltype(C_WeaponX__GetNextAttackAllowedTimeRaw_Script)>();
    C_WeaponX__SetNextAttackAllowedTime_Script = module.Offset(0x5B7DA0).RCast<decltype(C_WeaponX__SetNextAttackAllowedTime_Script)>();
    C_WeaponX__GetRodeoDamage_Script = module.Offset(0x5A8C50).RCast<decltype(C_WeaponX__GetRodeoDamage_Script)>();
    C_WeaponX__GetShotCount_Script = module.Offset(0x5A8EF0).RCast<decltype(C_WeaponX__GetShotCount_Script)>();
    C_WeaponX__GetWeaponClass = module.Offset(0x5A9740).RCast<decltype(C_WeaponX__GetWeaponClass)>();
    C_WeaponX__IsInCooldown = module.Offset(0x5B0610).RCast<decltype(C_WeaponX__IsInCooldown)>();
    C_WeaponX__IsCooldownPending = module.Offset(0x5B05B0).RCast<decltype(C_WeaponX__IsCooldownPending)>();
    C_WeaponX__GetWeaponDamageFlags = module.Offset(0x5A97A0).RCast<decltype(C_WeaponX__GetWeaponDamageFlags)>();
    C_WeaponX__GetWeaponExplosionDamageFlags = module.Offset(0x5A9800).RCast<decltype(C_WeaponX__GetWeaponExplosionDamageFlags)>();
    C_WeaponX__Script_GetImpactTableIndex = module.Offset(0x5B7490).RCast<decltype(C_WeaponX__Script_GetImpactTableIndex)>();
    C_WeaponX__Script_GetNPCMissFastPlayer = module.Offset(0x5B7590).RCast<decltype(C_WeaponX__Script_GetNPCMissFastPlayer)>();
    C_WeaponX__Script_GetMeleeLungeTargetRange = module.Offset(0x5B7580).RCast<decltype(C_WeaponX__Script_GetMeleeLungeTargetRange)>();
    C_WeaponX__Script_GetMeleeLungeTargetAngle = module.Offset(0x5B7570).RCast<decltype(C_WeaponX__Script_GetMeleeLungeTargetAngle)>();
    C_WeaponX__Script_GetMeleeCanHitHumanSized = module.Offset(0x5B7550).RCast<decltype(C_WeaponX__Script_GetMeleeCanHitHumanSized)>();
    C_WeaponX__Script_GetMeleeCanHitTitans = module.Offset(0x5B7560).RCast<decltype(C_WeaponX__Script_GetMeleeCanHitTitans)>();
    C_WeaponX__Script_GetDamageAmountForArmorType = module.Offset(0x5B7420).RCast<decltype(C_WeaponX__Script_GetDamageAmountForArmorType)>();
    C_WeaponX__Script_GetMeleeAttackRange = module.Offset(0x5B7540).RCast<decltype(C_WeaponX__Script_GetMeleeAttackRange)>();
    C_WeaponX__Script_GetMeleeAttackAngle = module.Offset(0x5B7530).RCast<decltype(C_WeaponX__Script_GetMeleeAttackAngle)>();
    C_WeaponX__Script_GetMeleeAnim3p = module.Offset(0x5B7520).RCast<decltype(C_WeaponX__Script_GetMeleeAnim3p)>();
    C_WeaponX__Script_GetWeaponReadyMsg = module.Offset(0x5B7610).RCast<decltype(C_WeaponX__Script_GetWeaponReadyMsg)>();
    C_WeaponX__Script_GetWeaponReadyHint = module.Offset(0x5B7600).RCast<decltype(C_WeaponX__Script_GetWeaponReadyHint)>();
    C_WeaponX__Script_GetGrenadeFuseTime = module.Offset(0x5B7470).RCast<decltype(C_WeaponX__Script_GetGrenadeFuseTime)>();
    C_WeaponX__Script_GetGrenadeIgnitionTime = module.Offset(0x5B7480).RCast<decltype(C_WeaponX__Script_GetGrenadeIgnitionTime)>();
    C_WeaponX__Script_GetAllowHeadShots = module.Offset(0x5B73F0).RCast<decltype(C_WeaponX__Script_GetAllowHeadShots)>();
    C_WeaponX__Script_GetCurrentAltFireIndex = module.Offset(0x5B7410).RCast<decltype(C_WeaponX__Script_GetCurrentAltFireIndex)>();
    C_WeaponX__Script_GetWeaponZoomFOV = module.Offset(0x5B76E0).RCast<decltype(C_WeaponX__Script_GetWeaponZoomFOV)>();
    C_WeaponX__Script_GetReloadMilestoneIndex = module.Offset(0x5B75A0).RCast<decltype(C_WeaponX__Script_GetReloadMilestoneIndex)>();
    C_WeaponX__Script_SetForcedADS = module.Offset(0x5B7740).RCast<decltype(C_WeaponX__Script_SetForcedADS)>();
    C_WeaponX__Script_ClearForcedADS = module.Offset(0x5B6FF0).RCast<decltype(C_WeaponX__Script_ClearForcedADS)>();
    C_WeaponX__Script_GetForcedADS = module.Offset(0x5B7460).RCast<decltype(C_WeaponX__Script_GetForcedADS)>();
    C_WeaponX__Script_GetInventoryIndex = module.Offset(0x5B74A0).RCast<decltype(C_WeaponX__Script_GetInventoryIndex)>();
    C_WeaponX__Script_GetChargeAnimIndex = module.Offset(0x5B7400).RCast<decltype(C_WeaponX__Script_GetChargeAnimIndex)>();
    C_WeaponX__Script_SetChargeAnimIndex = module.Offset(0x5B7720).RCast<decltype(C_WeaponX__Script_SetChargeAnimIndex)>();
    C_WeaponX__GetWeaponDamageForce = module.Offset(0x5A97B0).RCast<decltype(C_WeaponX__GetWeaponDamageForce)>();
    C_WeaponX__GetCoreDuration = module.Offset(0x5A71A0).RCast<decltype(C_WeaponX__GetCoreDuration)>();
    C_WeaponX__IsSustainedDischargeWeapon = module.Offset(0x5B08E0).RCast<decltype(C_WeaponX__IsSustainedDischargeWeapon)>();
    C_WeaponX__IsDischarging = module.Offset(0x5B05C0).RCast<decltype(C_WeaponX__IsDischarging)>();
    C_WeaponX__GetSustainedDischargeDuration = module.Offset(0x5A91F0).RCast<decltype(C_WeaponX__GetSustainedDischargeDuration)>();
    C_WeaponX__GetSustainedDischargeRemainder = module.Offset(0x5A9260).RCast<decltype(C_WeaponX__GetSustainedDischargeRemainder)>();
    C_WeaponX__GetSustainedDischargeFraction = module.Offset(0x5A9200).RCast<decltype(C_WeaponX__GetSustainedDischargeFraction)>();
    C_WeaponX__GetSustainedDischargePulseFrequency = module.Offset(0x5A9250).RCast<decltype(C_WeaponX__GetSustainedDischargePulseFrequency)>();
    C_WeaponX__SetSustainedDischargeFractionForced = module.Offset(0x5B7F60).RCast<decltype(C_WeaponX__SetSustainedDischargeFractionForced)>();
    C_WeaponX__IsSustainedLaserWeapon = module.Offset(0x5B0900).RCast<decltype(C_WeaponX__IsSustainedLaserWeapon)>();
    C_WeaponX__Script_DoMeleeHitConfirmation = module.Offset(0x5B7350).RCast<decltype(C_WeaponX__Script_DoMeleeHitConfirmation)>();
    C_WeaponX__Script_SetScriptTime0 = module.Offset(0x5B7790).RCast<decltype(C_WeaponX__Script_SetScriptTime0)>();
    C_WeaponX__Script_GetScriptTime0 = module.Offset(0x5B75C0).RCast<decltype(C_WeaponX__Script_GetScriptTime0)>();
    C_WeaponX__Script_SetScriptFlags0 = module.Offset(0x5B7780).RCast<decltype(C_WeaponX__Script_SetScriptFlags0)>();
    C_WeaponX__Script_GetScriptFlags0 = module.Offset(0x5B75B0).RCast<decltype(C_WeaponX__Script_GetScriptFlags0)>();
    C_WeaponX__IsLoadoutPickup = module.Offset(0x5B0630).RCast<decltype(C_WeaponX__IsLoadoutPickup)>();
    C_WeaponX__HideWeapon = module.Offset(0x5AB390).RCast<decltype(C_WeaponX__HideWeapon)>();
    C_WeaponX__GetWeaponOwner_Script = module.Offset(0x5A9C70).RCast<decltype(C_WeaponX__GetWeaponOwner_Script)>();
    C_WeaponX__HasSilencer = module.Offset(0x5AB350).RCast<decltype(C_WeaponX__HasSilencer)>();
    C_WeaponX__PlayWeaponEffectAndReturnViewEffectHandle_Script =
        module.Offset(0x5B31F0).RCast<decltype(C_WeaponX__PlayWeaponEffectAndReturnViewEffectHandle_Script)>();
    C_WeaponX__PlayWeaponEffectNoCull_Script = module.Offset(0x5B32A0).RCast<decltype(C_WeaponX__PlayWeaponEffectNoCull_Script)>();
    C_WeaponX__PlayWeaponEffect_Script = module.Offset(0x5B32C0).RCast<decltype(C_WeaponX__PlayWeaponEffect_Script)>();
    C_WeaponX__FireWeaponBolt_Script = module.Offset(0x5A5D10).RCast<decltype(C_WeaponX__FireWeaponBolt_Script)>();
    C_WeaponX__GetWeaponPrimaryAmmoCount_Script = module.Offset(0x5A9CB0).RCast<decltype(C_WeaponX__GetWeaponPrimaryAmmoCount_Script)>();
    C_WeaponX__SetWeaponPrimaryAmmoCount_Script = module.Offset(0x5B8420).RCast<decltype(C_WeaponX__SetWeaponPrimaryAmmoCount_Script)>();
    C_WeaponX__IsWeaponInAds_Script = module.Offset(0x5B0A30).RCast<decltype(C_WeaponX__IsWeaponInAds_Script)>();
    C_WeaponX__SmartAmmo_IsEnabled_Script = module.Offset(0x5B8D90).RCast<decltype(C_WeaponX__SmartAmmo_IsEnabled_Script)>();
    C_WeaponX__SmartAmmo_SetNewTargetTime = module.Offset(0x5B93A0).RCast<decltype(C_WeaponX__SmartAmmo_SetNewTargetTime)>();
    C_WeaponX__SmartAmmo_GetNewTargetTime = module.Offset(0x5B8D30).RCast<decltype(C_WeaponX__SmartAmmo_GetNewTargetTime)>();
    C_WeaponX__SmartAmmo_GetSearchAngle = module.Offset(0x5B8D80).RCast<decltype(C_WeaponX__SmartAmmo_GetSearchAngle)>();
    C_WeaponX__SmartAmmo_Script_GetTargets = module.Offset(0x5B90B0).RCast<decltype(C_WeaponX__SmartAmmo_Script_GetTargets)>();
    C_WeaponX__SmartAmmo_Script_SetTarget = module.Offset(0x5B9210).RCast<decltype(C_WeaponX__SmartAmmo_Script_SetTarget)>();
    C_WeaponX__SmartAmmo_Script_StoreTargets = module.Offset(0x5B9230).RCast<decltype(C_WeaponX__SmartAmmo_Script_StoreTargets)>();
    C_WeaponX__SmartAmmo_Script_GetFirePosition = module.Offset(0x5B8E90).RCast<decltype(C_WeaponX__SmartAmmo_Script_GetFirePosition)>();
    C_WeaponX__SmartAmmo_Script_TrackEntity = module.Offset(0x5B9360).RCast<decltype(C_WeaponX__SmartAmmo_Script_TrackEntity)>();
    C_WeaponX__SmartAmmo_Script_UntrackEntity = module.Offset(0x5B9380).RCast<decltype(C_WeaponX__SmartAmmo_Script_UntrackEntity)>();
    C_WeaponX__SmartAmmo_Script_GetNumTrackersOnEntity = module.Offset(0x5B8F10).RCast<decltype(C_WeaponX__SmartAmmo_Script_GetNumTrackersOnEntity)>();
    C_WeaponX__SmartAmmo_IsVisibleTarget = module.Offset(0x5B8DA0).RCast<decltype(C_WeaponX__SmartAmmo_IsVisibleTarget)>();
    C_WeaponX__SmartAmmo_Script_GetTrackedEntities = module.Offset(0x5B9110).RCast<decltype(C_WeaponX__SmartAmmo_Script_GetTrackedEntities)>();
    C_WeaponX__HasMod_Script = module.Offset(0x5AB2E0).RCast<decltype(C_WeaponX__HasMod_Script)>();
    C_WeaponX__SetModBitfield_Script = module.Offset(0x5B7B80).RCast<decltype(C_WeaponX__SetModBitfield_Script)>();
    C_WeaponX__GetModBitfield_Script = module.Offset(0x5A7EC0).RCast<decltype(C_WeaponX__GetModBitfield_Script)>();
    C_WeaponX__GetSmartAmmoHudLockStyle_Script = module.Offset(0x5A8F00).RCast<decltype(C_WeaponX__GetSmartAmmoHudLockStyle_Script)>();
    C_WeaponX__GetSmartAmmoWeaponType_Script = module.Offset(0x5A8F10).RCast<decltype(C_WeaponX__GetSmartAmmoWeaponType_Script)>();
    C_WeaponX__SetAttackKickScale_Script = module.Offset(0x5B7AC0).RCast<decltype(C_WeaponX__SetAttackKickScale_Script)>();
    C_WeaponX__SetAttackKickRollScale_Script = module.Offset(0x5B7AB0).RCast<decltype(C_WeaponX__SetAttackKickRollScale_Script)>();
    C_WeaponX__Script_GetWeaponSettingInt = module.Offset(0x5B7680).RCast<decltype(C_WeaponX__Script_GetWeaponSettingInt)>();
    C_WeaponX__Script_GetWeaponSettingFloat = module.Offset(0x5B7660).RCast<decltype(C_WeaponX__Script_GetWeaponSettingFloat)>();
    C_WeaponX__Script_GetWeaponSettingBool = module.Offset(0x5B7640).RCast<decltype(C_WeaponX__Script_GetWeaponSettingBool)>();
    C_WeaponX__Script_GetWeaponSettingVector = module.Offset(0x5B76C0).RCast<decltype(C_WeaponX__Script_GetWeaponSettingVector)>();
    C_WeaponX__Script_GetWeaponSettingString = module.Offset(0x5B76A0).RCast<decltype(C_WeaponX__Script_GetWeaponSettingString)>();
    C_WeaponX__Script_GetWeaponSettingAsset = module.Offset(0x5B7620).RCast<decltype(C_WeaponX__Script_GetWeaponSettingAsset)>();
    WeaponPlayerData_Client__m_PredMap = module.Offset(0xB4A660).RCast<datamap_t*>();
    SmartAmmo_WeaponData_Client__m_PredMap = module.Offset(0xB492A0).RCast<datamap_t*>();
})

enum class ChargeActivityModifier : int
{
    Uncharged,
    Level1,
    Level2,
    Level3,
    Level4,
    FullyCharged,
};

static constexpr int ACTIVITY_MODIFIER_CAPACITY = 32;
static constexpr std::uint16_t INVALID_ACTIVITY_MODIFIER = 0xFFFF;
static constexpr int PARTIAL_CHARGE_LEVEL_COUNT = 4;
static constexpr std::array<const char*, 6> CHARGE_MODIFIER_NAMES = {
    "", "charged_lv1", "charged_lv2", "charged_lv3", "charged_lv4", "fully_charged",
};

using InternActivityModifier_t = std::uint16_t* (*)(std::uint16_t* output, const char* name);
static InternActivityModifier_t v_InternActivityModifier;
static std::once_flag g_ChargeModifierInit;
static std::array<std::uint16_t, 6> g_ChargeModifierSymbols = {
    INVALID_ACTIVITY_MODIFIER, INVALID_ACTIVITY_MODIFIER, INVALID_ACTIVITY_MODIFIER,
    INVALID_ACTIVITY_MODIFIER, INVALID_ACTIVITY_MODIFIER, INVALID_ACTIVITY_MODIFIER,
};
static thread_local C_WeaponX* g_ChargedAttackWeapon;
static thread_local ChargeActivityModifier g_ChargedAttackModifier;

static void InitializeChargeModifierSymbols()
{
    std::call_once(g_ChargeModifierInit, [] {
        for (int i = 1; i < static_cast<int>(CHARGE_MODIFIER_NAMES.size()); ++i)
            v_InternActivityModifier(&g_ChargeModifierSymbols[i], CHARGE_MODIFIER_NAMES[i]);
    });
}

static ChargeActivityModifier GetAttackChargeActivity(C_WeaponX* weapon)
{
    const float chargeFraction = std::clamp(weapon->GetChargeFraction(), 0.0f, 1.0f);
    const int nativeLevel = weapon->GetWeaponChargeLevel();
    if (chargeFraction <= 0.0f)
        return ChargeActivityModifier::Uncharged;
    if (chargeFraction >= 1.0f)
        return ChargeActivityModifier::FullyCharged;

    return static_cast<ChargeActivityModifier>(std::clamp(nativeLevel + 1, 1, PARTIAL_CHARGE_LEVEL_COUNT));
}

DECLARE_MODULE(WeaponChargeActivityModifierHooks)

DECLARE_HOOK(C_WeaponX_PrimaryAttack_CaptureChargeLevel, client.dll + 0x5B48C0, [](auto& hook, C_WeaponX* weapon) -> char
{
    C_WeaponX* const previousWeapon = g_ChargedAttackWeapon;
    const ChargeActivityModifier previousModifier = g_ChargedAttackModifier;
    g_ChargedAttackWeapon = weapon;
    g_ChargedAttackModifier = GetAttackChargeActivity(weapon);

    const char result = hook.Original(weapon);

    g_ChargedAttackWeapon = previousWeapon;
    g_ChargedAttackModifier = previousModifier;
    return result;
})

DECLARE_HOOK(WeaponChargeActivityModifiers, client.dll + 0xBAE00,
             [](auto& hook, C_BaseCombatWeapon* weapon, std::uint16_t* modifiers) -> int
{
    int modifierCount = hook.Original(weapon, modifiers);
    if (weapon != g_ChargedAttackWeapon || g_ChargedAttackModifier == ChargeActivityModifier::Uncharged ||
        modifierCount >= ACTIVITY_MODIFIER_CAPACITY)
        return modifierCount;

    InitializeChargeModifierSymbols();
    const std::uint16_t modifier = g_ChargeModifierSymbols[static_cast<int>(g_ChargedAttackModifier)];
    if (modifier != INVALID_ACTIVITY_MODIFIER)
        modifiers[modifierCount++] = modifier;
    return modifierCount;
})

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", WeaponChargeActivityModifierSetup, WeaponSdkMethods, [](CModule module)
{
    v_InternActivityModifier = module.Offset(0x32FBF0).RCast<InternActivityModifier_t>();
    DISPATCH_MODULE(WeaponChargeActivityModifierHooks)
})
