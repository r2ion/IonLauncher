#pragma once

#include "client/basecombatcharacter.h"
#include "game/shared/usercmd.h"

#include <cstddef>

class C_BaseViewModel;


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

    bool IsMantling() const;
    C_WeaponX* GetActiveWeapon();
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
    std::byte m_Padding16A4[0x40C];
    CurrentData_Player_Client m_currentFramePlayer;
    CurrentData_LocalPlayer_Client m_currentFrameLocalPlayer;
    std::byte m_Padding1E4C[0x24];
    QAngle m_attackAngles;
    std::byte m_Padding1E7C[0xA44];
    int m_viewConeParity;
    int m_previousViewConeParity;
    std::byte m_Padding28C8[0x38];
    CUserCmd m_LastCmd;
    CHandle<C_BaseEntity> m_hCockpitProp;
    CHandle<C_BaseEntity> m_usePromptEntity;
    int m_afButtonForced;
    std::byte m_Padding2A54[4];
    CUserCmd* m_pCurrentCommand;
    std::byte m_Padding2A60[0x180];
    QAngle m_lastUCmdAttackAngles;
    std::byte m_Padding2BEC[0x109];
    bool m_bLerpMissingParent;
};

static_assert(offsetof(C_Player, m_bZooming) == 0x1698);
static_assert(offsetof(C_Player, m_zoomBaseFrac) == 0x169C);
static_assert(offsetof(C_Player, m_zoomBaseTime) == 0x16A0);
static_assert(offsetof(C_Player, m_currentFramePlayer) == 0x1AB0);
static_assert(offsetof(C_Player, m_attackAngles) == 0x1E70);
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

