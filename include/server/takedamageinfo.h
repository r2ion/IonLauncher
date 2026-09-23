#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/ehandle.h"
#include "mathlib/vector.h"

enum DeathPackage : std::int32_t;
enum FlinchDirection : std::int32_t;
enum HitboxIndex_t : std::int32_t;
enum Hitgroup_t : std::int32_t;

struct ScriptOriginatedDamageInfo
{
    std::int32_t m_scriptDamageType;       // 0x00
    std::int32_t m_damageSourceIdentifier; // 0x04
    std::int32_t m_scriptAttackerClass;    // 0x08
};

class CTakeDamageInfo
{
  public:
    Vector3D m_vecDamageForce;                     // 0x00
    Vector3D m_vecDamagePosition;                  // 0x0C
    Vector3D m_vecReportedPosition;                // 0x18
    EHANDLE m_hInflictor;                          // 0x24
    EHANDLE m_hAttacker;                           // 0x28
    EHANDLE m_hWeapon;                             // 0x2C
    std::int16_t m_hWeaponFileInfo;                // 0x30
    bool m_forceKill;                              // 0x32
    std::byte _pad33[0x1];                         // 0x33
    float m_flDamage;                              // 0x34
    float m_damageCriticalScale;                   // 0x38
    float m_flMaxDamage;                           // 0x3C
    float m_flHeavyArmorDamageScale;               // 0x40
    std::int32_t m_bitsDamageType;                 // 0x44
    float m_flRadius;                              // 0x48
    Hitgroup_t m_hitGroup;                         // 0x4C
    HitboxIndex_t m_hitBox;                        // 0x50
    ScriptOriginatedDamageInfo m_scriptDamageInfo; // 0x54
    DeathPackage m_deathPackage;                   // 0x60
    float m_distanceFromAttackOrigin;              // 0x64
    float m_distanceFromExplosionCenter;           // 0x68
    bool m_doDeathForce;                           // 0x6C
    std::byte _pad6D[0x3];                         // 0x6D
    std::int32_t m_damageFlags;                    // 0x70
    FlinchDirection m_flinchDirection;             // 0x74
};

static_assert(sizeof(CTakeDamageInfo) == 0x78);
