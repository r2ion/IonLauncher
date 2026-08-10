#include "serverauthentication.h"
#include "engine/shared/exploit_fixes/ns_limits.h"
#include "tier1/cvar.h"
#include "masterserver/masterserver.h"
#include "server/serverpresence.h"
#include "engine/hoststate.h"
#include "bansystem.h"
#include "core/convar/concommand.h"
#include "dedicated/dedicated.h"
#include "config/profile.h"
#include "core/tier0.h"
#include "engine/client/client.h"
#include "engine/client/clientstate.h"
#include "server/r2server.h"
#include "engine/r2engine.h"

#include <fstream>
#include <filesystem>
#include <string>
#include <thread>

// global vars
ServerAuthenticationManager* g_pServerAuthentication;
CServer__RejectConnectionType CServer__RejectConnection;

void ServerAuthenticationManager::AddRemotePlayer(std::string token, uint64_t uid, std::string username, std::string pdata)
{
	std::string uidS = std::to_string(uid);

	RemoteAuthData newAuthData {};
	strncpy_s(newAuthData.uid, sizeof(newAuthData.uid), uidS.c_str(), uidS.length());
	strncpy_s(newAuthData.username, sizeof(newAuthData.username), username.c_str(), username.length());
	newAuthData.pdata = new char[pdata.length()];
	newAuthData.pdataSize = pdata.length();
	memcpy(newAuthData.pdata, pdata.c_str(), newAuthData.pdataSize);

	std::lock_guard<std::mutex> guard(m_AuthDataMutex);
	m_RemoteAuthenticationData[token] = newAuthData;
}

void ServerAuthenticationManager::AddPlayer(CClient* pPlayer, const char* pToken)
{
	PlayerAuthenticationData additionalData;

	if (!pToken)
	{
		additionalData.pdataSize = PERSISTENCE_MAX_SIZE;
		additionalData.usingLocalPdata = pPlayer->m_iPersistenceReady == ePersistenceReady::READY_INSECURE;
		m_PlayerAuthenticationData.insert(std::make_pair(pPlayer, additionalData));
		return;
	}

	auto remoteAuthData = m_RemoteAuthenticationData.find(pToken);
	if (remoteAuthData != m_RemoteAuthenticationData.end())
		additionalData.pdataSize = remoteAuthData->second.pdataSize;
	else
		additionalData.pdataSize = PERSISTENCE_MAX_SIZE;

	additionalData.usingLocalPdata = pPlayer->m_iPersistenceReady == ePersistenceReady::READY_INSECURE;

	m_PlayerAuthenticationData.insert(std::make_pair(pPlayer, additionalData));
}

void ServerAuthenticationManager::RemovePlayer(CClient* pPlayer)
{
	if (m_PlayerAuthenticationData.count(pPlayer))
		m_PlayerAuthenticationData.erase(pPlayer);
}

bool ServerAuthenticationManager::VerifyPlayerName(const char* pAuthToken, const char* pName, char pOutVerifiedName[64])
{
	std::lock_guard<std::mutex> guard(m_AuthDataMutex);

	// always use name from masterserver if available
	// use of strncpy_s here should verify that this is always nullterminated within valid buffer size
	auto authData = m_RemoteAuthenticationData.find(pAuthToken);
	if (authData != m_RemoteAuthenticationData.end() && *authData->second.username)
		strncpy_s(pOutVerifiedName, 64, authData->second.username, 63);
	else
		strncpy_s(pOutVerifiedName, 64, pName, 63);

	// now, check that whatever name we have is actually valid
	// first, make sure it's >1 char
	if (!*pOutVerifiedName)
		return false;

	// next, make sure it's within a valid range of ascii characters
	for (int i = 0; pOutVerifiedName[i]; i++)
	{
		if (pOutVerifiedName[i] < 32 || pOutVerifiedName[i] > 126)
			return false;
	}

	return true;
}

