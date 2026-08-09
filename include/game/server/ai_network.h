#pragma once

#include "game/server/ai_node.h"
#include "engine/IEngineTrace.h"

#include <cstddef>
#include <cstdint>

class CAI_Network final : public IPartitionEnumerator
{
  public:
	std::int32_t m_iNumLinks; // +0x00008
	std::int32_t m_nUnk0; // +0x0000C
	CAI_HullData m_HullData[MAX_HULLS]; // +0x00010
	std::int32_t m_iNumZones[MAX_HULLS]; // +0x00088
	std::int32_t unk5; // +0x0009C, unk8 on disk
	char unk6[4]; // +0x000A0
	std::int32_t m_iNumHints; // +0x000A4
	std::int16_t m_Hints[2000]; // +0x000A8
	std::int32_t m_iNumScriptNodes; // +0x01048
	CAI_ScriptNode m_ScriptNodes[4000]; // +0x0104C
	std::int32_t m_iNumNodes; // +0x148CC
	CAI_Node** m_pAInode; // +0x148D0

  private:
	IterationRetval_t EnumElement(IHandleEntity* pHandleEntity) override;
};

static_assert(offsetof(CAI_Network, m_iNumLinks) == 0x8);
static_assert(offsetof(CAI_Network, m_nUnk0) == 0xC);
static_assert(offsetof(CAI_Network, m_HullData) == 0x10);
static_assert(offsetof(CAI_Network, m_iNumZones) == 0x88);
static_assert(offsetof(CAI_Network, unk5) == 0x9C);
static_assert(offsetof(CAI_Network, unk6) == 0xA0);
static_assert(offsetof(CAI_Network, m_iNumHints) == 0xA4);
static_assert(offsetof(CAI_Network, m_Hints) == 0xA8);
static_assert(offsetof(CAI_Network, m_iNumScriptNodes) == 0x1048);
static_assert(offsetof(CAI_Network, m_ScriptNodes) == 0x104C);
static_assert(offsetof(CAI_Network, m_iNumNodes) == 0x148CC);
static_assert(offsetof(CAI_Network, m_pAInode) == 0x148D0);
static_assert(sizeof(CAI_Network) == 0x148D8);
