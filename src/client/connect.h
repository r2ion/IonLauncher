#pragma once

#include "engine/net.h"
#include "masterserver/masterserver.h"
#include "core/tier0.h"
#include "engine/r2engine.h"
#include "modsystem/moddownloader.h"
#include "engine/localize.h"
#include <atomic>
#include <mutex>
#include <string_view>

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
		Direct = 0, // direct connect
		Matchmaking = 'V', // vanilla matchmaking
		LocalServer = 'L', // listen server AuthWithOwnServer
		RemoteServer = 'N', // server browser AuthWithServer
		P2P = 'P', // eos p2p
	};

	enum class eModAcceptState
	{
		NOT_DECIDED = 0,
		ACCEPTED = 1,
		DENIED = 2
	};

private:
	struct WorkshopSelection
	{
		std::string m_Name;
		std::string m_Version;
		ModDownloader::ModWorkshopAlternative m_Alternative;
	};

	static bool HasRequiredModVersion(std::string_view name, std::string_view version);

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
	std::atomic_bool m_bConnecting = false;
	float m_flConnectionStartTime = 0.0f;
	bool m_bAuthSucessful = false;
	std::mutex m_MapLoadMutex;
	bool m_bMapLoadAuthorized = false;
	std::string m_szMapName;
	std::string m_szLastServerID;
	std::string m_szLastServerPassword;
	std::string m_szLastServerAddress;
	bool m_bDownloadedMods = false;
	std::atomic<eModAcceptState> m_eModAcceptState{eModAcceptState::NOT_DECIDED};
	bool m_bUnloadingRemoteModsOnMatchmaking = false;
	bool m_bSolo = false;

	void ConnectToLocalServer();
	void ConnectToRemoteServer(const std::string& id, const std::string& password);
	void ConnectToVanillaMatchmakingServer(const std::string& address);
	void ConnectToP2PServer(const std::string& address);
	void ConnectToDirectServer(const std::string& address);
	void SendInfoRequestPacket(const CNetAdr& addr, bool serverAuthUs, bool requestMods);
	void SetPendingMap(std::string mapName);
	void ClearPendingMap();
	bool IsCancelled() { return !m_bConnecting.load(std::memory_order_acquire); }

	void InvokeConnectionStartCallbacks();
	void InvokeConnectionStoppedCallbacks(std::string reason = "");
	void InvokeConnectionMessageCallbacks(const std::string& message);

	void AuthenticateToMasterServer();
	static const char* LocalizationArgument(const std::string& argument) { return argument.c_str(); }
	static const char* LocalizationArgument(const char* argument) { return argument; }
	template <size_t Size>
	static const char* LocalizationArgument(const char (&argument)[Size]) { return argument; }
	template <typename... Args>
	void UpdateMessage(const std::string& message = "", const Args&... args)
	{
		m_szProgressMessage = Localize(message, LocalizationArgument(args)..., static_cast<const char*>(nullptr));
		InvokeConnectionMessageCallbacks(m_szProgressMessage);
	}

	void FinaliseJoiningLocalServer();
	void FinaliseJoiningServer(std::string& address);

	void DownloadMods(bool remoteServer, RemoteServerInfo* info);
	void ReloadModsAndConnect(std::vector<RemoteModInfo> requiredMods, std::string address);
public:
	void Connect(const std::string& address, eConnectionMode mode, bool useSCRPlaque = true, std::string mapName = "");
	void Connect(bool useSCRPlaque = true, std::string mapName = "");
	void Connect(const std::string& address, const std::string& password, bool useSCRPlaque, std::string mapName = "");
	bool DeferMapLoad(std::string_view mapName);

	void Interrupt(const std::string& reason = "")
	{
		m_bFailed = true;
		m_szFailReason = reason;

		m_bConnecting.store(false, std::memory_order_release);
		m_flConnectionStartTime = 0.0f;
		m_bAuthSucessful = false;
		m_bRetrying = false;
		m_bDownloadedMods = false;
		m_eModAcceptState.store(eModAcceptState::NOT_DECIDED, std::memory_order_release);
		m_bUnloadingRemoteModsOnMatchmaking = false;
		m_eCurrentMode = m_eLastMode;
		m_bSolo = false;
		ClearPendingMap();

		g_pModDownloader->CancelDownload();

		const std::string localizedReason = Localize(reason, static_cast<const char*>(nullptr));
		InvokeConnectionStoppedCallbacks(reason);
		spdlog::info("Connection interrupted: {}", localizedReason);

		if (m_bUseSCRPlaque)
		{
			Cbuf_AddText(
				Cbuf_GetCurrentPlayer(),
				fmt::format("disconnect \"{}\"", localizedReason).c_str(),
				cmd_source_t::kCommandSrcCode);
		}
	}
	void Retrying(bool retrying) { m_bRetrying = retrying; }
	void Finalise() { m_bConnecting.store(false, std::memory_order_release); InvokeConnectionStoppedCallbacks(); }
	void ResetState()
	{
		m_bFailed = false;
		m_szFailReason.clear();
		m_szProgressMessage.clear();
		m_bConnecting.store(false, std::memory_order_release);
		m_flConnectionStartTime = 0.0f;
		m_bAuthSucessful = false;
		m_bRetrying = false;
		m_bDownloadedMods = false;
		m_eModAcceptState = eModAcceptState::NOT_DECIDED;
		m_bUnloadingRemoteModsOnMatchmaking = false;
		m_bSolo = false;
		ClearPendingMap();
	}

	bool ParseAddress(const std::string& address, std::string& ip, int& port, bool& isV6);
	std::string& GetFailReason() { return m_szFailReason; }
	void SetProgressMessage(const std::string& message) { m_szProgressMessage = message; }
	std::string& GetProgressMessage() { return m_szProgressMessage; }
	bool IsFailed() { return m_bFailed; }
	bool IsConnecting() { return m_bConnecting.load(std::memory_order_acquire); }
	eConnectionMode GetCurrentMode() { return m_eCurrentMode; }
	void SetMatchmaking() { m_eLastMode = m_eCurrentMode; m_eCurrentMode = eConnectionMode::Matchmaking; }
	eConnectionMode DetermineModeFromAddress(const std::string& address);
	bool IsRetrying() { return m_bRetrying; }
	void SetModAcceptState(eModAcceptState state) { m_eModAcceptState.store(state, std::memory_order_release); }
	bool UnloadingRemoteModsOnMatchmaking() { return m_bUnloadingRemoteModsOnMatchmaking; }
	void SetUnloadingRemoteModsOnMatchmaking(bool unloading) { m_bUnloadingRemoteModsOnMatchmaking = unloading; }
};

extern ConnectionManager* g_pConnectionManager;
