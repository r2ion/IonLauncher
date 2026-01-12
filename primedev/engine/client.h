#pragma once

#include "engine/inetchannel.h"
#include "engine/inetmessage.h"
#include "shared/keyvalues.h"
#include "shared/signonstate.h"

extern void (*CClient__Disconnect)(void* self, uint32_t unknownButAlways1, const char* reason, ...);
extern void (*CClient__SendDataBlock)(void* self, bf_write* msg);
class CClientExtended;
using edict_t = uint16_t;

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
enum class ePersistenceReady : char
{
	NOT_READY,
	READY = 3,
	READY_INSECURE = 3,
	READY_REMOTE
};

class ServerDataBlockSender
{
public:
    virtual ~ServerDataBlockSender(void) = default;
};

class ServerDataBlock
{
	int32_t m_nLastBlockTick_maybe; //0x0000
	int32_t m_iBlockSize_maybe; //0x0004
	ServerDataBlockSender m_pSender; //0x0008
}; //Size: 0x0010

class IClientMessageHandler
{
public:
	virtual ~IClientMessageHandler(void) {};
	virtual bool ProcessStringCmd(void* msg) = 0;
	virtual bool ProcessScriptMessage(void) = 0;
	virtual bool ProcessSetConVar(void* msg) = 0;
	virtual bool nullsub_0(void) = 0;
	virtual bool ProcessSignonState(void* msg) = 0; // NET_SignonState
	virtual bool ProcessMove(void) = 0;
	virtual bool ProcessVoiceData(void) = 0;
	virtual bool ProcessDurangoVoiceData(void) = 0;
	virtual bool nullsub_1(void) = 0;
	virtual bool ProcessLoadingProgress(void) = 0;
	virtual bool ProcessPersistenceRequestSave(void) = 0;
	virtual bool nullsub_2(void) = 0;
	virtual bool nullsub_3(void) = 0;
	virtual bool ProcessSetPlaylistVarOverride(void) = 0;
	virtual bool ProcessClaimClientSidePickup(void) = 0;
	virtual bool ProcessCmdKeyValues(void) = 0;
	virtual bool ProcessClientTick(void) = 0;
	virtual bool ProcessClientSayText(void) = 0;
	virtual bool nullsub_4(void) = 0;
	virtual bool nullsub_5(void) = 0;
	virtual bool nullsub_6(void) = 0;
	virtual bool ProcessScriptMessageChecksum(void) = 0;
};

class CClient : public IClientMessageHandler, public INetMessageHandler
{
public:
	edict_t GetHandle(void) const { return m_nHandle; }
	bool IsFakePlayer(void) const { return m_bFakePlayer; }
	int GetUserID(void) const { return m_nUserID; }
	CClientExtended* GetClientExtended(void) const;

	uint32_t m_nUserID; //0x0010
	uint16_t m_nHandle; //0x0014
	char m_szServerName[256]; //0x0016
	char m_szClientName[256]; //0x0116
	char pad_0216[2]; //0x0216
	int32_t m_nCommandTick; //0x0218
	bool m_bUsePersistence; //0x021C
	char pad_021D[7]; //0x021D
	float m_flInitialConnectTime; //0x0224
	float m_flLastConnectTime; //0x0228
	float N000033D2; //0x022C
	float N000033EE; //0x0230
	char pad_0234[28]; //0x0234
	int32_t m_iTeamNum; //0x0250
	char pad_0254[4]; //0x0254
	KeyValues* m_ConVars; //0x0258
	char pad_0260[8]; //0x0260
	void *m_pServer; //0x0268
	char pad_0270[12]; //0x0270
	int32_t m_nSendtableCRC; //0x027C
	int32_t m_nMmDev; //0x0280
	float N0000340D; //0x0284
	char pad_0288[8]; //0x0288
	void *m_NetChannel; //0x0290
	char pad_0298[8]; //0x0298
	eSignonState m_nSignonState; //0x02A0
	bool mysterious_bool; //0x02A4
	char pad_02A5[7]; //0x02A5
	int32_t m_nDeltaTick; //0x02AC
	int32_t m_nStringTableAckTick; //0x02B0
	int32_t m_nSignonTick; //0x02B4
	int32_t m_nBaselineTick; //0x02B8
	char pad_02BC[4]; //0x02BC
	char m_szRegion[64]; //0x02C0
	char m_szPlatform[64]; //0x0300
	char pad_0340[24]; //0x0340
	char m_ClanTag[16]; //0x0358
	char pad_0341[280]; //0x0340
	int32_t m_nClientDllCRC_maybe; //0x0480
	bool m_bFakePlayer; //0x0484
	bool m_bReceivedPacket; // 0x485 ( Size: 1 )
	bool m_bLowViolence; // 0x486 ( Size: 1 )
	bool m_bFullyAuthenticated; // 0x487 ( Size: 1 )
	int32_t m_nNextTick; //0x048C
	float m_flNextMessageTime; //0x0490
	int32_t N0000346D; //0x0494
	float m_nPersistenceReadyTick_maybe; //0x0498
	int32_t m_nPersistenceState; //0x049C
	int unk;
	ePersistenceReady m_iPersistenceReady; //0x04A0
	char _unk_0x4a1[86 + 3];
	char m_PersistenceBuffer[PERSISTENCE_MAX_SIZE];
	char pad_04A0[61532 - PERSISTENCE_MAX_SIZE - 86]; //0x04A0
	char m_szPlatformID[32]; //0xF500
	char pad_F520[123120]; //0xF520
	float N000090D5; //0x2D610
	char pad_2D614[52]; //0x2D614
	uint64_t m_uiPlatformID; //0x2D648
	ServerDataBlock m_Datablock; //0x2D650
	CClient *m_pSelf; //0x2D660
	char pad_2D668[16]; //0x2D668
	uint32_t N0000922C; //0x2D678
	char pad_2D67C[12]; //0x2D67C
	double N000091D2; //0x2D688
	char pad_2D690[16]; //0x2D690
	void *N000091D3; //0x2D6A0
	char sendsnapshot_text[64]; //0x2D6A8
	void *N000091D5; //0x2D6E8
	void *N000091D6; //0x2D6F0
	uint32_t N000091D7; //0x2D6F8
	uint32_t N00009246; //0x2D6FC
	uint32_t N000091D8; //0x2D700
	float N0000924A; //0x2D704
	uint32_t N000091D9; //0x2D708
	uint32_t N0000924E; //0x2D70C
	uint32_t N000091DB; //0x2D710
	uint32_t N00009251; //0x2D714
	uint32_t N000091DD; //0x2D718
	uint32_t N00009255; //0x2D71C
	uint32_t N000091DF; //0x2D720

