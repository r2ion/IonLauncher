#pragma once

#include "client/basecombatcharacter.h"
#include "game/shared/usercmd.h"

#include <cstddef>

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

    bool IsMantling() const;

    std::byte m_Padding1490[0x620];
    CurrentData_Player_Client m_currentFramePlayer;
    CurrentData_LocalPlayer_Client m_currentFrameLocalPlayer;
    std::byte m_Padding1E4C[0xA74];
    int m_viewConeParity;
    int m_previousViewConeParity;
    std::byte m_Padding28C8[0x38];
    CUserCmd m_LastCmd;
    CHandle<C_BaseEntity> m_hCockpitProp;
    CHandle<C_BaseEntity> m_usePromptEntity;
    int m_afButtonForced;
    std::byte m_Padding2A54[4];
    CUserCmd* m_pCurrentCommand;
    std::byte m_Padding2A60[0x295];
    bool m_bLerpMissingParent;
};
