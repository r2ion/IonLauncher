#include "connect.h"
#include "core/vanilla.h"
#include "client/r2client.h"
#include "masterserver/masterserver.h"
#include "server/auth/serverauthentication.h"
#include "engine/r2engine.h"
#include "core/tier0.h"
#include "vscript/squirrel/squirrel.h"
#include "engine/models.h"

AUTOHOOK_INIT()

void(__fastcall* SCR_BeginLoadingPlaque)(const char* levelName) = nullptr;
void(__fastcall* SCR_EndLoadingPlaque)() = nullptr;

bool g_bConnectingToServer = false;
bool g_bRetryingConnection = false;

ConnectionManager* g_pConnectionManager = nullptr;

ConVar* Cvar_cl_resend_inforequest_timeout = nullptr;
ConVar* Cvar_cl_resend_inforequest_timeout_remote = nullptr;
ConVar* Cvar_cl_resend_inforequest_interval_ms = nullptr;
ConVar* Cvar_cl_unload_remote_mods_on_matchmaking = nullptr;

void ConnectionManager::Connect(bool useSCRPlaque, std::string mapName)
{
    const char* mp_gamemode = g_pCVar->FindVar("mp_gamemode") ? g_pCVar->FindVar("mp_gamemode")->GetString() : "";
    bool isSolo = (strcmp(mp_gamemode, "solo") == 0);
    m_bSolo = isSolo;

    if (useSCRPlaque && !isSolo)
        SCR_BeginLoadingPlaque(nullptr);

    if (m_bConnecting)
        return;

    ResetState();

    m_szMapName    = mapName;
    m_bUseSCRPlaque = useSCRPlaque;
    m_bConnecting  = true;
    m_eLastMode    = m_eCurrentMode;
    m_eCurrentMode = eConnectionMode::LocalServer;

    InvokeConnectionStartCallbacks();

    ConnectToLocalServer();
}

void ConnectionManager::Connect(const std::string& address, const std::string& password, bool useSCRPlaque, std::string mapName)
{
    const char* mp_gamemode = g_pCVar->FindVar("mp_gamemode") ? g_pCVar->FindVar("mp_gamemode")->GetString() : "";
    bool isSolo = (strcmp(mp_gamemode, "solo") == 0);
    m_bSolo = isSolo;

    if (useSCRPlaque && !isSolo)
        SCR_BeginLoadingPlaque(nullptr);

    if (m_bConnecting)
        return;

    ResetState();

    m_szMapName        = mapName;
    m_bUseSCRPlaque    = useSCRPlaque;
    m_bConnecting      = true;
    m_eLastMode        = m_eCurrentMode;
    m_eCurrentMode     = eConnectionMode::RemoteServer;
    m_szLastServerID   = address;
    m_szLastServerPassword = password;

    InvokeConnectionStartCallbacks();

    ConnectToRemoteServer(address, password);
}

void ConnectionManager::Connect(const std::string& address, ConnectionManager::eConnectionMode mode, bool useSCRPlaque, std::string mapName)
{
    const char* mp_gamemode = g_pCVar->FindVar("mp_gamemode") ? g_pCVar->FindVar("mp_gamemode")->GetString() : "";
    bool isSolo = (strcmp(mp_gamemode, "solo") == 0);
    m_bSolo = isSolo;

    if (useSCRPlaque && !isSolo)
        SCR_BeginLoadingPlaque(nullptr);

    if (m_bConnecting)
        return;

    ResetState();

    m_szMapName     = mapName;
    m_bUseSCRPlaque = useSCRPlaque;
    m_bConnecting   = true;
    m_eLastMode     = m_eCurrentMode;
    m_eCurrentMode  = mode;

    InvokeConnectionStartCallbacks();

    switch (mode)
    {
    case eConnectionMode::LocalServer:
        ConnectToLocalServer();
        break;
    case eConnectionMode::RemoteServer:
        ConnectToRemoteServer(m_szLastServerID, m_szLastServerPassword);
        break;
    case eConnectionMode::P2P:
        ConnectToP2PServer(address);
        break;
    case eConnectionMode::Direct:
        m_bConnecting = false;
		g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Northstar);
        if (!m_bRetrying)
            Cbuf_AddText(
                Cbuf_GetCurrentPlayer(),
                fmt::format("connect \"{}\"", address).c_str(),
                cmd_source_t::kCommandSrcCode);
        else
            Cbuf_AddText(
                Cbuf_GetCurrentPlayer(),
                "retry",
                cmd_source_t::kCommandSrcCode);
        break;
    default:
        Interrupt("Unknown connection mode");
        break;
    }
}

