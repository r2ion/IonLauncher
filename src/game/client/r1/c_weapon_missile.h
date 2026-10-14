#pragma once

#include "game/client/r1/c_projectile.h"
#include "game/shared/r1/weapon_missile.h"

class C_Missile : public C_Projectile
{
  public:
    bool m_hasPlayedWhizby;                  // 0x2120
    std::byte m_Padding2121[0x3];            // 0x2121
    Vector3D m_whizByStart;                  // 0x2124
    char m_whizBySoundName[64];              // 0x2130
    float m_fSpeed;                          // 0x2170
    float m_homingSpeed;                     // 0x2174
    float m_homingSpeedDodgingPlayer;        // 0x2178
    Vector3D m_launchDir;                    // 0x217C
    CHandle<C_BaseEntity> m_hSpecificTarget; // 0x2188
    Vector3D m_targetOffset;                 // 0x218C
    Vector3D m_targetPosition;               // 0x2198
    bool m_useTargetPosition;                // 0x21A4
    std::byte m_Padding21A5[0x3];            // 0x21A5
    float m_postIgnitionSpeed;               // 0x21A8
    float m_flGracePeriodEndsAt;             // 0x21AC
    bool m_pathSettingsInitialized;          // 0x21B0
    bool m_expandContractMissile;            // 0x21B1
    bool m_spiralMissile;                    // 0x21B2
    std::byte m_Padding21B3;                 // 0x21B3
    union
    {
        MissileSpiralSettings m_spiralSettings; // 0x21B4
        MissilePathSettings m_pathSettings;     // 0x21B4
    };
    MissilePathExpandContractSettings m_expandContractSettings; // 0x21D8
    float m_lastThinkTime;                                      // 0x2210
    CHandle<C_BaseEntity> m_explosionIgnoreEntity;              // 0x2214
    std::byte m_Reserved2218[0x8];                              // 0x2218
};

static_assert(sizeof(C_Missile) == 0x2220);
static_assert(offsetof(C_Missile, m_hasPlayedWhizby) == 0x2120);
static_assert(offsetof(C_Missile, m_fSpeed) == 0x2170);
static_assert(offsetof(C_Missile, m_launchDir) == 0x217C);
static_assert(offsetof(C_Missile, m_hSpecificTarget) == 0x2188);
static_assert(offsetof(C_Missile, m_pathSettingsInitialized) == 0x21B0);
static_assert(offsetof(C_Missile, m_pathSettings) == 0x21B4);
static_assert(offsetof(C_Missile, m_expandContractSettings) == 0x21D8);
static_assert(offsetof(C_Missile, m_explosionIgnoreEntity) == 0x2214);
