#pragma once

#include "inetchannel.h"
#include "inetmessage.h"
#include "irecipientfilter.h"

#include "shared/signonstate.h"
#include "engine/net_chan.h"
#include "engine/clockdriftmgr.h"
#include "engine/framesnapshot.h"
#include "engine/packed_entity.h"
#include "engine/client/community_party.h"
#include "engine/client/datablock_receiver.h"
#include "client_class.h"
#include "mathlib/vector.h"
#include "tier1/utlvector.h"
#include "tier1/mempool.h"
class CClientState;


extern char* g_pLocalPlayerUserID;
extern char* g_pLocalPlayerOriginToken;

using CClientState__SendStringCmd_t = void (__fastcall*)(CClientState* self, const char* command);
extern CClientState__SendStringCmd_t CClientState__SendStringCmd;

typedef bool (*CPlayer__IsMantling_t)(void* thisptr);
extern CPlayer__IsMantling_t CPlayer__IsMantling;
class SVC_Print;
class SVC_ServerInfo;
class CNetworkStringTableContainer;
class CNetworkStringTable;
class INetworkStringTable;
class SendTable;
class C_ServerClassInfo;
class SVC_SendTable;
class SVC_ClassInfo;
class SVC_SetPause;
class SVC_Playlists;
class SVC_CreateStringTable;
class SVC_UpdateStringTable;
class SVC_VoiceData;
class SVC_DurangoVoiceData;
class SVC_Sounds;
class SVC_FixAngle;
class SVC_CrosshairAngle;
class SVC_GrantClientSidePickup;
class SVC_UserMessage;
class SVC_Snapshot;
class SVC_TempEntities;
class SVC_ServerTick;
class SVC_Menu;
class SVC_CmdKeyValues;
class SVC_UseCachedPersistenceDefFile;
class SVC_PersistenceDefFile;
class SVC_PersistenceBaseline;
class SVC_PersistenceUpdateVar;
class SVC_PersistenceNotifySaved;
class SVC_DLCNotifyOwnership;
class SVC_MatchmakingStatus;
class SVC_PlaylistChange;
class SVC_SetTeam;
class SVC_RequestScreenshot;
class SVC_PlaylistOverrides;
class SVC_PlaylistPlayerCounts;
class SVC_NetProfileFrame;
class SVC_NetProfileTotals;


struct AddAngle
{
	float m_flTotal;
	float m_flStartTime;
};

struct CClientEventPackedData
{
	std::uint32_t m_nBits;
	std::uint32_t m_Padding0004;
	const std::uint8_t* m_pData;
};

class CEngineRecipientFilter : public IRecipientFilter
{
public:
	~CEngineRecipientFilter() override;
	bool IsReliable() const override;
	void MakeReliable() override;
	bool IsInitMessage() const override;
	int GetRecipientCount() const override;
	int GetRecipientIndex(int slot) const override;
	bool IsReplayMessage(int slot) const override;

	bool m_bInit;
	bool m_bReliable;
	std::uint8_t m_Padding000A[6];
	CUtlVector<std::int32_t> m_Recipients;
};

struct CClientEventInfo
{
	std::int16_t m_nClassId;
	std::uint8_t m_Padding0002[2];
	float m_flFireDelay;
	const SendTable* m_pSendTable;
	const ClientClass* m_pClientClass;
	CClientEventPackedData m_PackedData;
	std::int32_t m_nFlags;
	std::uint8_t m_Padding002C[4];
	CEngineRecipientFilter m_RecipientFilter;
};

struct CClientEventListNode
{
	CClientEventInfo m_Event;
	CClientEventListNode* m_pPrevious;
	CClientEventListNode* m_pNext;
};

struct CClientEventList
{
	void* m_pBlocks;
	std::intptr_t m_nAllocationCount;
	std::intptr_t m_nGrowSize;
	CClientEventListNode* m_pHead;
	CClientEventListNode* m_pTail;
	CClientEventListNode* m_pFirstFree;
	std::intptr_t m_nElementCount;
	std::intptr_t m_nAllocatedCount;
	void* m_pLastAllocationBlock;
	std::intptr_t m_nLastAllocationIndex;
	CClientEventListNode* m_pElements;
};


