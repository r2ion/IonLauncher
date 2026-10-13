#include "engine/client/clientstate.h"
#include "tier0/hooks.h"
#include "engine/cmd.h"
#include "common/netmessages.h"
#include "tier1/bitbuf.h"
#include "engine/net.h"
#include "modsystem/moddownloader.h"
#include "game/client/c_player.h"
#include "tier1/cvar.h"
#include "engine/server/player_limits.h"
#include "tier0/vanilla.h"
#include "util/utils.h"

#include <algorithm>
#include <cmath>

DECLARE_MODULE(EngineClientState)

bool g_bListeningforCustomServerInfoPacket = false;
char g_szLastServerInfoName[256] = {0};
bool g_bNextServerAuthUs = false;
bool g_bNextServerAllowingAuthUs = false;
bool g_bSendingClientSignonCapability = false;

std::atomic_bool g_bReceivedAuthNotify = false;
std::atomic_bool g_bReceivedServerInfo = false;

char* g_pLocalPlayerUserID;
char* g_pLocalPlayerOriginToken;
GetBaseLocalClientType GetBaseLocalClient;
GetLocalPlayerIndexType GetLocalPlayerIndex;

CClientStateExtended CClientState::sm_ClientStateExtended;

CClientStateIsPaused_t CClientState__IsPaused;
CClientStateGetFrameTime_t CClientState__GetFrameTime;
CClientStateSendStringCmd_t CClientState__SendStringCmd;

void* pAccumulateClientTimeReturnAddress;
void* g_pAdjustedClientFrameTimeReturnAddress;

bool CClientState::IsConnectedToExtendedServer() const
{
    return GetClientStateExtended()->IsExtendedServer();
}

CClientStateExtended* CClientState::GetClientStateExtended() const
{
    return &sm_ClientStateExtended;
}

bool CClientState::IsPaused() const
{
    return CClientState__IsPaused(this);
}

float CClientState::GetFrameTime() const
{
    return CClientState__GetFrameTime(this);
}

void CClientState::SendStringCmd(const char* command)
{
    CClientState__SendStringCmd(this, command);
}

double CClientState::GetPreciseClientTime() const
{
    auto& clock = GetClientStateExtended()->m_PreciseClientTime;
    if (this != clock.owner)
        return static_cast<double>(m_clientTime);

    if (m_clientTime != clock.clientTime || m_NetChannel != clock.channel || m_nServerCount != clock.serverCount ||
        m_nSignonState != clock.signonState)
    {
        clock.owner = nullptr;
        clock.remainder = 0.0;
        return static_cast<double>(m_clientTime);
    }

    return static_cast<double>(m_clientTime) + clock.remainder;
}

DECLARE_HOOK(CClockDriftMgr__AdjustFrameTime, engine.dll + 0x11F870,
             [](auto& hook, CClockDriftMgr* self, float hostFrameTime) -> float
{
    const float frameTime = hook.Original(self, hostFrameTime);
    if (hook.ReturnAddress() != g_pAdjustedClientFrameTimeReturnAddress)
        return frameTime;

    CClientState* const client = GetBaseLocalClient();
    if (self != &client->m_ClockDriftMgr)
        return frameTime;

    const double preciseTime = client->GetPreciseClientTime();
    const float duration = std::max(frameTime, 0.0f);
    const float nativeTime = std::min(client->m_clientTime + duration, client->m_flServerUptime);
    const float roundedTime = static_cast<float>(
        std::min(preciseTime + duration, static_cast<double>(client->m_flServerUptime)));
    const float correction = roundedTime - nativeTime;
    if (correction == 0.0f || !std::isfinite(correction))
        return frameTime;

    client->m_clientTime += correction;
    auto& clock = client->GetClientStateExtended()->m_PreciseClientTime;
    clock.owner = client;
    clock.channel = client->m_NetChannel;
    clock.serverCount = client->m_nServerCount;
    clock.signonState = client->m_nSignonState;
    clock.clientTime = client->m_clientTime;
    clock.remainder = preciseTime - static_cast<double>(client->m_clientTime);
    return frameTime;
})

