#pragma once

#include <cstddef>
#include <cstdint>

enum class GameMode_t : int
{
    NO_MODE = 0,
    MP_MODE,
    SP_MODE,
};

class CGlobalVarsBase
{
  public:
    double realtime;                        // 0x00
    int framecount;                         // 0x08
    float absoluteframetime;                // 0x0C
    float curtime;                          // 0x10
    float lastSnapTime;                     // 0x14
    float currentSnapTime;                  // 0x18
    float futureSnapTime;                   // 0x1C
    float snapLerp;                         // 0x20
    float lastInterpolationTime;            // 0x24
    float latestPredictedTime;              // 0x28
    float replayDelay;                      // 0x2C
    float frametime;                        // 0x30
    int maxClients;                         // 0x34
    GameMode_t gameType;                    // 0x38
    std::uint32_t tickcount;                // 0x3C
    float interval_per_tick;                // 0x40
    std::uint8_t m_Reserved0044[0x1C];       // 0x44
};

static_assert(sizeof(CGlobalVarsBase) == 0x60);
static_assert(alignof(CGlobalVarsBase) == 0x8);
static_assert(offsetof(CGlobalVarsBase, currentSnapTime) == 0x18);
static_assert(offsetof(CGlobalVarsBase, futureSnapTime) == 0x1C);
static_assert(offsetof(CGlobalVarsBase, snapLerp) == 0x20);
static_assert(offsetof(CGlobalVarsBase, lastInterpolationTime) == 0x24);
static_assert(offsetof(CGlobalVarsBase, replayDelay) == 0x2C);

