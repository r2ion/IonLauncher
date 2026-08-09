#pragma once

#include "inetchannel.h"
#include "inetmessage.h"
#include "tier1/keyvalues.h"
#include "tier1/utlvector.h"
#include "engine/server/datablock_sender.h"
#include "shared/signonstate.h"

#include <cstddef>
#include <cstdint>

class CClient;
class CClientExtended;

using CClientDisconnectFn = void (*)(CClient* pClient, std::uint32_t reputation, const char* reason, ...);
using CClientSendDataBlockFn = void (*)(CClient* pClient, bf_write* pMessage);

extern CClientDisconnectFn CClient__Disconnect;
extern CClientSendDataBlockFn CClient__SendDataBlock;

using ClientEdictHandle_t = std::uint16_t;

enum Reputation_t
{
	REP_NONE = 0,
	REP_REMOVE_ONLY,
	REP_MARK_BAD
};

// #56169 $DB69 PData size
// #512   $200	Trailing data
// #100	  $64	Safety buffer
const int PERSISTENCE_MAX_SIZE = 0xDDCD;

// note: NOT_READY and READY are the only entries we have here that are defined by the vanilla game
// entries after this are custom and used to determine the source of persistence, e.g. whether it is local or remote
enum class ePersistenceReady : std::int32_t
{
	NOT_READY,
	READY = 3,
	READY_INSECURE = 3,
	READY_REMOTE
};

struct Spike_t
{
	char m_szDesc[64]; // 0x0000
	std::int32_t m_nBits; // 0x0040
};

class CNetworkStatTrace
{
public:
	std::int32_t m_nMinWarningBytes; // 0x0000
	std::int32_t m_nStartBit; // 0x0004
	std::int32_t m_nCurBit; // 0x0008
	std::uint32_t m_Padding0C; // 0x000C
	CUtlVector<Spike_t> m_Records; // 0x0010
};

struct CClientUtlBuffer
{
	using OverflowFn = bool (*)(CClientUtlBuffer* buffer, std::int64_t size);

	std::uint8_t* m_pMemory; // 0x0000
	std::int64_t m_nAllocationCount; // 0x0008
	std::int64_t m_nGrowSize; // 0x0010
	std::int64_t m_nGet; // 0x0018
	std::int64_t m_nPut; // 0x0020
	std::uint8_t m_nError; // 0x0028
	std::uint8_t m_nFlags; // 0x0029
	std::uint8_t m_Reserved2A; // 0x002A
	std::uint8_t m_Padding2B; // 0x002B
	std::int32_t m_nTab; // 0x002C
	std::int64_t m_nMaxPut; // 0x0030
	std::int64_t m_nOffset; // 0x0038
	OverflowFn m_GetOverflowFn; // 0x0040
	OverflowFn m_PutOverflowFn; // 0x0048
	std::uint32_t m_nByteSwapFlags; // 0x0050
	std::uint8_t m_Padding54[4]; // 0x0054
};

struct CClientSnapshotMessageStorage
{
	static constexpr std::size_t MessageBufferSize = 0xF000;

	std::uint8_t m_TempEntitiesData[MessageBufferSize]; // 0x00000
	std::uint8_t m_PacketEntitiesData[MessageBufferSize]; // 0x0F000
	std::uint32_t m_Reserved1E000; // 0x1E000
};

static_assert(sizeof(Spike_t) == 0x44);
static_assert(offsetof(Spike_t, m_nBits) == 0x40);
static_assert(sizeof(CNetworkStatTrace) == 0x30);
static_assert(offsetof(CNetworkStatTrace, m_nMinWarningBytes) == 0x0);
static_assert(offsetof(CNetworkStatTrace, m_Records) == 0x10);
static_assert(sizeof(CClientUtlBuffer) == 0x58);
static_assert(offsetof(CClientUtlBuffer, m_nGet) == 0x18);
static_assert(offsetof(CClientUtlBuffer, m_nPut) == 0x20);
static_assert(offsetof(CClientUtlBuffer, m_nMaxPut) == 0x30);
static_assert(offsetof(CClientUtlBuffer, m_GetOverflowFn) == 0x40);
static_assert(offsetof(CClientUtlBuffer, m_PutOverflowFn) == 0x48);
static_assert(offsetof(CClientUtlBuffer, m_nByteSwapFlags) == 0x50);
static_assert(sizeof(CClientSnapshotMessageStorage) == 0x1E004);
static_assert(offsetof(CClientSnapshotMessageStorage, m_PacketEntitiesData) == 0xF000);
static_assert(offsetof(CClientSnapshotMessageStorage, m_Reserved1E000) == 0x1E000);



