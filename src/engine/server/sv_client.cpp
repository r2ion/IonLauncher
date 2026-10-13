#include "engine/server/sv_client.h"
#include "engine/server/server.h"
#include "common/netmessages.h"
#include "engine/server/server_limits.h"
#include "engine/networkstringtable.h"
#include "dedicated/dedicated.h"
#include "engine/client/clientstate.h"
#include "tier1/cvar.h"
#include "engine/server/serverauthentication_internal.h"
#include "engine/server/serverauthentication.h"
#include "engine/server/bansystem.h"
#include "engine/server/sv_main.h"
#include "engine/server/server_limits.h"
#include "engine/server/serverpresence.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"
#include "util/utils.h"

static ConVar* Cvar_ns_should_log_all_clientcommands;
static ConVar* Cvar_sv_cheats;

CClient__Disconnect_t CClient__Disconnect;
CClient__SendDataBlock_t CClient__SendDataBlock;
CClient* g_pClientArray;
bool g_bSendingServerSignonCapability = false;

void CClient::Disconnect(const Reputation_t nRepLevel, const char* reason, ...)
{
	if (m_nSignonState != eSignonState::NONE)
	{
		char szBuf[1024];
		{
			va_list vArgs;
			va_start(vArgs, reason);

			const int ret = snprintf(szBuf, sizeof(szBuf), reason, vArgs);

			if (ret < 0)
				szBuf[0] = '\0';

			va_end(vArgs);
		}
		CClient__Disconnect(this, nRepLevel, szBuf);
	}
}


CClientExtended* CClient::GetClientExtended(void) const
{
	return g_pServer->GetClientExtended(m_nUserID);
}

DECLARE_MODULE(EngineClientHooks)

DECLARE_HOOK(CClient__Clear, engine.dll + 0x101480, [](auto& hook, CClient* thisptr)
{
	/*g_pServer->GetClientExtended(thisptr->m_nUserID)->Reset();*/
	thisptr->GetClientExtended()->Reset();
	hook.Original(thisptr);
})

DECLARE_HOOK(CClient::ConnectionStart, engine.dll + 0x1019C0, [](auto& hook, CClient* self, CNetChan* channel) -> bool
{
    if (!hook.Original(self, channel))
        return false;

    return g_NetMessageHandler->RegisterMessagesCLC(channel, static_cast<IClientMessageHandler*>(self));
})