	void Disconnect(const Reputation_t nRepLevel, const char* reason, ...);

	inline void SendDataBlock(bf_write* msg) { CClient__SendDataBlock(this, msg); }

	virtual ~CClient();
	virtual bool ConnectionStart(INetChannelHandler* chan) {};
	virtual void ConnectionClosing(const char* reason, int unk1) {};
	virtual void ConnectionCrashed(const char* reason) {};
	virtual void PacketStart(int incoming_sequence, int outgoing_acknowledged) {};
	virtual void PacketEnd(void) {};

	virtual bool ProcessStringCmd(void* msg) {};
	virtual bool ProcessScriptMessage(void) {};
	virtual bool ProcessSetConVar(void* msg) {};
	virtual bool nullsub_0(void) {};
	virtual bool ProcessSignonState(void* msg) {}; // NET_SignonState
	virtual bool ProcessMove(void) {};
	virtual bool ProcessVoiceData(void) {};
	virtual bool ProcessDurangoVoiceData(void) {};
	virtual bool nullsub_1(void) {};
	virtual bool ProcessLoadingProgress(void) {};
	virtual bool ProcessPersistenceRequestSave(void) {};
	virtual bool nullsub_2(void) {};
	virtual bool nullsub_3(void) {};
	virtual bool ProcessSetPlaylistVarOverride(void) {};
	virtual bool ProcessClaimClientSidePickup(void) {};
	virtual bool ProcessCmdKeyValues(void) {};
	virtual bool ProcessClientTick(void) {};
	virtual bool ProcessClientSayText(void) {};
	virtual bool nullsub_4(void) {};
	virtual bool nullsub_5(void) {};
	virtual bool nullsub_6(void) {};
	virtual bool ProcessScriptMessageChecksum(void) {};
};

constexpr int guh = sizeof(CClient);
constexpr int bar = offsetof(CClient, m_szPlatformID);
constexpr int wire = 0xF500;

static_assert(sizeof(CClient) == 0x2D728);
static_assert(offsetof(CClient, m_szServerName) == 0x16);
static_assert(offsetof(CClient, m_ConVars) == 0x258);
static_assert(offsetof(CClient, m_nSignonState) == 0x2A0);
static_assert(offsetof(CClient, m_nStringTableAckTick) == 0x2B0);
static_assert(offsetof(CClient, m_nSignonTick) == 0x2B4);
static_assert(offsetof(CClient, m_ClanTag) == 0x358);
static_assert(offsetof(CClient, m_bFakePlayer) == 0x484);
static_assert(offsetof(CClient, m_bReceivedPacket) == 0x485);
static_assert(offsetof(CClient, m_bLowViolence) == 0x486);
static_assert(offsetof(CClient, m_bFullyAuthenticated) == 0x487);
static_assert(offsetof(CClient, m_iPersistenceReady) == 0x4A0);
static_assert(offsetof(CClient, m_PersistenceBuffer) == 0x4FA);
static_assert(offsetof(CClient, m_szPlatformID) == 0xF500);

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
