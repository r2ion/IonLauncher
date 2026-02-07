#pragma once

#include "engine/net.h"
#include "mod.h"
#include "vscript/squirrel/squirrel.h"

#include <optional>
#include <atomic>

namespace fs = std::filesystem;

class CClient;

class ModDownloader;

extern ModDownloader* g_pModDownloader;

class ModDownloader
{
public:

	struct modentry_s
	{

		std::string name;
		std::string version;
		ModSource platform = ModSource::Unknown;

		std::string dependencyString;
		std::string url;

		std::string checksum;
	};

private:
	const char* VERIFICATION_FLAG = "-disablemodverification";
	const char* CUSTOM_MODS_URL_FLAG = "-customverifiedurl=";
	const char* DEFAULT_MODS_LIST_URL = "https://raw.githubusercontent.com/R2Northstar/VerifiedMods/main/verified-mods.json";
	const float SERVER_MODINFO_TIMEOUT_SECONDS = 3.0f;
	char* modsListUrl;
	rapidjson::Document m_Document;
	std::vector<modentry_s> m_ParsedSchemaMods;
	bool m_bIsListeningForServerMods = false;
	std::vector<modentry_s> m_ServerRequestedMods;
	int m_iTotalServerRequestedMods = 0;
	std::optional<std::string> m_PendingWorkshopId;
	bool m_bDownloadReady = false;
	bool m_bDownloadCallbacksActive = false;
	std::atomic_bool m_bDownloadThreadRunning {false};

	ModSource ResolvePlatform(std::string input)
	{
		if (input.compare("thunderstore") == 0)
		{
			return ModSource::Thunderstore;
		}
		if (input.compare("modworkshop") == 0)
		{
			return ModSource::ModWorkshop;
		}
		return ModSource::Unknown;
	}

	std::string GetPlatformString(ModSource platform)
	{
		switch (platform)
		{
		case ModSource::Thunderstore:
			return std::string("thunderstore");
		case ModSource::ModWorkshop:
			return std::string("modworkshop");
		case ModSource::Remote:
			return std::string("remote");
		case ModSource::Unmanaged:
			return std::string("unmanaged");
		default:
			return std::string("unknown");
		}
	}

	struct VerifiedModVersion
	{
		std::string checksum;
		std::string downloadLink;
		ModSource platform;
	};
	struct VerifiedModDetails
	{
		std::unordered_map<std::string, VerifiedModVersion> versions = {};
	};
	std::unordered_map<std::string, VerifiedModDetails> verifiedMods = {};

	struct PendingModDownload
	{
		std::string name;
		std::string version;
		VerifiedModVersion versionInfo;
		std::optional<fs::path> destinationDir;
		std::optional<std::string> managedId;
	};


	static int ModFetchingProgressCallback(
		void* ptr, curl_off_t totalDownloadSize, curl_off_t finishedDownloadSize, curl_off_t totalToUpload, curl_off_t nowUploaded);
	std::tuple<fs::path, bool> FetchModFromDistantStore(std::string_view modName, VerifiedModVersion modVersion);
	bool IsModLegit(fs::path modPath, std::string_view expectedChecksum);
	void ExtractMod(fs::path modPath, fs::path destinationPath, ModSource platform);
	std::string GetModArchiveName(std::string url);
	std::string SanitizeFolderComponent(std::string value);

	void ParseSchemaDocument();
	bool BuildThunderstoreDownload(
		const std::string& dependencyName,
		const std::string& dependencyUrl,
		PendingModDownload& outDownload);
	bool DownloadModInternal(const PendingModDownload& download);
	bool IsModInstalled(std::string_view modName) const;
	bool StartDownloadThread(
		std::string modName,
		std::string modVersion,
		const VerifiedModVersion& version,
		const std::optional<fs::path>& destinationDir,
		const std::optional<std::string>& managedId,
		std::vector<PendingModDownload> dependencies);

public:
	ModDownloader();

