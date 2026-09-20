#pragma once

#include "modsystem/modinventory.h"
#include "modsystem/platform/modworkshop.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

enum class ModInstallAction;

struct ModBrowserEntry
{
    std::string id, name, author, shortDescription, description, version, selectedFileId, pageUrl;
    std::string thumbnailUrl, thumbnailFallbackUrl, thumbnailVersion;
    std::string updatedAt, publishedAt;
    ModSource source = ModSource::Unknown;
    uint64_t downloads = 0, likes = 0, views = 0;
    double score = 0, weeklyScore = 0, dailyScore = 0;
    bool installed = false, approved = false, suspended = false, hasDownload = false, disableModManagers = false;
    int updateState = static_cast<int>(ModUpdateState::LegacyUnknown);
};

struct ModBrowserDetails : ModBrowserEntry
{
    uint64_t selectedFileSize = 0;
    std::string selectedFileUpdatedAt;
    std::vector<std::string> dependencies;
};

struct ModBrowserPage
{
    std::vector<ModBrowserEntry> entries;
    ModPageMetadata metadata;
};

enum class ModBrowserLoadState
{
    Idle,
    Loading,
    Ready,
    Failed,
    Cancelled,
};

struct ModBrowserPageSnapshot
{
    uint64_t generation = 0;
    ModBrowserLoadState state = ModBrowserLoadState::Idle;
    std::string search;
    std::string sort = "bumped_at";
    int requestedPage = 1;
    std::shared_ptr<const ModBrowserPage> page;
    ModRequestError error;
    bool fromCache = false;
};

struct ModBrowserDetailsSnapshot
{
    uint64_t generation = 0;
    std::string id;
    ModBrowserLoadState state = ModBrowserLoadState::Idle;
    std::shared_ptr<const ModBrowserDetails> details;
    ModRequestError error;
    bool fromCache = false;
};

class CModBrowserService final
{
  public:
    using PageChangedCallback = std::function<void(uint64_t)>;
    using DetailsChangedCallback = std::function<void(const std::string&)>;
    using UpdatesChangedCallback = std::function<void(uint64_t, int, ModInventoryUpdateStage)>;

    static CModBrowserService& Get()
    {
        static CModBrowserService* s_pInstance = new CModBrowserService;
        return *s_pInstance;
    }

    uint64_t RequestPage(std::string search, std::string sort, int page, int filter, ModSource source = ModSource::Unknown,
                         bool forceRefresh = false);
    uint64_t RequestDetails(std::string id, bool forceRefresh = false);
    uint64_t RefreshTrackedMods(bool checkRemote = true);
    void CancelPageRequest();
    void CancelDetailsRequest();

    std::shared_ptr<const ModBrowserPageSnapshot> GetPageSnapshot() const;
    std::shared_ptr<const ModBrowserDetailsSnapshot> GetDetailsSnapshot() const;
    std::shared_ptr<const ModInventorySnapshot> GetInventorySnapshot() const;

    void SetPageChangedCallback(PageChangedCallback callback);
    void SetDetailsChangedCallback(DetailsChangedCallback callback);
    void SetUpdatesChangedCallback(UpdatesChangedCallback callback);
    void ClearCallbacks();

    void Shutdown();
    bool RequestOperation(ModInstallAction action, std::string_view id);
    static std::string BuildPageUrl(std::string_view id);
    static std::string BuildId(ModSource source, std::string_view packageId);

    CModBrowserService(const CModBrowserService&) = delete;
    CModBrowserService& operator=(const CModBrowserService&) = delete;

  private:
    CModBrowserService();
    ~CModBrowserService() = delete;

    static constexpr size_t MAX_PAGE_CACHE_ENTRIES = 32;
    static constexpr size_t MAX_DETAILS_CACHE_ENTRIES = 64;
    static constexpr int PAGE_BATCH_SIZE = 24;
    static constexpr int REMOTE_PAGE_BATCH_SIZE = 50;
    static constexpr std::string_view TITANFALL_2_SLUG = "titanfall-2";

