#pragma once

#include "modsystem/platform/modworkshop.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class ModUpdateState
{
    LegacyUnknown,
    Checking,
    Current,
    UpdateAvailable,
    MissingRemote,
    Unsupported,
    Error,
};

struct ModTrackedPackage
{
    fs::path packageRoot;
    ModSource source = ModSource::ModWorkshop;
    std::string packageId;
    std::string installedVersion;
    std::string remoteThumbnailUrl;
    uint64_t modId = 0;
    std::optional<ModWorkshopPackageState> installedState;
    std::optional<uint64_t> remoteSelectedFileId;
    std::string remoteVersion;
    std::string remoteUpdatedAt;
    std::optional<ModWorkshopThumbnail> remoteThumbnail;
    std::vector<ModContainedMod> containedMods;
    ModUpdateState updateState = ModUpdateState::LegacyUnknown;
    std::string error;
};

struct ModInventorySnapshot
{
    uint64_t generation = 0;
    std::vector<ModTrackedPackage> packages;
    int updateCount = 0;
    bool checking = false;
    std::string checkedAt;
    std::string error;
};

class CModInventory final
{
  public:
    static CModInventory& Get()
    {
        static CModInventory* s_pInstance = new CModInventory;
        return *s_pInstance;
    }

    void RefreshLocal();
    bool CheckForUpdates(CModWorkshopClient& client, const ModRequestOptions& options = {});
    std::shared_ptr<const ModInventorySnapshot> GetSnapshot() const;
    std::optional<ModTrackedPackage> FindPackage(uint64_t modId) const;
    std::optional<ModTrackedPackage> FindPackage(ModSource source, std::string_view packageId) const;

    CModInventory(const CModInventory&) = delete;
    CModInventory& operator=(const CModInventory&) = delete;

  private:
    CModInventory();
    ~CModInventory() = delete;

    static constexpr size_t MAX_MANIFEST_BYTES = 1024 * 1024;

    static std::optional<uint64_t> ParseId(std::string_view text);
    static void ReadContainedMod(const fs::path& modDirectory, std::vector<ModContainedMod>& mods);
    static std::vector<ModContainedMod> DiscoverContainedMods(const fs::path& packageRoot);

    void Publish(std::shared_ptr<ModInventorySnapshot> snapshot);
    bool PublishIfCurrent(const std::shared_ptr<const ModInventorySnapshot>& expected, std::shared_ptr<ModInventorySnapshot> snapshot);
    void RestoreCancelled(const std::shared_ptr<const ModInventorySnapshot>& current, const std::shared_ptr<const ModInventorySnapshot>& expected);

    mutable std::mutex m_Mutex;
    std::shared_ptr<const ModInventorySnapshot> m_Snapshot;
    uint64_t m_Generation = 0;
};

enum class ModInventoryUpdateStage
{
    LocalComplete,
    LocalCompleteRemotePending,
    RemoteComplete,
};
