#pragma once

#include "modsystem/mod.h"
#include "modsystem/platform/modplatform.h"
#include "modsystem/platform/modworkshop.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

enum class ModInstallAction
{
	Install,
	Update,
	Replace,
	Remove,
};

enum class ModInstallOperationState
{
	Idle,
	Queued,
	FetchingDetails,
	ResolvingDependencies,
	Downloading,
	Validating,
	Staging,
	Committing,
	Reloading,
	Done,
	Failed,
	Cancelled,
	AwaitingMigration,
};

struct ModInstallOperationSnapshot
{
	uint64_t generation = 0;
	uint64_t modId = 0;
	ModInstallAction action = ModInstallAction::Install;
	ModInstallOperationState state = ModInstallOperationState::Idle;
	std::string name;
	std::string version;
	std::string message;
	uint64_t progress = 0;
	uint64_t total = 0;
	float ratio = 0.0f;
	bool cancellationDeferred = false;
};

struct InstalledModRemovalInfo
{
	bool canDelete = false;
	int deleteModCount = 0;
};

class CModInstallService final
{
public:
	using OperationChangedCallback = std::function<void()>;

	static CModInstallService& Get()
	{
		static CModInstallService* s_pInstance = new CModInstallService;
		return *s_pInstance;
	}

	bool Request(ModInstallAction action, uint64_t modId, uint64_t expectedSelectedFileId = 0);
	bool RequestInstalledModRemoval(int modIndex);
	InstalledModRemovalInfo GetInstalledModRemovalInfo(int modIndex) const;
	void Cancel();
	std::shared_ptr<const ModInstallOperationSnapshot> GetSnapshot() const;
	bool DecideMigration(uint64_t generation, bool accept);
	void SetOperationChangedCallback(OperationChangedCallback callback);
	void Shutdown();

	CModInstallService(const CModInstallService&) = delete;
	CModInstallService& operator=(const CModInstallService&) = delete;

private:
	CModInstallService();
	~CModInstallService() = delete;

	static constexpr uint64_t MAX_ARCHIVE_BYTES = 1024ull * 1024 * 1024;
	static constexpr uint64_t MAX_EXPANDED_BYTES = 8ull * 1024 * 1024 * 1024;
	static constexpr uint64_t MAX_ENTRY_BYTES = 2ull * 1024 * 1024 * 1024;
	static constexpr uint64_t MAX_ARCHIVE_ENTRIES = 20000;
	static constexpr size_t MAX_MANIFEST_BYTES = 1024 * 1024;
	static constexpr size_t MAX_ICON_BYTES = 16 * 1024 * 1024;
	static constexpr size_t EXTRACTION_BUFFER_SIZE = 64 * 1024;
	static constexpr auto MAIN_THREAD_TIMEOUT = std::chrono::seconds(90);
	static constexpr std::array<std::string_view, 4> MOD_ICON_FILENAMES = {
	    "icon.webp",
	    "icon.png",
	    "icon.jpg",
	    "icon.jpeg",
	};

	struct InstallRequest
	{
		uint64_t generation = 0;
		uint64_t modId = 0;
		ModInstallAction action = ModInstallAction::Install;
		uint64_t expectedSelectedFileId = 0;
		std::filesystem::path installedRemovalRoot;
		std::string installedRemovalName;
		std::string installedRemovalVersion;
	};

	struct InstalledRemovalTarget
	{
		std::filesystem::path root;
		std::string name;
		std::string version;
		uint64_t managedModId = 0;
		int deleteModCount = 0;
		bool canDelete = false;
	};

	struct ArchiveEntry
	{
		std::string archiveName;
		std::filesystem::path relativePath;
		uint64_t compressedSize = 0;
		uint64_t uncompressedSize = 0;
		bool directory = false;
	};

	struct PackagePlan
	{
		ModSource source = ModSource::ModWorkshop;
		uint64_t modId = 0;
		uint64_t selectedFileId = 0;
		uint64_t expectedDownloadSize = 0;
		uint64_t downloadedSize = 0;
		std::string managedId;
		std::string name;
		std::string author;
		std::string version;
		std::string selectedFileUpdatedAt;
		std::string remoteModUpdatedAt;
		std::string downloadUrl;
		std::string iconUrl;
		std::string iconFallbackUrl;
		std::string iconFilename;
		std::string sha256;
		std::vector<ModWorkshopContainedMod> containedMods;
		std::optional<std::filesystem::path> oldRoot;
		std::filesystem::path destination;
		std::vector<std::filesystem::path> replacementRoots;
		std::filesystem::path archivePath;
		std::filesystem::path stagingRoot;
		std::filesystem::path backupRoot;
	};

	struct CommitRecord
	{
		PackagePlan* plan = nullptr;
		bool oldMoved = false;
		bool newPlaced = false;
		std::vector<std::pair<std::filesystem::path, std::filesystem::path>> replacementsMoved;
	};

	struct ConflictScan
	{
		std::vector<std::vector<std::filesystem::path>> rootsByPlan;
		std::vector<std::filesystem::path> roots;
		std::vector<std::string> names;
		std::unordered_map<std::string, bool> enabledStates;
		std::string error;
	};

	struct ExpectedPackage
	{
		std::filesystem::path root;
		std::vector<std::string> names;
	};

