#pragma once

#include "mathlib/vector.h"
#include "tier1/bitvec.h"
#include "tier1/utlvector.h"

#include <cstddef>
#include <cstdint>

inline constexpr int MAX_HULLS = 5;

enum NodeType_e
{
	NODE_ANY,
	NODE_DELETED,
	NODE_GROUND,
	NODE_AIR,
	NODE_CLIMB,
	NODE_WATER,
};

struct CAI_NodeLink
{
	std::int16_t m_iSrcID;
	std::int16_t m_iDestID;
	std::uint8_t m_iAcceptedMoveTypes[MAX_HULLS];
	std::uint8_t m_LinkInfo;
	char unk1;
	char unk2[5];
	std::int64_t m_nFlags;
};

class CAI_Node
{
  public:
	const Vector3& GetOrigin() const { return m_vOrigin; }
	Vector3& AccessOrigin() { return m_vOrigin; }
	float GetYaw() const { return m_flYaw; }

	int NumLinks() const { return m_Links.Count(); }
	void ClearLinks() { m_Links.Purge(); }
	CAI_NodeLink* GetLinkByIndex(int i) const { return m_Links[i]; }

	NodeType_e SetType(NodeType_e type) { return (m_eNodeType = type); }
	NodeType_e GetType() const { return m_eNodeType; }

	int SetInfo(int info) { return m_eNodeInfo = info; }
	int GetInfo() const { return m_eNodeInfo; }

	std::int32_t m_iID;
	Vector3 m_vOrigin;
	float m_flVOffset[MAX_HULLS];
	float m_flYaw;
	NodeType_e m_eNodeType;
	std::int32_t m_eNodeInfo;
	std::int32_t unk2[MAX_HULLS];
	char unk3[MAX_HULLS];
	char pad[3];
	float unk4[MAX_HULLS];
	CUtlVector<CAI_NodeLink*> m_Links;
	std::int32_t unk6;
	char unk7[16];
	std::int32_t unk8;
	char unk9[4];
	char unk10[8];
	char padTail[4];
};

struct CAI_TraverseNode
{
	float m_Quat[4];
	std::int32_t m_Index_MAYBE;
};

struct CAI_ScriptNode
{
	Vector3 m_vOrigin;
	std::int32_t m_nMin;
	std::int32_t m_nMax;
};

struct CAI_HullData
{
	CVarBitVec m_bitVec;
	char unk3[8];
};

static_assert(sizeof(CAI_NodeLink) == 0x18);
static_assert(offsetof(CAI_NodeLink, m_nFlags) == 0x10);
static_assert(sizeof(CAI_Node) == 0xA8);
static_assert(offsetof(CAI_Node, m_Links) == 0x60);
static_assert(offsetof(CAI_Node, unk8) == 0x94);
static_assert(offsetof(CAI_Node, unk9) == 0x98);
static_assert(offsetof(CAI_Node, unk10) == 0x9C);
static_assert(sizeof(CAI_TraverseNode) == 0x14);
static_assert(sizeof(CAI_ScriptNode) == 0x14);
static_assert(offsetof(CAI_ScriptNode, m_nMin) == 0xC);
static_assert(sizeof(CAI_HullData) == 0x18);
