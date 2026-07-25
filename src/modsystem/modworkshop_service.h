#pragma once

#include "modsystem/modworkshop_inventory.h"
#include "modsystem/platform/modworkshop.h"

#include <atomic>
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

enum class ModWorkshopLoadState
{
	Idle,
	Loading,
	Ready,
	Failed,
	Cancelled,
};

enum class ModWorkshopInventoryUpdateStage
{
	LocalComplete,
	LocalCompleteRemotePending,
	RemoteComplete,
};

struct ModWorkshopPageSnapshot
{
	uint64_t generation = 0;
	ModWorkshopLoadState state = ModWorkshopLoadState::Idle;
	std::string search;
	std::string sort = "bumped_at";
	int requestedPage = 1;
	std::shared_ptr<const ModWorkshopPage> page;
	ModWorkshopError error;
	bool fromCache = false;
};

struct ModWorkshopDetailsSnapshot
{
	uint64_t generation = 0;
	uint64_t modId = 0;
	ModWorkshopLoadState state = ModWorkshopLoadState::Idle;
	std::shared_ptr<const ModWorkshopDetails> details;
	ModWorkshopError error;
	bool fromCache = false;
};

class CModWorkshopService final
{
public:
	using PageChangedCallback = std::function<void(uint64_t)>;
	using DetailsChangedCallback = std::function<void(uint64_t)>;
	using UpdatesChangedCallback = std::function<void(uint64_t, int, ModWorkshopInventoryUpdateStage)>;

	static CModWorkshopService& Get()
	{
		static CModWorkshopService* s_pInstance = new CModWorkshopService;
		return *s_pInstance;
	}

	uint64_t RequestPage(std::string search, std::string sort, int page, bool forceRefresh = false,
	                     std::optional<std::vector<uint64_t>> restrictedIds = std::nullopt,
	                     std::optional<std::vector<uint64_t>> excludedIds = std::nullopt);
	uint64_t RequestDetails(uint64_t modId, bool forceRefresh = false);
	uint64_t RefreshTrackedMods(bool checkRemote = true);
	void CancelPageRequest();
	void CancelDetailsRequest();

	std::shared_ptr<const ModWorkshopPageSnapshot> GetPageSnapshot() const;
	std::shared_ptr<const ModWorkshopDetailsSnapshot> GetDetailsSnapshot() const;
	std::shared_ptr<const ModWorkshopInventorySnapshot> GetInventorySnapshot() const;

	void SetPageChangedCallback(PageChangedCallback callback);
	void SetDetailsChangedCallback(DetailsChangedCallback callback);
	void SetUpdatesChangedCallback(UpdatesChangedCallback callback);
	void ClearCallbacks();

	void Shutdown();

	CModWorkshopService(const CModWorkshopService&) = delete;
	CModWorkshopService& operator=(const CModWorkshopService&) = delete;

private:
	CModWorkshopService();
	~CModWorkshopService() = delete;

	static constexpr size_t MAX_PAGE_CACHE_ENTRIES = 32;
	static constexpr size_t MAX_DETAILS_CACHE_ENTRIES = 64;
	static constexpr int PAGE_BATCH_SIZE = 24;
	static constexpr int REMOTE_PAGE_BATCH_SIZE = 50;
	static constexpr std::string_view TITANFALL_2_SLUG = "titanfall-2";

	struct PageRequest
	{
		uint64_t generation = 0;
		std::string cacheKey;
		ModWorkshopListQuery query;
		std::vector<uint64_t> excludedIds;
	};

	struct DetailsRequest
	{
		uint64_t generation = 0;
		uint64_t modId = 0;
	};

	struct InventoryRequest
	{
		uint64_t generation = 0;
		bool checkRemote = true;
	};

	static std::string BuildPageCacheKey(std::string_view search, std::string_view sort, int page,
	                                     const std::optional<std::vector<uint64_t>>& restrictedIds,
	                                     const std::optional<std::vector<uint64_t>>& excludedIds);
	static bool ListModsExcluding(CModWorkshopClient& client, ModWorkshopListQuery query, const std::vector<uint64_t>& excludedIds,
	                              ModWorkshopPage& page, ModWorkshopError& error, const ModWorkshopRequestOptions& options);

	CModWorkshopClient m_Client;
	std::atomic<bool> m_Stopped = false;
	std::atomic<uint64_t> m_PageGeneration = 0;
	std::atomic<uint64_t> m_DetailsGeneration = 0;
	std::atomic<uint64_t> m_InventoryGeneration = 0;
	std::atomic<uint64_t> m_GameId = 0;
	mutable std::mutex m_PageMutex;
	std::shared_ptr<const ModWorkshopPageSnapshot> m_PageSnapshot;
	std::unordered_map<std::string, std::shared_ptr<const ModWorkshopPage>> m_PageCache;
	std::deque<std::string> m_PageCacheOrder;
	mutable std::mutex m_DetailsMutex;
	std::shared_ptr<const ModWorkshopDetailsSnapshot> m_DetailsSnapshot;
	std::unordered_map<uint64_t, std::shared_ptr<const ModWorkshopDetails>> m_DetailsCache;
	std::deque<uint64_t> m_DetailsCacheOrder;
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
	void NotifyDetails(uint64_t modId);
	void NotifyInventory(uint64_t generation, ModWorkshopInventoryUpdateStage stage);
	void PublishPage(std::shared_ptr<const ModWorkshopPageSnapshot> snapshot);
	void PublishDetails(std::shared_ptr<const ModWorkshopDetailsSnapshot> snapshot);
	void CachePage(const std::string& key, std::shared_ptr<const ModWorkshopPage> page);
	void CacheDetails(uint64_t modId, std::shared_ptr<const ModWorkshopDetails> details);
	void EnsurePageWorker();
	void EnsureDetailsWorker();
	void EnsureInventoryWorker();
	void RunPageWorker();
	void RunDetailsWorker();
	void RunInventoryWorker();
};
