#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/net.h"
#include "inetmessage.h"
#include "tier1/bitbuf.h"
#include "tier1/utlvector.h"

inline constexpr std::size_t NET_FRAMES_BACKUP = 128;
inline constexpr std::size_t NET_MESSAGE_GROUP_COUNT = 13;
inline constexpr std::size_t NET_CHANNEL_NAME_MAX_LENGTH = 32;

enum NetFlow_t : int
{
	FLOW_OUTGOING = 0,
	FLOW_INCOMING = 1,
	MAX_FLOWS = 2,
};

struct dataFragments_t
{
	char* buffer; // 0x0000
	std::int64_t blockSize; // 0x0008
	bool isCompressed; // 0x0010
	std::int64_t uncompressedSize; // 0x0018
	bool firstFragment; // 0x0020
	bool lastFragment; // 0x0021
	bool isOutbound; // 0x0022
	std::int32_t transferID; // 0x0024
	std::int32_t transferSize; // 0x0028
	std::int32_t currentOffset; // 0x002C
};

static_assert(sizeof(dataFragments_t) == 0x30);
static_assert(offsetof(dataFragments_t, buffer) == 0x00);
static_assert(offsetof(dataFragments_t, blockSize) == 0x08);
static_assert(offsetof(dataFragments_t, isCompressed) == 0x10);
static_assert(offsetof(dataFragments_t, uncompressedSize) == 0x18);
static_assert(offsetof(dataFragments_t, firstFragment) == 0x20);
static_assert(offsetof(dataFragments_t, transferID) == 0x24);
static_assert(offsetof(dataFragments_t, lastFragment) == 0x21);
static_assert(offsetof(dataFragments_t, isOutbound) == 0x22);
static_assert(offsetof(dataFragments_t, transferSize) == 0x28);
static_assert(offsetof(dataFragments_t, currentOffset) == 0x2C);

struct netframe_header_t
{
	float time; // 0x0000
	std::int32_t size; // 0x0004
	std::int16_t choked; // 0x0008
	bool valid; // 0x000A
	float latency; // 0x000C
};

static_assert(sizeof(netframe_header_t) == 0x10);
static_assert(offsetof(netframe_header_t, time) == 0x00);
static_assert(offsetof(netframe_header_t, size) == 0x04);
static_assert(offsetof(netframe_header_t, choked) == 0x08);
static_assert(offsetof(netframe_header_t, valid) == 0x0A);
static_assert(offsetof(netframe_header_t, latency) == 0x0C);

struct netframe_t
{
	std::int32_t dropped; // 0x0000
	float avg_latency; // 0x0004
	std::uint16_t msggroups[NET_MESSAGE_GROUP_COUNT]; // 0x0008
};

static_assert(sizeof(netframe_t) == 0x24);
static_assert(offsetof(netframe_t, dropped) == 0x00);
static_assert(offsetof(netframe_t, avg_latency) == 0x04);
static_assert(offsetof(netframe_t, msggroups) == 0x08);

struct netflow_t
{
	float nextcompute; // 0x0000
	float avgbytespersec; // 0x0004
	float avgpacketspersec; // 0x0008
	float avgloss; // 0x000C
	float avgchoke; // 0x0010
	float avglatency; // 0x0014
	std::int64_t totalpackets; // 0x0018
	std::int64_t totalbytes; // 0x0020
	std::int32_t currentindex; // 0x0028
	netframe_header_t frame_headers[NET_FRAMES_BACKUP]; // 0x002C
	netframe_t frames[NET_FRAMES_BACKUP]; // 0x082C
	netframe_t* currentframe; // 0x1A30
};

static_assert(sizeof(netflow_t) == 0x1A38);
static_assert(offsetof(netflow_t, nextcompute) == 0x00);
static_assert(offsetof(netflow_t, avgbytespersec) == 0x04);
static_assert(offsetof(netflow_t, avgpacketspersec) == 0x08);
static_assert(offsetof(netflow_t, avgloss) == 0x0C);
static_assert(offsetof(netflow_t, avgchoke) == 0x10);
static_assert(offsetof(netflow_t, avglatency) == 0x14);
static_assert(offsetof(netflow_t, totalpackets) == 0x18);
static_assert(offsetof(netflow_t, totalbytes) == 0x20);
static_assert(offsetof(netflow_t, currentindex) == 0x28);
static_assert(offsetof(netflow_t, frame_headers) == 0x2C);
static_assert(offsetof(netflow_t, frames) == 0x82C);
static_assert(offsetof(netflow_t, currentframe) == 0x1A30);

struct netchannel_info_t
{
	bool retrySendLong; // 0x0000
	char name[NET_CHANNEL_NAME_MAX_LENGTH]; // 0x0001
	netadr_t remoteAddress; // 0x0024
};

static_assert(sizeof(netchannel_info_t) == 0x3C);
static_assert(offsetof(netchannel_info_t, name) == 0x01);
static_assert(offsetof(netchannel_info_t, remoteAddress) == 0x24);

class INetChannelHandler;

class CNetChan
{
public:
	CNetChan();
	~CNetChan();

	const char* GetName() const;
	const netadr_t& GetRemoteAddress() const;
	std::int32_t GetDataRate() const;
	std::int32_t GetSocket() const;
	std::int32_t GetSequenceNr(int flow) const;
	double GetTimeConnected() const;
	float GetTimeSinceLastReceived() const;
	float GetTimeoutSeconds() const;
	float GetAvgChoke(int flow) const;
	float GetAvgData(int flow) const;
	float GetAvgLatency(int flow) const;
	float GetAvgLoss(int flow) const;
	float GetAvgPackets(int flow) const;
	std::int32_t GetTotalPackets(int flow) const;
	std::int32_t GetTotalData(int flow) const;

