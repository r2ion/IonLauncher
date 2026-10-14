#pragma once

#include "mathlib/vector.h"

#include <cstddef>
#include <cstdint>

enum class ScriptContext : int;

struct MissilePathSettings
{
    float randomFloat;         // 0x00
    float drift_windiness;     // 0x04
    float drift_intensity;     // 0x08
    float straight_time_min;   // 0x0C
    float straight_time_max;   // 0x10
    float straight_radius_min; // 0x14
    float straight_radius_max; // 0x18
};

struct MissileSpiralSettings
{
    float timeScale;            // 0x00
    std::int32_t index;         // 0x04
    bool adsPressed;            // 0x08
    bool afterBurn;             // 0x09
    std::byte m_Padding0A[0x2]; // 0x0A
    float up[3];               // 0x0C
    float right[3];            // 0x18
};

struct MissilePathExpandContractSettings
{
    Vector3D launchOutVec;        // 0x00
    Vector3D launchInVec;         // 0x0C
    float launchOutTime;          // 0x18
    float launchInLerpTime;       // 0x1C
    float launchInTime;           // 0x20
    float launchStraightLerpTime; // 0x24
    Vector3D endPos;              // 0x28
    bool applyRandSpread;         // 0x34
    std::byte m_Padding35[0x3];   // 0x35
};

static_assert(sizeof(MissilePathSettings) == 0x1C);
static_assert(sizeof(MissileSpiralSettings) == 0x24);
static_assert(sizeof(MissilePathExpandContractSettings) == 0x38);
static_assert(offsetof(MissilePathExpandContractSettings, endPos) == 0x28);
static_assert(offsetof(MissilePathExpandContractSettings, applyRandSpread) == 0x34);

template <ScriptContext context> void ApplyPredictedMissileRandomDrift(MissilePathSettings& settings);
