#pragma once

#include <cstddef>
#include <cstdint>

#define NUM_SERVER_FRAME_TIME_SCALES 24

class CClockDriftMgr
{
  public:
    float m_ClockOffsets[4];                                     // 0x0000
    std::int32_t m_iCurClockOffset;                              // 0x0010
    float m_serverFrameTimeScales[NUM_SERVER_FRAME_TIME_SCALES]; // 0x0014
    std::int32_t m_serverFrameTimeScaleIndex;                    // 0x0074
    float m_serverFrameTimeScaleAverage;                         // 0x0078
    float m_aheadBy;                                             // 0x007C
    float m_correctWithin;                                       // 0x0080
    std::uint32_t m_lastPlatTime;                                // 0x0084
    float m_lastServerTime;                                      // 0x0088
    std::int32_t m_nServerTick;                                  // 0x008C
    std::int32_t m_nClientTick;                                  // 0x0090
};

static_assert(sizeof(CClockDriftMgr) == 0x94);
