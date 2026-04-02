#pragma once

#include "engine/inetchannel.h"
#include "shared/signonstate.h"
#include "engine/net_chan.h"
#include "engine/datablock_receiver.h"

extern char* g_pLocalPlayerUserID;
extern char* g_pLocalPlayerOriginToken;

typedef void (__fastcall* CClientState__SendStringCmd_t)(const char*);
extern CClientState__SendStringCmd_t CClientState__SendStringCmd;

typedef bool (*CPlayer__IsMantling_t)(void* thisptr);
extern CPlayer__IsMantling_t CPlayer__IsMantling;
class CClientState;

typedef CClientState* (*GetBaseLocalClientType)();
extern GetBaseLocalClientType GetBaseLocalClient;

typedef int (*GetLocalPlayerIndexType)();
extern GetLocalPlayerIndexType GetLocalPlayerIndex;

class CUtlMemoryPool
{
public:
	int32_t N0000005C; //0x0000
	int32_t N00000152; //0x0004
	int32_t N0000005D; //0x0008
	int32_t N00000155; //0x000C
	int32_t N0000005E; //0x0010
	uint16_t N00000158; //0x0014
	uint16_t N0000015A; //0x0016
	char *N00000063; //0x0018
	void *N00000064; //0x0020
	void *N00000065; //0x0028
}; //Size: 0x0030

class CClientSnapshotManager
{
public:
	void *m_Frames; //0x0000 CClientSnaphopManager::m_Frames
	class CUtlMemoryPool m_ClientFramePool; //0x0008

	virtual ~CClientSnapshotManager();
}; //Size: 0x0038

class CClockDriftMgr
{
public:
	float N00000182; //0x0000 r5sdk_field_0_0
	float N0000017E; //0x0004 r5sdk_field_0_1
	float N000001CD; //0x0008 r5sdk_field_0_2
	float N0000017F; //0x000C r5sdk_field_0_3
	int32_t N000001D0; //0x0010 r5sdk_field_1
	float N0000006B; //0x0014 m_ClockOffsets
	float N000001D3; //0x0018
	float N0000006C; //0x001C
	float N000001D6; //0x0020
	float N0000006D; //0x0024
	float N000001D9; //0x0028
	float N0000006E; //0x002C
	float N000001E1; //0x0030
	float N0000006F; //0x0034
	float N000001E3; //0x0038
	float N00000178; //0x003C
	float N000000EE; //0x0040
	float N000001E6; //0x0044
	float N00000173; //0x0048
	float N00000170; //0x004C
	float N00000070; //0x0050
	float N000001EA; //0x0054
	float N00000071; //0x0058
	float N000001EC; //0x005C
	float N00000072; //0x0060
	float N000001EE; //0x0064
	float N00000073; //0x0068
	float N000001F0; //0x006C
	float N00000074; //0x0070
	float N000001F2; //0x0074
	float N00000075; //0x0078
	float N000001F4; //0x007C
	float N00000076; //0x0080
	float N000001F6; //0x0084
	float m_flServerTickTime; //0x0088
	int32_t m_nServerTick; //0x008C
	int32_t m_nClientTick; //0x0090
}; //Size: 0x0094

class ClientDataBlockReceiver : public NetDatablockReceiver
{
public:
	virtual ~ClientDataBlockReceiver();
};


