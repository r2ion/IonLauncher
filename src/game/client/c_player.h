#pragma once

#include "game/client/c_basecombatcharacter.h"
#include "game/shared/player_shared.h"
#include "game/shared/usercmd.h"
#include "game/shared/weapon_prediction.h"
#include "playerstate.h"

#include <cstddef>
#include <cstdint>

class C_BaseViewModel;
class C_PlayerExtended;

struct CurrentData_Player_Client
{
    float timeBase;
    std::byte m_Padding004[0x194];
    float m_flHullHeight;
    float m_traversalAnimProgress;
    float m_sprintTiltFrac;
    QAngle m_angEyeAngles;
};

struct CurrentData_LocalPlayer_Client
{
    std::byte m_Padding000[0x198];
    QAngle m_viewConeAngleMin;
    QAngle m_viewConeAngleMax;
    Vector3D m_stepSmoothingOffset;
    QAngle m_vecPunchBase_Angle;
    QAngle m_vecPunchBase_AngleVel;
    QAngle m_vecPunchWeapon_Angle;
    QAngle m_vecPunchWeapon_AngleVel;
};

static_assert(sizeof(C_BaseCombatCharacter) == 0x1490);

class C_Player : public C_BaseCombatCharacter
{
  public:
    static C_Player* GetLocalViewPlayer(int splitScreenSlot = -1);
    static C_Player* GetLocalPlayer(int splitScreenSlot = -1);
    static C_Player* SetCrosshairQueryPlayer(C_Player* player);
    C_PlayerExtended* GetPlayerExtended();
    bool UsesWeaponPrediction() const;

    PredictedMovementState GetPredictedMovementState();
    void RestorePredictedMovementState(const PredictedMovementState& state);
    float GetPredictedMovementScale(PredictedMovementScale setting) const;
    void SetPredictedMovementScale(PredictedMovementScale setting, float value);
    bool GetPredictedMovementFlag(PredictedMovementFlag setting) const;
    void SetPredictedMovementFlag(PredictedMovementFlag setting, bool value);
    Vector3D GetPredictedMovementVelocity();
    void SetPredictedMovementVelocity(const Vector3D& velocity);
    void SetPredictedDodgePower(float power);

    static C_PlayerExtended sm_PlayerExtended;

    bool IsMantling() const;
    C_WeaponX* GetActiveWeapon();
    std::uint64_t GetPlatformUserId() const
    {
        return m_platformUserId;
    }
    C_BaseViewModel* GetViewModel();
    float GetZoomFraction();
    float GetZoomOutDuration();
    Vector3D GetAimDirection();
    int InterpolateFieldsInternal(float currentTime, const SingleSnapshotValues* secondSnapshot, float secondSnapshotTime) override;
    bool CanPresentLocalPlayer() const;
    void UpdateZoomPresentation();

    std::byte m_Padding1490[0x208];
    bool m_bZooming; // 0x1698
    std::byte m_Padding1699[3];
    float m_zoomBaseFrac;
    float m_zoomBaseTime;
    std::byte m_Padding16A4[0x1F4];
    float m_flSuitPower; // m_Local + 0x20;
    std::byte m_Padding189C[0x28];
    float m_powerRegenRateScale; // m_Local + 0x4C
    float m_dodgePowerDelayScale; // m_Local + 0x50
    std::byte m_Padding18CC[0x191];
    bool m_titanDisembarkEnabled; // m_Local + 0x1E5;
    std::byte m_Padding1A5E[0x52];
    CurrentData_Player_Client m_currentFramePlayer;
    CurrentData_LocalPlayer_Client m_currentFrameLocalPlayer;
    std::byte m_Padding1E4C[0xC];
    CPlayerState pl; // 0x1E58
    std::byte m_Padding1ED8[0x148];
    std::uint64_t m_platformUserId; // 0x2020
    std::byte m_Padding2028[0x49C];
    float m_wallrunFrictionScale; // 0x24C4
    float m_groundFrictionScale;  // 0x24C8
    std::byte m_Padding24CC[0x3F4];
    int m_viewConeParity;
    int m_previousViewConeParity;
    std::byte m_Padding28C8[0x2C];
    std::uint32_t m_playerFlags; // 0x28F4
    std::byte m_Padding28F8[0x8];
    CUserCmd m_LastCmd;
    CHandle<C_BaseEntity> m_hCockpitProp;
    CHandle<C_BaseEntity> m_usePromptEntity;
    int m_afButtonForced;
    std::byte m_Padding2A54[4];
    CUserCmd* m_pCurrentCommand;
    std::byte m_Padding2A60[0x168];
    float m_airSpeed;        // 0x2BC8
    float m_airAcceleration; // 0x2BCC
    std::byte m_Padding2BD0[0x10];
    QAngle m_lastUCmdAttackAngles;
    std::byte m_Padding2BEC[0x109];
    bool m_bLerpMissingParent;
};

