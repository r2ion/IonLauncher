#include "modworkshop_service.h"
#include "rtech/rui/workshop_thumbnail_service.h"

#include "tier0/frametask.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <format>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

class CModWorkshopPageCollector final
{
public:
	CModWorkshopPageCollector(ModWorkshopPage& page, const std::unordered_set<uint64_t>& excluded, size_t firstEntry, size_t capacity)
	    : m_Page(page), m_Excluded(excluded), m_FirstEntry(firstEntry), m_Capacity(capacity)
	{
	}

	void Consume(ModWorkshopPage& remotePage)
	{
		for (ModWorkshopCatalogEntry& entry : remotePage.entries)
		{
			if (m_Excluded.contains(entry.id))
				continue;
			if (m_FilteredIndex >= m_FirstEntry && m_Page.entries.size() < m_Capacity)
				m_Page.entries.push_back(std::move(entry));
			++m_FilteredIndex;
		}
	}

private:
	ModWorkshopPage& m_Page;
	const std::unordered_set<uint64_t>& m_Excluded;
	size_t m_FirstEntry;
	size_t m_Capacity;
	size_t m_FilteredIndex = 0;
};

std::string CModWorkshopService::BuildPageCacheKey(std::string_view search, std::string_view sort, int page,
                                                   const std::optional<std::vector<uint64_t>>& restrictedIds,
                                                   const std::optional<std::vector<uint64_t>>& excludedIds)
{
	std::string key = std::format("{}\x1f{}\x1f{}", search, sort, page);
	if (restrictedIds)
	{
		key += "\x1frestricted";
		for (uint64_t id : *restrictedIds)
			key += std::format("\x1f{}", id);
	}
	if (excludedIds)
	{
		key.push_back('\x1f');
		key += "excluded";
		for (uint64_t id : *excludedIds)
			key += std::format("\x1f{}", id);
	}
	return key;
}

bool CModWorkshopService::ListModsExcluding(CModWorkshopClient& client, ModWorkshopListQuery query, const std::vector<uint64_t>& excludedIds,
                                            ModWorkshopPage& page, ModWorkshopError& error, const ModWorkshopRequestOptions& options)
{
	if (excludedIds.empty())
		return client.ListMods(query, page, error, options);

	const std::unordered_set<uint64_t> excluded(excludedIds.begin(), excludedIds.end());
	const int requestedPage = std::max(1, query.page);
	query.page = 1;
	query.limit = REMOTE_PAGE_BATCH_SIZE;
	query.ids.clear();

	ModWorkshopPage remotePage;
	if (!client.ListMods(query, remotePage, error, options))
		return false;

	const int remoteLastPage = std::max(1, remotePage.metadata.lastPage);
	int filteredTotal = 0;
	if (remoteLastPage == 1)
	{
		filteredTotal = static_cast<int>(
		    std::ranges::count_if(remotePage.entries, [&excluded](const ModWorkshopCatalogEntry& entry) { return !excluded.contains(entry.id); }));
	}
	else
	{
		ModWorkshopListQuery excludedQuery = query;
		excludedQuery.limit = 1;
		excludedQuery.ids = excludedIds;
		ModWorkshopPage excludedPage;
		if (!client.ListMods(excludedQuery, excludedPage, error, options))
			return false;
		filteredTotal = std::max(0, remotePage.metadata.total - excludedPage.metadata.total);
	}

	const int lastPage = std::max(1, (filteredTotal + PAGE_BATCH_SIZE - 1) / PAGE_BATCH_SIZE);
	const int currentPage = std::min(requestedPage, lastPage);
	page = {};
	page.metadata.currentPage = currentPage;
	page.metadata.lastPage = lastPage;
	page.metadata.perPage = PAGE_BATCH_SIZE;
	page.metadata.total = filteredTotal;
	if (filteredTotal == 0)
		return true;

	const size_t firstEntry = static_cast<size_t>(currentPage - 1) * PAGE_BATCH_SIZE;
	page.entries.reserve(std::min(PAGE_BATCH_SIZE, filteredTotal));
	CModWorkshopPageCollector collector(page, excluded, firstEntry, PAGE_BATCH_SIZE);
	collector.Consume(remotePage);
	for (int remotePageNumber = 2; page.entries.size() < PAGE_BATCH_SIZE && remotePageNumber <= remoteLastPage; ++remotePageNumber)
	{
		query.page = remotePageNumber;
		if (!client.ListMods(query, remotePage, error, options))
			return false;
		collector.Consume(remotePage);
	}
	return true;
}