typedef CClientState* (*GetBaseLocalClientType)();
extern GetBaseLocalClientType GetBaseLocalClient;

typedef int (*GetLocalPlayerIndexType)();
extern GetLocalPlayerIndexType GetLocalPlayerIndex;


class CClientSnapshotManager
{
public:
	CFrameSnapshot* m_Frames; // 0x0008; follows the implicit vtable pointer
	CUtlMemoryPool m_ClientFramePool; // 0x0010

	CClientSnapshotManager();
	virtual ~CClientSnapshotManager();
};

static_assert(sizeof(CClientSnapshotManager) == 0x40);
static_assert(offsetof(CClientSnapshotManager, m_Frames) == 0x8);
static_assert(offsetof(CClientSnapshotManager, m_ClientFramePool) == 0x10);


class IServerMessageHandler : public INetMessageHandler
{
public:
	virtual ~IServerMessageHandler() = default;
	virtual bool ProcessPrint(SVC_Print* message) = 0;
	virtual bool ProcessServerInfo(SVC_ServerInfo* message) = 0;
	virtual bool ProcessSendTable(SVC_SendTable* message) = 0;
	virtual bool ProcessClassInfo(SVC_ClassInfo* message) = 0;
	virtual bool ProcessSetPause(SVC_SetPause* message) = 0;
	virtual bool ProcessPlaylists(SVC_Playlists* message) = 0;
	virtual bool ProcessCreateStringTable(SVC_CreateStringTable* message) = 0;
	virtual bool ProcessUpdateStringTable(SVC_UpdateStringTable* message) = 0;
	virtual bool ProcessVoiceData(SVC_VoiceData* message) = 0;
	virtual bool ProcessDurangoVoiceData(SVC_DurangoVoiceData* message) = 0;
	virtual bool ProcessSounds(SVC_Sounds* message) = 0;
	virtual bool ProcessFixAngle(SVC_FixAngle* message) = 0;
	virtual bool ProcessCrosshairAngle(SVC_CrosshairAngle* message) = 0;
	virtual bool ProcessGrantClientSidePickup(SVC_GrantClientSidePickup* message) = 0;
	virtual bool ProcessUserMessage(SVC_UserMessage* message) = 0;
	virtual bool ProcessSnapshot(SVC_Snapshot* message) = 0;
	virtual bool ProcessTempEntities(SVC_TempEntities* message) = 0;
	virtual bool ProcessServerTick(SVC_ServerTick* message) = 0;
	virtual bool ProcessMenu(SVC_Menu* message) = 0;
	virtual bool ProcessCmdKeyValues(SVC_CmdKeyValues* message) = 0;
	virtual bool ProcessUseCachedPersistenceDefFile(SVC_UseCachedPersistenceDefFile* message) = 0;
	virtual bool ProcessPersistenceDefFile(SVC_PersistenceDefFile* message) = 0;
	virtual bool ProcessPersistenceBaseline(SVC_PersistenceBaseline* message) = 0;
	virtual bool ProcessPersistenceUpdateVar(SVC_PersistenceUpdateVar* message) = 0;
	virtual bool ProcessPersistenceNotifySaved(SVC_PersistenceNotifySaved* message) = 0;
	virtual bool ProcessDLCNotifyOwnership(SVC_DLCNotifyOwnership* message) = 0;
	virtual bool ProcessMatchmakingStatus(SVC_MatchmakingStatus* message) = 0;
	virtual bool ProcessPlaylistChange(SVC_PlaylistChange* message) = 0;
	virtual bool ProcessSetTeam(SVC_SetTeam* message) = 0;
	virtual bool ProcessRequestScreenshot(SVC_RequestScreenshot* message) = 0;
	virtual bool ProcessPlaylistOverrides(SVC_PlaylistOverrides* message) = 0;
	virtual bool ProcessPlaylistPlayerCounts(SVC_PlaylistPlayerCounts* message) = 0;
	virtual bool ProcessNetProfileFrame(SVC_NetProfileFrame* message) = 0;
	virtual bool ProcessNetProfileTotals(SVC_NetProfileTotals* message) = 0;
};