void ConnectionManager::InvokeConnectionStartCallbacks()
{
	bool scriptManagedLoading = !m_bUseSCRPlaque && !m_bSolo;

	g_pSquirrel[ScriptContext::UI]->AsyncCall("NSUICodeCallback_ConnectionStarted", scriptManagedLoading);
}

void ConnectionManager::InvokeConnectionMessageCallbacks(const std::string& message)
{
	g_pSquirrel[ScriptContext::UI]->AsyncCall("NSUICodeCallback_ConnectionMessage", message.c_str());
}

void ConnectionManager::InvokeConnectionStoppedCallbacks(std::string reason)
{
	g_pSquirrel[ScriptContext::UI]->AsyncCall("NSUICodeCallback_ConnectionStopped", reason.c_str());
}

bool ConnectionManager::ParseAddress(const std::string& address, std::string& ip, int& port, bool& isV6)
{
    ip.clear();
    port = -1;
    isV6 = false;

    std::string s = address;

    // trim
    const auto begin = s.find_first_not_of(" \t");
    if (begin == std::string::npos)
        return false;
    const auto end = s.find_last_not_of(" \t");
    s = s.substr(begin, end - begin + 1);

    if (s.empty())
        return false;

    // [v6] or [v6]:port
    if (s.front() == '[')
    {
        const auto close = s.find(']');
        if (close == std::string::npos)
            return false;

        ip = s.substr(1, close - 1);

        if (close + 1 < s.size() && s[close + 1] == ':')
        {
            const std::string portStr = s.substr(close + 2);
            if (!portStr.empty())
            {
                try { port = std::stoi(portStr); }
                catch (...) { port = -1; }
            }
        }
    }
    else
    {
        // host:port (single ':') OR bare v6 / v4
        const auto firstColon = s.find(':');
        const auto lastColon  = s.rfind(':');

        if (firstColon != std::string::npos && firstColon == lastColon)
        {
            ip = s.substr(0, firstColon);
            const std::string portStr = s.substr(firstColon + 1);
            if (!portStr.empty())
            {
                try { port = std::stoi(portStr); }
                catch (...) { port = -1; }
            }
        }
        else
        {
            // bare IP (v4 or v6, or v4-mapped)
            ip = s;
        }
    }

    if (ip.empty())
        return false;

    // Decide if it's real IPv6. Treat ::ffff:a.b.c.d as IPv4 (not v6).
    if (ip.find(':') != std::string::npos)
    {
        const std::string prefix = "::ffff:";
        if (ip.rfind(prefix, 0) == 0)
        {
            std::string rest = ip.substr(prefix.size());
            // v4-mapped form ::ffff:a.b.c.d -> NOT v6
            if (!(rest.find('.') != std::string::npos && rest.find(':') == std::string::npos))
                isV6 = true; // some other IPv6, or non‑standard form
        }
        else
        {
            isV6 = true; // normal IPv6
        }
    }

    return true;
}

ConnectionManager::eConnectionMode ConnectionManager::DetermineModeFromAddress(const std::string& address)
{
	if(address.find("localhost") != std::string::npos)
		return eConnectionMode::LocalServer;

	std::string ip;
	int port;
	bool isV6;

	if(!ParseAddress(address, ip, port, isV6))
		return eConnectionMode::Direct;

	if(isV6)
		return eConnectionMode::P2P;

	return eConnectionMode::Direct;
}

void ConnectionManager::AuthenticateToMasterServer()
{
	if (g_pMasterServerManager->m_bOriginAuthWithMasterServerDone || g_pMasterServerManager->m_bOriginAuthWithMasterServerInProgress)
		return;

	int agreedToSendToken = g_pCVar->FindVar("ns_has_agreed_to_send_token")->GetInt();
	if (agreedToSendToken != AGREED_TO_SEND_TOKEN)
		return;

	UpdateMessage("#DIALOG_AUTHENTICATING_MASTERSERVER");

	float startTime = Plat_FloatTime();
	float timeOut = g_pCVar->FindVar("cl_resend_timeout")->GetFloat();

	g_pMasterServerManager->AuthenticateOriginWithMasterServer();

	while (!g_pMasterServerManager->m_bOriginAuthWithMasterServerDone && Plat_FloatTime() - startTime < timeOut && !IsCancelled())
		Sleep(100);

	RETURN_IF_CANCELLED()

	UpdateMessage();

	if (!g_pMasterServerManager->m_bOriginAuthWithMasterServerDone)
		Interrupt("#AUTHENTICATION_FAILED_BODY");
	else
		spdlog::info("Successfully authenticated with master server for origin auth");
}

