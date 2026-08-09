#pragma once

#include "engine/net.h"
class CNetChan;
class NET_StringCmd;
class NET_SetConVar;
class NET_SignonState;
class CLC_ClientInfo;
class CLC_Move;
class CLC_VoiceData;
class CLC_DurangoVoiceData;
class CLC_FileCRCCheck;
class CLC_LoadingProgress;
class CLC_PersistenceRequestSave;
class CLC_PersistenceClientToken;
class CLC_SetClientEntitlements;
class CLC_SetPlaylistVarOverride;
class CLC_ClaimClientSidePickup;
class CLC_ClientSayText;
class CLC_ClientTick;
class CLC_CmdKeyValues;
class CLC_Screenshot;
class CLC_PINTelemetryData;

class INetMessageHandler
{
public:
	virtual ~INetMessageHandler(void) = 0;
	virtual bool ProcessStringCmd(NET_StringCmd* message) = 0;
	virtual bool ProcessSetConVar(NET_SetConVar* message) = 0;
	virtual bool ProcessSignonState(NET_SignonState* message) = 0;
};

class IClientMessageHandler : public INetMessageHandler
{
public:
	virtual ~IClientMessageHandler() = default;
	virtual bool ProcessClientInfo(CLC_ClientInfo* message) = 0;
	virtual bool ProcessMove(CLC_Move* message) = 0;
	virtual bool ProcessVoiceData(CLC_VoiceData* message) = 0;
	virtual bool ProcessDurangoVoiceData(CLC_DurangoVoiceData* message) = 0;
	virtual bool ProcessFileCRCCheck(CLC_FileCRCCheck* message) = 0;
	virtual bool ProcessLoadingProgress(CLC_LoadingProgress* message) = 0;
	virtual bool ProcessPersistenceRequestSave(CLC_PersistenceRequestSave* message) = 0;
	virtual bool ProcessPersistenceClientToken(CLC_PersistenceClientToken* message) = 0;
	virtual bool ProcessSetClientEntitlements(CLC_SetClientEntitlements* message) = 0;
	virtual bool ProcessSetPlaylistVarOverride(CLC_SetPlaylistVarOverride* message) = 0;
	virtual bool ProcessClaimClientSidePickup(CLC_ClaimClientSidePickup* message) = 0;
	virtual bool ProcessClientSayText(CLC_ClientSayText* message) = 0;
	virtual bool ProcessClientTick(CLC_ClientTick* message) = 0;
	virtual bool ProcessCmdKeyValues(CLC_CmdKeyValues* message) = 0;
	virtual bool ProcessScreenshot(CLC_Screenshot* message) = 0;
	virtual bool ProcessPINTelemetryData(CLC_PINTelemetryData* message) = 0;
};



class INetChannelHandler
{
public:
	virtual ~INetChannelHandler() = 0;
	virtual bool ConnectionStart(CNetChan* channel) = 0;
	virtual void ConnectionClosing(const char* reason, int unknown) = 0;
	virtual void ConnectionCrashed(const char* reason) = 0;
	virtual void PacketStart(int incomingSequence, int outgoingAcknowledged) = 0;
	virtual void PacketEnd() = 0;
};

class IConnectionlessPacketHandler
{
public:
	virtual ~IConnectionlessPacketHandler(void) = 0;
	virtual bool ProcessConnectionlessPacket(netpacket_t* packet) = 0;
};
