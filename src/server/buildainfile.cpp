#include "core/convar/convar.h"
#include "engine/hoststate.h"
#include "engine/r2engine.h"
#include "engine/bitvec.h"
#include "util/utlvector.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>

namespace fs = std::filesystem;

const int AINET_VERSION_NUMBER = 57;
const int AINET_SCRIPT_VERSION_NUMBER = 21;
const int PLACEHOLDER_CRC = 0;
const int MAX_HULLS = 5;

#pragma pack(push, 1)
struct CAI_NodeLink
{
	short srcId;
	short destId;
	bool hulls[MAX_HULLS];
	char unk0;
	char unk1; // maps => unk0 on disk
	char unk2[5];
	int64_t flags;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct CAI_NodeLinkDisk
{
	short srcId;
	short destId;
	char unk0;
	bool hulls[MAX_HULLS];
};
#pragma pack(pop)

enum NodeType_e // !TODO: unconfirmed for r1/r2/r5.
{
	NODE_ANY, // Used to specify any type of node (for search)
	NODE_DELETED, // Used in wc_edit mode to remove nodes during runtime
	NODE_GROUND,
	NODE_AIR,
	NODE_CLIMB,
	NODE_WATER
};

struct Vector3D
{
	float x, y, z;
};

#pragma pack(push, 1)
struct CAI_Node
{
	const Vector3D& GetOrigin() const { return m_vOrigin; }
	Vector3D& AccessOrigin() { return m_vOrigin; }
	float GetYaw() const { return m_flYaw; }

	int NumLinks() const { return m_Links.Count(); }
	void ClearLinks() { m_Links.Purge(); }
	CAI_NodeLink* GetLinkByIndex(int i) const { return m_Links[i]; }

	NodeType_e SetType(NodeType_e type) { return (m_eNodeType = type); }
	NodeType_e GetType() const { return m_eNodeType; }

	int SetInfo(int info) { return m_eNodeInfo = info; }
	int GetInfo() const { return m_eNodeInfo; }

	int m_iID; // ID for this node
	Vector3D m_vOrigin; // location of this node in space
	float m_flVOffset[MAX_HULLS]; // vertical offset for each hull type, assuming ground node, 0 otherwise
	float m_flYaw; // NPC on this node should face this yaw to face the hint, or climb a ladder

	NodeType_e m_eNodeType; // The type of node; always 2 in buildainfile.
	int m_eNodeInfo; // bits that tell us more about this nodes

	int unk2[MAX_HULLS]; // maps directly to unk2 in disk struct, despite being ints rather than shorts

	// view server.dll+393672 for context and death wish
	char unk3[MAX_HULLS]; // hell on earth, should map to unk3 on disk
	char pad[3]; // aligns next bytes
	float unk4[MAX_HULLS]; // i have no fucking clue, calculated using some kind of demon hell function float magic

	CUtlVector<CAI_NodeLink*> m_Links;
	int unk6; // should match up to unk4 on disk
	char unk7[16]; // padding until next bit
	int unk8; // should match up to unk5 on disk
	char unk9[4]; // padding until next bit
	char unk10[8]; // should match up to unk6 on disk
	char padTail[4]; // tail padding to match engine size (0xA8)
};
#pragma pack(pop)

static_assert(sizeof(CAI_Node) == 168);

#pragma pack(push, 1)
struct CAI_NodeOld
{
	int index; // not present on disk
	float x;
	float y;
	float z;
	float hulls[MAX_HULLS];
	float yaw;

	int unk0; // always 2 in buildainfile, maps directly to unk0 in disk struct
	int unk1; // maps directly to unk1 in disk struct
	int unk2[MAX_HULLS]; // maps directly to unk2 in disk struct, despite being ints rather than shorts

	// view server.dll+393672 for context and death wish
	char unk3[MAX_HULLS]; // hell on earth, should map to unk3 on disk
	char pad[3]; // aligns next bytes
	float unk4[MAX_HULLS]; // i have no fucking clue, calculated using some kind of demon hell function float magic