void ConnectionManager::SendInfoRequestPacket(const CNetAdr& addr, bool serverAuthUs, bool requestMods)
{
    g_bReceivedServerInfo = false;
    if (serverAuthUs)
        g_bReceivedAuthNotify = false;

	char buffer[256];
	bf_write msg(buffer, sizeof(buffer));
	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteChar(A2S_REQUESTCUSTOMSERVERINFO);
	msg.WriteLong(CUSTOMSERVERINFO_VERSION);
	msg.WriteByte(requestMods);
	msg.WriteLong(MODDOWNLOADINFO_VERSION);
	msg.WriteByte(serverAuthUs);

	if(serverAuthUs)
	{
		msg.WriteString(g_pLocalPlayerUserID);
		msg.WriteString(g_pMasterServerManager->m_sOwnClientAuthToken);
	}

	NET_SendPacket(nullptr, NS_CLIENT, &addr, msg.GetData(), msg.GetNumBytesWritten(), nullptr, false, 0, true);

	float startTime = Plat_FloatTime();
	float timeOut = g_pCVar->FindVar("cl_resend_inforequest_timeout")->GetFloat();

	if(m_eCurrentMode == eConnectionMode::RemoteServer)
		timeOut = g_pCVar->FindVar("cl_resend_inforequest_timeout_remote")->GetFloat();

	UpdateMessage("#REQUESTING_CUSTOM_SERVER_INFO");

	while(!g_bReceivedServerInfo && !IsCancelled() && Plat_FloatTime() - startTime < timeOut)
	{
		int retryInterval = g_pCVar->FindVar("cl_resend_inforequest_interval_ms")->GetInt();
		NET_SendPacket(nullptr, NS_CLIENT, &addr, msg.GetData(), msg.GetNumBytesWritten(), nullptr, false, 0, true);
		Sleep(retryInterval);
	}


	startTime = Plat_FloatTime();

	if(serverAuthUs)
	{
		while(!g_bReceivedAuthNotify && Plat_FloatTime() - startTime < timeOut && !IsCancelled())
			Sleep(100);

		RETURN_IF_CANCELLED()

		if(!g_bReceivedAuthNotify)
			Interrupt("#AUTHENTICATION_FAILED_BODY");
	}
}

