#pragma once

#include <cstddef>
#include <cstdint>

class CClockDriftMgr
{
public:
	static constexpr int NUM_CLOCKDRIFT_SAMPLES = 24;

	float m_flClockData[4]; // 0x0000
	std::int32_t m_nClockDataState; // 0x0010
	float m_flClockOffsets[NUM_CLOCKDRIFT_SAMPLES]; // 0x0014
	std::int32_t m_nCurrentClockOffset; // 0x0074
	float m_flClockDifference; // 0x0078
	float m_flClockDifferencePrevious; // 0x007C
	std::int32_t m_nSimulationTick; // 0x0080
	float m_flClientTickTime; // 0x0084
	float m_flServerTickTime; // 0x0088
	std::int32_t m_nServerTick; // 0x008C
	std::int32_t m_nClientTick; // 0x0090
};

static_assert(sizeof(CClockDriftMgr) == 0x94);
static_assert(offsetof(CClockDriftMgr, m_flClockData) == 0x0);
static_assert(offsetof(CClockDriftMgr, m_flClockOffsets) == 0x14);
static_assert(offsetof(CClockDriftMgr, m_nCurrentClockOffset) == 0x74);
static_assert(offsetof(CClockDriftMgr, m_nServerTick) == 0x8C);
static_assert(offsetof(CClockDriftMgr, m_nClientTick) == 0x90);
