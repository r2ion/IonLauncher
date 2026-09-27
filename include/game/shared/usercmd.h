#pragma once

#include "mathlib/vector.h"

#include <cstddef>
#include <cstdint>

class CUserCmd
{
  public:
    int command_number;
    int tick_count;
    float command_time;
    QAngle viewangles;
    bool hasLocalViewAngles;
    std::byte pad_0019[3];
    QAngle localViewAngles;
    QAngle attackangles;
    float forwardmove;
    float sidemove;
    float upmove;
    std::uint32_t buttons;
    std::uint8_t impulse;
    std::byte pad_0045;
    std::int16_t weaponselect;
    std::int32_t meleetarget;
    std::uint8_t unknown_004C;
    std::byte pad_004D[3];
    std::uint32_t random_seed;
    bool respawnInputDebounce;
    bool queuePrimaryAttack;
    bool hasbeenpredicted;
    std::byte pad_0057;
    Vector3D playerEyePositionAfterLastPrediction;
    float playerFovAfterLastPrediction;
    QAngle headangles;
    Vector3D headoffset;
    Vector3D camerapos;
    QAngle cameraangles;
    bool trace_camera;
    std::byte pad_0099[3];
    int snapshot_start_tick;
    int snapshot_end_tick;
    std::uint32_t predicted_server_event_ack;
    std::uint32_t previous_predicted_server_event_ack;
    float frameTime;
    std::uint64_t creation_time_ms;
    Vector3D touchingSlipTriggersOrigin[6];
    Vector3D touchingSlipTriggersDirection[6];
};

static_assert(sizeof(CUserCmd) == 0x148);
