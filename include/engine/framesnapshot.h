#pragma once

#include <cstddef>
#include <cstdint>

struct CFrameSnapshot
{
	bool m_bInUse; // 0x0000
	std::uint8_t m_Pad0001[3];
	std::int32_t m_nLastEntity; // 0x0004
	std::int32_t m_nViewEntity; // 0x0008
	std::uint32_t m_nSpectatorReplayState; // 0x000C
	std::uint8_t m_Unknown0010; // 0x0010
	std::uint8_t m_Pad0011[3];
	std::uint32_t m_Unknown0014; // 0x0014
	std::uint32_t m_Unknown0018; // 0x0018
	std::uint8_t m_nActiveSplitScreenPlayerSlot; // 0x001C
	std::uint8_t m_Pad001D[3];
	std::int32_t m_nReplayTick; // 0x0020
	bool m_bCanProcessSignedOnLocalClientInput; // 0x0024
	bool m_bDeletePending; // 0x0025
	bool m_bEntitiesProcessed; // 0x0026
	std::uint8_t m_Pad0027;
	std::int32_t m_nDeltaTick; // 0x0028
	std::int32_t m_nServerTick; // 0x002C
	float m_flHostFrameTime; // 0x0030
	float m_flHostFrameTimeStdDeviation; // 0x0034
	bool m_bStruggling; // 0x0038
	std::uint8_t m_nServerCPU; // 0x0039
	std::uint8_t m_Pad003A[2];
	std::int32_t m_nCommandTick; // 0x003C
	CFrameSnapshot* m_pNextSnapshot; // 0x0040
	std::uint32_t m_nFirstPackedEntityOffset; // 0x0048
	std::uint32_t m_PackedEntityOffsets[4096]; // 0x004C
	std::uint8_t m_EntityDataBits[512]; // 0x404C
	std::uint8_t m_EntityPresentBits[512]; // 0x424C
	std::uint8_t m_EntityInterpolationBits[512]; // 0x444C
	std::uint8_t m_Pad464C[4];
};

static_assert(sizeof(CFrameSnapshot) == 0x4650);
static_assert(offsetof(CFrameSnapshot, m_bInUse) == 0x0000);
static_assert(offsetof(CFrameSnapshot, m_nLastEntity) == 0x0004);
static_assert(offsetof(CFrameSnapshot, m_nViewEntity) == 0x0008);
static_assert(offsetof(CFrameSnapshot, m_nSpectatorReplayState) == 0x000C);
static_assert(offsetof(CFrameSnapshot, m_Unknown0010) == 0x0010);
static_assert(offsetof(CFrameSnapshot, m_Unknown0014) == 0x0014);
static_assert(offsetof(CFrameSnapshot, m_Unknown0018) == 0x0018);
static_assert(offsetof(CFrameSnapshot, m_nActiveSplitScreenPlayerSlot) == 0x001C);
static_assert(offsetof(CFrameSnapshot, m_nReplayTick) == 0x0020);
static_assert(offsetof(CFrameSnapshot, m_bCanProcessSignedOnLocalClientInput) == 0x0024);
static_assert(offsetof(CFrameSnapshot, m_bDeletePending) == 0x0025);
static_assert(offsetof(CFrameSnapshot, m_bEntitiesProcessed) == 0x0026);
static_assert(offsetof(CFrameSnapshot, m_nDeltaTick) == 0x0028);
static_assert(offsetof(CFrameSnapshot, m_nServerTick) == 0x002C);
static_assert(offsetof(CFrameSnapshot, m_flHostFrameTime) == 0x0030);
static_assert(offsetof(CFrameSnapshot, m_flHostFrameTimeStdDeviation) == 0x0034);
static_assert(offsetof(CFrameSnapshot, m_bStruggling) == 0x0038);
static_assert(offsetof(CFrameSnapshot, m_nServerCPU) == 0x0039);
static_assert(offsetof(CFrameSnapshot, m_nCommandTick) == 0x003C);
static_assert(offsetof(CFrameSnapshot, m_pNextSnapshot) == 0x0040);
static_assert(offsetof(CFrameSnapshot, m_nFirstPackedEntityOffset) == 0x0048);
static_assert(offsetof(CFrameSnapshot, m_PackedEntityOffsets) == 0x004C);
static_assert(offsetof(CFrameSnapshot, m_EntityDataBits) == 0x404C);
static_assert(offsetof(CFrameSnapshot, m_EntityPresentBits) == 0x424C);
static_assert(offsetof(CFrameSnapshot, m_EntityInterpolationBits) == 0x444C);