void CModWorkshopService::NotifyPage(uint64_t generation)
{
	PageChangedCallback callback;
	{
		std::scoped_lock lock(m_CallbackMutex);
		callback = m_PageChanged;
	}
	if (callback)
		RunInMainThread([callback = std::move(callback), generation] { callback(generation); });
}

void CModWorkshopService::NotifyDetails(uint64_t modId)
{
	DetailsChangedCallback callback;
	{
		std::scoped_lock lock(m_CallbackMutex);
		callback = m_DetailsChanged;
	}
	if (callback)
		RunInMainThread([callback = std::move(callback), modId] { callback(modId); });
}

void CModWorkshopService::NotifyInventory(uint64_t generation, ModWorkshopInventoryUpdateStage stage)
{
	UpdatesChangedCallback callback;
	{
		std::scoped_lock lock(m_CallbackMutex);
		callback = m_UpdatesChanged;
	}
	if (!callback)
		return;
	const std::shared_ptr<const ModWorkshopInventorySnapshot> snapshot = CModWorkshopInventory::Get().GetSnapshot();
	const int updateCount = snapshot ? snapshot->updateCount : 0;
	RunInMainThread([callback = std::move(callback), generation, updateCount, stage] { callback(generation, updateCount, stage); });
}

void CModWorkshopService::PublishPage(std::shared_ptr<const ModWorkshopPageSnapshot> snapshot)
{
	const uint64_t generation = snapshot->generation;
	{
		std::scoped_lock lock(m_PageMutex);
		m_PageSnapshot = snapshot;
	}
	if (snapshot->state == ModWorkshopLoadState::Ready && snapshot->page)
		CWorkshopThumbnailService::Get().RequestPage(generation, snapshot->page->entries);
	NotifyPage(generation);
}

void CModWorkshopService::PublishDetails(std::shared_ptr<const ModWorkshopDetailsSnapshot> snapshot)
{
	const uint64_t modId = snapshot->modId;
	{
		std::scoped_lock lock(m_DetailsMutex);
		m_DetailsSnapshot = std::move(snapshot);
	}
	NotifyDetails(modId);
}

void CModWorkshopService::CachePage(const std::string& key, std::shared_ptr<const ModWorkshopPage> page)
{
	std::scoped_lock lock(m_PageMutex);
	m_PageCache.insert_or_assign(key, std::move(page));
	m_PageCacheOrder.push_back(key);
	while (m_PageCache.size() > MAX_PAGE_CACHE_ENTRIES && !m_PageCacheOrder.empty())
	{
		std::string oldest = std::move(m_PageCacheOrder.front());
		m_PageCacheOrder.pop_front();
		if (std::ranges::find(m_PageCacheOrder, oldest) == m_PageCacheOrder.end())
			m_PageCache.erase(oldest);
	}
}

void CModWorkshopService::CacheDetails(uint64_t modId, std::shared_ptr<const ModWorkshopDetails> details)
{
	std::scoped_lock lock(m_DetailsMutex);
	m_DetailsCache.insert_or_assign(modId, std::move(details));
	m_DetailsCacheOrder.push_back(modId);
	while (m_DetailsCache.size() > MAX_DETAILS_CACHE_ENTRIES && !m_DetailsCacheOrder.empty())
	{
		const uint64_t oldest = m_DetailsCacheOrder.front();
		m_DetailsCacheOrder.pop_front();
		if (std::ranges::find(m_DetailsCacheOrder, oldest) == m_DetailsCacheOrder.end())
			m_DetailsCache.erase(oldest);
	}
}