#pragma pack(push, 4)


class CClientState : public INetChannelHandler, public IConnectionlessPacketHandler, public IServerMessageHandler, public CClientSnapshotManager
{
public:
	int32_t m_Socket; // 0x0058
	std::uint8_t m_Padding005C[4]; // 0x005C
	CNetChan* m_NetChannel; // 0x0060
	double m_flConnectTime; // 0x0068
	int32_t m_nRetryNumber; // 0x0070
	int32_t m_nChallengeRetryLimit; // 0x0074
	bool m_bConnectionEncrypted; // 0x0078
	std::uint8_t m_Padding0079[3]; // 0x0079
	netadr_t m_RemoteAddress; // 0x007C
	bool m_bChallengeRequest; // 0x0094
	bool m_bSendChallengeRequest; // 0x0095
	bool m_bReconnectWithNetParams; // 0x0096
	uint8_t m_connectionState; // 0x0097
	eSignonState m_nSignonState; // 0x0098
	std::uint8_t m_Padding009C[4]; // 0x009C
	double m_flNextCmdTime; // 0x00A0
	int32_t m_nServerCount; // 0x00A8
	int32_t m_nInSequenceNr; // 0x00AC
	float m_flClockDriftFrameTime; // 0x00B0
	CClockDriftMgr m_ClockDriftMgr; // 0x00B4
	bool m_bCanProcessLocalClientInput; // 0x0148
	bool m_bPendingPredictionUpdate; // 0x0149
	std::uint8_t m_Padding014A[2]; // 0x014A
	int32_t m_nDeltaTick; // 0x014C
	int32_t m_nStringTableAckTick; // 0x0150
	int32_t m_nProcessedDeltaTick; // 0x0154
	int32_t m_nProcessedStringTableAckTick; // 0x0158
	bool m_bPendingTicksAvailable; // 0x015C
	std::uint8_t m_Padding015D[3]; // 0x015D
	bool m_bPaused; // 0x0160
	uint8_t m_tickState; // 0x0161
	uint16_t m_tickFlags; // 0x0162
	int32_t m_nTickState; // 0x0164
	int32_t m_nViewEntity; // 0x0168
	uint32_t m_nPlayerSlot; // 0x016C
	int32_t m_nSplitScreenSlot; // 0x0170
	char m_szLevelFileName[64]; // 0x0174
	char m_szLevelBaseName[64]; // 0x01B4
	char m_szLastLevelBaseName[64]; // 0x01F4
	char m_szSkyBoxBaseName[64]; // 0x0234
	bool m_bInMpLobbyMenu; // 0x0274
	std::uint8_t m_Padding0275[3]; // 0x0275
	int32_t m_nTeam; // 0x0278
	int32_t m_nMaxClients; // 0x027C
	std::uint8_t m_Reserved0280[4]; // 0x0280
	int32_t m_nNumPlayersToConnect; // 0x0284
	float m_flTickTime; // 0x0288
	float m_flOldTickTime; // 0x028C
	bool m_bSignonChallengeReceived; // 0x0290
	std::uint8_t m_Padding0291[3]; // 0x0291
	std::uint32_t m_nChallenge; // 0x0294
	netadr_t m_ChallengeAddress; // 0x0298
	bool m_bUseLocalSendTableFile; // 0x02B0
	uint8_t m_sendTableState; // 0x02B1
	uint16_t m_sendTableFlags; // 0x02B2
	uint32_t m_nSendTableState; // 0x02B4
	C_ServerClassInfo* m_pServerClasses; // 0x02B8
	int32_t m_nServerClasses; // 0x02C0
	int32_t m_nServerClassBits; // 0x02C4
	char m_szEncryptionKey[0x800]; // 0x02C8
	std::uint32_t m_nEncryptionKeySize; // 0x0AC8
	std::uint8_t m_Padding0ACC[4]; // 0x0ACC
	CNetworkStringTableContainer* m_StringTableContainer; // 0x0AD0
	int32_t m_nPersistenceVersion; // 0x0AD8
	int32_t m_nPersistenceState; // 0x0ADC
	uint8_t m_PersistenceData[61430]; // 0x0AE0
	uint16_t m_nPersistenceDataSize; // 0xFAD6
	bool m_bPersistenceBaselineReceived; // 0xFAD8
	std::uint8_t m_PaddingFAD9[3]; // 0xFAD9
	std::int32_t m_nPersistenceBaselineVersion; // 0xFADC
	std::int32_t m_nPersistenceNotifySavedCount; // 0xFAE0
	bool m_bRestrictServerCommands; // 0xFAE4
	bool m_bRestrictClientCommands; // 0xFAE5
	char m_szServerAddress[1024]; // 0xFAE6
	std::uint8_t m_PaddingFEE6[2]; // 0xFEE6
	ClientDataBlockReceiver m_DataBlockReceiver; // 0xFEE8
	bool m_bClientRequestedDisconnect; // 0xFF28
	char m_szErrorMessage[512]; // 0xFF29
	std::uint8_t m_Padding10129[3]; // 0x10129
	int32_t m_nTimeSinceLastUserCmd; // 0x1012C
	CFrameSnapshot* m_pPrevFrameSnapshot; // 0x10130
	CFrameSnapshot* m_pPendingFrameSnapshot; // 0x10138
	CFrameSnapshot* m_pCurrentFrameSnapshot; // 0x10140
	int32_t m_nServerTick; // 0x10148
	int32_t m_nPreviousServerTick; // 0x1014C
	bool m_bClockCorrectionEnabled; // 0x10150
	bool m_bReceivedFullEntityPacket; // 0x10151
	bool m_bResetFrameSnapshots; // 0x10152
	std::uint8_t m_Padding10153; // 0x10153
	std::int32_t m_nPendingServerTick; // 0x10154
	int32_t m_nClientTick; // 0x10158
	float m_flFrameTime; // 0x1015C
	float m_flPreviousFrameTime; // 0x10160
	int32_t m_nOutgoingCommandNumber; // 0x10164
	int32_t m_nCurrentMovementSequence; // 0x10168
	int32_t m_nCommandAck; // 0x1016C
	int32_t m_nLastCommandAck; // 0x10170
	int32_t m_nServerCommandAck; // 0x10174
	bool m_bPredictionState; // 0x10178
	std::uint8_t m_Padding10179[3]; // 0x10179
	int32_t m_nPredictionTick; // 0x1017C
	int32_t m_nPredictionFrame; // 0x10180
	float m_flServerUptime; // 0x10184
	bool m_bIsWatchingReplay; // 0x10188
	std::uint8_t m_Padding10189[3]; // 0x10189
	int32_t m_nReplayState; // 0x1018C
	bool m_bIsSpectatorReplay; // 0x10190
	bool m_bIsReplayRoundWinning; // 0x10191
	std::uint8_t m_Padding10192[2]; // 0x10192
	std::int32_t m_nReplayRoundWinningPlayerIndex; // 0x10194
	std::int32_t m_nReplayPlayerIndex; // 0x10198
	std::uint8_t m_nActiveSplitScreenPlayerSlot; // 0x1019C
	bool m_bCanProcessSignedOnLocalClientInput; // 0x1019D
	std::uint8_t m_Padding1019E[2]; // 0x1019E
	std::uint32_t m_nServerMapCRC; // 0x101A0
	QAngle m_ViewAngles; // 0x101A4
	std::uint32_t m_nViewOriginOverrideSequence; // 0x101B0
	Vector3 m_ViewOriginOverride; // 0x101B4
	std::int32_t m_nViewOriginOverrideValue; // 0x101C0
	std::int32_t m_nViewOriginOverrideStateId; // 0x101C4
	std::uint32_t m_nViewOriginOverrideActivationTick; // 0x101C8
	std::uint8_t m_Padding101CC[4]; // 0x101CC
	CUtlVector<AddAngle> m_AddedAngles; // 0x101D0
	float m_flAddedAngleTotal; // 0x101F0
	float m_flPreviousAddedAngleTotal; // 0x101F4
	bool m_bLocalClientViewStateEnabled; // 0x101F8
	std::uint8_t m_Padding101F9[7]; // 0x101F9
	ClientPartyState m_PartyState; // 0x10200
	std::uint8_t m_Reserved13188[0x10]; // 0x13188
	CClientEventList m_Events; // 0x13198
	std::int32_t m_nDemoNumber; // 0x131F0
	char m_szDemoFileNames[32]; // 0x131F4
	std::uint8_t m_Padding13214[4]; // 0x13214
	std::uint8_t m_Reserved13218[0x3E0]; // 0x13218
	CNetworkStringTable* m_pModelPrecacheTable; // 0x135F8
	CNetworkStringTable* m_pDecalPrecacheTable; // 0x13600
	CNetworkStringTable* m_pInstanceBaselineTable; // 0x13608
	CNetworkStringTable* m_pLightStyleTable; // 0x13610
	CNetworkStringTable* m_pUserInfoTable; // 0x13618
	CNetworkStringTable* m_pServerQueryInfoTable; // 0x13620
	PackedEntity m_EntityBaselines[2048]; // 0x13628
	bool m_bEntityBaselinesInitialized; // 0x1B628
	std::uint8_t m_Reserved1B629[0xB]; // 0x1B629