void ConnectionManager::DownloadMods(bool remoteServer, RemoteServerInfo* info)
{
	int unverifiedModCount = g_pModDownloader->GetTotalServerRequestedMods();
	std::vector<ModDownloader::modentry_s> unverifiedMods = g_pModDownloader->GetServerRequestedMods();

	bool needToDownloadMods = false;

	UpdateMessage("#CHECKING_REQUIRED_MODS");

	for(const auto& mod : unverifiedMods)
	{
		bool found = false;

		for(auto& existingMod : g_pModManager->m_LoadedMods)
		{
			if(existingMod.Name == mod.name )
			{
				if(existingMod.IsCoreMod())
				{
					found = true;
					break;
				}

				if(existingMod.Version == mod.version)
				{
					found = true;
					break;
				}
			}
		}

		if(!found)
		{
			needToDownloadMods = true;
			break;
		}
	}

	for(RemoteModInfo& mod : info->requiredMods)
	{
		bool found = false;

		for(auto& existingMod : g_pModManager->m_LoadedMods)
		{
			if(existingMod.Name == mod.Name )
			{
				if(existingMod.IsCoreMod())
				{
					found = true;
					break;
				}

				if(existingMod.Version == mod.Version)
				{
					found = true;
					break;
				}
			}
		}

		if(!found)
		{
			needToDownloadMods = true;
			break;
		}
	}

	if(!needToDownloadMods)
		return;

	if(!m_bUseSCRPlaque && unverifiedModCount > 0)
	{
		g_pSquirrel[ScriptContext::UI]->AsyncCall("NSUICodeCallback_ConfirmDownloadMods", unverifiedModCount, info->name);
		while(m_eModAcceptState == eModAcceptState::NOT_DECIDED && !IsCancelled())
			Sleep(100);

		if(m_eModAcceptState == eModAcceptState::DENIED)
			Interrupt();

		RETURN_IF_CANCELLED()
	}

	g_pSquirrel[ScriptContext::UI]->AsyncCall("NSUICodeCallback_DownloadingModsStarted");

	for(const auto& mod : info->requiredMods)
	{
		UpdateMessage("#DOWNLOADING_MOD_TEXT", mod.Name, mod.Version);

		bool found = false;

		for(auto& existingMod : g_pModManager->m_LoadedMods)
		{
			if(existingMod.Name == mod.Name )
			{
				if(existingMod.IsCoreMod())
				{
					found = true;
					break;
				}

				if(existingMod.Version == mod.Version)
				{
					found = true;
					break;
				}
			}
		}

		if(found)
			continue;

		spdlog::info("Auto-downloading mod {} version {}", mod.Name, mod.Version);
		g_pModDownloader->DownloadMod(mod.Name, mod.Version);
		m_bDownloadedMods = true;

        while (!IsCancelled())
        {
            auto state = g_pModDownloader->modState.state;
            if (state != ModDownloader::DOWNLOADING &&
                state != ModDownloader::CHECKSUMING &&
                state != ModDownloader::EXTRACTING)
            {
                break;
            }

            Sleep(100);
        }

		RETURN_IF_CANCELLED()

		bool downloadFailed = true;
		bool downloadAborted = false;
		std::string interruptMessage;

		switch(g_pModDownloader->modState.state)
		{
			case ModDownloader::DONE:
				downloadFailed = false;
				break;
			case ModDownloader::ABORTED:
				downloadAborted = true;
				downloadFailed = false;
				break;
			case ModDownloader::FAILED_READING_ARCHIVE:
				interruptMessage = "#FAILED_READING_ARCHIVE";
				break;
			case ModDownloader::FAILED_WRITING_TO_DISK:
				interruptMessage = "#FAILED_WRITING_TO_DISK";
				break;
			case ModDownloader::MOD_FETCHING_FAILED:
				interruptMessage = "#MOD_FETCHING_FAILED";
				break;
			case ModDownloader::MOD_CORRUPTED: // Downloaded archive checksum does not match verified hash
				interruptMessage = "#MOD_CORRUPTED";
				break;
			case ModDownloader::NO_DISK_SPACE_AVAILABLE:
				interruptMessage = "#NO_DISK_SPACE_AVAILABLE";
				break;
			case ModDownloader::UNKNOWN_PLATFORM:
				interruptMessage = "#MOD_FETCHING_FAILED_GENERAL";
				break;
			default:
				break;
		}

		if(downloadAborted)
			Interrupt();

		if(downloadFailed)
			Interrupt(fmt::format("Download for {} v{} failed: {}", mod.Name, mod.Version, interruptMessage));

		RETURN_IF_CANCELLED()
	}

	g_pSquirrel[ScriptContext::UI]->AsyncCall("NSUICodeCallback_DownloadingModsStopped");

	RETURN_IF_CANCELLED()

	UpdateMessage();
}