void CModWorkshopService::EnsurePageWorker()
{
	std::scoped_lock lock(m_PageRequestMutex);
	if (!m_PageWorker.joinable() && !m_Stopped.load(std::memory_order_acquire))
		m_PageWorker = std::thread([this] { RunPageWorker(); });
}

void CModWorkshopService::EnsureDetailsWorker()
{
	std::scoped_lock lock(m_DetailsRequestMutex);
	if (!m_DetailsWorker.joinable() && !m_Stopped.load(std::memory_order_acquire))
		m_DetailsWorker = std::thread([this] { RunDetailsWorker(); });
}

void CModWorkshopService::EnsureInventoryWorker()
{
	std::scoped_lock lock(m_InventoryRequestMutex);
	if (!m_InventoryWorker.joinable() && !m_Stopped.load(std::memory_order_acquire))
		m_InventoryWorker = std::thread([this] { RunInventoryWorker(); });
}

void CModWorkshopService::RunPageWorker()
{
	for (;;)
	{
		PageRequest request;
		{
			std::unique_lock lock(m_PageRequestMutex);
			m_PageRequestChanged.wait(lock, [this] { return m_Stopped.load(std::memory_order_acquire) || m_PendingPageRequest.has_value(); });
			if (m_Stopped.load(std::memory_order_acquire))
				return;
			request = std::move(*m_PendingPageRequest);
			m_PendingPageRequest.reset();
		}

		ModWorkshopRequestOptions options;
		options.isCancelled = [this, generation = request.generation]
		{ return m_Stopped.load(std::memory_order_acquire) || m_PageGeneration.load(std::memory_order_acquire) != generation; };

		uint64_t resolvedGameId = m_GameId.load(std::memory_order_acquire);
		ModWorkshopError error;
		if (resolvedGameId == 0 && m_Client.ResolveGameId(TITANFALL_2_SLUG, resolvedGameId, error, options))
		{
			m_GameId.store(resolvedGameId, std::memory_order_release);
		}

		if (resolvedGameId == 0)
		{
			if (options.isCancelled())
				continue;
			auto snapshot = std::make_shared<ModWorkshopPageSnapshot>();
			snapshot->generation = request.generation;
			snapshot->state = ModWorkshopLoadState::Failed;
			snapshot->search = request.query.search;
			snapshot->sort = request.query.sort;
			snapshot->requestedPage = request.query.page;
			snapshot->page = std::make_shared<ModWorkshopPage>();
			snapshot->error = std::move(error);
			PublishPage(std::move(snapshot));
			continue;
		}

		request.query.gameId = resolvedGameId;
		ModWorkshopPage page;
		if (!ListModsExcluding(m_Client, request.query, request.excludedIds, page, error, options))
		{
			if (options.isCancelled())
				continue;
			auto snapshot = std::make_shared<ModWorkshopPageSnapshot>();
			snapshot->generation = request.generation;
			snapshot->state = error.code == ModWorkshopErrorCode::Cancelled ? ModWorkshopLoadState::Cancelled : ModWorkshopLoadState::Failed;
			snapshot->search = request.query.search;
			snapshot->sort = request.query.sort;
			snapshot->requestedPage = request.query.page;
			snapshot->page = std::make_shared<ModWorkshopPage>();
			snapshot->error = std::move(error);
			PublishPage(std::move(snapshot));
			continue;
		}
		if (options.isCancelled())
			continue;

		auto immutablePage = std::make_shared<const ModWorkshopPage>(std::move(page));
		CachePage(request.cacheKey, immutablePage);
		auto snapshot = std::make_shared<ModWorkshopPageSnapshot>();
		snapshot->generation = request.generation;
		snapshot->state = ModWorkshopLoadState::Ready;
		snapshot->search = request.query.search;
		snapshot->sort = request.query.sort;
		snapshot->requestedPage = request.query.page;
		snapshot->page = std::move(immutablePage);
		PublishPage(std::move(snapshot));
	}
}

