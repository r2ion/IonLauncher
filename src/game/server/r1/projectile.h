#pragma once

#include "game/server/baseanimating.h"
#include "game/shared/takedamageinfo.h"
#include "game/shared/weapon_parse.h"
#include "tier1/utlvector.h"

#include <cstddef>
#include <cstdint>

class CProjectile : public CBaseAnimating
{
  public:
    bool m_weaponDataIsSet;                                           // 0x0EB8
    bool m_forceAdjustToGunBarrelDisabled;                            // 0x0EB9
    std::byte m_Padding0EBA[0x2];                                     // 0x0EBA
    std::uint32_t m_weaponClassIndex;                                 // 0x0EBC
    float m_destructionDistance;                                      // 0x0EC0
    std::int32_t m_passThroughDepthTotal;                             // 0x0EC4
    std::uint32_t m_modBitfield;                                      // 0x0EC8
    std::uint32_t m_overrideMods;                                     // 0x0ECC
    std::int32_t m_projectileTrailIndex;                              // 0x0ED0
    std::int32_t m_impactEffectTable;                                 // 0x0ED4
    bool m_reducedEffects;                                            // 0x0ED8
    std::byte m_Padding0ED9[0x3];                                     // 0x0ED9
    float m_projectileCreationTimeServer;                             // 0x0EDC
    const FileWeaponInfo_Server* m_wpnData;                           // 0x0EE0
    WEAPON_FILE_INFO_HANDLE m_hWeaponFileInfo;                        // 0x0EE8
    std::byte m_Padding0EEA[0x2];                                     // 0x0EEA
    std::int32_t m_weaponChargeLevel;                                 // 0x0EEC
    WeaponModValues m_modVars;                                        // 0x0EF0
    bool m_modVarsAreValid;                                           // 0x1B90
    std::byte m_Padding1B91[0x3];                                     // 0x1B91
    Vector3D m_launchOrigin;                                          // 0x1B94
    HSCRIPT__* m_scriptCB[5];                                         // 0x1BA0
    bool m_hasPlayedTrailEffect;                                      // 0x1BC8
    std::byte m_Padding1BC9[0x3];                                     // 0x1BC9
    std::int32_t m_projectileLifeTimeEndTick;                         // 0x1BCC
    float m_projectileCreationTime;                                   // 0x1BD0
    bool m_isVortexRefired;                                           // 0x1BD4
    bool m_damageAliveOnly;                                           // 0x1BD5
    bool m_usesPositionFunction;                                      // 0x1BD6
    bool m_usesVelocityFunction;                                      // 0x1BD7
    Vector3D m_lastCollisionNormal;                                   // 0x1BD8
    std::int32_t m_bounceIndex;                                       // 0x1BE4
    std::int32_t m_randomInt;                                         // 0x1BE8
    bool m_thrownByAI;                                                // 0x1BEC
    bool m_takeAllDamage;                                             // 0x1BED
    bool m_pendingLagCompensation;                                    // 0x1BEE
    std::byte m_Padding1BEF;                                          // 0x1BEF
    float m_lagCompSpawnTime;                                         // 0x1BF0
    float m_perPolyRadius[2];                                         // 0x1BF4
    Vector3D m_posBeforePhysicsSimulate;                              // 0x1BFC
    bool m_hasIgnited;                                                // 0x1C08
    bool m_inLagCompensation;                                         // 0x1C09
    std::byte m_Padding1C0A[0x6];                                     // 0x1C0A
    CUtlVectorFixedGrowable<CHandle<CBaseEntity>, 16> m_passEntities; // 0x1C10
    float m_impactDamageOverride;                                     // 0x1C78
    ScriptOriginatedDamageInfo m_scriptDamageInfo;                    // 0x1C7C
    std::int32_t m_touchScriptDamageType;                             // 0x1C88
    std::int32_t m_passThroughCount;                                  // 0x1C8C
    float m_damageForce;                                              // 0x1C90
    float m_damageForceForExplosions;                                 // 0x1C94
    std::int32_t m_attackerType;                                      // 0x1C98
    std::byte m_Reserved1C9C[0x4];                                    // 0x1C9C
};

static_assert(sizeof(CProjectile) == 0x1CA0);
static_assert(offsetof(CProjectile, m_weaponDataIsSet) == 0x0EB8);
static_assert(offsetof(CProjectile, m_modVars) == 0x0EF0);
static_assert(offsetof(CProjectile, m_launchOrigin) == 0x1B94);
static_assert(offsetof(CProjectile, m_usesVelocityFunction) == 0x1BD7);
static_assert(offsetof(CProjectile, m_passEntities) == 0x1C10);
static_assert(offsetof(CProjectile, m_impactDamageOverride) == 0x1C78);
static_assert(offsetof(CProjectile, m_scriptDamageInfo) == 0x1C7C);
static_assert(offsetof(CProjectile, m_attackerType) == 0x1C98);