	bool m_bProcessingMessages; // 0x0000
	bool m_bShouldDelete; // 0x0001
	bool m_bStopProcessing; // 0x0002
	bool m_bShuttingDown; // 0x0003
	std::int32_t m_nOutSequenceNr; // 0x0004
	std::int32_t m_nInSequenceNr; // 0x0008
	std::int32_t m_nOutSequenceNrAck; // 0x000C
	std::int32_t m_nChokedPackets; // 0x0010
	std::int32_t m_nRealTimePackets; // 0x0014
	std::int32_t m_nLastRecvFlags; // 0x0018
	RTL_SRWLOCK m_Lock; // 0x0020
	bf_write m_StreamReliable; // 0x0028
	CUtlMemory<std::uint8_t> m_ReliableDataBuffer; // 0x0048
	bf_write m_StreamUnreliable; // 0x0060
	CUtlMemory<std::uint8_t> m_UnreliableDataBuffer; // 0x0080
	bf_write m_StreamVoice; // 0x0098
	CUtlMemory<std::uint8_t> m_VoiceDataBuffer; // 0x00B8
	std::int32_t m_Socket; // 0x00D0
	std::int32_t m_MaxReliablePayloadSize; // 0x00D4
	float last_received; // 0x00D8
	double connect_time; // 0x00E0
	std::uint32_t m_Rate; // 0x00E8
	double m_fClearTime; // 0x00F0
	CUtlVector<dataFragments_t*> m_WaitingList; // 0x00F8
	dataFragments_t m_ReceiveList; // 0x0118
	std::int32_t m_nSubOutFragmentsAck; // 0x0148
	std::int32_t m_nSubInFragments; // 0x014C
	std::int32_t m_nNonceHost; // 0x0150
	std::uint32_t m_nNonceRemote; // 0x0154
	bool m_bReceivedRemoteNonce; // 0x0158
	bool m_bInReliableState; // 0x0159
	bool m_bPendingRemoteNonceAck; // 0x015A
	std::uint32_t m_nSubOutSequenceNr; // 0x015C
	std::int32_t m_nLastRecvNonce; // 0x0160
	bool m_bUseCompression; // 0x0164
	std::uint32_t m_ChallengeNr; // 0x0168
	float m_Timeout; // 0x016C
	INetChannelHandler* m_MessageHandler; // 0x0170
	CUtlVector<INetMessage*> m_NetMessages; // 0x0178
	void* m_pUnusedInterface; // 0x0198
	std::int32_t m_nQueuedPackets; // 0x01A0
	float m_flRemoteFrameTime; // 0x01A4
	float m_flRemoteFrameTimeStdDeviation; // 0x01A8
	std::uint8_t m_nServerCPU; // 0x01AC
	std::int32_t m_nMaxRoutablePayloadSize; // 0x01B0
	std::int32_t m_nSplitPacketSequence; // 0x01B4
	std::int64_t m_StreamSendBuffer; // 0x01B8
	bf_write m_StreamSend; // 0x01C0
	bool m_bConnecting; // 0x01E0
	netflow_t m_DataFlow[MAX_FLOWS]; // 0x01E8
	std::int32_t m_nLifetimePacketsDropped; // 0x3658
	std::int32_t m_nSessionPacketsDropped; // 0x365C
	std::int32_t m_nSessionRecvs; // 0x3660
	std::uint32_t m_nLifetimeRecvs; // 0x3664
	netchannel_info_t m_ChannelInfo; // 0x3668
};

static_assert(sizeof(CNetChan) == 0x36A8);
static_assert(offsetof(CNetChan, m_bProcessingMessages) == 0x0000);
static_assert(offsetof(CNetChan, m_Lock) == 0x0020);
static_assert(offsetof(CNetChan, m_StreamReliable) == 0x0028);
static_assert(offsetof(CNetChan, m_ReliableDataBuffer) == 0x0048);
static_assert(offsetof(CNetChan, m_Socket) == 0x00D0);
static_assert(offsetof(CNetChan, last_received) == 0x00D8);
static_assert(offsetof(CNetChan, connect_time) == 0x00E0);
static_assert(offsetof(CNetChan, m_Rate) == 0x00E8);
static_assert(offsetof(CNetChan, m_fClearTime) == 0x00F0);
static_assert(offsetof(CNetChan, m_WaitingList) == 0x00F8);
static_assert(offsetof(CNetChan, m_ReceiveList) == 0x0118);
static_assert(offsetof(CNetChan, m_nSubOutSequenceNr) == 0x015C);
static_assert(offsetof(CNetChan, m_ChallengeNr) == 0x0168);
static_assert(offsetof(CNetChan, m_Timeout) == 0x016C);
static_assert(offsetof(CNetChan, m_MessageHandler) == 0x0170);
static_assert(offsetof(CNetChan, m_NetMessages) == 0x0178);
static_assert(offsetof(CNetChan, m_StreamSend) == 0x01C0);
static_assert(offsetof(CNetChan, m_bConnecting) == 0x01E0);
static_assert(offsetof(CNetChan, m_DataFlow) == 0x01E8);
static_assert(offsetof(CNetChan, m_nLifetimePacketsDropped) == 0x3658);
static_assert(offsetof(CNetChan, m_nSessionPacketsDropped) == 0x365C);
static_assert(offsetof(CNetChan, m_nSessionRecvs) == 0x3660);
static_assert(offsetof(CNetChan, m_nLifetimeRecvs) == 0x3664);
static_assert(offsetof(CNetChan, m_ChannelInfo) == 0x3668);
