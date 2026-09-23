#pragma once

#include <cstddef>
#include <cstdint>

#include "mathlib/vector.h"

class CPlayerState
{
  public:
    void* __vftable;                       // 0x00
    std::int32_t currentClass;             // 0x08
    std::int32_t requestedClass;           // 0x0C
    std::int32_t onDeathClass;             // 0x10
    std::int32_t oldClass;                 // 0x14
    QAngle a_angle;                        // 0x18
    std::byte _pad24[0x4];                 // 0x24
    const char* netname;                   // 0x28
    std::int32_t fixangle;                 // 0x30
    QAngle anglechange;                    // 0x34
    std::int32_t index;                    // 0x40
    bool replay;                           // 0x44
    std::byte _pad45[0x3];                 // 0x45
    std::int32_t lastPlayerView_tickcount; // 0x48
    Vector3D lastPlayerView_origin;        // 0x4C
    QAngle lastPlayerView_angle;           // 0x58
    bool deadflag;                         // 0x64
    std::byte _pad65[0x3];                 // 0x65
    QAngle localViewAngles;                // 0x68
    QAngle worldViewAngles;                // 0x74
};

static_assert(sizeof(CPlayerState) == 0x80);