class CClient : public INetChannelHandler, public IClientMessageHandler
{
public:
	CClient();
	ClientEdictHandle_t GetHandle() const { return m_nHandle; }
	bool IsFakePlayer() const { return m_bFakePlayer; }
	uint32_t GetUserID() const { return m_nUserID; }
	CClientExtended* GetClientExtended(void) const;

	std::uint32_t m_nUserID; // 0x0010
	ClientEdictHandle_t m_nHandle; // 0x0014
	char m_szClientName[256]; // 0x0016
	char m_szOriginalName[256]; // 0x0116
	std::uint8_t m_Padding216[2]; // 0x0216
	std::int32_t m_nCommandTick; // 0x0218
	bool m_bGoodReputation; // 0x021C
	std::uint8_t m_Padding21D[3]; // 0x021D
	std::uint64_t m_nCommunityDataHandle; // 0x0220
	std::uint32_t m_nCommunityDataState; // 0x0228
	float m_flCommunityXpRate; // 0x022C
	float m_flServerTime; // 0x0230
	std::uint8_t m_Reserved234[0x10]; // 0x0234
	std::int32_t m_nRate; // 0x0244
	std::uint8_t m_Reserved248[8]; // 0x0248
	std::int32_t m_iTeamNum; // 0x0250
	std::uint8_t m_Padding254[4]; // 0x0254
	KeyValues* m_ConVars; // 0x0258
	bool m_bConVarsChanged; // 0x0260
	bool m_bSendServerInfo; // 0x0261
	bool m_bSendSignonData; // 0x0262
	bool m_bFullStateAchieved; // 0x0263
	std::uint8_t m_Padding264[4]; // 0x0264
	void* m_pServer; // 0x0268
	float m_flReplayDelay; // 0x0270
	bool m_bReplayEnabled; // 0x0274
	bool m_bReplayRequested; // 0x0275
	std::uint8_t m_Padding276[2]; // 0x0276
	std::int32_t m_nReplaySnapshotTick; // 0x0278
	std::int32_t m_nReplaySnapshotSequence; // 0x027C
	bool m_bReplayOnly; // 0x0280
	bool m_bReplayState; // 0x0281
	bool m_bKickedByFairFight; // 0x0282
	std::uint8_t m_Padding283; // 0x0283
	std::uint32_t m_nSendTableCRC; // 0x0284
	std::int32_t m_nServerCount; // 0x0288
	std::uint8_t m_Padding28C[4]; // 0x028C
	CNetChan* m_NetChannel; // 0x0290
	std::uint8_t m_Reserved298[8]; // 0x0298
	eSignonState m_nSignonState; // 0x02A0
	bool m_bGameDllClientActivated; // 0x02A4
	std::uint8_t m_Padding2A5[7]; // 0x02A5
	std::int32_t m_nDeltaTick; // 0x02AC
	std::int32_t m_nStringTableAckTick; // 0x02B0
	std::int32_t m_nSignonTick; // 0x02B4
	std::int32_t m_nBaselineUpdateTick; // 0x02B8
	std::int32_t m_nLoadingProgress; // 0x02BC
	char m_szRegion[64]; // 0x02C0
	char m_szPlatformDescription[16]; // 0x0300
	bool m_bCommunityDataValid; // 0x0310
	std::uint8_t m_Padding311[3]; // 0x0311
	std::uint32_t m_nCommunityUserId; // 0x0314
	char m_szCommunityName[64]; // 0x0318
	char m_szClanTag[16]; // 0x0358
	bool m_bHappyHour; // 0x0368
	char m_szFaction[16]; // 0x0369
	std::uint8_t m_Padding379[3]; // 0x0379
	std::int32_t m_nBaselineUsed; // 0x037C
	std::uint8_t m_BaselinesSent[0x100]; // 0x0380
	std::int32_t m_nForceWaitForTick; // 0x0480
	bool m_bFakePlayer; // 0x0484
	bool m_bReceivedPacket; // 0x0485
	bool m_bLowViolence; // 0x0486
	bool m_bFullyAuthenticated; // 0x0487
	std::int32_t m_nSendState; // 0x0488
	std::int32_t m_nNextTick; // 0x048C
	float m_flNextMessageTime; // 0x0490
	std::int32_t m_nUpdateIntervalTicks; // 0x0494
	float m_flUpdateRate; // 0x0498
	std::int32_t m_nPersistenceState; // 0x049C
	ePersistenceReady m_iPersistenceReady; // 0x04A0
	std::uint8_t m_Reserved4A4[0x56]; // 0x04A4
	std::uint8_t m_PersistenceBuffer[PERSISTENCE_MAX_SIZE]; // 0x04FA
	std::uint8_t m_ReservedE2C7[0x1235]; // 0xE2C7
	std::uint32_t m_nPersistenceStorageState; // 0xF4FC
	char m_szPlatformID[32]; // 0xF500
	std::uint8_t m_ReservedF520[0x60]; // 0xF520
	std::uint32_t m_nPersistenceBaselineState; // 0xF580
	bool m_bPersistenceBaselineSent; // 0xF584
	std::uint8_t m_PaddingF585[3]; // 0xF585
	std::uint32_t m_nPersistenceRequestHandle; // 0xF588
	std::uint8_t m_PaddingF58C[4]; // 0xF58C
	CClientUtlBuffer m_PersistenceSerializationBuffer; // 0xF590
	CClientSnapshotMessageStorage m_SnapshotMessageStorage; // 0xF5E8
	bool m_bPersistenceBufferValid; // 0x2D5EC
	std::uint8_t m_Padding2D5ED[3]; // 0x2D5ED
	std::uint32_t m_nPersistenceBufferSize; // 0x2D5F0
	bool m_bPersistenceSaveRequested; // 0x2D5F4
	std::uint8_t m_Padding2D5F5[3]; // 0x2D5F5
	std::uint32_t m_DlcOwnershipBits[4]; // 0x2D5F8
	std::int32_t m_nPartyChangeNumber; // 0x2D608
	bool m_bInClientPrintf; // 0x2D60C
	std::uint8_t m_Padding2D60D[3]; // 0x2D60D
	float m_flClientConnectedTime; // 0x2D610
	std::uint32_t m_Padding2D614; // 0x2D614
	CNetworkStatTrace m_Trace; // 0x2D618
	std::uint64_t m_nPlatformUserId; // 0x2D648
	ServerDataBlock m_ServerDataBlock; // 0x2D650
	bool m_bVoiceLoopback; // 0x2D6F8
	std::uint8_t m_Padding2D6F9[3]; // 0x2D6F9
	std::uint32_t m_VoiceStreams; // 0x2D6FC
	std::int32_t m_nLastPacketTick; // 0x2D700
	float m_flReplaySnapshotTime; // 0x2D704
	std::int32_t m_nLastMovementTick; // 0x2D708
	ClientEdictHandle_t m_nSnapshotClientHandle; // 0x2D70C
	std::uint8_t m_Padding2D70E[2]; // 0x2D70E
	std::uint32_t m_nSnapshotSequence; // 0x2D710
	std::int32_t m_nSnapshotDeltaTick; // 0x2D714
	std::uint32_t m_nSnapshotBaselineSequence; // 0x2D718
	std::int32_t m_nSnapshotBaselineTick; // 0x2D71C
	std::int32_t m_nLastSnapshotSequence; // 0x2D720
	std::uint8_t m_Padding2D724[4]; // 0x2D724