bool ServerAuthenticationManager::IsDuplicateAccount(CClient* pPlayer, const char* pPlayerUid)
{
	if (m_bAllowDuplicateAccounts)
		return false;

	bool bHasUidPlayer = false;
	for (int i = 0; i < g_pGlobals->m_nMaxClients; i++)
		if (&g_pClientArray[i] != pPlayer && !strcmp(pPlayerUid, g_pClientArray[i].m_szPlatformID))
			return true;

	return false;
}

bool ServerAuthenticationManager::CheckAuthentication(CClient* pPlayer, uint64_t iUid, char* pAuthToken)
{
	std::string sUid = std::to_string(iUid);

	// check whether this player's authentication is valid but don't actually write anything to the player, we'll do that later
	// if we don't need auth this is valid
	if (Cvar_ns_auth_allow_insecure->GetBool())
		return true;

	// local server that doesn't need auth (probably sp) and local player
	if (m_bStartingLocalSPGame && !strcmp(sUid.c_str(), g_pLocalPlayerUserID))
		return true;

	// don't allow duplicate accounts
	if (IsDuplicateAccount(pPlayer, sUid.c_str()))
		return false;

	std::lock_guard<std::mutex> guard(m_AuthDataMutex);
	auto authData = m_RemoteAuthenticationData.find(pAuthToken);
	if (authData != m_RemoteAuthenticationData.end() && !strcmp(sUid.c_str(), authData->second.uid))
		return true;

	return false;
}

void ServerAuthenticationManager::AuthenticatePlayer(CClient* pPlayer, uint64_t iUid, char* pAuthToken)
{
	// for bot players, generate a new uid
	if (pPlayer->m_bFakePlayer)
		iUid = 0; // is this a good way of doing things :clueless:
	std::string sUid = std::to_string(iUid);

	// copy uuid
	strcpy(pPlayer->m_szPlatformID, sUid.c_str());

	std::lock_guard<std::mutex> guard(m_AuthDataMutex);
	if (!pAuthToken)
	{
		pPlayer->m_iPersistenceReady = ePersistenceReady::READY_INSECURE;
		return;
	}
	auto authData = m_RemoteAuthenticationData.find(pAuthToken);
	if (authData != m_RemoteAuthenticationData.end())
	{
		// if we're resetting let script handle the reset with InitPersistentData() on connect
		if (!m_bForceResetLocalPlayerPersistence || strcmp(sUid.c_str(), g_pLocalPlayerUserID))
		{
			// copy pdata into buffer
			memcpy(pPlayer->m_PersistenceBuffer, authData->second.pdata, authData->second.pdataSize);
		}

		// set persistent data as ready
		pPlayer->m_iPersistenceReady = ePersistenceReady::READY_REMOTE;
	}
	// we probably allow insecure at this point, but make sure not to write anyway if not insecure
	else if (Cvar_ns_auth_allow_insecure->GetBool() || pPlayer->m_bFakePlayer)
	{
		// set persistent data as ready
		// note: actual placeholder persistent data is populated in script with InitPersistentData()
		pPlayer->m_iPersistenceReady = ePersistenceReady::READY_INSECURE;
	}
}

bool ServerAuthenticationManager::RemovePlayerAuthData(CClient* pPlayer)
{
	if (!Cvar_ns_erase_auth_info->GetBool()) // keep auth data forever
		return false;

	// hack for special case where we're on a local server, so we erase our own newly created auth data on disconnect
	if (m_bNeedLocalAuthForNewgame && !strcmp(pPlayer->m_szPlatformID, g_pLocalPlayerUserID))
		return false;

	// we don't have our auth token at this point, so lookup authdata by uid
	for (auto& auth : m_RemoteAuthenticationData)
	{
		if (!strcmp(pPlayer->m_szPlatformID, auth.second.uid))
		{
			// pretty sure this is fine, since we don't iterate after the erase
			// i think if we iterated after it'd be undefined behaviour tho
			std::lock_guard<std::mutex> guard(m_AuthDataMutex);

			delete[] auth.second.pdata;
			m_RemoteAuthenticationData.erase(auth.first);
			return true;
		}
	}

	return false;
}

