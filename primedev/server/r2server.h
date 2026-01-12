#pragma once

#include "player.h"
#include "engine/inetchannel.h"
#include "engine/client.h"
#include "util/utlmemory.h"

#define MAX_PLAYERS 32
#define MAX_TEAMS 32

class CClientExtended;

class CBaseEntity;
extern CBaseEntity* (*Server_GetEntityByIndex)(int index);

class CServer : public IConnectionlessPacketHandler
{
public:
	inline CClientExtended* GetClientExtended(const int nIndex) { return &sm_ClientsExtended[nIndex]; }

	int32_t m_State; //0x0008
	int32_t m_Socket; //0x000C
	int32_t m_nTickCount; //0x0010
	bool m_bResetMaxTeams; //0x0014
	char m_szMapname[64]; //0x0015
	char m_szMapGroupName[64]; //0x0055
	char m_szPassword[32]; //0x0095 // unused, sv_password doesn't affect this
	char pad_00B5[3]; //0x00B5
	bool m_bUnknownBool; //0x00B8
	char pad_00B9[7]; //0x00B9
	uint32_t m_ClientDllCRC; //0x00C0
	char pad_00C4[4]; //0x00C4
	void *m_StringTables; //0x00C8
	void *m_pInstanceBaselineTable; //0x00D0
	void *m_pLightStyleTable; //0x00D8
	void *m_pUserInfoTable; //0x00E0
	void *m_pServerQueryTable; //0x00E8
	bool m_bApplyOverlays; //0x00F0
	bool m_bUpdateFrame; //0x00F1
	bool m_bUseReputation; //0x00F2
	bool m_bSimulating; //0x00F3
	char pad_00F4[4]; //0x00F4
	bf_write m_Signon; //0x00F8
	CUtlMemory<uint8_t> m_SignonBuffer; //0x0118
	int32_t m_nServerClasses; //0x0130
	int32_t m_nServerClassBits; //0x0134
	char m_szHostInfo[128]; //0x0138
	char pad_01B8[16]; //0x01B8
	float mysterious_Float; //0x01C8
	char pad_01CC[112]; //0x01CC
	float m_flStartTime; //0x023C
	int32_t m_iMaxPlayers; //0x0240
	int32_t m_iMaxTeams; //0x0244
	float m_flTickInterval; //0x0248
	float m_flTimescale; //0x024C
	CClient m_Clients[MAX_PLAYERS];
	void *N00003346; //0x5AE750
	char pad_5AE758[8]; //0x5AE758
	void *N00003348; //0x5AE760
	int32_t N00003349; //0x5AE768
	char pad_5AE76C[20]; //0x5AE76C
	float m_fCPUPercent; //0x5AE780
	float m_fStartTime; //0x5AE784
	float m_fLastCPUCheckTime; //0x5AE788
	bool m_bTeams[MAX_TEAMS];

	static CClientExtended sm_ClientsExtended[MAX_PLAYERS];
}; //Size: 0x5AF7E4

extern CServer* g_pServer;