void CModWorkshopService::RunDetailsWorker()
{
	for (;;)
	{
		DetailsRequest request;
		{
			std::unique_lock lock(m_DetailsRequestMutex);
			m_DetailsRequestChanged.wait(lock, [this] { return m_Stopped.load(std::memory_order_acquire) || m_PendingDetailsRequest.has_value(); });
			if (m_Stopped.load(std::memory_order_acquire))
				return;
			request = *m_PendingDetailsRequest;
			m_PendingDetailsRequest.reset();
		}

		ModWorkshopRequestOptions options;
		options.isCancelled = [this, generation = request.generation]
		{ return m_Stopped.load(std::memory_order_acquire) || m_DetailsGeneration.load(std::memory_order_acquire) != generation; };

		ModWorkshopDetails details;
		ModWorkshopError error;
		if (!m_Client.GetMod(request.modId, details, error, options))
		{
			if (options.isCancelled())
				continue;
			auto snapshot = std::make_shared<ModWorkshopDetailsSnapshot>();
			snapshot->generation = request.generation;
			snapshot->modId = request.modId;
			snapshot->state = error.code == ModWorkshopErrorCode::Cancelled ? ModWorkshopLoadState::Cancelled : ModWorkshopLoadState::Failed;
			snapshot->error = std::move(error);
			PublishDetails(std::move(snapshot));
			continue;
		}
		if (options.isCancelled())
			continue;

		auto immutableDetails = std::make_shared<const ModWorkshopDetails>(std::move(details));
		CacheDetails(request.modId, immutableDetails);
		auto snapshot = std::make_shared<ModWorkshopDetailsSnapshot>();
		snapshot->generation = request.generation;
		snapshot->modId = request.modId;
		snapshot->state = ModWorkshopLoadState::Ready;
		snapshot->details = std::move(immutableDetails);
		PublishDetails(std::move(snapshot));
	}
}

void CModWorkshopService::RunInventoryWorker()
{
	for (;;)
	{
		InventoryRequest request;
		{
			std::unique_lock lock(m_InventoryRequestMutex);
			m_InventoryRequestChanged.wait(lock,
			                               [this] { return m_Stopped.load(std::memory_order_acquire) || m_PendingInventoryRequest.has_value(); });
			if (m_Stopped.load(std::memory_order_acquire))
				return;
			request = *m_PendingInventoryRequest;
			m_PendingInventoryRequest.reset();
		}

		CModWorkshopInventory::Get().RefreshLocal();
		if (m_InventoryGeneration.load(std::memory_order_acquire) != request.generation)
			continue;
		NotifyInventory(request.generation, request.checkRemote ? ModWorkshopInventoryUpdateStage::LocalCompleteRemotePending
		                                                        : ModWorkshopInventoryUpdateStage::LocalComplete);
		if (!request.checkRemote)
			continue;

		ModWorkshopRequestOptions options;
		options.isCancelled = [this, generation = request.generation]
		{ return m_Stopped.load(std::memory_order_acquire) || m_InventoryGeneration.load(std::memory_order_acquire) != generation; };
		CModWorkshopInventory::Get().CheckForUpdates(m_Client, options);
		if (!options.isCancelled())
			NotifyInventory(request.generation, ModWorkshopInventoryUpdateStage::RemoteComplete);
	}
}

CModWorkshopService::CModWorkshopService()
{
	auto initialPage = std::make_shared<ModWorkshopPageSnapshot>();
	initialPage->page = std::make_shared<ModWorkshopPage>();
	m_PageSnapshot = std::move(initialPage);

	auto initialDetails = std::make_shared<ModWorkshopDetailsSnapshot>();
	m_DetailsSnapshot = std::move(initialDetails);
}

