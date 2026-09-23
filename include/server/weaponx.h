#pragma once

#include <cstddef>
#include <cstdint>

#include "server/basecombatweapon.h"
#include "smartammo.h"
#include "weapon_parse.h"
#include "weapon_playerdata.h"
#include "weapon_x_shared.h"

class CWeaponX : public CBaseCombatWeapon
{
  public:
    ServerClass* GetServerClass() override = 0; // 3
    int YouForgotToImplementOrDeclareServerClass() override = 0; // 4
    ServerDataMap* GetDataDescMap() override = 0;    // 5
    ScriptClassDesc_t* GetScriptDesc() override = 0; // 6
    void PostNewModel() override = 0;                // 8
    int UpdateTransmitState() override = 0; // 20
    const char* GetTracerType() override = 0;                   // 22
    void Spawn() override = 0;                                  // 23
    void Precache() override = 0;                               // 24
    void OnRestore() override = 0;                              // 40
    const CWeaponX* MyWeaponXPointerConst() const override = 0; // 70
    CWeaponX* MyWeaponXPointer() override = 0;                  // 71
    void UpdateOnRemove() override = 0;                         // 117
    void MakeTracer(const Vector3D& start, const trace_t& trace, CBaseEntity* entity, unsigned int tracerType) override = 0; // 120
    int GetTracerAttachment() override = 0; // 121
    void HandleAnimEvent(animevent_t* event) override = 0;                    // 258
    void ClearPredictedAnimEvents() override = 0;                             // 272
    void ExecPredictedAnimEvent(int event, const char* options) override = 0; // 273
    void OnPickedUp(CBaseCombatCharacter* newOwner) override = 0;             // 274

    WeaponState_e GetWeaponState() const
    {
        return m_weapState;
    }
    void SetWeaponState(WeaponState_e state)
    {
        m_weapState = state;
        m_bInReload = state == WEAP_STATE_RELOAD;
    }
    int GetWeaponChargeLevel() const
    {
        return m_lastChargeLevel;
    }
    float GetChargeFraction() const
    {
        return m_lastChargeFrac;
    }
    CBaseEntity* GetWeaponUtilityEntity() const
    {
        return m_utilityEnt.Get();
    }