DECLARE_HOOK(CClientState__Clear, engine.dll + 0x8C580, [](auto& hook, CClientState* self)
{
    C_PlayerExtended::ClearClientState();
    self->GetClientStateExtended()->SetIsExtendedServer(false);
    auto& clock = self->GetClientStateExtended()->m_PreciseClientTime;
    if (self == clock.owner)
    {
        clock.owner = nullptr;
        clock.remainder = 0.0;
    }
    auto& skinErrors = self->GetClientStateExtended()->m_PredictionSkinErrorLog;
    if (self == skinErrors.owner)
        skinErrors = {};
    hook.Original(self);
})
DECLARE_HOOK(CClientState::ConnectionStart, engine.dll + 0x8CB40, [](auto& hook, CClientState* self, CNetChan* channel) -> bool
{
    if (!hook.Original(self, channel))
        return false;

    return g_NetMessageHandler->RegisterMessagesSVC(channel, static_cast<IServerMessageHandler*>(self));
})

DECLARE_HOOK(CClientState__SetClientTime, engine.dll + 0x91AD0, [](auto& hook, CClientState* self, float clientTime)
{
    auto& clock = self->GetClientStateExtended()->m_PreciseClientTime;
    double remainder = 0.0;

    if (hook.ReturnAddress() == pAccumulateClientTimeReturnAddress)
    {
        const double time = std::min(self->GetPreciseClientTime() + self->m_frameTime, static_cast<double>(self->m_flServerUptime));
        if (std::isfinite(time))
            remainder = time - static_cast<double>(clientTime);
    }
    hook.Original(self, clientTime);
    clock.owner = self;
    clock.channel = self->m_NetChannel;
    clock.serverCount = self->m_nServerCount;
    clock.signonState = self->m_nSignonState;
    clock.clientTime = clientTime;
    clock.remainder = remainder;
})

ON_DLL_LOAD_CLIENT("engine.dll", ClientStateMethods, [](CModule module)
{
    g_pLocalPlayerUserID = module.Offset(0x13F8E688).RCast<char*>();
    g_pLocalPlayerOriginToken = module.Offset(0x13979C80).RCast<char*>();
    GetBaseLocalClient = module.Offset(0x78200).RCast<GetBaseLocalClientType>();
    GetLocalPlayerIndex = module.Offset(0x52260).RCast<GetLocalPlayerIndexType>();

    CClientState__IsPaused = module.Offset(0x8F520).RCast<CClientStateIsPaused_t>();
    CClientState__GetFrameTime = module.Offset(0x8E400).RCast<CClientStateGetFrameTime_t>();
    CClientState__SendStringCmd = module.Offset(0x91A10).RCast<CClientStateSendStringCmd_t>();
    pAccumulateClientTimeReturnAddress = module.Offset(0x15952B).RCast<void*>();
    g_pAdjustedClientFrameTimeReturnAddress = module.Offset(0x1594E5).RCast<void*>();
    DISPATCH_HOOK(EngineClientState, CClientState::ConnectionStart)
    DISPATCH_HOOK(EngineClientState, CClientState__Clear)
    DISPATCH_HOOK(EngineClientState, CClientState__SetClientTime)
    DISPATCH_HOOK(EngineClientState, CClockDriftMgr__AdjustFrameTime)
})

// Set execution markers properly if there's space
// If not enough space, ignore command
// clang-format off
DECLARE_HOOK(CClientState::ProcessStringCmd, engine.dll + 0x1A1C20,
( [](auto& hook, CClientState* self, int64_t a2) -> bool
// clang-format on
{
	if (!Cbuf_HasRoomForExecutionMarkers(2))
		return true;
	return hook.Original(self, a2);
}))

ON_DLL_LOAD_RELIESON("engine.dll", ClientStringCmdSecurity, CmdBufferSecurity, [](CModule)
{
    DISPATCH_HOOK(EngineClientState, CClientState::ProcessStringCmd)
})