void ConnectionManager::ConnectToRemoteServer(const std::string& id, const std::string& password)
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Northstar);

	std::string serverId = id;
	std::string serverPassword = password;

	std::thread authThread([&, serverId, serverPassword]()
		{
			AuthenticateToMasterServer();

			RETURN_IF_CANCELLED();

			g_pMasterServerManager->RequestServerList();

			UpdateMessage("#REQUESTING_SERVERS");

			while(g_pMasterServerManager->m_bScriptRequestingServerList && !IsCancelled())
				Sleep(100);

			RETURN_IF_CANCELLED()

			spdlog::info("Authenticating with remote server for uid {}", g_pLocalPlayerUserID);

			m_flConnectionStartTime = Plat_FloatTime();
			float maxTime = g_pCVar->FindVar("cl_resend_timeout")->GetFloat();

			RemoteServerInfo* serverInfo = nullptr;

			for (auto& server : g_pMasterServerManager->m_vRemoteServers)
			{
				if (std::string(server.id) == serverId)
				{
					serverInfo = &server;
					break;
				}
			}

			if (!serverInfo)
			{
				spdlog::error("Failed to find server info for id {}", serverId);
				Interrupt("Failed to authenticate with remote server: server not found");
				return;
			}

			g_pMasterServerManager->AuthenticateWithServer(
				g_pLocalPlayerUserID,
				g_pMasterServerManager->m_sOwnClientAuthToken,
				*serverInfo,
				serverPassword.c_str());

			UpdateMessage("#DIALOG_SERVERCONNECTING_MSG", serverInfo->name);

			while (!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer &&
				   Plat_FloatTime() - g_pConnectionManager->m_flConnectionStartTime < maxTime && !IsCancelled())
				Sleep(100);

			RETURN_IF_CANCELLED()

			if (!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer)
			{
				spdlog::error("Timed out authenticating with remote server for uid {}", g_pLocalPlayerUserID);
				Interrupt("Failed to authenticate with remote server: timeout");
				return;
			}

			m_bAuthSucessful = true;

			RemoteServerConnectionInfo& info = g_pMasterServerManager->m_pendingConnectionInfo;
			g_pCVar->FindVar("serverfilter")->SetValue(info.authToken);

			std::string ip = fmt::format(
				"{}.{}.{}.{}",
				info.ip.S_un.S_un_b.s_b1,
				info.ip.S_un.S_un_b.s_b2,
				info.ip.S_un.S_un_b.s_b3,
				info.ip.S_un.S_un_b.s_b4);
			int port = info.port;

			std::string address = fmt::format("{}:{}", ip, port);

			if(!g_pCVar->FindVar("allow_mod_auto_download")->GetBool())
			{
				FinaliseJoiningServer(address);
				return;
			}

			RETURN_IF_CANCELLED()

			UpdateMessage("#MANIFESTO_FETCHING_TEXT");
			g_pModDownloader->FetchModsListFromAPI();

			while(g_pModDownloader->modState.state == ModDownloader::MANIFESTO_FETCHING && !IsCancelled())
				Sleep(100);

			RETURN_IF_CANCELLED()

			std::string netAdr = fmt::format("[::ffff:{}]:{}", ip, port);
			SendInfoRequestPacket(CNetAdr(netAdr.c_str()), false, true);

			RETURN_IF_CANCELLED()

			DownloadMods(true, serverInfo);

			RETURN_IF_CANCELLED()

			ReloadMods(serverInfo);

			RETURN_IF_CANCELLED()

			FinaliseJoiningServer(address);
		});

	authThread.detach();
}

void ConnectionManager::ConnectToP2PServer(const std::string& address)
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Northstar);

	std::thread authThread([&, address]()
		{
			AuthenticateToMasterServer();

			RETURN_IF_CANCELLED()

			spdlog::info("Authenticating with P2P server for uid {}", g_pLocalPlayerUserID);

			std::string ip;
			int port;
			bool isV6;

			if(!ParseAddress(address, ip, port, isV6) || !isV6 || port == -1 || ip.empty())
			{
				Interrupt("Failed to authenticate with P2P server: invalid p2p address");
				return;
			}

			std::string formattedIP = fmt::format("[{}]:{}", ip, port);

			spdlog::info("P2P server address parsed as {}", formattedIP);

			in_addr6 addr6;

			CNetAdr addr = CNetAdr(formattedIP.c_str());

			SendInfoRequestPacket(addr, true, false);

			RETURN_IF_CANCELLED()

			FinaliseJoiningServer(formattedIP);
		});

	authThread.detach();
}

void ConnectionManager::ReloadMods(RemoteServerInfo* info)
{
    UpdateMessage("#RELOADING_MODS");

    bool shouldReloadMods = false;

	if(m_bDownloadedMods)
		shouldReloadMods = true;

    for (Mod& loaded : g_pModManager->m_LoadedMods)
    {
        bool wasEnabled = loaded.m_bEnabled;
        bool isRequired = false;

        for (const auto& required : info->requiredMods)
        {
            if (loaded.Name != required.Name)
                continue;

            if (loaded.IsCoreMod() || loaded.Version == required.Version)
            {
                isRequired = true;
                break;
            }
        }

        if (isRequired)
            loaded.m_bEnabled = true;
        else if (loaded.RequiredOnClient)
            loaded.m_bEnabled = false;

        // Only trigger reload if we had to enable a RequiredOnClient mod
        if (!wasEnabled && loaded.m_bEnabled && loaded.RequiredOnClient)
            shouldReloadMods = true;
    }

    if (shouldReloadMods)
	{
    	g_pModManager->LoadMods();

    	Cbuf_AddText(
    	    Cbuf_GetCurrentPlayer(),
    	    "reload_localization; loadPlaylists; weapon_reparse; playerSettings_reparse; uiscript_reset",
    	    cmd_source_t::kCommandSrcCode);

    	Cbuf_Execute();
	}

    if(m_bRetrying)
        Sleep(500); // going too fast here can cause the ui to just not ever start
}