	CClientState();
	bool ConnectInternal(const char* publicAddress, bool reservedConnectFlag, bool challengeRequest,
		int playerCount, const char* joinType);
	void Disconnect(bool sendTrackingContext);
	bool ProcessEntitySnapshots();
	int DecodeSnapshotEntities(CFrameSnapshot* fromSnapshot, CFrameSnapshot* toSnapshot);
	bool IsPaused() const;
	bool ProcessSignonStateInternal(eSignonState state, int serverCount, NET_SignonState* message);


	inline void SendStringCmd(const char* command) { CClientState__SendStringCmd(this, command); }

	~CClientState() override;
	bool ConnectionStart(CNetChan* channel) override;
	void ConnectionClosing(const char* reason, int reputation) override;
	void ConnectionCrashed(const char* reason) override;
	void PacketStart(int incomingSequence, int outgoingAcknowledged) override;
	void PacketEnd() override;
	virtual void FileRequested(const char* fileName, unsigned int transferId);
	virtual void Clear();
	virtual bool ConnectSplitScreen(const char* publicAddress, bool silent, int playerCount, const char* joinType);
	virtual bool ConnectWithJoinType(const char* publicAddress, const char* joinType);
	virtual bool Connect(const char* publicAddress);
	virtual void SetConnectionEncrypted();
	virtual void CheckForResend(bool force);
	virtual bool IsConnectedToServer(const char* serverAddress);
	virtual bool LinkClasses();
	virtual int GetChallengeRetryLimit();
	virtual const char* GetPlatformName();

