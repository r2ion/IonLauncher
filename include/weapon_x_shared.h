#pragma once

#include "mathlib/vector.h"
#include <cstddef>
#include <cstdint>

enum WeaponState_e : int
{
    WEAP_STATE_IDLE = 0,
    WEAP_STATE_DEPLOY = 1,
    WEAP_STATE_HOLSTER = 2,
    WEAP_STATE_RAISE = 3,
    WEAP_STATE_LOWER = 4,
    WEAP_STATE_CHARGE = 5,
    WEAP_STATE_CHARGE_RELEASE = 6,
    WEAP_STATE_SUSTAINED_DISCHARGE = 7,
    WEAP_STATE_SPRINT = 8,
    WEAP_STATE_ATTACK = 9,
    WEAP_STATE_RELOAD = 10,
    WEAP_STATE_DISCARD = 11,
    WEAP_STATE_CUSTOM_ACTIVITY = 12,
    WEAP_STATE_RECHAMBER = 13,
    WEAP_STATE_TOSS = 14,
    WEAP_STATE_TOSS_PREP = 15,
    WEAP_STATE_TOSS_PREP_PULLOUT = 16,
    WEAP_STATE_TOSS_HOLD = 17,
    WEAP_STATE_TOSS_HOLD_SPRINTING = 18,
    WEAP_STATE_COOLDOWN = 19,
    WEAP_STATE_COUNT = 20,
};

struct FiredBulletInfo
{
    Vector3D destination;            // 0x0
    std::uint32_t passMods;          // 0xC
    Vector3D source;                 // 0x10
    std::uint32_t sourceEntity : 12; // 0x1C, bit 0
    std::uint32_t burstFire : 2;
    std::uint32_t fireDelay : 13;
    std::uint32_t hasTrace : 1;
    std::uint32_t reflected : 1;
    std::uint32_t hasImpact : 1;
    std::uint32_t impactSplash : 1;
    std::uint32_t : 1;
    std::uint32_t impactEntity : 12; // 0x20, bit 0
    std::uint32_t impactEffectFlags : 10;
    std::uint32_t impactSurface : 8;
    std::uint32_t : 2;
    Vector3D impactNormal; // 0x24
};

struct WeaponReloadInfo
{
    std::uint32_t sourceEntity : 12;
    std::uint32_t reloadTime : 10;
    std::uint32_t : 10;
};