class C_PlayerExtended
{
    friend class C_Player;

  public:
    C_PlayerExtended();

    bool SelectBaseline(C_Player* player, bool receivedNewWorldUpdate, int acknowledgedCommand);
    void RestoreBaseline(int commandNumber);
    void StoreResults(int commandNumber);
    void Reset();

    static void ClearClientState();
    CPlayerPredictionState m_WeaponPrediction;

    std::array<PredictedWeaponSnapshot_t, MULTIPLAYER_BACKUP> m_Authoritative;
    unsigned int m_nNextAuthoritative = 0;
};

static_assert(offsetof(C_Player, m_bZooming) == 0x1698);
static_assert(offsetof(C_Player, m_zoomBaseFrac) == 0x169C);
static_assert(offsetof(C_Player, m_zoomBaseTime) == 0x16A0);
static_assert(offsetof(C_Player, m_flSuitPower) == 0x1898);
static_assert(offsetof(C_Player, m_powerRegenRateScale) == 0x18C4);
static_assert(offsetof(C_Player, m_dodgePowerDelayScale) == 0x18C8);
static_assert(offsetof(C_Player, m_currentFramePlayer) == 0x1AB0);
static_assert(offsetof(C_Player, m_titanDisembarkEnabled) == 0x1A5D);
static_assert(offsetof(C_Player, m_wallrunFrictionScale) == 0x24C4);
static_assert(offsetof(C_Player, m_groundFrictionScale) == 0x24C8);
static_assert(offsetof(C_Player, m_playerFlags) == 0x28F4);
static_assert(offsetof(C_Player, m_airSpeed) == 0x2BC8);
static_assert(offsetof(C_Player, m_airAcceleration) == 0x2BCC);
static_assert(offsetof(C_Player, pl) == 0x1E58);
static_assert(offsetof(C_Player, pl) + offsetof(CPlayerState, a_angle) == 0x1E70);
static_assert(offsetof(C_Player, m_platformUserId) == 0x2020);
static_assert(offsetof(C_Player, m_LastCmd) == 0x2900);
static_assert(offsetof(C_Player, m_lastUCmdAttackAngles) == 0x2BE0);
static_assert(offsetof(C_Player, m_bLerpMissingParent) == 0x2CF5);

using C_Player_IsMantling_t = bool (*)(const C_Player*);
using C_Player_GetLocalPlayer_t = C_Player* (*)(int);
using C_Player_GetActiveWeapon_t = C_WeaponX* (*)(C_Player*);
using C_Player_GetViewModel_t = C_BaseViewModel* (*)(C_Player*);
using C_Player_GetZoomFraction_t = float (*)(C_Player*);
using C_Player_GetZoomOutDuration_t = float (*)(C_Player*);
using C_Player_GetAimDirection_t = Vector3D* (*)(C_Player*, Vector3D*);