	static std::string SanitizeFolderComponent(std::string value);
	static std::string IconFilenameForPath(std::string_view path);
	static bool HasPackageIcon(const std::filesystem::path& packageRoot);
	static std::string LowerAscii(std::string value);
	static bool NormalizeAbsolutePath(const std::filesystem::path& path, std::filesystem::path& normalized);
	static std::string PathKey(const std::filesystem::path& path);
	static bool TryGetDirectChildRoot(const std::filesystem::path& path, const std::filesystem::path& allowedRoot,
	                                  std::filesystem::path& directChild);
	static bool TryDeriveRemovalRoot(const Mod& mod, std::filesystem::path& deletionRoot);
	static bool ValidateRemovalRoot(const std::filesystem::path& deletionRoot);
	static uint64_t ParseManagedModId(const Mod& mod);
	static bool IsReservedWindowsName(std::string_view component);
	static bool NormalizeArchivePath(std::string_view rawName, std::string& normalized, bool& directory, std::string& errorMessage);
	static bool IsSupportedManifestPath(std::string_view relativePath);
	static bool ContainsSupportedManifest(std::span<const ArchiveEntry> entries, std::string_view prefix);
	static bool PathMatchesKey(const std::filesystem::path& path, std::string_view key);
	static bool InspectArchive(const std::filesystem::path& archivePath, std::vector<ArchiveEntry>& entries, std::string& errorMessage);
	static bool ExtractArchive(const std::filesystem::path& archivePath, const std::filesystem::path& stagingRoot,
	                           std::span<const ArchiveEntry> entries, const std::function<bool()>& cancelled,
	                           const std::function<void(uint64_t, uint64_t)>& progress, std::string& errorMessage);
	static bool ReadStagedManifest(const std::filesystem::path& manifestPath, ModWorkshopContainedMod& containedMod, std::string& errorMessage);
	static bool ValidateStagedPackage(const std::filesystem::path& stagingRoot, std::vector<ModWorkshopContainedMod>& containedMods,
	                                  std::string& errorMessage);
	static bool ComputeSha256(const std::filesystem::path& filePath, std::string& hashText, std::string& errorMessage);

	CModWorkshopClient m_Client;
	mutable std::mutex m_SnapshotMutex;
	std::shared_ptr<const ModInstallOperationSnapshot> m_Snapshot;
	std::mutex m_RequestMutex;
	std::condition_variable m_RequestChanged;
	std::optional<InstallRequest> m_PendingRequest;
	std::thread m_Worker;
	std::atomic<bool> m_Stopped = false;
	std::atomic<bool> m_CancelRequested = false;
	bool m_Busy = false;
	uint64_t m_NextGeneration = 0;
	std::mutex m_CallbackMutex;
	OperationChangedCallback m_OperationChanged;
	std::chrono::steady_clock::time_point m_LastProgressPublish{};
	enum class MigrationDecision
	{
		None,
		Pending,
		Accepted,
		Declined,
	};
	std::mutex m_MigrationMutex;
	std::condition_variable m_MigrationChanged;
	uint64_t m_MigrationGeneration = 0;
	MigrationDecision m_MigrationDecision = MigrationDecision::None;

	bool InspectInstalledRemoval(int modIndex, InstalledRemovalTarget& target) const;
	void Notify();
	void Publish(std::shared_ptr<ModInstallOperationSnapshot> next);
	void Transition(ModInstallOperationState state, std::string message, std::string name = {}, std::string version = {});
	void ReportProgress(uint64_t progressValue, uint64_t totalValue);
	bool IsCancelled() const;
	bool RunOnMainThreadAndWait(std::function<bool()> function, bool& result);
	bool CaptureRuntimeState(const std::vector<PackagePlan>& plans, std::unordered_set<std::string>& installedModNames,
	                         std::unordered_map<std::string, bool>& enabledStates, std::string& errorMessage);
	bool UnloadRuntimeForFilesystemMutation(std::string& errorMessage);
	bool ResolveModWorkshop(uint64_t modId, bool root, ModInstallAction action, uint64_t expectedSelectedFileId,
	                        const ModWorkshopRequestOptions& options, const std::unordered_set<std::string>& installedModNames,
	                        std::vector<PackagePlan>& plans, std::unordered_map<uint64_t, size_t>& resolved, std::unordered_set<uint64_t>& visiting,
	                        std::string& errorMessage);
	bool StagePlan(PackagePlan& plan, size_t planIndex, const std::filesystem::path& jobRoot, uint64_t totalExpectedBytes,
	               uint64_t completedExpectedBytes, std::string& errorMessage);
	bool ResolveUnmanagedReplacements(uint64_t generation, std::vector<PackagePlan>& plans, std::unordered_map<std::string, bool>& enabledStates,
	                                  std::string& errorMessage);
	bool ReloadAndVerify(const std::vector<PackagePlan>& plans, std::unordered_map<std::string, bool> enabledStates, std::string& errorMessage);
	bool ReloadAfterRollback(std::unordered_map<std::string, bool> enabledStates, std::string& errorMessage);
	bool CommitPlans(std::vector<PackagePlan>& plans, const std::unordered_map<std::string, bool>& enabledStates, std::string& errorMessage,
	                 bool& preserveRecoveryFiles);
	bool ExecuteInstalledRemove(const InstallRequest& request, std::string& errorMessage, bool& preserveRecoveryFiles);
	bool ExecuteRemove(const InstallRequest& request, std::string& errorMessage);
	bool ExecuteInstall(const InstallRequest& request, std::string& errorMessage, bool& preserveRecoveryFiles);
	void RunWorker();
};