void ConnectionManager::FinaliseJoiningServer(std::string& address)
{
	if(m_bRetrying)
	{
		Retrying(false);
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "retry", cmd_source_t::kCommandSrcCode);
	}
	else
	{
		std::string command;

		Cbuf_AddText(
			Cbuf_GetCurrentPlayer(),
			fmt::format("connect {}", address).c_str(),
		 	cmd_source_t::kCommandSrcCode);
	}
}

void ConnectionManager::FinaliseJoiningLocalServer()
{
	if(m_bRetrying)
	{
		Retrying(false);
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "retry", cmd_source_t::kCommandSrcCode);
	}
	else
	{
		std::string command;

		if(!m_szMapName.empty())
			Cbuf_AddText(
				Cbuf_GetCurrentPlayer(),
				fmt::format("map {}", m_szMapName).c_str(),
				cmd_source_t::kCommandSrcCode);
		else
			Cbuf_AddText(
				Cbuf_GetCurrentPlayer(),
				"map mp_lobby",
				cmd_source_t::kCommandSrcCode);
	}
}

void ConnectionManager::ConnectToLocalServer()
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Northstar);

	std::thread authThread([&]()
		{
			AuthenticateToMasterServer();

			RETURN_IF_CANCELLED();

			spdlog::info("Authenticating with own server for uid {}", g_pLocalPlayerUserID);

			m_flConnectionStartTime = Plat_FloatTime();
			float maxTime = g_pCVar->FindVar("cl_resend_timeout")->GetFloat();

			g_pMasterServerManager->AuthenticateWithOwnServer(
				g_pLocalPlayerUserID,
				g_pMasterServerManager->m_sOwnClientAuthToken,
				{});

			UpdateMessage("Getting account data.");

			while(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer &&
				  Plat_FloatTime() - g_pConnectionManager->m_flConnectionStartTime < maxTime && !IsCancelled())
				Sleep(100);

			RETURN_IF_CANCELLED()

			if(!g_pMasterServerManager->m_bSuccessfullyAuthenticatedWithGameServer)
			{
				spdlog::error("Timed out authenticating with own server for uid {}", g_pLocalPlayerUserID);
				Interrupt("Failed to authenticate with own server: timeout");
				return;
			}

			if (g_pServerAuthentication->m_RemoteAuthenticationData.size())
				g_pCVar->FindVar("serverfilter")->SetValue(g_pServerAuthentication->m_RemoteAuthenticationData.begin()->first.c_str());

			m_bAuthSucessful = true;

			RETURN_IF_CANCELLED()

			FinaliseJoiningLocalServer();
		});

	authThread.detach();
}

// clang-format off
AUTOHOOK(matchmake, engine.dll + 0xF220, int*, __fastcall, ())
// clang-format on
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Vanilla);
	g_pConnectionManager->SetMatchmaking();

	if(Cvar_cl_unload_remote_mods_on_matchmaking->GetBool() && !g_pConnectionManager->UnloadingRemoteModsOnMatchmaking())
	{
		std::thread unloadThread([]()
			{
				g_pConnectionManager->SetUnloadingRemoteModsOnMatchmaking(true);

				spdlog::info("Unloading remote requiredOnClient mods for matchmaking");

				int affectedMods = 0;

				for (auto& loaded : g_pModManager->m_LoadedMods)
				{
					if (loaded.RequiredOnClient && loaded.m_bIsRemote && loaded.m_bEnabled && !loaded.IsCoreMod())
					{
						affectedMods++;
						loaded.m_bEnabled = false;
					}
				}

				if( affectedMods > 0)
				{
					g_pModManager->LoadMods();
					Cbuf_AddText(
						Cbuf_GetCurrentPlayer(),
						"reload_localization; loadPlaylists; weapon_reparse; playerSettings_reparse; uiscript_reset; matchmake",
						cmd_source_t::kCommandSrcCode);
				}

				Cbuf_AddText(
					Cbuf_GetCurrentPlayer(),
					"matchmake",
					cmd_source_t::kCommandSrcCode);

				Cbuf_Execute();
			});
		unloadThread.detach();
		return 0;
	}

	return matchmake();
}

