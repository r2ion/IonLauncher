#pragma once

#include "client/basecombatweapon.h"
#include "engine/deferredtrace.h"
#include "smartammo.h"
#include "tier1/utlobjectreference.h"
#include "weapon_parse.h"
#include "weapon_playerdata.h"
#include "weapon_x_shared.h"

#include <cstddef>
#include <cstdint>

class CParticleEffect;
struct ParticleHandleListItem;
struct SQVM;

enum ChargeEffectState_e : int
{
    CHARGE_OFF = 0,
    CHARGE_INCREASING = 1,
    CHARGE_DECREASING = 2,
    CHARGE_FULL = 3,
};

class C_WeaponX : public C_BaseCombatWeapon
{
  public:
    Vector3D GetAttackPosition_Script();
    Vector3D GetAttackDirection_Script();
    void AllowUse(bool arg1);
    void Script_EmitWeaponSound_1p3p(char const * const arg1, char const * const arg2);
    void EmitWeaponSound_Script(char const * arg1);
    void StopWeaponSound_Script(char const * const arg1);
    bool IsNetOptimized() const;
    void StopWeaponEffect_Script(char const * arg1, char const * arg2);
    void SetWeaponBurstFireCount_Script(int arg1);
    int GetWeaponBurstFireCount_Script();
    void SetWeaponSkin(int arg1);
    void SetWeaponCamo(unsigned int arg1);
    void FireWeaponBullet_Script(Vector3D const & arg1, Vector3D const & arg2, int arg3, int arg4);
    void FireWeaponBullet_Internal(const Vector3D& arg1, const Vector3D& arg2, int arg3, int arg4, bool arg5, bool arg6, bool arg7, bool arg8, bool arg9, bool arg10, bool arg11);
    C_BaseEntity* FireWeaponGrenade_Script(const Vector3D& arg1, const Vector3D& arg2, const Vector3D& arg3, float arg4, int arg5, int arg6, bool arg7, bool arg8, bool arg9);
    C_BaseEntity* FireWeaponMissile_Script(const Vector3D& arg1, const Vector3D& arg2, float arg3, int arg4, int arg5, bool arg6, bool arg7);
    int GetProjectilesPerShot_Script() const;
    int GetAmmoPerShot_Script() const;
    char const * GetAmmoDisplay_Script() const;
    int GetWeaponPrimaryClipCountMax_Script();
    void SetWeaponPrimaryClipCount_Script(int arg1);
    void SetWeaponPrimaryClipCountAbsolute_Script(int arg1);
    void SetWeaponPrimaryClipCountNoRegenReset_Script(int arg1);
    bool IsWeaponRegenDraining_Script();
    int GetWeaponPrimaryClipCount_Script();
    void Script_RegenerateAmmoReset();
    int Script_GetLifetimeShotsRemaining();
    void Script_SetLifetimeShotsRemaining(int arg1);
    void Script_SetLifetimeShotsRemainingInfinite();
    C_BaseEntity * GetWeaponUtilityEntity() const;
    bool IsWeaponAdsButtonPressed() const;
    int GetWeaponType() const;
    bool IsChargeWeapon() const;
    bool IsWeaponCharging_Script();
    float GetWeaponChargeTime_Script();
    float GetWeaponChargeTimeRemaining_Script();
    float GetWeaponChargeFractionClamped() const;
    void Script_SetWeaponChargeFraction(float arg1);
    void Script_SetWeaponChargeFractionForced(float arg1);
    int GetWeaponChargeLevel_Script() const;
    int GetWeaponChargeLevelMax_Script() const;
    float GetChargeDuration() const;
    float GetWeaponReadyToFireProgress() const;
    int Script_GetWeaponChargeEnergyCost();
    int Script_GetWeaponDefaultEnergyCost(int arg1);
    void Script_ResetWeaponToDefaultEnergyCost();
    int Script_GetWeaponCurrentEnergyCost();
    void Script_SetWeaponEnergyCost(int arg1);
    ScriptVariant_t GetWeaponInfoFileKeyField(char const * arg1);
    ScriptAsset GetWeaponInfoFileKeyFieldAsset(char const * arg1);
    bool IsWeaponDisabled_Script(C_BaseEntity * arg1);
    bool Script_DeployWeapon();
    bool Script_DeployWeaponInstant();
    bool Raise();
    bool ShouldPredictProjectiles();
    ScriptVariant_t SmartAmmo_Script_GetStoredTargets();
    void SmartAmmo_Clear(bool arg1, bool arg2);
    bool IsReadyToFire() const;
    bool IsBurstFireInProgress() const;
    int GetBurstFireShotsPending() const;
    float TimeUntilReadyToFire() const;
    char const * GetWeaponName() const;
    int GetDamageSourceID();
    float GetMaxDamageFarDist();
    void ForceRelease();
    bool IsForceRelease() const;
    void AddMod_Script(char const * const arg1);
    void RemoveMod_Script(char const * const arg1);
    int GetMods_Script(SQVM * arg1);
    int SetMods_Script(SQVM * arg1);
    bool IsOffhandWeapon() const;
    float GetNextAttackAllowedTime_Script() const;
    float GetNextAttackAllowedTimeRaw_Script() const;
    void SetNextAttackAllowedTime_Script(float arg1);
    int GetRodeoDamage_Script() const;
    int GetShotCount_Script() const;
    char const * GetWeaponClass() const;
    bool IsInCooldown() const;
    bool IsCooldownPending() const;
    int GetWeaponDamageFlags() const;
    int GetWeaponExplosionDamageFlags() const;
    int Script_GetImpactTableIndex() const;
    bool Script_GetNPCMissFastPlayer() const;
    float Script_GetMeleeLungeTargetRange() const;
    float Script_GetMeleeLungeTargetAngle() const;
    bool Script_GetMeleeCanHitHumanSized() const;
    bool Script_GetMeleeCanHitTitans() const;
    int Script_GetDamageAmountForArmorType(int arg1) const;
    float Script_GetMeleeAttackRange() const;
    float Script_GetMeleeAttackAngle() const;
    char const * Script_GetMeleeAnim3p() const;
    char const * Script_GetWeaponReadyMsg() const;
    char const * Script_GetWeaponReadyHint() const;
    float Script_GetGrenadeFuseTime() const;
    float Script_GetGrenadeIgnitionTime() const;
    bool Script_GetAllowHeadShots() const;
    int Script_GetCurrentAltFireIndex() const;
    float Script_GetWeaponZoomFOV();
    int Script_GetReloadMilestoneIndex();
    void Script_SetForcedADS();
    void Script_ClearForcedADS();
    int Script_GetForcedADS() const;
    int Script_GetInventoryIndex() const;
    int Script_GetChargeAnimIndex() const;
    void Script_SetChargeAnimIndex(int arg1);
    float GetWeaponDamageForce() const;
    float GetCoreDuration() const;
    bool IsSustainedDischargeWeapon() const;
    bool IsDischarging() const;
    float GetSustainedDischargeDuration() const;
    float GetSustainedDischargeRemainder() const;
    float GetSustainedDischargeFraction() const;
    float GetSustainedDischargePulseFrequency() const;
    void SetSustainedDischargeFractionForced(float arg1);
    bool IsSustainedLaserWeapon() const;
    void Script_DoMeleeHitConfirmation(float arg1);
    void Script_SetScriptTime0(float arg1);
    float Script_GetScriptTime0();
    void Script_SetScriptFlags0(int arg1);
    int Script_GetScriptFlags0();
    bool IsLoadoutPickup();
    void HideWeapon();
    C_BaseEntity* GetWeaponOwner_Script() const;
    bool HasSilencer() const;
    int PlayWeaponEffectAndReturnViewEffectHandle_Script(const char* arg1, const char* arg2, const char* arg3);
    void PlayWeaponEffectNoCull_Script(const char* arg1, const char* arg2, const char* arg3);
    void PlayWeaponEffect_Script(const char* arg1, const char* arg2, const char* arg3);
    C_BaseEntity* FireWeaponBolt_Script(const Vector3D& arg1, const Vector3D& arg2, float arg3, int arg4, int arg5, bool arg6, int arg7);
    int GetWeaponPrimaryAmmoCount_Script() const;
    void SetWeaponPrimaryAmmoCount_Script(int arg1);
    bool IsWeaponInAds_Script();
    bool SmartAmmo_IsEnabled_Script();
    void SmartAmmo_SetNewTargetTime();
    float SmartAmmo_GetNewTargetTime();
    float SmartAmmo_GetSearchAngle();
    ScriptVariant_t SmartAmmo_Script_GetTargets();
    void SmartAmmo_Script_SetTarget(C_BaseEntity* arg1, float arg2);
    void SmartAmmo_Script_StoreTargets();
    Vector3D SmartAmmo_Script_GetFirePosition(C_BaseEntity* arg1, int arg2);
    void SmartAmmo_Script_TrackEntity(C_BaseEntity* arg1, float arg2);
    void SmartAmmo_Script_UntrackEntity(C_BaseEntity* arg1);
    int SmartAmmo_Script_GetNumTrackersOnEntity(C_BaseEntity* arg1);
    bool SmartAmmo_IsVisibleTarget(C_BaseEntity* arg1);
    ScriptVariant_t SmartAmmo_Script_GetTrackedEntities();
    bool HasMod_Script(const char* arg1);
    void SetModBitfield_Script(int arg1);
    int GetModBitfield_Script();
    const char* GetSmartAmmoHudLockStyle_Script();
    const char* GetSmartAmmoWeaponType_Script();
    void SetAttackKickScale_Script(float arg1);
    void SetAttackKickRollScale_Script(float arg1);
    int Script_GetWeaponSettingInt(SQVM* arg1);
    int Script_GetWeaponSettingFloat(SQVM* arg1);
    int Script_GetWeaponSettingBool(SQVM* arg1);
    int Script_GetWeaponSettingVector(SQVM* arg1);
    int Script_GetWeaponSettingString(SQVM* arg1);
    int Script_GetWeaponSettingAsset(SQVM* arg1);
    void EmitWeaponNpcSound(float soundRadius, float soundDuration);
    void EmitWeaponNpcSound_DontUpdateLastFiredTime(float soundRadius, float soundDuration);
    void ShowWeapon();
    void SetViewmodelAmmoModelIndex_Script(int modelIndex);

