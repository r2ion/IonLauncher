#include "engine/net_chan.h"
#include "tier0/callbacks.h"

using CNetChanCanPacketFn = bool (*)(const CNetChan*);
using CNetChanSendNetMsgFn = bool (*)(CNetChan*, INetMessage*, bool, bool);
using CNetChanSendDatagramFn = std::int32_t (*)(CNetChan*, bf_write*);
using CNetChanSetChokedFn = void (*)(CNetChan*);

CNetChanCanPacketFn s_CNetChanCanPacket;
CNetChanSendNetMsgFn s_CNetChanSendNetMsg;
CNetChanSendDatagramFn s_CNetChanSendDatagram;
CNetChanSetChokedFn s_CNetChanSetChoked;

bool CNetChan::CanPacket() const
{
	return s_CNetChanCanPacket(this);
}

bool CNetChan::SendNetMsg(INetMessage& message, const bool forceReliable, const bool voice)
{
	return s_CNetChanSendNetMsg(this, &message, forceReliable, voice);
}

std::int32_t CNetChan::SendDatagram(bf_write* const datagram)
{
	return s_CNetChanSendDatagram(this, datagram);
}

void CNetChan::SetChoked()
{
	s_CNetChanSetChoked(this);
}

ON_DLL_LOAD("engine.dll", NetChan, [](CModule module)
{
	s_CNetChanCanPacket = module.Offset(0x20F620).RCast<CNetChanCanPacketFn>();
	s_CNetChanSendNetMsg = module.Offset(0x213270).RCast<CNetChanSendNetMsgFn>();
	s_CNetChanSendDatagram = module.Offset(0x212CD0).RCast<CNetChanSendDatagramFn>();
	s_CNetChanSetChoked = module.Offset(0x213760).RCast<CNetChanSetChokedFn>();
})