void ServerAuthenticationManager::WritePersistentData(CClient* pPlayer)
{
	if (pPlayer->m_iPersistenceReady == ePersistenceReady::READY_REMOTE)
	{
		g_pMasterServerManager->WritePlayerPersistentData(
			pPlayer->m_szPlatformID, (const char*)pPlayer->m_PersistenceBuffer, m_PlayerAuthenticationData[pPlayer].pdataSize);
	}
	else if (Cvar_ns_auth_allow_insecure_write->GetBool())
	{
		// todo: write pdata to disk here
	}
}

// auth hooks

// store these in vars so we can use them in CBaseClient::Connect
// this is fine because ptrs won't decay by the time we use this, just don't use it outside of calls from cbaseclient::connectclient
char* pNextPlayerToken;
uint64_t iNextPlayerUid;

ConVar* Cvar_ns_allowuserclantags;

void ConCommand_ns_resetpersistence(const CCommand& args)
{
	NOTE_UNUSED(args);
	if (*g_pServerState == server_state_t::ss_active)
	{
		spdlog::error("ns_resetpersistence must be entered from the main menu");
		return;
	}

	spdlog::info("resetting persistence on next lobby load...");
	g_pServerAuthentication->m_bForceResetLocalPlayerPersistence = true;
}

DECLARE_MODULE(ServerAuthenticationHooks)
DECLARE_HOOK(CServer::ConnectClient, engine.dll + 0x114430, [](auto& hook,
	void* self,
	void* addr,
	void* a3,
	uint32_t a4,
	uint32_t a5,
	int32_t a6,
	void* a7,
	char* playerName,
	char* serverFilter,
	void* a10,
	char a11,
	void* a12,
	char a13,
	char a14,
	int64_t uid,
	uint32_t a16,
	uint32_t a17) -> void*
{
	// auth tokens are sent with serverfilter, can't be accessed from player struct to my knowledge, so have to do this here
	pNextPlayerToken = serverFilter;
	iNextPlayerUid = uid;

	return hook.Original(self, addr, a3, a4, a5, a6, a7, playerName, serverFilter, a10, a11, a12, a13, a14, uid, a16, a17);
})
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

ON_DLL_LOAD_RELIESON("engine.dll", ServerAuthentication, (ConCommand, ConVar), [](CModule module)
{
	DISPATCH_MODULE(ServerAuthenticationHooks)
	g_pServerAuthentication = new ServerAuthenticationManager;

	g_pServerAuthentication->Cvar_ns_erase_auth_info =
		new ConVar("ns_erase_auth_info", "1", FCVAR_GAMEDLL, "Whether auth info should be erased from this server on disconnect or crash");
	g_pServerAuthentication->Cvar_ns_auth_allow_insecure =
		new ConVar("ns_auth_allow_insecure", "0", FCVAR_GAMEDLL, "Whether this server will allow unauthenicated players to connect");
	g_pServerAuthentication->Cvar_ns_auth_allow_insecure_write = new ConVar(
		"ns_auth_allow_insecure_write",
		"0",
		FCVAR_GAMEDLL,
		"Whether the pdata of unauthenticated clients will be written to disk when changed");

	RegisterConCommand(
		"ns_resetpersistence", ConCommand_ns_resetpersistence, "resets your pdata when you next enter the lobby", FCVAR_NONE);

	// patch to disable kicking based on incorrect serverfilter in connectclient, since we repurpose it for use as an auth token
	module.Offset(0x114655).Patch("EB");

	// patch to disable fairfight marking players as cheaters and kicking them
	module.Offset(0x101012).Patch("E9 90 00");

	CServer__RejectConnection = module.Offset(0x1182E0).RCast<CServer__RejectConnectionType>();

	if (CommandLine()->CheckParm("-allowdupeaccounts"))
	{
		// patch to allow same of multiple account
		module.Offset(0x114510).Patch("EB");

		g_pServerAuthentication->m_bAllowDuplicateAccounts = true;
	}
})
