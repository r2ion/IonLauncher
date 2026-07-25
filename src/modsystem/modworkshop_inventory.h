#pragma once

#include "modsystem/platform/modplatform.h"
#include "modsystem/platform/modworkshop.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

enum class ModWorkshopUpdateState
{
	LegacyUnknown,
	Checking,
	Current,
	UpdateAvailable,
	MissingRemote,
	Unsupported,
	Error,
};

struct ModWorkshopTrackedPackage
{
	fs::path packageRoot;
	uint64_t modId = 0;
	std::optional<ModWorkshopPackageState> installedState;
	std::optional<uint64_t> remoteSelectedFileId;
	std::string remoteVersion;
	std::string remoteUpdatedAt;
	std::optional<ModWorkshopThumbnail> remoteThumbnail;
	std::vector<ModWorkshopContainedMod> containedMods;
	ModWorkshopUpdateState updateState = ModWorkshopUpdateState::LegacyUnknown;
	std::string error;
};

struct ModWorkshopInventorySnapshot
{
	uint64_t generation = 0;
	std::vector<ModWorkshopTrackedPackage> packages;
	int updateCount = 0;
	bool checking = false;
	std::string checkedAt;
	std::string error;
};

class CModWorkshopInventory final
{
public:
	static CModWorkshopInventory& Get()
	{
		static CModWorkshopInventory* s_pInstance = new CModWorkshopInventory;
		return *s_pInstance;
	}

	void RefreshLocal();
	bool CheckForUpdates(CModWorkshopClient& client, const ModWorkshopRequestOptions& options = {});
	std::shared_ptr<const ModWorkshopInventorySnapshot> GetSnapshot() const;
	std::optional<ModWorkshopTrackedPackage> FindPackage(uint64_t modId) const;

	CModWorkshopInventory(const CModWorkshopInventory&) = delete;
	CModWorkshopInventory& operator=(const CModWorkshopInventory&) = delete;

private:
	CModWorkshopInventory();
	~CModWorkshopInventory() = delete;

	static constexpr size_t MAX_MANIFEST_BYTES = 1024 * 1024;
	static constexpr size_t UPDATE_BATCH_SIZE = 50;
	static constexpr std::string_view TITANFALL_2_SLUG = "titanfall-2";

	static std::optional<uint64_t> ParseId(std::string_view text);
	static void ReadContainedMod(const fs::path& modDirectory, std::vector<ModWorkshopContainedMod>& mods);
	static std::vector<ModWorkshopContainedMod> DiscoverContainedMods(const fs::path& packageRoot);

	void Publish(std::shared_ptr<ModWorkshopInventorySnapshot> snapshot);
	bool PublishIfCurrent(const std::shared_ptr<const ModWorkshopInventorySnapshot>& expected,
	                      std::shared_ptr<ModWorkshopInventorySnapshot> snapshot);
	void RestoreCancelled(const std::shared_ptr<const ModWorkshopInventorySnapshot>& current,
	                      const std::shared_ptr<const ModWorkshopInventorySnapshot>& expected);

	mutable std::mutex m_Mutex;
	std::shared_ptr<const ModWorkshopInventorySnapshot> m_Snapshot;
	uint64_t m_Generation = 0;
};