	bool ProcessConnectionlessPacket(netpacket_t* packet) override;

	bool ProcessStringCmd(NET_StringCmd* message) override;
	bool ProcessSetConVar(NET_SetConVar* message) override;
	bool ProcessSignonState(NET_SignonState* message) override;
	bool ProcessPrint(SVC_Print* message) override;
	bool ProcessServerInfo(SVC_ServerInfo* message) override;
	bool ProcessSendTable(SVC_SendTable* message) override;
	bool ProcessClassInfo(SVC_ClassInfo* message) override;
	bool ProcessSetPause(SVC_SetPause* message) override;
	bool ProcessPlaylists(SVC_Playlists* message) override;
	bool ProcessCreateStringTable(SVC_CreateStringTable* message) override;
	bool ProcessUpdateStringTable(SVC_UpdateStringTable* message) override;
	bool ProcessVoiceData(SVC_VoiceData* message) override;
	bool ProcessDurangoVoiceData(SVC_DurangoVoiceData* message) override;
	bool ProcessSounds(SVC_Sounds* message) override;
	bool ProcessFixAngle(SVC_FixAngle* message) override;
	bool ProcessCrosshairAngle(SVC_CrosshairAngle* message) override;
	bool ProcessGrantClientSidePickup(SVC_GrantClientSidePickup* message) override;
	bool ProcessUserMessage(SVC_UserMessage* message) override;
	bool ProcessSnapshot(SVC_Snapshot* message) override;
	bool ProcessTempEntities(SVC_TempEntities* message) override;
	bool ProcessServerTick(SVC_ServerTick* message) override;
	bool ProcessMenu(SVC_Menu* message) override;
	bool ProcessCmdKeyValues(SVC_CmdKeyValues* message) override;
	bool ProcessUseCachedPersistenceDefFile(SVC_UseCachedPersistenceDefFile* message) override;
	bool ProcessPersistenceDefFile(SVC_PersistenceDefFile* message) override;
	bool ProcessPersistenceBaseline(SVC_PersistenceBaseline* message) override;
	bool ProcessPersistenceUpdateVar(SVC_PersistenceUpdateVar* message) override;
	bool ProcessPersistenceNotifySaved(SVC_PersistenceNotifySaved* message) override;
	bool ProcessDLCNotifyOwnership(SVC_DLCNotifyOwnership* message) override;
	bool ProcessMatchmakingStatus(SVC_MatchmakingStatus* message) override;
	bool ProcessPlaylistChange(SVC_PlaylistChange* message) override;
	bool ProcessSetTeam(SVC_SetTeam* message) override;
	bool ProcessRequestScreenshot(SVC_RequestScreenshot* message) override;
	bool ProcessPlaylistOverrides(SVC_PlaylistOverrides* message) override;
	bool ProcessPlaylistPlayerCounts(SVC_PlaylistPlayerCounts* message) override;
	bool ProcessNetProfileFrame(SVC_NetProfileFrame* message) override;
	bool ProcessNetProfileTotals(SVC_NetProfileTotals* message) override;
};
#pragma pack(pop)
static_assert(sizeof(AddAngle) == 0x8);
static_assert(sizeof(CClientEventPackedData) == 0x10);
static_assert(offsetof(CClientEventPackedData, m_pData) == 0x8);
static_assert(sizeof(CEngineRecipientFilter) == 0x30);
static_assert(offsetof(CEngineRecipientFilter, m_Recipients) == 0x10);
static_assert(sizeof(CClientEventInfo) == 0x60);
static_assert(offsetof(CClientEventInfo, m_flFireDelay) == 0x4);
static_assert(offsetof(CClientEventInfo, m_pSendTable) == 0x8);
static_assert(offsetof(CClientEventInfo, m_pClientClass) == 0x10);
static_assert(offsetof(CClientEventInfo, m_PackedData) == 0x18);
static_assert(offsetof(CClientEventInfo, m_nFlags) == 0x28);
static_assert(offsetof(CClientEventInfo, m_RecipientFilter) == 0x30);
static_assert(sizeof(CClientEventListNode) == 0x70);
static_assert(offsetof(CClientEventListNode, m_pPrevious) == 0x60);
static_assert(offsetof(CClientEventListNode, m_pNext) == 0x68);
static_assert(sizeof(CClientEventList) == 0x58);
static_assert(offsetof(CClientEventList, m_pHead) == 0x18);
static_assert(offsetof(CClientEventList, m_pTail) == 0x20);
static_assert(offsetof(CClientEventList, m_pFirstFree) == 0x28);
static_assert(offsetof(CClientEventList, m_nElementCount) == 0x30);
static_assert(offsetof(CClientEventList, m_nAllocatedCount) == 0x38);
static_assert(offsetof(CClientEventList, m_pLastAllocationBlock) == 0x40);
static_assert(offsetof(CClientEventList, m_nLastAllocationIndex) == 0x48);
static_assert(offsetof(CClientEventList, m_pElements) == 0x50);

