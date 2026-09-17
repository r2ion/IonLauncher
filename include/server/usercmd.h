#pragma once

#include <cstddef>
#include <cstdint>

#include "mathlib/vector.h"

class alignas(4) SV_CUserCmd
{
  public:
    std::uint32_t command_number;
    std::uint32_t tick_count;
    float command_time;
    Vector3D worldViewAngles;
    std::byte gap18[4];
    Vector3D localViewAngles;
    Vector3D attackangles;
    Vector3D move;
    std::uint32_t buttons;
    std::uint8_t impulse;
    std::int16_t weaponselect;
    std::uint32_t meleetarget;
    bool unk;
    bool unkownBool;
    bool unknownBool2;
    bool unknownBool3;
    std::uint32_t random_seed;
    bool unknownBool54;
    bool unknownBool55;
    bool unknownBool56;
    bool unknownBool57;
    Vector3D headAngles;
    Vector3D headOffset;
    Vector3D cameraPos;
    Vector3D cameraAngles;
    std::byte gap88[4];
    int tickSomething;
    std::uint32_t dword90;
    std::uint32_t predictedServerEventAck;
    std::uint32_t dword98;
    float frameTime;
    std::byte reservedA0[0x98];
};

static_assert(sizeof(SV_CUserCmd) == 0x138);
static_assert(alignof(SV_CUserCmd) == 4);
static_assert(offsetof(SV_CUserCmd, command_time) == 0x8);
static_assert(offsetof(SV_CUserCmd, attackangles) == 0x28);
static_assert(offsetof(SV_CUserCmd, buttons) == 0x40);
static_assert(offsetof(SV_CUserCmd, tickSomething) == 0x8C);
static_assert(offsetof(SV_CUserCmd, dword90) == 0x90);
static_assert(offsetof(SV_CUserCmd, frameTime) == 0x9C);