	void NotifyDownloadStarted()
	{
		if (m_bDownloadCallbacksActive)
			return;
		if (!g_pSquirrel[ScriptContext::UI] || !g_pSquirrel[ScriptContext::UI]->m_pSQVM)
			return;
		m_bDownloadCallbacksActive = true;
		g_pSquirrel[ScriptContext::UI]->AsyncCall("NSUICodeCallback_DownloadingModsStarted");
	}

	void NotifyDownloadStopped()
	{
		if (!m_bDownloadCallbacksActive)
			return;
		m_bDownloadCallbacksActive = false;
		if (!g_pSquirrel[ScriptContext::UI] || !g_pSquirrel[ScriptContext::UI]->m_pSQVM)
			return;
		g_pSquirrel[ScriptContext::UI]->AsyncCall("NSUICodeCallback_DownloadingModsStopped");
	}

	void NotifyConfirmDownloadMods(int modCount, const std::string& serverName)
	{
		if (!g_pSquirrel[ScriptContext::UI] || !g_pSquirrel[ScriptContext::UI]->m_pSQVM)
			return;
		g_pSquirrel[ScriptContext::UI]->AsyncCall("NSUICodeCallback_ConfirmDownloadMods", modCount, serverName.c_str());
	}

	void FetchModsListFromAPI();
	bool IsModAuthorized(std::string_view modName, std::string_view modVersion);
	void DownloadMod(std::string modName, std::string modVersion);
	void SetDownloadReady(bool ready);
	bool IsDownloadReady() const { return m_bDownloadReady; }
	bool IsDownloadThreadRunning() const { return m_bDownloadThreadRunning.load(); }
	void QueueWorkshopDownload(std::string id);
	bool StartPendingWorkshopDownload();
	bool IsDownloadInProgress() const
	{
		return modState.state < ModInstallState::DONE;
	}

	enum ModInstallState
	{
		// Initial states
		MANIFEST_FETCHING,
		CHECKING_DETAILS, // fetching platform specific details i.e thunderstore or modworkshop info

		// Normal installation process
		DOWNLOADING,
		CHECKSUMING,
		EXTRACTING,
		DONE, // Everything went great, mod can be used in-game
		ABORTED, // User cancelled mod install process

		// Errors
		FAILED, // Generic error message, should be avoided as much as possible
		FAILED_READING_ARCHIVE,
		FAILED_WRITING_TO_DISK,
		MOD_FETCHING_FAILED,
		MOD_CORRUPTED, // Downloaded archive checksum does not match verified hash
		NO_DISK_SPACE_AVAILABLE,
		INVALID_DEPENDENCY,
		NOT_FOUND, // Mod is not currently being auto-downloaded
		UNKNOWN_PLATFORM
	};

	struct MOD_STATE
	{
		ModInstallState state;
		std::string name;
		std::string version;
		int progress;
		int total;
		float ratio;
	} modState = {};

	void CancelDownload();

	void LoadServerModSchema();
	void PromptUserConfirmation();
	rapidjson::Document& GetServerModSchemaDocument() { return m_Document; }
	bool ParseServerDownloadLinks();
	void DownloadServerMods();
	static int ServerModFetchingProgressCallback(
		void* ptr, curl_off_t totalDownloadSize, curl_off_t finishedDownloadSize, curl_off_t totalToUpload, curl_off_t nowUploaded);
	std::vector<modentry_s>& GetServerModsToInstall() { return m_ParsedSchemaMods; }
	std::vector<modentry_s>& GetServerRequestedMods() { return m_ServerRequestedMods; }
	bool SendModInfoConnectionlessPacket(netadr_t& adr, modentry_s& mod, int index, int totalMods);
	bool RecvModInfoConnectionlessPacket(bf_read& msg);
	bool AllowingServerModDownloads() { return m_bIsListeningForServerMods; }
	void SetIsListeningForServerMods(bool state) { m_bIsListeningForServerMods = state; }
	bool IsListeningForServerMods() { return m_bIsListeningForServerMods; }
	void SetTotalServerRequestedMods(int totalMods) { m_iTotalServerRequestedMods = totalMods; }
	int GetTotalServerRequestedMods() { return m_iTotalServerRequestedMods; }
	float GetServerModInfoTimeoutSeconds() const { return SERVER_MODINFO_TIMEOUT_SECONDS; }
};