// total travesty, still needs to be adjusted because live compiler reaction idfk
class CClientState : public CClientSnapshotManager, public INetChannelHandler, public IConnectionlessPacketHandler
{
public:
	int32_t m_Socket; //0x0058
	CNetChan *m_NetChannel; //0x0060 m_NetChannel
	double m_flConnectTime; //0x0068 unk_while_connecting
	int m_nRetryNumber; //0x0070 unk_true_on_connecting_and_connected
	int m_nChallengeRetryLimit;
	char encrypted_connection_MAYBE;
	netadr_t addr;
	bool m_bUnk_used_during_auth;
	char m_bSendChallengeRequest;
	char m_bDoNetParamsReconnect_MAYBE;
	char field_97;
	int unkmaybepadidk;
	int usedinconnectionlesspacketM;
	int __padding;
	eSignonState m_nSignonState; //0x0098 m_nSignonState
	double m_flNextCmdTime; //0x00A0 m_flNextCmdTime
	int32_t m_nServerCount; //0x00A8 m_nServerCount
	int32_t m_nInSequenceNr; //0x00AC m_nInSequenceNr
	float m_flClockDriftFrameTime; //0x00B0 m_flClockDriftFrameTime
	class CClockDriftMgr m_ClockDriftMgr; //0x00B4
	char pad_0148[4]; //0x0148
	int32_t m_nDeltaTick; //0x014C m_nDeltaTick
	int32_t m_nStringTableAckTick; //0x0150 m_nStringTableAckTick
	int32_t m_nProcessedDeltaTick; //0x0154 m_nProcessedDeltaTick
	int32_t m_nProcessedStringTableAckTick; //0x0158 m_nProcessedStringTableAckTick
	bool m_bPendingTicksAvailable; //0x015C m_bPendingTicksAvailable
	char N00000252[1];
	bool m_bPaused;
	uint8_t unk_byte; //0x0161 gets set in sub_8C7B0 in engine.dll
	uint16_t N0000023C; //0x0162
	uint32_t N00000257; //0x0166
	int32_t N00000259; //0x0168
	char m_szLevelFileName[64]; //0x0174
	char m_szLevelBaseName[64]; //0x01B4
	char m_szLastLevelBaseName[64]; //0x01F4
	char m_szSkyBoxBaseName_unused[64]; //0x0234
	bool m_bInMpLobbyMenu; //0x0274 m_bInMpLobbyMenu
	char pad_0275[3]; //0x0275
	int32_t m_nTeam; //0x0278 m_nTeam
	int32_t m_nMaxClients; //0x027C m_nMaxClients
	char pad_0280[4]; //0x0280
	int m_nNumPlayersToConnect; //0x0284
	float m_flTickTime; //0x0288 m_flTickTime
	float m_flOldTickTime; //0x028C m_flOldTickTime
	char pad_0290[4]; //0x0290
	uint8_t m_bSignonChallengeReceived; //0x0294 m_bSignonChallengeReceived
	uint32_t N00000265; //0x0295
	char pad_0299[20]; //0x0299
	bool m_bUseLocalSendTableFile; //0x02B0 m_bUseLocalSendTableFile
	uint8_t N0000029F; //0x02B1
	uint16_t N000002D6; //0x02B2
	uint32_t N000002DA; //0x02B4
	void *m_pServerClasses; //0x02B8
	int32_t m_nServerClasses; //0x02C0
	int32_t m_nServerClassBits; //0x02C4
	char pad_02C8[2056]; //0x02C8
	void *m_StringTableContainer; //0x0AD0
	int32_t m_PersistenceVersion; //0x0AD8
	int32_t N0000ACC7; //0x0ADC
	char m_PersistenceData[61430]; //0x0AE0
	uint16_t N0000DCD6; //0xFAD6
	bool unk_bool; //0xFAD8
	char pad_FADE[13]; //0xFADE
	char m_szServerAddress[1024]; //0xFAE6
	ClientDataBlockReceiver m_DataBlockReceiver; //0xFEE6
	char m_szErrorMessage[512]; //0xFF28
	uint32_t pad_fish; //0x10128
	int32_t m_nTimeSinceLastUserCmd; //0x1012C
	void *m_PrevFrameSnapshot; //0x10130
	void *m_SomeOtherFrameSnapshot; //0x10138
	void *m_CurrFrameSnapshot; //0x10140
	int32_t m_nServerTick; //0x10148
	int32_t m_nSomeTick; //0x1014C not sure about this one
	char pad_10150[8]; //0x10150
	int32_t m_nSomeOtherTick; //0x10158 also not sure about this one
	float m_flFrameTime; //0x1015C
	float m_flPreviousFrameTime_maybe; //0x10160
	int32_t m_nOutgoingCommandNr; //0x10164
	int32_t N00012BBB; //0x10168
	int32_t N0000DD15; //0x1016C
	int32_t N00012BDB; //0x10170
	int32_t N0000DD16; //0x10174 these are all related to whatever m_nOutgoingCommandNr is
	bool N00012BDE; //0x10178
	char pad_10179[3]; //0x10179
	int32_t N0000DD17; //0x1017C
	int pad_10180; //0x10180
	float m_flServerUptime; //0x10184
	bool m_bIsWatchingReplay;
	int unk_probably_replay_related_or_pad;
	bool m_bIsSpectatorReplay;

	inline void SendStringCmd(const char* cmd) { CClientState__SendStringCmd(cmd); }

	virtual ~CClientState();
	virtual bool ProcessStringCmd(void* msg);
	virtual bool ProcessSetConVar(void* msg);
	virtual bool ProcessSignonState(void* msg);
	virtual bool ProcessPrint(void* msg);
	virtual bool ProcessServerInfo(void* msg);
	virtual bool ProcessSendTable(void* msg);
	virtual bool ProcessClassInfo(void* msg);
	virtual bool ProcessSetPause(void* msg);
	virtual bool ProcessPlaylists(void* msg);
	virtual bool ProcessCreateStringTable(void* msg);
	virtual bool ProcessUpdateStringTable(void* msg);
	virtual bool ProcessVoiceData(void* msg);
	virtual bool sub_1A0720(); // dunno what this does
	virtual bool ProcessFixAngles(void* msg);
	virtual bool ProcessCrosshairAngle(void* msg);
	virtual bool sub_1A0840(); // does clientdll stuff, cba to figure out what message this is
	virtual bool ProcessUserMessage(void* msg);
	virtual bool FinishSignonState_New(void* msg); // not confident on this one
	virtual bool ProcessTempEntities(void* msg);
	virtual bool sub_1A08A0(); // another func that returns 1
	virtual bool sub_19F3F0(); // same as above
	virtual bool notsurewhatmessagethisis(void* msg);
	virtual bool ProcessUseCachedPersistenceDefFile(void* msg);
	virtual bool ProcessPersistenceDefFile(void* msg);
	virtual bool ProcessPersistenceBaseline(void* msg);
	virtual bool ProcessPersistenceUpdateVar(void* msg);
	virtual bool ProcessPersistenceNotifySaved(void* msg);
	virtual bool ProcessDLCNotifyOwnership(void* msg);
	virtual bool ProcessMatchmakingStatus(void* msg);
	virtual bool ProcessPlaylistChange(void* msg);
	virtual bool ProcessSetTeam(void* msg);
	virtual bool ProcessRequestScreenshot(void* msg);
	virtual bool ProcessPlaylistOverrides(void* msg);
	virtual bool ProcessPlaylistPlayerCounts(void* msg);
	virtual bool ProcessNetProfileFrame(void* msg);
	virtual bool ProcessNetProfileTotals(void* msg);
};