    struct PageRequest
    {
        uint64_t generation = 0;
        std::string cacheKey;
        std::string search, sort;
        int page = 1, filter = 0;
        ModSource source = ModSource::Unknown;
        bool forceRefresh = false;
        std::shared_ptr<const ModInventorySnapshot> inventory;
    };

    struct DetailsRequest
    {
        uint64_t generation = 0;
        std::string id;
    };

    struct InventoryRequest
    {
        uint64_t generation = 0;
        bool checkRemote = true;
    };

    bool LoadCatalog(ModSource source, bool forceRefresh, std::vector<const ModBrowserEntry*>& entries, ModRequestError& error,
                     const ModRequestOptions& options);
    bool LoadDetails(std::string_view id, ModBrowserDetails& details, ModRequestError& error, const ModRequestOptions& options);
    static void ApplyInventory(ModBrowserEntry& entry, const ModInventorySnapshot& inventory);
    struct CatalogCache
    {
        std::vector<ModBrowserEntry> entries;
        std::chrono::steady_clock::time_point fetchedAt{};
        bool valid = false;
    };
    CatalogCache m_WorkshopCatalog, m_ThunderstoreCatalog;

    CModWorkshopClient m_Client;
    std::atomic<bool> m_Stopped = false;
    std::atomic<uint64_t> m_PageGeneration = 0;
    std::atomic<uint64_t> m_DetailsGeneration = 0;
    std::atomic<uint64_t> m_InventoryGeneration = 0;
    std::atomic<uint64_t> m_GameId = 0;
    mutable std::mutex m_PageMutex;
    std::shared_ptr<const ModBrowserPageSnapshot> m_PageSnapshot;
    std::unordered_map<std::string, std::shared_ptr<const ModBrowserPage>> m_PageCache;
    std::deque<std::string> m_PageCacheOrder;
    mutable std::mutex m_DetailsMutex;
    std::shared_ptr<const ModBrowserDetailsSnapshot> m_DetailsSnapshot;
    struct CachedDetails
    {
        std::shared_ptr<const ModBrowserDetails> details;
        std::chrono::steady_clock::time_point fetchedAt;
    };
    std::unordered_map<std::string, CachedDetails> m_DetailsCache;
    std::deque<std::string> m_DetailsCacheOrder;
    std::mutex m_PageRequestMutex;
    std::condition_variable m_PageRequestChanged;
    std::optional<PageRequest> m_PendingPageRequest;
    std::thread m_PageWorker;
    std::mutex m_DetailsRequestMutex;
    std::condition_variable m_DetailsRequestChanged;
    std::optional<DetailsRequest> m_PendingDetailsRequest;
    std::thread m_DetailsWorker;
    std::mutex m_InventoryRequestMutex;
    std::condition_variable m_InventoryRequestChanged;
    std::optional<InventoryRequest> m_PendingInventoryRequest;
    std::thread m_InventoryWorker;
    std::mutex m_CallbackMutex;
    PageChangedCallback m_PageChanged;
    DetailsChangedCallback m_DetailsChanged;
    UpdatesChangedCallback m_UpdatesChanged;

    void NotifyPage(uint64_t generation);
    void NotifyDetails(const std::string& id);
    void NotifyInventory(uint64_t generation, ModInventoryUpdateStage stage);
    void PublishPage(std::shared_ptr<const ModBrowserPageSnapshot> snapshot);
    void PublishDetails(std::shared_ptr<const ModBrowserDetailsSnapshot> snapshot);
    void CachePage(uint64_t generation, const std::string& key, std::shared_ptr<const ModBrowserPage> page);
    void CacheDetails(uint64_t generation, const std::string& id, std::shared_ptr<const ModBrowserDetails> details);
    void EnsurePageWorker();
    void EnsureDetailsWorker();
    void EnsureInventoryWorker();
    void RunPageWorker();
    void RunDetailsWorker();
    void RunInventoryWorker();
};
