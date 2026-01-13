#pragma once

#include "engine/bitbuf.h"
#include "engine/net.h"
#include "engine/inetmessage.h"
#include "util/utlvector.h"

typedef struct dataFragments_s
{
public:
	void *buffer; //0x0000
	int64_t blockSize; //0x0008
	bool isCompressed; //0x0010
	int64_t uncompressedSize; //0x0018
	bool firstFragment; //0x0020
	bool lastFragment; //0x0021
	bool isOutBound; //0x0022
	int transferID; //0x0024
	int transferSize; //0x0028
	int currentOffset; //0x002C
} dataFragments_t; //Size: 0x0030

class netframe_header_s
{
public:
	float time; //0x0000
	int32_t size; //0x0004
	int16_t choked; //0x0008
	bool valid; //0x000A
	float latency; //0x000C
}; //Size: 0x0010

class netframe_s
{
public:
	int32_t dropped; //0x0000
	float avg_latency; //0x0004
	float m_flInterpolationAmount; //0x0008
	unsigned __int16 msggroups[12];
}; //Size: 0x0024

class netflow_s
{
public:
	float nextcompute; //0x0000
	float avgbytespersec; //0x0004
	float avgpacketspersec; //0x0008
	float avgloss; //0x000C
	float avgchoke; //0x0010
	float avglatency; //0x0014
	float latency; //0x0018
	float maxlatency; //0x001C
	int32_t totalpackets; //0x0020
	int32_t totalbytes; //0x0024
	int32_t currentindex; //0x0028
	class netframe_header_s frame_headers[128]; //0x002C
	class netframe_s frames[128]; //0x082C
	void *currentframe; //0x1A2C
}; //Size: 0x1A38

class CNetChan
{
public:
	bool m_bProcessingMessages; //0x0000
	bool m_bShouldDelete; //0x0001
	bool m_bStopProcessing; //0x0002
	bool m_bShuttingDown; //0x0003
	int m_nOutSequenceNr; //0x0004
	int m_nInSequenceNr; //0x0008
	int m_nOutSequenceNrAck; //0x000C
	int m_nChokedPackets; //0x0010
	int m_nRealTimePackets; //0x0014
	int m_nLastRecvFlags; //0x0018
	void *m_Lock; //0x0020
	bf_write m_StreamReliable; //0x0028
	CUtlMemory<uint8_t> m_ReliableDataBuffer; //0x0048
	bf_write m_StreamUnreliable; //0x0060
	CUtlMemory<uint8_t> m_UnreliableDataBuffer; //0x0080
	bf_write m_StreamVoice; //0x0098
	CUtlMemory<uint8_t> m_VoiceDataBuffer; //0x00B8
	int m_Socket; //0x00D0
	int m_MaxReliablePayloadSize; //0x00D4
	float last_received; //0x00D8
	double connect_time; //0x00E0
	uint32_t m_Rate; //0x00E8
	double m_fClearTime; //0x00F0
	CUtlVector<dataFragments_t*> m_WaitingList; //0x00F8
	dataFragments_t m_ReceiveList; //0x0118
	int m_nSubOutFragmentsAck; //0x0148
	int m_nSubInFragments; //0x014C
	int m_nNonceHost; //0x0150
	uint32_t m_nNonceRemote; //0x0154
	bool m_bReceivedRemoteNonce; //0x0158
	bool m_bInReliableState; //0x0159
	bool m_bPendingRemoteNonceAck; //0x015A
	uint32_t m_nSubOutSequenceNr; //0x0160
	int m_nLastRecvNonce; //0x0164
	bool m_bUseCompression; //0x0168
	float m_Timeout; //0x016C
	void *m_MessageHandler; //0x0170
	CUtlVector<INetMessage*> m_NetMessages; //0x0178
	void *m_pDemoRecorder; //0x0198
	int m_nQueuedPackets; //0x01A0
	float m_flRemoteFrameTime; //0x01A4
	float m_flRemoteFrameTimeStdDeviation; //0x01A8
	uint8_t m_nServerCPU; //0x01AC
	int m_nMaxRoutablePayloadSize; //0x01B0
	int m_nSplitPacketSequence; //0x01B4
	void *m_StreamSendBuffer; //0x01B8
	bf_write m_StreamSend; //0x01C0
	bool m_bConnecting; //0x01E0
	netflow_s m_DataFlow[2]; //0x01E8
	int m_nLifetimePacketsDropped; //0x3658
	int m_nSessionPacketsDropped; //0x365C
	int m_nSessionRecvs; //0x3660
	uint32_t m_nLiftimeRecvs; //0x3664
	bool m_bRetrySendLong; //0x3668
	char m_Name[32]; //0x3669
	int __pad;
	netadr_t remote_address; //0x368C
}; //Size: 0x36A4
