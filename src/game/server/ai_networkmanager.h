#pragma once

#include <cstddef>

class CAI_Network;
class Vector3D;
struct CAI_NetworkManager;

class CAI_NetworkEditTools
{
public:
	int m_nNextWCIndex;
	Vector3D* m_pWCPosition;
	int m_debugNetOverlays;
	int* m_pNodeIndexTable;
	CAI_NetworkManager* m_pManager;
	CAI_Network* m_pNetwork;
};

struct CAI_NetworkManager
{
	char unk0[2528];
	CAI_NetworkEditTools* m_pEditOps;
	CAI_Network* m_pNetwork;
};

static_assert(offsetof(CAI_NetworkManager, m_pEditOps) == 0x9E0);
static_assert(offsetof(CAI_NetworkManager, m_pNetwork) == 0x9E8);