	void ActivatePlayer();
	void Clear();
	void ClientPrintf(const char* format, ...);
	bool Connect(const char* name, CNetChan* channel, bool fakePlayer, void* userInfo,
		char* disconnectReason, int disconnectReasonSize);
	void FillSignOnFullServerInfo(NET_SignonState* state);
	bool FillUserInfo(void* playerInfo) const;
	const char* GetClientName() const;
	const char* GetAddressString() const;
	bool UpdatePartyChangeNumber();
	const char* GetPartySub() const;
	std::uint64_t GetPlatformUserId() const;
	bool SendServerData();
	int GetRate() const;
	const char* GetStryderSecurity() const;
	const char* GetUserSetting(const char* name) const;
	void Inactivate();
	void Init(int clientSlot, void* server);
	bool IsCommandAllowed(const void* command) const;
	bool IsHearingClient(int senderIndex) const;
	bool IsHumanPlayer() const;
	bool CanSendPacket() const;
	bool IsTracing() const;
	void ForceFullUpdate();
	void PerformDisconnection(const char* reason, bool silent);
	bool IsPersistenceReady() const;
	void NotifyGameDLLConnected();
	void SpawnPlayer();
	bool ProcessSignonStateMsg(eSignonState requestedState);
	bool Reconnect();
	void SavePlaylistShuffleVars(const void* shuffleState);
	bool SendNetMsgEx(INetMessage* message, bool replayOnly, bool forceReliable, bool voice);
	bool SendNetMsg(INetMessage* message, bool forceReliable, bool voice);
	void SendReplaySnapshotData(const void* snapshot);
	bool SendServerInfo();
	bool SendSignonState();
	bool SendSignonData();
	int SendSnapshot(void* snapshot, bool* snapshotOverflowed);
	void SetName(const char* name);
	void ApplyCommunityData(std::uint32_t communityId, const char* communityName,
		const char* clanTag, bool happyHour, float xpRate, const char* faction);
	void SetServerDataBlockSize(int size);
	void SetPlatformUserId(std::uint64_t platformUserId);
	void SetServerDataBlockTick(int tick);
	void SetSignonState(eSignonState state);
	void SetUpdateRate(float updateRate);
	void SetTeam(int team);
	void SetTraceThreshold(int thresholdBits);
	void SetUserCVar(const char* name, const char* value);
	bool ShouldSendMessages();
	void TraceNetworkData(bf_write* message, const char* format, ...);
	void TraceNetworkMsg(int bitCount, const char* format, ...);
	bool CheckSendServerData();
	bool UpdateAcknowledgedFramecount(int sequence, int tick);
	void SendPendingDataBlock();
	void UpdateSendState();
	void UpdateUserSettings();
	bool HasDeltaFrame() const;
	void SendPendingFixAngle();