// clang-format off
AUTOHOOK(silentconnect, engine.dll + 0x76F00, int*, __fastcall, (__int64 a1))
// clang-format on
{
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Vanilla);
	g_pConnectionManager->SetMatchmaking();

	return silentconnect(a1);
}

//clang-format off
AUTOHOOK(Host_Disconnect, engine.dll + 0x15ABE0, void, __fastcall, (bool bShowMainMenu))
//clang-format on
{
	g_pConnectionManager->Retrying(false);
	g_pConnectionManager->Finalise();
	g_pConnectionManager->ResetState();

	Host_Disconnect(bShowMainMenu);
}

//clang-format off
AUTOHOOK(concommand_disconnect, engine.dll + 0x15C080, int*, __fastcall, (__int64 args))
//clang-format on
{
	g_pConnectionManager->Retrying(false);
	g_pConnectionManager->Finalise();
	g_pConnectionManager->ResetState();

	return concommand_disconnect(args);
}


// clang-format off
AUTOHOOK(CL_FullyConnected, engine.dll + 0x72D20, int, __fastcall, ())
// clang-format on
{
	g_pConnectionManager->Finalise();
	g_pConnectionManager->ResetState();

	return CL_FullyConnected();
}

// clang-format off
AUTOHOOK(retry, engine.dll + 0x73D10, int*, __fastcall, (__int64 a1))
// clang-format on
{
	g_bRetryingConnection = true;
	g_pConnectionManager->Retrying(true);
	return retry(a1);
}

// clang-format off
AUTOHOOK(concommand_connect, engine.dll + 0x76720, __int64, __fastcall, (const CCommand* args))
// clang-format on
{
    spdlog::info("connect command called with {} args", args->ArgC());

    if (!g_pConnectionManager->IsConnecting())
    {
        std::string cmd = args->GetCommandString();

        auto firstSpace = cmd.find(' ');
        if (firstSpace == std::string::npos)
            return concommand_connect(args);

        std::string rest = cmd.substr(firstSpace + 1);

        auto nonSpace = rest.find_first_not_of(" \t");
        if (nonSpace == std::string::npos)
            return concommand_connect(args);
        rest = rest.substr(nonSpace);

        std::string address;
        bool useSCRPlaque = true;

        auto sep = rest.find_first_of(" \t");
        if (sep == std::string::npos)
        {
            address = rest;
        }
        else
        {
            address = rest.substr(0, sep);

            auto flagStart = rest.find_first_not_of(" \t", sep);
            if (flagStart != std::string::npos)
            {
                std::string flag = rest.substr(flagStart);
                if (!flag.empty() && flag[0] == '0')
                    useSCRPlaque = false;
            }
        }

        // Decide how to handle the address.
        auto mode = g_pConnectionManager->DetermineModeFromAddress(address);

        // For plain IP/hostname direct connects, don't go through ConnectionManager;
        // just use the game's original behavior to avoid recursive re-connects.
        if (mode == ConnectionManager::eConnectionMode::Direct)
            return concommand_connect(args);

        const char* mp_gamemode = g_pCVar->FindVar("mp_gamemode") ? g_pCVar->FindVar("mp_gamemode")->GetString() : "";
        bool isSolo = (mp_gamemode && strcmp(mp_gamemode, "solo") == 0);

        if (isSolo && mode == ConnectionManager::eConnectionMode::LocalServer)
            return concommand_connect(args);

        // NOTE: original code here used `||` which is always true; keeping behavior,
        // but you probably want `&&` if you revisit this logic.
        if (mode != ConnectionManager::eConnectionMode::LocalServer ||
            mode != ConnectionManager::eConnectionMode::P2P ||
            !isSolo)
        {
            if (g_pConnectionManager->GetCurrentMode() == ConnectionManager::eConnectionMode::Matchmaking)
                return concommand_connect(args);
        }

        if (g_pConnectionManager->IsRetrying())
            mode = g_pConnectionManager->GetCurrentMode();
        else
            spdlog::info(
                "Determined connection mode from address '{}': {}",
                address,
                static_cast<int>(mode)); // log as int, not char

        g_pConnectionManager->Connect(address, mode, useSCRPlaque);
        return 0;
    }

    return concommand_connect(args);
}