	CAI_NodeLink** links;
	char unk5[16];
	int linkcount;
	int unk11; // bad name lmao
	int unk6; // should match up to unk4 on disk
	char unk7[16]; // padding until next bit
	int unk8; // should match up to unk5 on disk
	char unk9[4]; // padding until next bit
	char unk10[8]; // should match up to unk6 on disk
};
#pragma pack(pop)

constexpr size_t kNodeUnk8Offset = offsetof(CAI_Node, unk8);
constexpr size_t kNodeOldUnk8Offset = offsetof(CAI_NodeOld, unk8);
constexpr size_t kNodeUnk9Offset = offsetof(CAI_Node, unk9);
constexpr size_t kNodeOldUnk9Offset = offsetof(CAI_NodeOld, unk9);
constexpr size_t kNodeUnk10Offset = offsetof(CAI_Node, unk10);
constexpr size_t kNodeOldUnk10Offset = offsetof(CAI_NodeOld, unk10);
static_assert(kNodeUnk8Offset == kNodeOldUnk8Offset);
static_assert(kNodeUnk9Offset == kNodeOldUnk9Offset);
static_assert(kNodeUnk10Offset == kNodeOldUnk10Offset);

// the way CAI_Nodes are represented in on-disk ain files
#pragma pack(push, 1)
struct CAI_NodeDisk
{
	// int m_iID; // ID for this node
	float x;
	float y;
	float z;
	float yaw;
	float hulls[MAX_HULLS];

	char unk0;
	int unk1;
	short unk2[MAX_HULLS];
	char unk3[MAX_HULLS];
	short unk4;
	short unk5;
	char unk6[8];
}; // total size of 68 bytes
#pragma pack(pop)

#pragma pack(push, 1)
struct UnkNodeStruct0
{
	int index;
	char unk0;
	char unk1; // maps to unk1 on disk
	char pad0[2]; // padding to +8

	float x;
	float y;
	float z;

	char pad5[4];
	int* unk2; // maps to unk5 on disk;
	char pad1[16]; // pad to +48
	int unkcount0; // maps to unkcount0 on disk

	char pad2[4]; // pad to +56
	int* unk3;
	char pad3[16]; // pad to +80
	int unkcount1;

	char pad4[128]; // pad to +0xD4
	float unk4; // derived from unk5 on load
	char unk5;
	char pad6[7];
};
#pragma pack(pop)

static_assert(sizeof(UnkNodeStruct0) == 0xE0);
static_assert(offsetof(UnkNodeStruct0, unk4) == 0xD4);
static_assert(offsetof(UnkNodeStruct0, unk5) == 0xD8);

int* pUnkStruct0Count;
UnkNodeStruct0*** pppUnkNodeStruct0s;

struct UnkLinkStruct1
{
	short unk0;
	short unk1;
	float unk2;
	char unk3;
	char unk4;
	char unk5;
	char pad0;
};
static_assert(sizeof(UnkLinkStruct1) == 12);

#pragma pack(push, 1)
struct UnkLinkStruct1Disk
{
	short unk0;
	short unk1;
	float unk2;
	char unk3;
	char unk4;
	char unk5;
};
#pragma pack(pop)

static_assert(sizeof(UnkLinkStruct1Disk) == 11);

int* pUnkLinkStruct1Count;
UnkLinkStruct1*** pppUnkStruct1s;

struct CAI_TraverseNode
{
	float m_Quat[4];
	int m_Index_MAYBE;
};

CUtlVector<CAI_TraverseNode>* g_pAITraverseNodes = nullptr;

#pragma pack(push, 1)
struct CAI_ScriptNode
{
	float x;
	float y;
	float z;
	uint64_t scriptdata;
};
#pragma pack(pop)

//=============================================================================
//>> CAI_HullData
//=============================================================================
struct CAI_HullData
{
	CVarBitVec m_bitVec;

	// Unknown, possible part of CVarBitVec ??? see [r5apex_ds + 1A52B0] if,
	// this is part of CVarBitVec, it seems to be unused in any of the
	// compiled CVarBitVec and CLargeVarBitVec methods so i think it should be
	// just part of this struct.
	char unk3[8];
};

struct CAI_Network;
struct CAI_NetworkManager;

class CAI_NetworkEditTools
{
public:
	//-----------------
	// WC Editing
	//-----------------
	int m_nNextWCIndex;
	Vector3D* m_pWCPosition;