	void Disconnect(const Reputation_t nRepLevel, const char* reason, ...);

	inline void SendDataBlock(bf_write* msg) { CClient__SendDataBlock(this, msg); }

	~CClient() override;
	bool ConnectionStart(CNetChan* channel) override;
	void ConnectionClosing(const char* reason, int reputation) override;
	void ConnectionCrashed(const char* reason) override;
	void PacketStart(int incomingSequence, int outgoingAcknowledged) override;
	void PacketEnd() override;

	bool ProcessStringCmd(NET_StringCmd* message) override;
	bool ProcessSetConVar(NET_SetConVar* message) override;
	bool ProcessSignonState(NET_SignonState* message) override;
	bool ProcessClientInfo(CLC_ClientInfo* message) override;
	bool ProcessMove(CLC_Move* message) override;
	bool ProcessVoiceData(CLC_VoiceData* message) override;
	bool ProcessDurangoVoiceData(CLC_DurangoVoiceData* message) override;
	bool ProcessFileCRCCheck(CLC_FileCRCCheck* message) override;
	bool ProcessLoadingProgress(CLC_LoadingProgress* message) override;
	bool ProcessPersistenceRequestSave(CLC_PersistenceRequestSave* message) override;
	bool ProcessPersistenceClientToken(CLC_PersistenceClientToken* message) override;
	bool ProcessSetClientEntitlements(CLC_SetClientEntitlements* message) override;
	bool ProcessSetPlaylistVarOverride(CLC_SetPlaylistVarOverride* message) override;
	bool ProcessClaimClientSidePickup(CLC_ClaimClientSidePickup* message) override;
	bool ProcessClientSayText(CLC_ClientSayText* message) override;
	bool ProcessClientTick(CLC_ClientTick* message) override;
	bool ProcessCmdKeyValues(CLC_CmdKeyValues* message) override;
	bool ProcessScreenshot(CLC_Screenshot* message) override;
	bool ProcessPINTelemetryData(CLC_PINTelemetryData* message) override;
};

