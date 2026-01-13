#pragma once

#include "engine/net.h"

class INetChannelHandler
{
public:
	virtual ~INetChannelHandler() = 0;
	virtual void*ConnectionStart(INetChannelHandler* chan) = 0;
	virtual void ConnectionClosing(const char* reason, int unk) = 0;
	virtual void ConnectionCrashed(const char* reason) = 0;
	virtual void PacketStart(int incoming_sequence, int outgoing_acknowledged) = 0;
	virtual void PacketEnd(void) = 0;
	virtual void FileRequested(const char* fileName, unsigned int transferID) = 0;
	virtual void ChannelDisconnect(const char* fileName) = 0;
	virtual void ConnectSplitScreen( const char *pchPublicAddress, bool unk_flag_silentconnectmaybe, int numPlayers, const char* szJoinType ) = 0;
	virtual void Connect( const char *pchPublicAddress, const char* szJoinType ) = 0;
	virtual void Connect( const char *pchPublicAddress ) = 0;
};

class IConnectionlessPacketHandler
{
public:
	virtual ~IConnectionlessPacketHandler(void) = 0;
	virtual bool ProcessConnectionlessPacket(netpacket_t* packet) = 0;
};