    WeaponState_e m_weapState;                         // 0xFC0
    bool m_allowedToUse;                               // 0xFC4
    bool m_discarded;                                  // 0xFC5
    std::byte _padFC6[0x2];                            // 0xFC6
    int m_forcedADS;                                   // 0xFC8
    int m_forceRelease;                                // 0xFCC
    bool m_forceReleaseFromServer;                     // 0xFD0
    std::byte _padFD1[0x3];                            // 0xFD1
    int m_customActivity;                              // 0xFD4
    int m_customActivitySequence;                      // 0xFD8
    CHandle<CBaseEntity> m_customActivityOwner;        // 0xFDC
    float m_customActivityEndTime;                     // 0xFE0
    std::byte _padFE4[0x4];                            // 0xFE4
    WeaponPlayerData m_playerData;                     // 0xFE8
    bool m_smartAmmoEnable;                            // 0x1090
    std::byte _pad1091[0x7];                           // 0x1091
    SmartAmmo_WeaponData m_smartAmmo;                  // 0x1098
    bool m_needsReloadCheck;                           // 0x1288
    bool m_needsCooldown;                              // 0x1289
    bool m_needsEmptyCycleCheck;                       // 0x128A
    std::byte _pad128B[0x1];                           // 0x128B
    int m_skinOverride;                                // 0x128C
    bool m_skinOverrideIsValid;                        // 0x1290
    std::byte _pad1291[0x3];                           // 0x1291
    float m_chargeStartTime;                           // 0x1294
    float m_chargeEndTime;                             // 0x1298
    float m_lastChargeFrac;                            // 0x129C
    float m_lastRegenTime;                             // 0x12A0
    bool m_stockPileWasDraining;                       // 0x12A4
    std::byte _pad12A5[0x3];                           // 0x12A5
    int m_lastChargeLevel;                             // 0x12A8
    int m_chargeEnergyDepleteStepCounter;              // 0x12AC
    int m_burstFireCount;                              // 0x12B0
    int m_burstFireIndex;                              // 0x12B4
    int m_shotCount;                                   // 0x12B8
    float m_sustainedDischargeEndTime;                 // 0x12BC
    std::uint32_t m_modBitfieldFromPlayer;             // 0x12C0
    std::uint32_t m_modBitfieldInternal;               // 0x12C4
    std::uint32_t m_modBitfieldCurrent;                // 0x12C8
    int m_curSharedEnergyCost;                         // 0x12CC
    bool m_scriptActivated;                            // 0x12D0
    bool m_isLoadoutPickup;                            // 0x12D1
    std::byte _pad12D2[0x2];                           // 0x12D2
    CHandle<CBaseEntity> m_utilityEnt;                 // 0x12D4
    int m_weaponNameIndex;                             // 0x12D8
    int m_animModelIndexPredictingClientOnly;          // 0x12DC
    int m_animSequencePredictingClientOnly;            // 0x12E0
    std::byte _pad12E4[0x4];                           // 0x12E4
    HSCRIPT__* m_weaponScriptCB_[37];                  // 0x12E8
    WeaponModValues m_modVars;                         // 0x1410
    int m_tracerAttachment[2];                         // 0x20B0
    int m_damageSourceIdentifier;                      // 0x20B8
    bool m_activityModifierSymbolForNameIsSet;         // 0x20BC
    std::byte _pad20BD[0x1];                           // 0x20BD
    CUtlSymbol m_activityModifierSymbolForName;        // 0x20BE
    bool m_hasAltAnim_adsIn[3];                        // 0x20C0
    bool m_hasAltAnim_adsOut[3];                       // 0x20C3
    bool m_hasAltAnim_idle[3];                         // 0x20C6
    bool m_hasAltAnim_attack[3];                       // 0x20C9
    bool m_hasAltAnim_oneHandedAdsIn[3];               // 0x20CC
    bool m_hasAltAnim_oneHandedAdsOut[3];              // 0x20CF
    bool m_hasAltAnim_oneHandedIdle[3];                // 0x20D2
    bool m_hasAltAnim_oneHandedAttack[3];              // 0x20D5
    bool m_cookWarningSoundActive;                     // 0x20D8
    bool m_loopSoundActive_1p;                         // 0x20D9
    bool m_loopSoundActive_3p;                         // 0x20DA
    std::byte _pad20DB[0x1];                           // 0x20DB
    float m_loopSoundLastAttackClockTime;              // 0x20DC
    float m_loopSoundLastAttackClockTimeWithFireDelay; // 0x20E0
    int m_loopSoundCurrentParity;                      // 0x20E4
    int m_loopSoundActiveParity_1p;                    // 0x20E8
    float m_attackKickScale;                           // 0x20EC
    float m_attackKickRollScale;                       // 0x20F0
    WeaponString_t m_prevViewModelWpnStr;              // 0x20F4
    WeaponString_t m_prevWorldModelWpnStr;             // 0x20F6
    WeaponString_t m_prevHolsterModelWpnStr;           // 0x20F8
    std::byte _pad20FA[0x2];                           // 0x20FA
    EHANDLE newProjectiles[8];                         // 0x20FC
    int newProjectileCount;                            // 0x211C
    CWeaponX* m_smartAmmoNextWeapon;                   // 0x2120
    CWeaponX* m_smartAmmoPrevWeapon;                   // 0x2128
    float m_npcUseCheckTime;                           // 0x2130
    float m_npcUseCheckDist;                           // 0x2134
    float m_sustainedDischargeNextPulseTime;           // 0x2138
    std::byte _pad213C[0x4];                           // 0x213C
};

static_assert(sizeof(WeaponModValues) == 0xCA0);
static_assert(sizeof(WeaponPlayerData) == 0xA8);
static_assert(sizeof(SmartAmmo_WeaponData) == 0x1F0);
static_assert(sizeof(CWeaponX) == 0x2140);