ON_DLL_LOAD("engine.dll", CClient, [](CModule module)
{
	DISPATCH_HOOK(EngineClientHooks, CClient__Clear)
	DISPATCH_HOOK(EngineClientHooks, CClient::ConnectionStart)

	CClient__Disconnect = module.Offset(0x1012C0).RCast<CClient__Disconnect_t>();
	CClient__SendDataBlock = module.Offset(0x104870).RCast<CClient__SendDataBlock_t>();
	g_pClientArray = module.Offset(0x12A53F90).RCast<CClient*>();
})
// prevent invalid user CMDs
// clang-format off
DECLARE_HOOK(CClient::ProcessUsercmds, engine.dll + 0x1040F0,
([](auto& hook, void* thisptr, CLC_Move* msg) -> bool // 40 55 56 48 83 EC 58
// clang-format on
{

	const char* BLOCK_PREFIX = "ProcessUserCmds: ";

	if (msg->m_nBackupCommands < 0)
	{
		return BLOCKED_INFO("Invalid m_nBackupCommands (" << msg->m_nBackupCommands << ")");
	}

	if (msg->m_nNewCommands < 0)
	{
		return BLOCKED_INFO("Invalid m_nNewCommands (" << msg->m_nNewCommands << ")");
	}

	if (msg->m_nLength <= 0)
		return BLOCKED_INFO("Invalid message length (" << msg->m_nLength << ")");

	return hook.Original(thisptr, msg);
}))
// ratelimit stringcmds, and prevent remote clients from calling commands that they shouldn't
// clang-format off
DECLARE_HOOK(CGameClient::ExecuteStringCommand, engine.dll + 0x1022E0,
([](auto& hook, CClient* self, uint32_t unknown, const char* pCommandString) -> bool
// clang-format on
{
	if (Cvar_ns_should_log_all_clientcommands->GetBool())
		spdlog::info("player {} (UID: {}) sent command: \"{}\"", self->m_szClientName, self->m_szPlatformID, pCommandString);

	if (!g_pServerLimits->CheckStringCommandLimits(self))
	{
		self->Disconnect(REP_REMOVE_ONLY, "Sent too many stringcmd commands");
		return false;
	}

	// verify the command we're trying to execute is FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS, if it's a concommand
	char* commandBuf[1040]; // assumedly this is the size of CCommand since we don't have an actual constructor
	memset(commandBuf, 0, sizeof(commandBuf));
	CCommand tempCommand = *(CCommand*)&commandBuf;

	if (!tempCommand.Tokenize(pCommandString, cmd_source_t::kCommandSrcCode) || !tempCommand.ArgC())
		return false;

	ConCommand* command = g_pCVar->FindCommand(tempCommand.Arg(0));

	// if the command doesn't exist pass it on to ExecuteStringCommand for script clientcommands and stuff
	if (command && !command->IsFlagSet(FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS))
	{
		// ensure FCVAR_GAMEDLL concommands without FCVAR_GAMEDLL_FOR_REMOTE_CLIENTS can't be executed by remote clients
		if (IsDedicatedServer())
			return false;

		if (strcmp(self->m_szPlatformID, g_pLocalPlayerUserID))
			return false;
	}

	// check for and block abusable legacy portal 2 commands
	// these aren't actually concommands weirdly enough, they seem to just be hardcoded
	if (!Cvar_sv_cheats->GetBool())
	{
		constexpr const char* blockedCommands[] = {
			"emit", // Sound-playing exploit (likely for Portal 2 coop devs testing splitscreen sound or something)

			// These both execute a command for every single entity for some reason, nice one valve
			"pre_go_to_hub",
			"pre_go_to_calibration",

			"end_movie", // Calls "__MovieFinished" script function, not sure exactly what this does but it certainly isn't needed
			"load_recent_checkpoint" // This is the instant-respawn exploit, literally just calls RespawnPlayer()
		};

		size_t iCmdLength = strlen(tempCommand.Arg(0));

		bool bIsBadCommand = false;
		for (auto& blockedCommand : blockedCommands)
		{
			if (iCmdLength != strlen(blockedCommand))
				continue;

			for (int i = 0; tempCommand.Arg(0)[i]; i++)
				if (tolower(tempCommand.Arg(0)[i]) != blockedCommand[i])
					goto NEXT_COMMAND; // break out of this loop, then go to next command

			// this is a command we need to block
			return false;
		NEXT_COMMAND:;
		}
	}

	return hook.Original(self, unknown, pCommandString);
}))
// prevent clients from crashing servers through overflowing CNetworkStringTableContainer::WriteBaselines

// clang-format off
DECLARE_HOOK(CClient::SendServerInfo, engine.dll + 0x104FB0,
([](auto& hook, CClient* self)
// clang-format on
{
	bWasWritingStringTableSuccessful = true;
	hook.Original(self);
	if (!bWasWritingStringTableSuccessful)
		self->Disconnect(REP_REMOVE_ONLY, "Overflowed CNetworkStringTableContainer::WriteBaselines, try restarting your client and reconnecting");
}))

ON_DLL_LOAD_RELIESON("engine.dll", ServerClientSecurity, (ServerLimits, StringTableBaselines), [](CModule)
{
    DISPATCH_HOOK(EngineClientHooks, CClient::ProcessUsercmds)
    DISPATCH_HOOK(EngineClientHooks, CGameClient::ExecuteStringCommand)
    DISPATCH_HOOK(EngineClientHooks, CClient::SendServerInfo)
})

