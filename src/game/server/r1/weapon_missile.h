#pragma once

#include "game/server/r1/projectile.h"
#include "game/shared/r1/weapon_missile.h"

class CMissile : public CProjectile
{
  public:
    float m_flAugerTime;                    // 0x1CA0
    float m_flMarkDeadTime;                 // 0x1CA4
    float m_flDamage;                       // 0x1CA8
    float m_fExplosionInnerRadius;          // 0x1CAC
    float m_fExplosionOuterRadius;          // 0x1CB0
    float m_lifetime;                       // 0x1CB4
    float m_fSpeed;                         // 0x1CB8
    float m_homingSpeed;                    // 0x1CBC
    float m_homingSpeedDodgingPlayer;       // 0x1CC0
    Vector3D m_launchDir;                   // 0x1CC4
    CHandle<CBaseEntity> m_hSpecificTarget; // 0x1CD0
    Vector3D m_targetOffset;                // 0x1CD4
    Vector3D m_targetPosition;              // 0x1CE0
    bool m_useTargetPosition;               // 0x1CEC
    std::byte m_Padding1CED[0x3];           // 0x1CED
    float m_postIgnitionSpeed;              // 0x1CF0
    float m_flGracePeriodEndsAt;            // 0x1CF4
    bool m_pathSettingsInitialized;         // 0x1CF8
    bool m_expandContractMissile;           // 0x1CF9
    bool m_spiralMissile;                   // 0x1CFA
    std::byte m_Padding1CFB;                // 0x1CFB
    union
    {
        MissileSpiralSettings m_spiralSettings; // 0x1CFC
        MissilePathSettings m_pathSettings;     // 0x1CFC
    };
    MissilePathExpandContractSettings m_expandContractSettings; // 0x1D20
    float m_lastThinkTime;                                      // 0x1D58
    CHandle<CBaseEntity> m_explosionIgnoreEntity;               // 0x1D5C
};

static_assert(sizeof(CMissile) == 0x1D60);
static_assert(offsetof(CMissile, m_flAugerTime) == 0x1CA0);
static_assert(offsetof(CMissile, m_fSpeed) == 0x1CB8);
static_assert(offsetof(CMissile, m_launchDir) == 0x1CC4);
static_assert(offsetof(CMissile, m_hSpecificTarget) == 0x1CD0);
static_assert(offsetof(CMissile, m_pathSettingsInitialized) == 0x1CF8);
static_assert(offsetof(CMissile, m_pathSettings) == 0x1CFC);
static_assert(offsetof(CMissile, m_expandContractSettings) == 0x1D20);
static_assert(offsetof(CMissile, m_explosionIgnoreEntity) == 0x1D5C);