	//-----------------
	// Debugging Tools
	//-----------------
	int m_debugNetOverlays;
	int* m_pNodeIndexTable;

	//-----------------
	// Network pointers
	//-----------------
	CAI_NetworkManager* m_pManager;
	CAI_Network* m_pNetwork;
};

#pragma pack(push, 1)
struct CAI_Network
{
	// +0
	void* m_pVTable; // <-- 'this'.
	// +8
	int linkcount; // this is uninitialised and never set on ain build, fun!
	int m_nUnk0;

	CAI_HullData m_HullData[MAX_HULLS];
	int m_iNumZones[MAX_HULLS]; // +0x0088

	// +156
	int unk5; // unk8 on disk
	// +160
	char unk6[4];
	// +164
	int hintcount;
	// +168
	short hints[2000]; // these probably aren't actually hints, but there's 1 of them per hint so idk
	// +4168
	int scriptnodecount;
	// +4172
	CAI_ScriptNode scriptnodes[4000];
	// +84172
	int nodecount;
	// +84176
	CAI_Node** nodes;
};
#pragma pack(pop)

static_assert(sizeof(CAI_Network) == 84184);

struct CAI_NetworkManager
{
	char unk0[2528];
	CAI_NetworkEditTools* m_pEditOps;
	CAI_Network* m_pNetwork;
};

ConVar* Cvar_ns_ai_dumpAINfileFromLoad;
__int64 (*sub_387F80)(__int64 a1, CAI_Network* a2);
CAI_NetworkManager** g_ppAINetworkManager = nullptr;

void DumpAINInfo(CAI_Network* aiNetwork)
{
	fs::path writePath(fmt::format("{}/maps/graphs", g_pModName));
	writePath /= g_pGlobals->m_pMapName;
	writePath += ".ain";

	// dump from memory
	spdlog::info("writing ain file {}", writePath.string());
	spdlog::info("");
	spdlog::info("");
	spdlog::info("");
	spdlog::info("");
	spdlog::info("");

	std::ofstream writeStream(writePath, std::ofstream::binary);
	spdlog::info("writing ainet version: {}", AINET_VERSION_NUMBER);
	writeStream.write((char*)&AINET_VERSION_NUMBER, sizeof(int));

	int mapVersion = g_pGlobals->m_nMapVersion;
	spdlog::info("writing map version: {}", mapVersion);
	writeStream.write((char*)&mapVersion, sizeof(int));
	spdlog::info("writing placeholder crc: {}", PLACEHOLDER_CRC);
	writeStream.write((char*)&PLACEHOLDER_CRC, sizeof(int));

	int calculatedLinkcount = 0;

	// path nodes
	spdlog::info("writing nodecount: {}", aiNetwork->nodecount);
	writeStream.write((char*)&aiNetwork->nodecount, sizeof(int));

	for (int i = 0; i < aiNetwork->nodecount; i++)
	{
		CAI_Node* aiNode = aiNetwork->nodes[i];
		// construct on-disk node struct
		CAI_NodeDisk diskNode;
		diskNode.x = aiNode->m_vOrigin.x;
		diskNode.y = aiNode->m_vOrigin.y;
		diskNode.z = aiNode->m_vOrigin.z;
		diskNode.yaw = aiNode->GetYaw();
		memcpy(diskNode.hulls, aiNode->m_flVOffset, sizeof(diskNode.hulls));
		diskNode.unk0 = static_cast<char>(aiNode->GetType());
		diskNode.unk1 = aiNode->GetInfo();

		for (int j = 0; j < MAX_HULLS; j++)
		{
			diskNode.unk2[j] = (short)aiNode->unk2[j];
			spdlog::info((short)aiNode->unk2[j]);
		}

		memcpy(diskNode.unk3, aiNode->unk3, sizeof(diskNode.unk3));
		diskNode.unk4 = static_cast<short>(aiNode->unk6);
		diskNode.unk5 = static_cast<short>(aiNode->unk8);
		memcpy(diskNode.unk6, aiNode->unk10, sizeof(diskNode.unk6));

		spdlog::info("writing node {} from {} to {:x}", aiNode->m_iID, (void*)aiNode, writeStream.tellp());
		writeStream.write((char*)&diskNode, sizeof(CAI_NodeDisk));

		calculatedLinkcount += aiNode->NumLinks();
	}

	// links
	spdlog::info("linkcount: {}", aiNetwork->linkcount);
	spdlog::info("calculated total linkcount: {}", calculatedLinkcount);

	calculatedLinkcount /= 2;
	if (Cvar_ns_ai_dumpAINfileFromLoad->GetBool())
	{
		if (aiNetwork->linkcount == calculatedLinkcount)
			spdlog::info("caculated linkcount is normal!");
		else
			spdlog::warn("calculated linkcount has weird value! this is expected on build!");
	}

	spdlog::info("writing linkcount: {}", calculatedLinkcount);
	writeStream.write((char*)&calculatedLinkcount, sizeof(int));

	for (int i = 0; i < aiNetwork->nodecount; i++)
	{
		const CAI_Node* aiNode = aiNetwork->nodes[i];
		for (int j = 0; j < aiNode->NumLinks(); j++)
		{
			// skip links that don't originate from current node
			const CAI_NodeLink* nodeLink = aiNode->GetLinkByIndex(j);
			if (nodeLink->srcId != aiNode->m_iID)
				continue;

			CAI_NodeLinkDisk diskLink;
			diskLink.srcId = nodeLink->srcId;
			diskLink.destId = nodeLink->destId;
			diskLink.unk0 = nodeLink->unk1;
			memcpy(diskLink.hulls, nodeLink->hulls, sizeof(diskLink.hulls));

			spdlog::info("writing link {} => {} to {:x}", diskLink.srcId, diskLink.destId, writeStream.tellp());
			writeStream.write((char*)&diskLink, sizeof(CAI_NodeLinkDisk));
		}
	}

	// WC lookup table (Hammer node IDs)
	spdlog::info("writing {:x} bytes for wc lookup table at {:x}", aiNetwork->nodecount * sizeof(uint32_t), writeStream.tellp());
	const CAI_NetworkEditTools* pEditOps = nullptr;
	if (g_ppAINetworkManager && *g_ppAINetworkManager)
		pEditOps = (*g_ppAINetworkManager)->m_pEditOps;

	if (pEditOps && pEditOps->m_pNodeIndexTable)
	{
		std::map<int, int> wcIDs;
		bool bCheckForProblems = false;
		for (int node = 0; node < aiNetwork->nodecount; node++)
		{
			const int nIndex = pEditOps->m_pNodeIndexTable[node];
			auto it = wcIDs.find(nIndex);
			if (it != wcIDs.end())
			{
				if (!bCheckForProblems)
				{
					spdlog::error("******* MAP CONTAINS DUPLICATE HAMMER NODE IDS! CHECK FOR PROBLEMS IN HAMMER TO CORRECT *******");
					bCheckForProblems = true;
				}
			}
			else
			{
				wcIDs[nIndex] = node;
			}
			writeStream.write((char*)(&nIndex), sizeof(int));
		}
	}
	else
	{
		uint32_t* unkNodeBlock = new uint32_t[aiNetwork->nodecount];
		memset(unkNodeBlock, 0, aiNetwork->nodecount * sizeof(uint32_t));
		writeStream.write((char*)unkNodeBlock, aiNetwork->nodecount * sizeof(uint32_t));
		delete[] unkNodeBlock;
	}

	// traverse nodes
	short traverseNodeCount = 0;
	if (g_pAITraverseNodes)
		traverseNodeCount = static_cast<short>(g_pAITraverseNodes->Count());

	spdlog::info("writing {} traversal nodes at {:x}...", traverseNodeCount, writeStream.tellp());
	writeStream.write((char*)&traverseNodeCount, sizeof(short));
	for (int i = 0; i < traverseNodeCount; i++)
	{
		const CAI_TraverseNode& traverseNode = (*g_pAITraverseNodes)[i];
		writeStream.write((char*)(&traverseNode.m_Quat), sizeof(float) * 4);
		writeStream.write((char*)(&traverseNode.m_Index_MAYBE), sizeof(int));
	}

	// hull data blocks
	spdlog::info("writing hull data blocks at {:x}", writeStream.tellp());
	for (int i = 0; i < MAX_HULLS; i++)
	{
		const CAI_HullData& hullData = aiNetwork->m_HullData[i];
		const int numHullZones = aiNetwork->m_iNumZones[i];

		const unsigned short numHullBits = (unsigned short)hullData.m_bitVec.GetNumBits();
		const unsigned short numHullInts = (unsigned short)hullData.m_bitVec.GetNumDWords();

		writeStream.write((char*)&numHullZones, sizeof(int));
		writeStream.write((char*)&numHullBits, sizeof(unsigned short));
		writeStream.write((char*)&numHullInts, sizeof(unsigned short));
		writeStream.write((char*)(hullData.m_bitVec.Base()), numHullInts * sizeof(int));
	}

	// unknown struct that's seemingly node-related
	spdlog::info("writing {} unknown node structs at {:x}", *pUnkStruct0Count, writeStream.tellp());
	writeStream.write((char*)pUnkStruct0Count, sizeof(*pUnkStruct0Count));
	for (int i = 0; i < *pUnkStruct0Count; i++)
	{
		spdlog::info("writing unknown node struct {} at {:x}", i, writeStream.tellp());
		UnkNodeStruct0* nodeStruct = (*pppUnkNodeStruct0s)[i];

		writeStream.write((char*)&nodeStruct->index, sizeof(nodeStruct->index));
		writeStream.write((char*)&nodeStruct->unk1, sizeof(nodeStruct->unk1));

		writeStream.write((char*)&nodeStruct->x, sizeof(nodeStruct->x));
		writeStream.write((char*)&nodeStruct->y, sizeof(nodeStruct->y));
		writeStream.write((char*)&nodeStruct->z, sizeof(nodeStruct->z));

		writeStream.write((char*)&nodeStruct->unkcount0, sizeof(nodeStruct->unkcount0));
		for (int j = 0; j < nodeStruct->unkcount0; j++)
		{
			short unk2Short = (short)nodeStruct->unk2[j];
			writeStream.write((char*)&unk2Short, sizeof(unk2Short));
		}

		writeStream.write((char*)&nodeStruct->unkcount1, sizeof(nodeStruct->unkcount1));
		for (int j = 0; j < nodeStruct->unkcount1; j++)
		{
			short unk3Short = (short)nodeStruct->unk3[j];
			writeStream.write((char*)&unk3Short, sizeof(unk3Short));
		}

		writeStream.write((char*)&nodeStruct->unk5, sizeof(nodeStruct->unk5));
	}

	// unknown struct that's seemingly link-related
	spdlog::info("writing {} unknown link structs at {:x}", *pUnkLinkStruct1Count, writeStream.tellp());
	writeStream.write((char*)pUnkLinkStruct1Count, sizeof(*pUnkLinkStruct1Count));
	for (int i = 0; i < *pUnkLinkStruct1Count; i++)
	{
		// disk and memory structs are literally identical here so just directly write
		spdlog::info("writing unknown link struct {} at {:x}", i, writeStream.tellp());
		writeStream.write((char*)(*pppUnkStruct1s)[i], sizeof(UnkLinkStruct1Disk));
	}

	// some weird int idk what this is used for
	writeStream.write((char*)&aiNetwork->unk5, sizeof(aiNetwork->unk5));

	// tf2-exclusive stuff past this point, i.e. ain v57 only
	spdlog::info("writing {} script nodes at {:x}", aiNetwork->scriptnodecount, writeStream.tellp());
	writeStream.write((char*)&aiNetwork->scriptnodecount, sizeof(aiNetwork->scriptnodecount));
	for (int i = 0; i < aiNetwork->scriptnodecount; i++)
	{
		// disk and memory structs are literally identical here so just directly write
		spdlog::info("writing script node {} at {:x}", i, writeStream.tellp());
		writeStream.write((char*)&aiNetwork->scriptnodes[i], sizeof(aiNetwork->scriptnodes[i]));
	}

	spdlog::info("writing {} hints at {:x}", aiNetwork->hintcount, writeStream.tellp());
	writeStream.write((char*)&aiNetwork->hintcount, sizeof(aiNetwork->hintcount));
	for (int i = 0; i < aiNetwork->hintcount; i++)
	{
		spdlog::info("writing hint data {} at {:x}", i, writeStream.tellp());
		writeStream.write((char*)&aiNetwork->hints[i], sizeof(aiNetwork->hints[i]));
	}

	writeStream.close();
}

static __int64(__fastcall* o_pCAI_NetworkBuilder__BuildPathPatrol)(void* builder, CAI_Network* aiNetwork) = nullptr;
static __int64 __fastcall h_CAI_NetworkBuilder__BuildPathPatrol(void* builder, CAI_Network* aiNetwork)
{
	__int64 result = o_pCAI_NetworkBuilder__BuildPathPatrol(builder, aiNetwork);
	if (sub_387F80)
		sub_387F80((__int64)builder, aiNetwork);
	return result;
}

static void(__fastcall* o_pCAI_NetworkBuilder__Build)(void* builder, CAI_Network* aiNetwork, void* unknown) = nullptr;
static void __fastcall h_CAI_NetworkBuilder__Build(void* builder, CAI_Network* aiNetwork, void* unknown)
{
	o_pCAI_NetworkBuilder__Build(builder, aiNetwork, unknown);

	DumpAINInfo(aiNetwork);
}

static void(__fastcall* o_pLoadAINFile)(void* aimanager, void* buf, const char* filename) = nullptr;
static void __fastcall h_LoadAINFile(void* aimanager, void* buf, const char* filename)
{
	o_pLoadAINFile(aimanager, buf, filename);

	if (Cvar_ns_ai_dumpAINfileFromLoad->GetBool())
	{
		spdlog::info("running DumpAINInfo for loaded file {}", filename);
		DumpAINInfo(*(CAI_Network**)((char*)aimanager + 2536));
	}
}

ON_DLL_LOAD("server.dll", BuildAINFile, (CModule module))
{
	o_pCAI_NetworkBuilder__BuildPathPatrol =
		module.Offset(0x387DC0).RCast<decltype(o_pCAI_NetworkBuilder__BuildPathPatrol)>();
	HookAttach(&(PVOID&)o_pCAI_NetworkBuilder__BuildPathPatrol, (PVOID)h_CAI_NetworkBuilder__BuildPathPatrol);

	o_pCAI_NetworkBuilder__Build = module.Offset(0x385E20).RCast<decltype(o_pCAI_NetworkBuilder__Build)>();
	HookAttach(&(PVOID&)o_pCAI_NetworkBuilder__Build, (PVOID)h_CAI_NetworkBuilder__Build);

	o_pLoadAINFile = module.Offset(0x3933A0).RCast<decltype(o_pLoadAINFile)>();
	HookAttach(&(PVOID&)o_pLoadAINFile, (PVOID)h_LoadAINFile);

	Cvar_ns_ai_dumpAINfileFromLoad = new ConVar(
		"ns_ai_dumpAINfileFromLoad", "0", FCVAR_NONE, "For debugging: whether we should dump ain data for ains loaded from disk");

	pUnkStruct0Count = module.Offset(0x1063BF8).RCast<int*>();
	pppUnkNodeStruct0s = module.Offset(0x1063BE0).RCast<UnkNodeStruct0***>();
	pUnkLinkStruct1Count = module.Offset(0x1063AA8).RCast<int*>();
	pppUnkStruct1s = module.Offset(0x1063A90).RCast<UnkLinkStruct1***>();
	g_pAITraverseNodes = module.Offset(0x10639C0).RCast<CUtlVector<CAI_TraverseNode>*>();
	sub_387F80 = module.Offset(0x387F80).RCast<decltype(sub_387F80)>();
	g_ppAINetworkManager = module.Offset(0x10613F8).RCast<CAI_NetworkManager**>();
}