uint64_t CModWorkshopService::RequestPage(std::string search, std::string sort, int page, bool forceRefresh,
                                          std::optional<std::vector<uint64_t>> restrictedIds, std::optional<std::vector<uint64_t>> excludedIds)
{
	if (search.size() > 150)
		search.resize(150);
	page = std::max(1, page);
	if (restrictedIds)
		std::ranges::sort(*restrictedIds);
	if (excludedIds)
		std::ranges::sort(*excludedIds);
	const uint64_t generation = m_PageGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
	const std::string cacheKey = BuildPageCacheKey(search, sort, page, restrictedIds, excludedIds);

	if (restrictedIds && restrictedIds->empty())
	{
		auto emptyPage = std::make_shared<ModWorkshopPage>();
		emptyPage->metadata.currentPage = 1;
		emptyPage->metadata.lastPage = 1;
		auto snapshot = std::make_shared<ModWorkshopPageSnapshot>();
		snapshot->generation = generation;
		snapshot->state = ModWorkshopLoadState::Ready;
		snapshot->search = std::move(search);
		snapshot->sort = std::move(sort);
		snapshot->requestedPage = page;
		snapshot->page = std::move(emptyPage);
		PublishPage(std::move(snapshot));
		return generation;
	}

	if (!forceRefresh)
	{
		std::shared_ptr<const ModWorkshopPage> cached;
		{
			std::scoped_lock lock(m_PageMutex);
			const auto found = m_PageCache.find(cacheKey);
			if (found != m_PageCache.end())
				cached = found->second;
		}
		if (cached)
		{
			auto snapshot = std::make_shared<ModWorkshopPageSnapshot>();
			snapshot->generation = generation;
			snapshot->state = ModWorkshopLoadState::Ready;
			snapshot->search = std::move(search);
			snapshot->sort = std::move(sort);
			snapshot->requestedPage = page;
			snapshot->page = std::move(cached);
			snapshot->fromCache = true;
			PublishPage(std::move(snapshot));
			return generation;
		}
	}

	auto loading = std::make_shared<ModWorkshopPageSnapshot>();
	loading->generation = generation;
	loading->state = ModWorkshopLoadState::Loading;
	loading->search = search;
	loading->sort = sort;
	loading->requestedPage = page;
	loading->page = std::make_shared<ModWorkshopPage>();
	PublishPage(std::move(loading));

	EnsurePageWorker();
	{
		std::scoped_lock lock(m_PageRequestMutex);
		m_PendingPageRequest = PageRequest{.generation = generation,
		                                   .cacheKey = cacheKey,
		                                   .query = ModWorkshopListQuery{.gameId = 0,
		                                                                 .search = std::move(search),
		                                                                 .sort = std::move(sort),
		                                                                 .page = page,
		                                                                 .limit = PAGE_BATCH_SIZE,
		                                                                 .ids = restrictedIds ? std::move(*restrictedIds) : std::vector<uint64_t>{}},
		                                   .excludedIds = excludedIds ? std::move(*excludedIds) : std::vector<uint64_t>{}};
	}
	m_PageRequestChanged.notify_one();
	return generation;
}

uint64_t CModWorkshopService::RequestDetails(uint64_t modId, bool forceRefresh)
{
	const uint64_t generation = m_DetailsGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
	if (!forceRefresh)
	{
		std::shared_ptr<const ModWorkshopDetails> cached;
		{
			std::scoped_lock lock(m_DetailsMutex);
			const auto found = m_DetailsCache.find(modId);
			if (found != m_DetailsCache.end())
				cached = found->second;
		}
		if (cached)
		{
			auto snapshot = std::make_shared<ModWorkshopDetailsSnapshot>();
			snapshot->generation = generation;
			snapshot->modId = modId;
			snapshot->state = ModWorkshopLoadState::Ready;
			snapshot->details = std::move(cached);
			snapshot->fromCache = true;
			PublishDetails(std::move(snapshot));
			return generation;
		}
	}

	auto loading = std::make_shared<ModWorkshopDetailsSnapshot>();
	loading->generation = generation;
	loading->modId = modId;
	loading->state = ModWorkshopLoadState::Loading;
	PublishDetails(std::move(loading));

	EnsureDetailsWorker();
	{
		std::scoped_lock lock(m_DetailsRequestMutex);
		m_PendingDetailsRequest = DetailsRequest{generation, modId};
	}
	m_DetailsRequestChanged.notify_one();
	return generation;
}

