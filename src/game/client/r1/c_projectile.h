#pragma once

#include "game/client/c_baseanimating.h"
#include "game/shared/weapon_parse.h"
#include "gametrace.h"
#include "tier1/utlvector.h"

#include <cstddef>
#include <cstdint>

class CParticleEffect;

class C_Projectile : public C_BaseAnimating
{
  public:
    bool m_weaponDataIsSet;                                            // 0x1230
    bool m_forceAdjustToGunBarrelDisabled;                             // 0x1231
    std::byte m_Padding1232[0x2];                                      // 0x1232
    std::uint32_t m_weaponClassIndex;                                  // 0x1234
    float m_destructionDistance;                                       // 0x1238
    std::int32_t m_passThroughDepthTotal;                              // 0x123C
    std::uint32_t m_modBitfield;                                       // 0x1240
    std::uint32_t m_overrideMods;                                      // 0x1244
    std::int32_t m_projectileTrailIndex;                               // 0x1248
    std::int32_t m_impactEffectTable;                                  // 0x124C
    bool m_reducedEffects;                                             // 0x1250
    std::byte m_Padding1251[0x3];                                      // 0x1251
    float m_projectileCreationTimeServer;                              // 0x1254
    const FileWeaponInfo_Client* m_wpnData;                            // 0x1258
    WEAPON_FILE_INFO_HANDLE m_hWeaponFileInfo;                         // 0x1260
    std::byte m_Padding1262[0x2];                                      // 0x1262
    std::int32_t m_weaponChargeLevel;                                  // 0x1264
    WeaponModValues m_modVars;                                         // 0x1268
    bool m_modVarsAreValid;                                            // 0x1F08
    std::byte m_Padding1F09[0x3];                                      // 0x1F09
    Vector3D m_launchOrigin;                                           // 0x1F0C
    HSCRIPT__* m_scriptCB[5];                                          // 0x1F18
    bool m_hasPlayedTrailEffect;                                       // 0x1F40
    std::byte m_Padding1F41[0x3];                                      // 0x1F41
    std::int32_t m_projectileLifeTimeEndTick;                          // 0x1F44
    float m_projectileCreationTime;                                    // 0x1F48
    bool m_isVortexRefired;                                            // 0x1F4C
    bool m_damageAliveOnly;                                            // 0x1F4D
    bool m_usesPositionFunction;                                       // 0x1F4E
    bool m_usesVelocityFunction;                                       // 0x1F4F
    Vector3D m_lastCollisionNormal;                                    // 0x1F50
    std::int32_t m_bounceIndex;                                        // 0x1F5C
    std::int32_t m_randomInt;                                          // 0x1F60
    bool m_thrownByAI;                                                 // 0x1F64
    std::byte m_Padding1F65[0x3];                                      // 0x1F65
    float m_perPolyRadius[2];                                          // 0x1F68
    Vector3D m_posBeforePhysicsSimulate;                               // 0x1F70
    bool m_hasIgnited;                                                 // 0x1F7C
    bool m_inLagCompensation;                                          // 0x1F7D
    std::byte m_Padding1F7E[0x2];                                      // 0x1F7E
    CUtlVectorFixedGrowable<CHandle<C_BaseEntity>, 16> m_passEntities; // 0x1F80
    bool m_wantStartTrailEffect;                                       // 0x1FE8
    bool m_hasValidGunBarrelPosition;                                  // 0x1FE9
    bool m_hasCalledPostDataUpdate;                                    // 0x1FEA
    std::byte m_Padding1FEB[0x5];                                      // 0x1FEB
    CParticleEffect* m_trailEffects[1];                                // 0x1FF0
    Vector3D m_prevOrigin;                                             // 0x1FF8
    Vector3D m_nextOrigin;                                             // 0x2004
    QAngle m_prevAngles;                                               // 0x2010
    QAngle m_nextAngles;                                               // 0x201C
    float m_prevPredictedIntervalStartTime;                            // 0x2028
    float m_prevPredictedIntervalEndTime;                              // 0x202C
    float m_nextPredictedIntervalStartTime;                            // 0x2030
    Vector3D m_nextPredictedIntervalOrigin;                            // 0x2034
    GameTrace m_prevMovementTrace;                                     // 0x2040
    std::byte m_Reserved20B0[0x18];                                    // 0x20B0
    float m_predictedImpactTime;                                       // 0x20C8
    bool m_hasDoneClientInit;                                          // 0x20CC
    bool m_hasPlayedWhizby;                                            // 0x20CD
    std::byte m_Padding20CE[0x2];                                      // 0x20CE
    Vector3D m_whizByStart;                                            // 0x20D0
    char m_whizBySoundName[64];                                        // 0x20DC
    std::uint32_t m_oldModBitField;                                    // 0x211C
};

static_assert(sizeof(C_BaseAnimating) == 0x1230);
static_assert(sizeof(C_Projectile) == 0x2120);
static_assert(offsetof(C_Projectile, m_weaponDataIsSet) == 0x1230);
static_assert(offsetof(C_Projectile, m_modVars) == 0x1268);
static_assert(offsetof(C_Projectile, m_launchOrigin) == 0x1F0C);
static_assert(offsetof(C_Projectile, m_usesVelocityFunction) == 0x1F4F);
static_assert(offsetof(C_Projectile, m_passEntities) == 0x1F80);
static_assert(offsetof(C_Projectile, m_wantStartTrailEffect) == 0x1FE8);
static_assert(offsetof(C_Projectile, m_trailEffects) == 0x1FF0);
static_assert(offsetof(C_Projectile, m_prevMovementTrace) == 0x2040);
static_assert(offsetof(C_Projectile, m_predictedImpactTime) == 0x20C8);
static_assert(offsetof(C_Projectile, m_oldModBitField) == 0x211C);