static_assert(sizeof(CClientState) == 0x1B634);
static_assert(offsetof(CClientState, m_Socket) == 0x58);
static_assert(offsetof(CClientState, m_NetChannel) == 0x60);
static_assert(offsetof(CClientState, m_nSignonState) == 0x98);
static_assert(offsetof(CClientState, m_szLevelBaseName) == 0x1B4);
static_assert(offsetof(CClientState, m_szEncryptionKey) == 0x2C8);
static_assert(sizeof(((CClientState*)nullptr)->m_szEncryptionKey) == 0x800);
static_assert(offsetof(CClientState, m_nEncryptionKeySize) == 0xAC8);
static_assert(offsetof(CClientState, m_StringTableContainer) == 0xAD0);
static_assert(offsetof(CClientState, m_szServerAddress) == 0xFAE6);
static_assert(offsetof(CClientState, m_DataBlockReceiver) == 0xFEE8);
static_assert(offsetof(CClientState, m_flServerUptime) == 0x10184);
static_assert(offsetof(CClientState, m_bIsReplayRoundWinning) == 0x10191);
static_assert(offsetof(CClientState, m_nReplayRoundWinningPlayerIndex) == 0x10194);
static_assert(offsetof(CClientState, m_nReplayPlayerIndex) == 0x10198);
static_assert(offsetof(CClientState, m_nActiveSplitScreenPlayerSlot) == 0x1019C);
static_assert(offsetof(CClientState, m_bCanProcessSignedOnLocalClientInput) == 0x1019D);
static_assert(offsetof(CClientState, m_nServerMapCRC) == 0x101A0);
static_assert(offsetof(CClientState, m_ViewAngles) == 0x101A4);
static_assert(offsetof(CClientState, m_nViewOriginOverrideSequence) == 0x101B0);
static_assert(offsetof(CClientState, m_ViewOriginOverride) == 0x101B4);
static_assert(offsetof(CClientState, m_nViewOriginOverrideValue) == 0x101C0);
static_assert(offsetof(CClientState, m_nViewOriginOverrideStateId) == 0x101C4);
static_assert(offsetof(CClientState, m_nViewOriginOverrideActivationTick) == 0x101C8);
static_assert(offsetof(CClientState, m_AddedAngles) == 0x101D0);
static_assert(offsetof(CClientState, m_flAddedAngleTotal) == 0x101F0);
static_assert(offsetof(CClientState, m_flPreviousAddedAngleTotal) == 0x101F4);
static_assert(offsetof(CClientState, m_bLocalClientViewStateEnabled) == 0x101F8);
static_assert(offsetof(CClientState, m_PartyState) == 0x10200);
static_assert(offsetof(CClientState, m_Events) == 0x13198);
static_assert(offsetof(CClientState, m_nDemoNumber) == 0x131F0);
static_assert(offsetof(CClientState, m_szDemoFileNames) == 0x131F4);
static_assert(offsetof(CClientState, m_pModelPrecacheTable) == 0x135F8);
static_assert(offsetof(CClientState, m_pDecalPrecacheTable) == 0x13600);
static_assert(offsetof(CClientState, m_pInstanceBaselineTable) == 0x13608);
static_assert(offsetof(CClientState, m_pLightStyleTable) == 0x13610);
static_assert(offsetof(CClientState, m_pUserInfoTable) == 0x13618);
static_assert(offsetof(CClientState, m_pServerQueryInfoTable) == 0x13620);
static_assert(offsetof(CClientState, m_EntityBaselines) == 0x13628);
static_assert(offsetof(CClientState, m_bEntityBaselinesInitialized) == 0x1B628);