ON_DLL_LOAD_RELIESON("server.dll", ClientCommandLogging, ConVar, [](CModule)
{
    Cvar_ns_should_log_all_clientcommands =
        new ConVar("ns_should_log_all_clientcommands", "0", FCVAR_NONE, "Whether to log all clientcommands");
    Cvar_sv_cheats = g_pCVar->FindVar("sv_cheats");
})
DECLARE_HOOK(CClient::ProcessSignonState, engine.dll + 0x104390,
             [](auto& hook, IClientMessageHandler* handler, NET_SignonState* message) -> bool
{
	if (g_pVanillaCompatibility->GetVanillaCompatibility())
		return hook.Original(handler, message);

	if (message->m_nSignonState == eSignonState::CONNECTED)
	{
		CClient* client = static_cast<CClient*>(handler);
		client->GetClientExtended()->SetIsExtendedClient(message->m_bIsExtendedClient);
	}

	return hook.Original(handler, message);
})

DECLARE_HOOK(CClient::SendSignonState, engine.dll + 0x1055B0, [](auto& hook, CClient* client) -> bool
{
    const bool previousFlag = g_bSendingServerSignonCapability;
    g_bSendingServerSignonCapability =
        !g_pVanillaCompatibility->GetVanillaCompatibility() && client->GetClientExtended()->IsExtendedClient();
    const ScopeGuard restoreFlag([previousFlag] { g_bSendingServerSignonCapability = previousFlag; });
    return hook.Original(client);
})

ON_DLL_LOAD("engine.dll", ServerSignonCapability, [](CModule)
{
    DISPATCH_HOOK(EngineClientHooks, CClient::ProcessSignonState)
    DISPATCH_HOOK(EngineClientHooks, CClient::SendSignonState)
})

void DispatchClientAuthenticationHooks()
{
	DISPATCH_HOOK(EngineClientHooks, CClient::Connect)
	DISPATCH_HOOK(EngineClientHooks, CClient::ActivatePlayer)
	DISPATCH_HOOK(EngineClientHooks, CClient::Disconnect)
}

void ConfigureClientAuthentication(CModule module)
{
	// Disable retail FairFight marking and kicking authenticated players.
	module.Offset(0x101012).Patch("E9 90 00");
}
DECLARE_HOOK(CClient::Connect, engine.dll + 0x101740, [](auto& hook,
	CClient* self,
	char* pName,
	void* pNetChannel,
	char bFakePlayer,
	void* a5,
	char pDisconnectReason[256],
	void* a7) -> bool
{
	self->GetClientExtended()->Reset();
	const char* pAuthenticationFailure = nullptr;
	char pVerifiedName[64];

	if (!bFakePlayer)
	{
		if (!g_pServerAuthentication->VerifyPlayerName(pNextPlayerToken, pName, pVerifiedName))
			pAuthenticationFailure = "Invalid Name.";
		else if (!g_pBanSystem->IsUIDAllowed(iNextPlayerUid))
			pAuthenticationFailure = "Banned From server.";
		else if (!g_pServerAuthentication->CheckAuthentication(self, iNextPlayerUid, pNextPlayerToken))
			pAuthenticationFailure = "Authentication Failed.";
	}
	else
		strncpy_s(pVerifiedName, pName, 63);

	if (pAuthenticationFailure)
	{
		spdlog::info("{}'s (uid {}) connection was rejected: \"{}\"", pName, iNextPlayerUid, pAuthenticationFailure);
		strncpy_s(pDisconnectReason, 256, pAuthenticationFailure, 255);
		return false;
	}

	if (!hook.Original(self, pVerifiedName, pNetChannel, bFakePlayer, a5, pDisconnectReason, a7))
		return false;

	g_pServerAuthentication->AuthenticatePlayer(self, iNextPlayerUid, pNextPlayerToken);
	g_pServerAuthentication->AddPlayer(self, pNextPlayerToken);
	g_pServerLimits->AddPlayer(self);

	return true;
})
DECLARE_HOOK(CClient::ActivatePlayer, engine.dll + 0x100F80, [](auto& hook, CClient* self)
{
	// if we're authed, write our persistent data
	// RemovePlayerAuthData returns true if it removed successfully, i.e. on first call only, and we only want to write on >= second call
	// (since this func is called on map loads)
	if (self->m_iPersistenceReady >= ePersistenceReady::READY && !g_pServerAuthentication->RemovePlayerAuthData(self))
	{
		g_pServerAuthentication->m_bForceResetLocalPlayerPersistence = false;
		g_pServerAuthentication->WritePersistentData(self);
		g_pServerPresence->SetPlayerCount((int)g_pServerAuthentication->m_PlayerAuthenticationData.size());
	}

	hook.Original(self);
})
DECLARE_HOOK(CClient::Disconnect, engine.dll + 0x1012C0, [](auto& hook, CClient* self, uint32_t unknownButAlways1, const char* pReason, ...)
{
	char buf[4096] = {};
	if (hook.HasVarArgs())
	{
		va_list copied;
		va_copy(copied, *hook.VarArgs());
		vsnprintf_s(buf, sizeof(buf), _TRUNCATE, pReason, copied);
		va_end(copied);
	}
	else
	{
		_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s", pReason ? pReason : "");
	}

	// this reason is used while connecting to a local server, hacky, but just ignore it
	if (strcmp(pReason, "Connection closing"))
	{
		spdlog::info("Player {} disconnected: \"{}\"", self->m_szClientName, buf);

		// dcing, write persistent data
		if (g_pServerAuthentication->m_PlayerAuthenticationData[self].needPersistenceWriteOnLeave)
			g_pServerAuthentication->WritePersistentData(self);

		memset(self->m_PersistenceBuffer, 0, g_pServerAuthentication->m_PlayerAuthenticationData[self].pdataSize);
		g_pServerAuthentication->RemovePlayerAuthData(self);

		g_pServerAuthentication->RemovePlayer(self);
		g_pServerLimits->RemovePlayer(self);
	}

	g_pServerPresence->SetPlayerCount((int)g_pServerAuthentication->m_PlayerAuthenticationData.size());

	hook.Original(self, unknownButAlways1, "%s", buf);
})