uint64_t CModWorkshopService::RefreshTrackedMods(bool checkRemote)
{
	const uint64_t generation = m_InventoryGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
	EnsureInventoryWorker();
	{
		std::scoped_lock lock(m_InventoryRequestMutex);
		m_PendingInventoryRequest = InventoryRequest{generation, checkRemote};
	}
	m_InventoryRequestChanged.notify_one();
	return generation;
}

void CModWorkshopService::CancelPageRequest()
{
	const uint64_t generation = m_PageGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
	std::shared_ptr<const ModWorkshopPageSnapshot> current = GetPageSnapshot();
	if (!current || current->state != ModWorkshopLoadState::Loading)
		return;
	auto cancelled = std::make_shared<ModWorkshopPageSnapshot>(*current);
	cancelled->generation = generation;
	cancelled->state = ModWorkshopLoadState::Cancelled;
	cancelled->error = {ModWorkshopErrorCode::Cancelled, 0, 0, {}, "ModWorkshop page request cancelled"};
	PublishPage(std::move(cancelled));
}

void CModWorkshopService::CancelDetailsRequest()
{
	const uint64_t generation = m_DetailsGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
	std::shared_ptr<const ModWorkshopDetailsSnapshot> current = GetDetailsSnapshot();
	if (!current || current->state != ModWorkshopLoadState::Loading)
		return;
	auto cancelled = std::make_shared<ModWorkshopDetailsSnapshot>(*current);
	cancelled->generation = generation;
	cancelled->state = ModWorkshopLoadState::Cancelled;
	cancelled->error = {ModWorkshopErrorCode::Cancelled, 0, 0, {}, "ModWorkshop details request cancelled"};
	PublishDetails(std::move(cancelled));
}

std::shared_ptr<const ModWorkshopPageSnapshot> CModWorkshopService::GetPageSnapshot() const
{
	std::scoped_lock lock(m_PageMutex);
	return m_PageSnapshot;
}

std::shared_ptr<const ModWorkshopDetailsSnapshot> CModWorkshopService::GetDetailsSnapshot() const
{
	std::scoped_lock lock(m_DetailsMutex);
	return m_DetailsSnapshot;
}

std::shared_ptr<const ModWorkshopInventorySnapshot> CModWorkshopService::GetInventorySnapshot() const
{
	return CModWorkshopInventory::Get().GetSnapshot();
}

void CModWorkshopService::SetPageChangedCallback(PageChangedCallback callback)
{
	std::scoped_lock lock(m_CallbackMutex);
	m_PageChanged = std::move(callback);
}

void CModWorkshopService::SetDetailsChangedCallback(DetailsChangedCallback callback)
{
	std::scoped_lock lock(m_CallbackMutex);
	m_DetailsChanged = std::move(callback);
}

void CModWorkshopService::SetUpdatesChangedCallback(UpdatesChangedCallback callback)
{
	std::scoped_lock lock(m_CallbackMutex);
	m_UpdatesChanged = std::move(callback);
}

void CModWorkshopService::ClearCallbacks()
{
	std::scoped_lock lock(m_CallbackMutex);
	m_PageChanged = {};
	m_DetailsChanged = {};
	m_UpdatesChanged = {};
}

void CModWorkshopService::Shutdown()
{
	if (m_Stopped.exchange(true, std::memory_order_acq_rel))
		return;
	ClearCallbacks();
	m_PageRequestChanged.notify_all();
	m_DetailsRequestChanged.notify_all();
	m_InventoryRequestChanged.notify_all();
	if (m_PageWorker.joinable() && m_PageWorker.get_id() != std::this_thread::get_id())
		m_PageWorker.join();
	if (m_DetailsWorker.joinable() && m_DetailsWorker.get_id() != std::this_thread::get_id())
		m_DetailsWorker.join();
	if (m_InventoryWorker.joinable() && m_InventoryWorker.get_id() != std::this_thread::get_id())
	{
		m_InventoryWorker.join();
	}
	CWorkshopThumbnailService::Get().Shutdown();
}