// clang-format off
AUTOHOOK(connectWithKey, engine.dll + 0x768C0, int*, __fastcall, (const CCommand* args))
// clang-format on
{
	if(!g_pConnectionManager->IsConnecting())
	{
		if(g_pConnectionManager->GetCurrentMode() == ConnectionManager::eConnectionMode::Matchmaking)
			return connectWithKey(args);

		if(args->ArgC() < 3)
			return connectWithKey(args);

		const char* address = args->Arg(1);

		auto mode = g_pConnectionManager->DetermineModeFromAddress(address);
		if(g_pConnectionManager->IsRetrying())
			mode = g_pConnectionManager->GetCurrentMode();
		else
			spdlog::info("Determined connection mode from address '{}': {}", address, static_cast<int>(mode));

		int argCount = args->ArgC();
		bool useSCRPlaque = true;

		if(argCount == 4)
			atoi(args->Arg(3)) == 0 ? useSCRPlaque = false : useSCRPlaque = true;

		g_pConnectionManager->Connect(address, mode, useSCRPlaque);

		return 0;
	}

	return connectWithKey(args);
}

void ConCommand_connectWithRemoteId(const CCommand& args)
{
	if(args.ArgC() < 3)
	{
		spdlog::warn("connectWithRemoteId called with insufficient arguments");
		return;
	}

	std::string remoteId = args.Arg(1);
	std::string password = args.Arg(2);

	bool useSCRPlaque = true;

	if(args.ArgC() == 4)
		atoi(args.Arg(3)) == 0 ? useSCRPlaque = false : useSCRPlaque = true;

	if(password == "0")
		password = "";

	g_pConnectionManager->Connect(remoteId, password, useSCRPlaque);
}

ADD_SQFUNC("void", NSDecideModDownload, "bool accept", "", ScriptContext::UI)
{
	bool accepted = g_pSquirrel[context]->getbool(sqvm, 1);

	if (accepted)
		g_pConnectionManager->SetModAcceptState(ConnectionManager::eModAcceptState::ACCEPTED);
	else
		g_pConnectionManager->SetModAcceptState(ConnectionManager::eModAcceptState::DENIED);

	return SQRESULT_NULL;
}

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", ConnectHooks, ConVar, (CModule module))
{
	AUTOHOOK_DISPATCH();

	g_pConnectionManager = new ConnectionManager();

	Cvar_cl_resend_inforequest_timeout = new ConVar(
		"cl_resend_inforequest_timeout",
		"10.0",
		FCVAR_CLIENTDLL | FCVAR_ARCHIVE_PLAYERPROFILE,
		"Max time in seconds to wait for a server info response when connecting to non-remote servers.");
	Cvar_cl_resend_inforequest_timeout_remote = new ConVar(
		"cl_resend_inforequest_timeout_remote",
		"2.5",
		FCVAR_CLIENTDLL | FCVAR_ARCHIVE_PLAYERPROFILE,
		"Max time in seconds to wait for a server info response when connecting to remote servers.");
	Cvar_cl_resend_inforequest_interval_ms = new ConVar(
		"cl_resend_inforequest_interval_ms",
		"500",
		FCVAR_CLIENTDLL | FCVAR_ARCHIVE_PLAYERPROFILE,
		"Interval in milliseconds between server info requests when connecting to a server.");
	Cvar_cl_unload_remote_mods_on_matchmaking = new ConVar(
		"cl_unload_remote_mods_on_matchmaking",
		"1",
		FCVAR_CLIENTDLL | FCVAR_ARCHIVE_PLAYERPROFILE,
		"Whether to unload remote requiredOnClient mods when joining matchmaking (vanilla) games.");
	RegisterConCommand("connectWithRemoteId", ConCommand_connectWithRemoteId, "Connects to a server using its remote ID from the master server", FCVAR_CLIENTDLL);

	SCR_BeginLoadingPlaque = module.Offset(0xB92E0).RCast<decltype(SCR_BeginLoadingPlaque)>();
	SCR_EndLoadingPlaque = module.Offset(0xB9470).RCast<decltype(SCR_EndLoadingPlaque)>();
}
