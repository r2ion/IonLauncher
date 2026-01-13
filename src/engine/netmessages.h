#pragma once

#include "inetmessage.h"
#include "inetchannel.h"

// custom connectionless packet types
#define A2S_SIGREQ1 'T'

#define MODDOWNLOADINFO_VERSION 1
#define S2C_MODDOWNLOADINFO 'V'

#define CUSTOMSERVERINFO_VERSION 2
#define A2S_REQUESTCUSTOMSERVERINFO 'Q'
#define S2A_CUSTOMSERVERINFO 'R'

#define S2C_CLIENTNOTIFY 'U'
#define CLIENTNOTIFY_VERSION 1
#define NOTIFY_AUTHENTICATED 1

extern bool g_bListeningforCustomServerInfoPacket;
extern char g_szLastServerInfoName[256];
extern bool g_bNextServerAuthUs;
extern bool g_bNextServerAllowingAuthUs;
extern bool g_bReceivedAuthNotify;
extern bool g_bReceivedServerInfo;

enum class NetMessageType : int
{
    net_StringCmd = 3,
    net_SetConVar = 4,
    net_SignonState = 5,
    clc_ClientTick = 59,
    clc_ClientInfo = 44,
    clc_Move = 45,
    clc_VoiceData = 46,
    clc_DurangoVoiceData = 47,
    clc_FileCRCCheck = 49,
    clc_LoadingProgress = 51,
    clc_PersistenceRequestSave = 52,
    clc_PersistenceClientToken = 53,
    clc_SetClientEntitlements = 54,
    clc_SetPlaylistVarOverride = 55,
    clc_ClaimClientSidePickup = 56,
    clc_ClientSayText = 60,
    clc_Screenshot = 61,
    clc_CmdKeyValues = 58,
    clc_PINTelemetryData = 62,
    svc_Print = 15,
    svc_ServerInfo = 6,
    svc_SendTable = 7,
    svc_ClassInfo = 8,
    svc_SetPause = 9,
    svc_Playlists = 10,
    svc_CreateStringTable = 11,
    svc_UpdateStringTable = 12,
    svc_VoiceData = 13,
    svc_DurangoVoiceData = 14,
    svc_Sounds = 16,
    svc_FixAngle = 17,
    svc_CrosshairAngle = 18,
    svc_GrantClientSidePickup = 19,
    svc_UserMessage = 33,
    svc_Snapshot = 36,
    svc_ServerTick = 21,
    svc_TempEntities = 37,
    svc_Menu = 39,
    svc_CmdKeyValues = 40,
    svc_UseCachedPersistenceDefFile = 23,
    svc_PersistenceDefFile = 22,
    svc_PersistenceBaseline = 24,
    svc_PersistenceUpdateVar = 25,
    svc_PersistenceNotifySaved = 26,
    svc_DLCNotifyOwnership = 27,
    svc_MatchmakingStatus = 28,
    svc_PlaylistChange = 29,
    svc_SetTeam = 30,
    svc_RequestScreenshot = 41,
    svc_PlaylistOverrides = 31,
    svc_PlaylistPlayerCounts = 32,
    svc_NetProfileFrame = 42,
    svc_NetProfileTotals = 43,
    __NEXT_INDEX__ = 63,
};

enum class NSCustomNetMessages : int
{
	net_SendPersistenceChecksum = static_cast<int>(NetMessageType::__NEXT_INDEX__),
};

class CNetMessage : public INetMessage
{
public:
	virtual void	SetNetChannel(CNetChan* netchan) { m_NetChannel = netchan; }
	virtual void	SetReliable(bool state) { m_bReliable = state; }
	virtual bool	IsReliable(void) const { return m_bReliable; }
	virtual int		GetGroup(void) const { return m_nGroup; }
	virtual CNetChan* GetNetChannel(void) const { return m_NetChannel; }
	virtual int     MysteryReturn0() const { return 0; }

	int m_nGroup;
	bool m_bReliable;
	CNetChan* m_NetChannel;
	INetChannelHandler* m_pMessageHandler;
};