static_assert(sizeof(CClient) == 0x2D728);
static_assert(offsetof(CClient, m_nUserID) == 0x10);
static_assert(offsetof(CClient, m_szClientName) == 0x16);
static_assert(offsetof(CClient, m_nCommandTick) == 0x218);
static_assert(offsetof(CClient, m_nCommunityDataHandle) == 0x220);
static_assert(offsetof(CClient, m_nRate) == 0x244);
static_assert(offsetof(CClient, m_iTeamNum) == 0x250);
static_assert(offsetof(CClient, m_ConVars) == 0x258);
static_assert(offsetof(CClient, m_bConVarsChanged) == 0x260);
static_assert(offsetof(CClient, m_pServer) == 0x268);
static_assert(offsetof(CClient, m_flReplayDelay) == 0x270);
static_assert(offsetof(CClient, m_bReplayEnabled) == 0x274);
static_assert(offsetof(CClient, m_nReplaySnapshotTick) == 0x278);
static_assert(offsetof(CClient, m_bReplayOnly) == 0x280);
static_assert(offsetof(CClient, m_nSendTableCRC) == 0x284);
static_assert(offsetof(CClient, m_NetChannel) == 0x290);
static_assert(offsetof(CClient, m_nSignonState) == 0x2A0);
static_assert(offsetof(CClient, m_bGameDllClientActivated) == 0x2A4);
static_assert(offsetof(CClient, m_nDeltaTick) == 0x2AC);
static_assert(offsetof(CClient, m_nStringTableAckTick) == 0x2B0);
static_assert(offsetof(CClient, m_nBaselineUpdateTick) == 0x2B8);
static_assert(offsetof(CClient, m_nLoadingProgress) == 0x2BC);
static_assert(offsetof(CClient, m_bCommunityDataValid) == 0x310);
static_assert(offsetof(CClient, m_szClanTag) == 0x358);
static_assert(offsetof(CClient, m_BaselinesSent) == 0x380);
static_assert(offsetof(CClient, m_nForceWaitForTick) == 0x480);
static_assert(offsetof(CClient, m_bFakePlayer) == 0x484);
static_assert(offsetof(CClient, m_bReceivedPacket) == 0x485);
static_assert(offsetof(CClient, m_bLowViolence) == 0x486);
static_assert(offsetof(CClient, m_bFullyAuthenticated) == 0x487);
static_assert(offsetof(CClient, m_nNextTick) == 0x48C);
static_assert(offsetof(CClient, m_flUpdateRate) == 0x498);
static_assert(offsetof(CClient, m_iPersistenceReady) == 0x4A0);
static_assert(offsetof(CClient, m_PersistenceBuffer) == 0x4FA);
static_assert(offsetof(CClient, m_nPersistenceStorageState) == 0xF4FC);
static_assert(offsetof(CClient, m_szPlatformID) == 0xF500);
static_assert(offsetof(CClient, m_nPersistenceBaselineState) == 0xF580);
static_assert(offsetof(CClient, m_nPersistenceRequestHandle) == 0xF588);
static_assert(offsetof(CClient, m_PersistenceSerializationBuffer) == 0xF590);
static_assert(offsetof(CClient, m_SnapshotMessageStorage) == 0xF5E8);
static_assert(offsetof(CClient, m_bPersistenceBufferValid) == 0x2D5EC);
static_assert(offsetof(CClient, m_nPersistenceBufferSize) == 0x2D5F0);
static_assert(offsetof(CClient, m_bPersistenceSaveRequested) == 0x2D5F4);
static_assert(offsetof(CClient, m_DlcOwnershipBits) == 0x2D5F8);
static_assert(offsetof(CClient, m_nPartyChangeNumber) == 0x2D608);
static_assert(offsetof(CClient, m_flClientConnectedTime) == 0x2D610);
static_assert(offsetof(CClient, m_Trace) == 0x2D618);
static_assert(offsetof(CClient, m_nPlatformUserId) == 0x2D648);
static_assert(offsetof(CClient, m_ServerDataBlock) == 0x2D650);
static_assert(offsetof(CClient, m_bVoiceLoopback) == 0x2D6F8);
static_assert(offsetof(CClient, m_VoiceStreams) == 0x2D6FC);
static_assert(offsetof(CClient, m_nLastPacketTick) == 0x2D700);
static_assert(offsetof(CClient, m_flReplaySnapshotTime) == 0x2D704);
static_assert(offsetof(CClient, m_nLastMovementTick) == 0x2D708);
static_assert(offsetof(CClient, m_nSnapshotClientHandle) == 0x2D70C);
static_assert(offsetof(CClient, m_nSnapshotSequence) == 0x2D710);
static_assert(offsetof(CClient, m_nSnapshotDeltaTick) == 0x2D714);
static_assert(offsetof(CClient, m_nSnapshotBaselineSequence) == 0x2D718);
static_assert(offsetof(CClient, m_nSnapshotBaselineTick) == 0x2D71C);
static_assert(offsetof(CClient, m_nLastSnapshotSequence) == 0x2D720);

extern CClient* g_pClientArray;

class CClientExtended
{
	friend class CClient;

public:
	CClientExtended()
	{
		Reset();
	}

	void Reset()
	{
		m_bIsCommsBanned = false;
	}
	void SetClientIsCommsBanned(bool bBanned) { m_bIsCommsBanned = bBanned; }
	bool IsClientCommsBanned() const { return m_bIsCommsBanned; }

private:
	bool m_bIsCommsBanned;
};

static_assert(sizeof(CClientExtended) == 0x1);