    int GetWeaponChargeLevel() const;
    float GetChargeFraction() const;
    bool DeployWeapon(bool forceDeploy);
    bool Holster();
    bool FastHolster();
    bool Lower();
    bool Reload();
    bool PrimaryAttack();
    bool ChargeEnd_Internal(bool allowAttack, bool updateState);
    bool HolsterInternal(bool fastHolster);
    bool RaiseInternal(bool fromSprint, bool arg2);
    bool SustainedDischargeBegin();

    WeaponState_e GetWeaponState() const { return m_weapState; }
    void SetWeaponState(WeaponState_e state)
    {
        m_weapState = state;
        m_bInReload = state == WEAP_STATE_RELOAD;
    }

    struct BufferedBullet_s
    {
        double fireTime;
        FiredBulletInfo bullet;
    };

    struct AltFireIndex
    {
        unsigned int m_muzzleFlashBarrel : 2;
        unsigned int m_shellEjectBarrel : 2;
    };

    WeaponState_e m_weapState; // 0x12B8
    bool m_allowedToUse;
    bool m_discarded;
    int m_forcedADS; // 0x12C0
    int m_forceRelease;
    int m_customActivity;
    int m_customActivitySequence;
    CHandle<C_BaseEntity> m_customActivityOwner; // 0x12D0
    float m_customActivityEndTime;
    WeaponPlayerData_Client m_playerData;    // 0x12D8
    bool m_smartAmmoEnable;                  // 0x1380
    SmartAmmo_WeaponData_Client m_smartAmmo; // 0x1388
    bool m_needsReloadCheck;                 // 0x1578
    bool m_needsCooldown;
    bool m_needsEmptyCycleCheck;
    int m_skinOverride;
    bool m_skinOverrideIsValid; // 0x1580
    float m_chargeStartTime;
    float m_chargeEndTime;
    float m_lastChargeFrac;
    float m_lastRegenTime; // 0x1590
    bool m_stockPileWasDraining;
    int m_lastChargeLevel;
    int m_chargeEnergyDepleteStepCounter;
    int m_burstFireCount; // 0x15A0
    int m_burstFireIndex;
    int m_shotCount;
    float m_sustainedDischargeEndTime;
    unsigned int m_modBitfieldFromPlayer; // 0x15B0
    unsigned int m_modBitfieldInternal;
    unsigned int m_modBitfieldCurrent;
    int m_curSharedEnergyCost;
    bool m_scriptActivated; // 0x15C0
    bool m_isLoadoutPickup;
    CHandle<C_BaseEntity> m_utilityEnt;
    int m_weaponNameIndex;
    int m_animModelIndexPredictingClientOnly;
    int m_animSequencePredictingClientOnly; // 0x15D0
    HSCRIPT__* m_callbacks[37];             // 0x15D8
    WeaponModValues m_modVars;              // 0x1700
    int m_tracerAttachment[2];              // 0x23A0
    int m_damageSourceIdentifier;
    bool m_activityModifierSymbolForNameIsSet;
    CUtlSymbol m_activityModifierSymbolForName;
    bool m_hasAltAnim_adsIn[3]; // 0x23B0
    bool m_hasAltAnim_adsOut[3];
    bool m_hasAltAnim_idle[3];
    bool m_hasAltAnim_attack[3];
    bool m_hasAltAnim_oneHandedAdsIn[3];
    bool m_hasAltAnim_oneHandedAdsOut[3];
    bool m_hasAltAnim_oneHandedIdle[3];
    bool m_hasAltAnim_oneHandedAttack[3];
    bool m_cookWarningSoundActive; // 0x23C8
    bool m_loopSoundActive_1p;
    bool m_loopSoundActive_3p;
    float m_loopSoundLastAttackClockTime;
    float m_loopSoundLastAttackClockTimeWithFireDelay; // 0x23D0
    int m_loopSoundCurrentParity;
    int m_loopSoundActiveParity_1p;
    float m_attackKickScale;
    float m_attackKickRollScale; // 0x23E0
    WeaponString_t m_prevViewModel;
    WeaponString_t m_prevWorldModel;
    WeaponString_t m_prevHolsterModel;
    bool m_hasBeenInitialized;
    bool m_scriptActivatedClient;
    CHandle<C_BaseCombatCharacter> m_scriptActivatedClientOwner;
    float m_scriptActivatedTime; // 0x23F0
    bool m_ownerChangeCallbackInProgress;
    C_BaseCombatCharacter* m_ownerChangeCallbackOldOwner;
    CHandle<C_BaseEntity> m_customActivityAttachedModel; // 0x2400
    ParticleHandleListItem* m_viewModelParticleHandle;
    unsigned int m_modBitfieldCurrentOld; // 0x2410
    int m_viewmodelAmmoModelIndex;
    int m_bulletBufferHead;
    int m_bulletBufferTail;
    int m_numBufferedBullets;                   // 0x2420
    BufferedBullet_s m_bufferedBullets[16];     // 0x2428
    int m_numImmediateFireBullets;              // 0x27A8
    FiredBulletInfo m_immediateFireBullets[16]; // 0x27AC
    bool m_hasBufferedReload;                   // 0x2AAC
    double m_bufferedReloadTime;
    WeaponReloadInfo m_bufferedReload;
    CUtlReference<CParticleEffect> m_sustainedLaserEffects[32]; // 0x2AC0
    DeferredTraceHandle_t m_sustainedLaserDeferredTraces[32];   // 0x2DC0
    bool m_sustainedLaserEffectsCreated;                        // 0x2E40
    CParticleEffect* m_sustainedLaserImpactEffect;
    Vector3D m_sustainedLaserLastImpactPos; // 0x2E50
    int m_sustainedLaserLastImpactMaterial_SOUND;
    std::uint64_t m_sustainedLaserLastImpactSound; // 0x2E60
    CHandle<C_BaseEntity> m_sustainedLaserImpactEntity;
    CHandle<C_BaseEntity> m_sustainedLaserWhizbyEntity;
    int m_sustainedLaserImpactEffectTableIndex; // 0x2E70
    bool m_sustainedLaserImpactEffectCreated;
    CParticleEffect* m_grenadeArcIndicatorEffect[3];
    CParticleEffect* m_grenadeArcImpactIndicatorEffect[3]; // 0x2E90
    int m_grenadeArcIndicatorCount;                        // 0x2EA8
    bool m_chargeStartedOnClient;
    bool m_sustainedDischargeStartedOnClient;
    ChargeEffectState_e m_chargeEffectState; // 0x2EB0
    bool m_chargeEffectStartedOnLocalPlayer;
    bool m_smartAmmoLockEffectState;
    const char* m_activeChargeFullSound1P;
    const char* m_activeChargeFullSound3P; // 0x2EC0
    const char* m_activeChargeSound1P;
    const char* m_activeChargeSound3P;
    const char* m_activeChargeDrainSound1P;
    const char* m_activeChargeDrainSound3P; // 0x2EE0
    const char* m_activeChargeEffectName1P;
    const char* m_activeChargeEffectName3P;
    const char* m_activeChargeEffect2Name1P;
    const char* m_activeChargeEffect2Name3P; // 0x2F00
    const char* m_activeSmartAmmoLockEffectName1P;
    const char* m_activeSmartAmmoLockEffectName3P;
    const char* m_activeSmartAmmoLockEffect2Name1P;
    const char* m_activeSmartAmmoLockEffect2Name3P; // 0x2F20
    int m_weaponReadySoundPlayedCount;
    AltFireIndex m_localAltFireIndex;
};