size_t __fastcall ShouldAllowAlltalk()
{
    // this needs to return a 64 bit integer where 0 = true and 1 = false
    static ConVar* Cvar_sv_alltalk = g_pCVar->FindVar("sv_alltalk");
    if (Cvar_sv_alltalk->GetBool())
        return 0;

	// lobby should default to alltalk, otherwise don't allow it
	return strcmp(g_pGlobals->mapname, "mp_lobby");
}

CClient* AdjustShiftedThisPointer(CClient* shiftedPointer)
{
	/* Original function called method "CClient::ExecuteStringCommand" with an optimization
	 * that shifted the 'this' pointer with 8 bytes.
	 * Since this has been inlined with "CClient::ProcessStringCmd" as of S2, the shifting
	 * happens directly to anything calling this function. */
	char* pShifted = reinterpret_cast<char*>(shiftedPointer) - 8;
	return reinterpret_cast<CClient*>(pShifted);
}

DECLARE_HOOK(CClient_ProcessVoiceData, engine.dll + 0x104560, [](auto& hook, CClient* client, void* pMsg) -> bool
{
	auto pAdj = AdjustShiftedThisPointer(client);
	auto extended = pAdj->GetClientExtended();
	if (!extended)
	{
		spdlog::error("failed to get extended");
		return hook.Original(client, pMsg);
	}
	if (extended->IsClientCommsBanned())
	{
		return true;
	}
	return hook.Original(client, pMsg);
})
ON_DLL_LOAD_RELIESON("engine.dll", ServerAllTalk, ConVar, [](CModule module)
{
	DISPATCH_HOOK(EngineClientHooks, CClient_ProcessVoiceData)
	// replace strcmp function called in CClient::ProcessVoiceData with our own code that calls ShouldAllowAllTalk
	CMemory base = module.Offset(0x1085FA);

	base.Patch("48 B8"); // mov rax, 64 bit int
	// (uint8_t*)&ShouldAllowAlltalk doesn't work for some reason? need to make it a uint64 first
	uint64_t pShouldAllowAllTalk = reinterpret_cast<uint64_t>(ShouldAllowAlltalk);
	base.Offset(0x2).Patch((uint8_t*)&pShouldAllowAllTalk, 8);
	base.Offset(0xA).Patch("FF D0"); // call rax

	// nop until compare (test eax, eax)
	base.Offset(0xC).NoOP(0x7);
})