DECLARE_HOOK(CClientState__ProcessConnectionlessPacket, engine.dll + 0x19F400, [](auto& hook, CClientState* self, netpacket_t* packet) -> bool
{
	bf_read msg(packet->data, packet->size);
	unsigned int header = msg.ReadLong();

	bool serverAuthingUs = false;
	char buff[512];
	int version;
	int notifyType;


	if (header == CONNECTIONLESS_HEADER)
	{
		char packetType = msg.ReadChar();

		switch(packetType)
		{
			case S2C_MODDOWNLOADINFO:
				return g_pModDownloader->RecvModInfoConnectionlessPacket(msg);
			case S2A_CUSTOMSERVERINFO:
            {
                version = msg.ReadLong();
				if(version != CUSTOMSERVERINFO_VERSION)
					break;


				msg.ReadChar(); // marker
				if(!msg.ReadString(g_szLastServerInfoName, sizeof(g_szLastServerInfoName)))
					return false;

				msg.ReadString(buff, sizeof(buff)); // desc
				msg.ReadString(buff, sizeof(buff)); // map
				msg.ReadString(buff, sizeof(buff)); // playlist
				msg.ReadByte(); // reserved
				msg.ReadLong(); // player count
				msg.ReadLong(); // max players
				msg.ReadChar(); // d/l
				msg.ReadString(buff, sizeof(buff)); // region

				msg.ReadByte();
				serverAuthingUs = msg.ReadByte() != 0;
                const int totalServerMods = msg.ReadLong();
                if (msg.IsOverflowed())
                    return false;
                g_pModDownloader->SetServerRequestedModCount(totalServerMods);

                if(serverAuthingUs && g_bNextServerAuthUs)
					g_bNextServerAllowingAuthUs = true;


				g_bReceivedServerInfo.store(true, std::memory_order_release);
				g_bListeningforCustomServerInfoPacket = false;
				return true;
            }
            case S2C_CLIENTNOTIFY:
				version = msg.ReadLong();
				spdlog::info("Received client notify packet, version {}", version);
				if(version != CLIENTNOTIFY_VERSION)
					break;

				notifyType = msg.ReadLong();
				msg.ReadFloat();

				switch(notifyType)
				{
					case NOTIFY_AUTHENTICATED:
					{
						char authToken[256];
						if (!msg.ReadString(authToken, sizeof(authToken)))
							return false;

						g_pCVar->FindVar("serverfilter")->SetValue(authToken);
						g_bReceivedAuthNotify.store(true, std::memory_order_release);
						break;
					}
					default:
						break;
				}

				return true;
			default:
				break;
		}
	}

	return hook.Original(self, packet);
})

DECLARE_HOOK(CClientState::ProcessSignonStateInternal, engine.dll + 0x91D20,
             [](auto& hook,
                CClientState* clientState,
                eSignonState state,
                int serverCount,
                NET_SignonState* message) -> bool
{
	if (g_pVanillaCompatibility->GetVanillaCompatibility() || state != eSignonState::CONNECTED)
		return hook.Original(clientState, state, serverCount, message);

	const bool wasSendingClientSignonCapability = g_bSendingClientSignonCapability;
	g_bSendingClientSignonCapability = true;
	const ScopeGuard restoreSendingState(
	    [wasSendingClientSignonCapability] { g_bSendingClientSignonCapability = wasSendingClientSignonCapability; });
	return hook.Original(clientState, state, serverCount, message);
})

ON_DLL_LOAD_RELIESON("engine.dll", ClientNetHooks, (CmdBuffer, EngineServerGlobals, EngineSystem), [](CModule)
{
    DISPATCH_HOOK(EngineClientState, CClientState__ProcessConnectionlessPacket)
})

ON_DLL_LOAD("engine.dll", ClientSignonCapability, [](CModule)
{
    DISPATCH_HOOK(EngineClientState, CClientState::ProcessSignonStateInternal)
})

ON_DLL_LOAD("engine.dll", MaxPlayersServerInfo, [](CModule module)
{
    if (!MaxPlayersIncreaseEnabled())
        return;

	// patch max players somewhere in CClientState
	ChangeOffset<unsigned char>(module.Offset(0x1A162C + 2), NEW_MAX_PLAYERS - 1); // original: 31 (32 - 1)
})
