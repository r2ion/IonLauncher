#pragma once

#include "engine/net.h"
#include "masterserver/masterserver.h"
#include "core/tier0.h"
#include "engine/r2engine.h"
#include "mods/autodownload/moddownloader.h"
#include "engine/localize.h"

extern bool g_bConnectingToServer;
extern bool g_bRetryingConnection;

// mirrored in script
const int NOT_DECIDED_TO_SEND_TOKEN = 0;
const int AGREED_TO_SEND_TOKEN = 1;
const int DISAGREED_TO_SEND_TOKEN = 2;

#define RETURN_IF_CANCELLED() \
	if (IsCancelled())        \
	{                         \
		return;               \
	}

class ConnectionManager
{
public:
	enum class eConnectionMode : char
	{
		Direct = 0, // unauthenticated direct connect
		Matchmaking = 'V', // vanilla matchmaking
		LocalServer = 'L', // listen server AuthWithOwnServer
		RemoteServer = 'N', // server browser AuthWithServer
		P2P = 'P', // connectionless self auth hack only for p2p
	};

	enum class eModAcceptState
	{
		NOT_DECIDED = 0,
		ACCEPTED = 1,
		DENIED = 2
	};

private:
	eConnectionMode m_eCurrentMode = eConnectionMode::Direct;
	eConnectionMode m_eLastMode = eConnectionMode::Direct;
	bool m_bRetrying = false; // if we're retrying we need to check if we should use connectWithKey
	int m_nAttemptCount = 0;
	bool m_bIncomingP2PServerAuthenticating = false;
	std::string m_szFailReason;
	std::string m_szProgressMessage = "";
	bool m_bFinished = false;
	bool m_bFailed = false;
	bool m_bUseSCRPlaque = false; // whether we're expecting script to show progress or use LoadingProgress from BaseModUI
	bool m_bConnecting = false;
	float m_flConnectionStartTime = 0.0f;
	bool m_bAuthSucessful = false;
	std::string m_szMapName;
	std::string m_szLastServerID;
	std::string m_szLastServerPassword;
	std::string m_szLastServerAddress;
	bool m_bDownloadedMods = false;
	eModAcceptState m_eModAcceptState = eModAcceptState::NOT_DECIDED;
	bool m_bUnloadingRemoteModsOnMatchmaking = false;
	bool m_bSolo = false;

	void ConnectToLocalServer();
	void ConnectToRemoteServer(const std::string& id, const std::string& password);
	void ConnectToVanillaMatchmakingServer(const std::string& address);
	void ConnectToP2PServer(const std::string& address);
	void SendInfoRequestPacket(const CNetAdr& addr, bool serverAuthUs, bool requestMods);
	bool IsCancelled() { return !m_bConnecting; }

	void InvokeConnectionStartCallbacks();
	void InvokeConnectionStoppedCallbacks(std::string reason = "");
	void InvokeConnectionMessageCallbacks(const std::string& message);

	void AuthenticateToMasterServer();
	template <typename... Args>
	void UpdateMessage(const std::string& message = "", Args... args) { m_szProgressMessage = Localize(message.c_str(), args...); InvokeConnectionMessageCallbacks(m_szProgressMessage); }

	void FinaliseJoiningLocalServer();
	void FinaliseJoiningServer(std::string& address);

	void DownloadMods(bool remoteServer, RemoteServerInfo* info);
	void ReloadMods(RemoteServerInfo* info);
public:
	void Connect(const std::string& address, eConnectionMode mode, bool useSCRPlaque = true, std::string mapName = "");
	void Connect(bool useSCRPlaque = true, std::string mapName = "");
	void Connect(const std::string& address, const std::string& password, bool useSCRPlaque, std::string mapName = "");

    template <typename... Args>
    void Interrupt(const std::string& reason = "", Args... args)
    {
        m_bFailed = true;
        m_szFailReason = reason;

        m_bConnecting = false;
        m_flConnectionStartTime = 0.0f;
        m_bAuthSucessful = false;
        m_bRetrying = false;
        m_bDownloadedMods = false;
        m_eModAcceptState = eModAcceptState::NOT_DECIDED;
        m_bUnloadingRemoteModsOnMatchmaking = false;
        m_eCurrentMode = m_eLastMode;
		m_bSolo = false;

        g_pModDownloader->CancelDownload();

        InvokeConnectionStoppedCallbacks(reason);
        spdlog::info("Connection interrupted: {}", Localize(reason.c_str(), args...));

        if (m_bUseSCRPlaque)
        {
            Cbuf_AddText(
                Cbuf_GetCurrentPlayer(),
                fmt::format("disconnect \"{}\"", Localize(reason.c_str(), args...)).c_str(),
                cmd_source_t::kCommandSrcCode);
        }
    }
	void Retrying(bool retrying) { m_bRetrying = retrying; }
	void Finalise() { m_bConnecting = false; InvokeConnectionStoppedCallbacks(); }
	void ResetState()
	{
		m_bFailed = false;
		m_szFailReason.clear();
		m_szProgressMessage.clear();
		m_bConnecting = false;
		m_flConnectionStartTime = 0.0f;
		m_bAuthSucessful = false;
		m_bRetrying = false;
		m_bDownloadedMods = false;
		m_eModAcceptState = eModAcceptState::NOT_DECIDED;
		m_bUnloadingRemoteModsOnMatchmaking = false;
		m_bSolo = false;
	}

	bool ParseAddress(const std::string& address, std::string& ip, int& port, bool& isV6);
	std::string& GetFailReason() { return m_szFailReason; }
	void SetProgressMessage(const std::string& message) { m_szProgressMessage = message; }
	std::string& GetProgressMessage() { return m_szProgressMessage; }
	bool IsFailed() { return m_bFailed; }
	bool IsConnecting() { return m_bConnecting; }
	eConnectionMode GetCurrentMode() { return m_eCurrentMode; }
	void SetMatchmaking() { m_eLastMode = m_eCurrentMode; m_eCurrentMode = eConnectionMode::Matchmaking; }
	eConnectionMode DetermineModeFromAddress(const std::string& address);
	bool IsRetrying() { return m_bRetrying; }
	void SetModAcceptState(eModAcceptState state) { m_eModAcceptState = state; }
	bool UnloadingRemoteModsOnMatchmaking() { return m_bUnloadingRemoteModsOnMatchmaking; }
	void SetUnloadingRemoteModsOnMatchmaking(bool unloading) { m_bUnloadingRemoteModsOnMatchmaking = unloading; }
};

extern ConnectionManager* g_pConnectionManager;
