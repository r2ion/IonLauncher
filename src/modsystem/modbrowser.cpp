#include "modsystem/modbrowser.h"
#include "modsystem/modinstaller.h"
#include "modsystem/platform/thunderstore.h"
#include "tier0/frametask.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <format>
#include <random>
#include <unordered_set>
#include <utility>

class CModBrowserData final
{
  public:
    static bool Parse(std::string_view id, ModSource& source, std::string& packageId, uint64_t& modId)
    {
        modId = 0;
        if (id.starts_with("modworkshop:"))
        {
            id.remove_prefix(12);
            const auto [end, error] = std::from_chars(id.data(), id.data() + id.size(), modId);
            if (id.empty() || error != std::errc() || end != id.data() + id.size() || modId == 0 || std::to_string(modId) != id)
                return false;
            source = ModSource::ModWorkshop;
        }
        else if (id.starts_with("thunderstore:"))
        {
            id.remove_prefix(13);
            std::string owner, name;
            if (!CThunderstoreClient::ParsePackageId(id, owner, name))
                return false;
            source = ModSource::Thunderstore;
        }
        else
            return false;
        packageId = id;
        return true;
    }
    static std::string Lower(std::string_view value)
    {
        std::string result(value);
        std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }
    static ModBrowserEntry Convert(const ModWorkshopCatalogEntry& value)
    {
        ModBrowserEntry entry;
        entry.source = ModSource::ModWorkshop;
        entry.id = "modworkshop:" + std::to_string(value.id);
        entry.name = value.name;
        entry.author = value.author;
        entry.shortDescription = value.shortDescription;
        entry.description = value.description;
        entry.version = value.version;
        entry.selectedFileId = value.selectedFileId ? std::to_string(*value.selectedFileId) : std::string();
        entry.pageUrl = CModWorkshopClient::BuildModPageUrl(value.id);
        entry.downloads = value.downloads;
        entry.likes = value.likes;
        entry.views = value.views;
        entry.score = value.score;
        entry.weeklyScore = value.weeklyScore;
        entry.dailyScore = value.dailyScore;
        entry.approved = value.approved;
        entry.suspended = value.suspended;
        entry.hasDownload = value.hasDownload;
        entry.disableModManagers = value.disableModManagers;
        entry.updatedAt = value.updatedAt;
        entry.publishedAt = value.publishedAt;
        if (value.thumbnail)
        {
            entry.thumbnailUrl = CModWorkshopClient::BuildThumbnailUrl(*value.thumbnail);
            auto original = *value.thumbnail;
            original.hasThumbnail = false;
            if (value.thumbnail->hasThumbnail)
                entry.thumbnailFallbackUrl = CModWorkshopClient::BuildThumbnailUrl(original);
            entry.thumbnailVersion = value.thumbnail->updatedAt;
        }
        return entry;
    }
    static ModBrowserEntry Convert(const CThunderstoreClient::PackageDetails& value)
    {
        ModBrowserEntry entry;
        entry.source = ModSource::Thunderstore;
        entry.id = "thunderstore:" + value.m_Namespace + "-" + value.m_Name;
        entry.name = value.m_Name;
        entry.author = value.m_Namespace;
        entry.shortDescription = value.m_Description;
        entry.description = value.m_Readme.empty() ? value.m_Description : value.m_Readme;
        entry.version = entry.selectedFileId = value.m_Version;
        entry.pageUrl = value.m_PageUrl;
        entry.thumbnailUrl = value.m_IconUrl;
        entry.thumbnailVersion = value.m_Version;
        entry.downloads = value.m_Downloads;
        entry.likes = value.m_Rating;
        entry.approved = value.m_Active;
        entry.suspended = value.m_Deprecated;
        entry.hasDownload = value.m_Active && !value.m_DownloadUrl.empty();
        entry.disableModManagers =
            value.m_Namespace == "northstar" && value.m_Name == "Northstar" || value.m_Namespace == "ebkr" && value.m_Name == "r2modman";
        entry.updatedAt = value.m_UpdatedAt;
        entry.publishedAt = value.m_CreatedAt;
        return entry;
    }
};

std::string CModBrowserService::BuildId(ModSource source, std::string_view packageId)
{
    std::string id;
    if (source == ModSource::ModWorkshop)
        id = "modworkshop:";
    else if (source == ModSource::Thunderstore)
        id = "thunderstore:";
    else
        return {};
    id += packageId;
    ModSource parsedSource;
    std::string parsedId;
    uint64_t modId;
    return CModBrowserData::Parse(id, parsedSource, parsedId, modId) ? id : std::string();
}

std::string CModBrowserService::BuildPageUrl(std::string_view id)
{
    ModSource source;
    std::string packageId;
    uint64_t modId;
    if (!CModBrowserData::Parse(id, source, packageId, modId))
        return {};
    if (source == ModSource::ModWorkshop)
        return CModWorkshopClient::BuildModPageUrl(modId);
    std::string owner, name;
    CThunderstoreClient::ParsePackageId(packageId, owner, name);
    return "https://thunderstore.io/c/northstar/p/" + owner + "/" + name + "/";
}

bool CModBrowserService::RequestOperation(ModInstallAction action, std::string_view id)
{
    if (m_Stopped.load(std::memory_order_acquire))
        return false;
    ModSource source;
    std::string packageId;
    uint64_t modId;
    if (!CModBrowserData::Parse(id, source, packageId, modId))
        return false;
    return source == ModSource::ModWorkshop ? CModInstallService::Get().Request(action, modId)
                                            : CModInstallService::Get().RequestThunderstore(action, std::move(packageId));
}

void CModBrowserService::ApplyInventory(ModBrowserEntry& entry, const ModInventorySnapshot& inventory)
{
    entry.installed = false;
    entry.updateState = static_cast<int>(ModUpdateState::LegacyUnknown);
    const std::string_view packageId = std::string_view(entry.id).substr(entry.id.find(':') + 1);
    for (const auto& package : inventory.packages)
    {
        if (package.source == entry.source && package.packageId == packageId)
        {
            entry.installed = true;
            entry.updateState = static_cast<int>(package.updateState);
            return;
        }
    }
}

bool CModBrowserService::LoadCatalog(ModSource source, bool forceRefresh, std::vector<const ModBrowserEntry*>& entries, ModRequestError& error,
                                     const ModRequestOptions& options)
{
    auto& cache = source == ModSource::ModWorkshop ? m_WorkshopCatalog : m_ThunderstoreCatalog;
    if (!forceRefresh && cache.valid && std::chrono::steady_clock::now() - cache.fetchedAt < std::chrono::minutes(5))
    {
        entries.reserve(entries.size() + cache.entries.size());
        for (const auto& entry : cache.entries)
            entries.push_back(&entry);
        return true;
    }
    std::vector<ModBrowserEntry> fetched;
    if (source == ModSource::Thunderstore)
    {
        std::vector<CThunderstoreClient::PackageDetails> packages;
        auto bounded = options;
        bounded.maxResponseBytes = 32 * 1024 * 1024;
        if (!CThunderstoreClient::FetchCatalog(packages, error, bounded))
            return false;
        fetched.reserve(packages.size());
        for (const auto& package : packages)
        {
            // Launchers and client replacements cannot be installed into this mod profile.
            const bool externalInstall =
                (package.m_Namespace == "northstar" && (package.m_Name == "Northstar" || package.m_Name == "NorthstarReleaseCandidate")) ||
                (package.m_Namespace == "ebkr" && package.m_Name == "r2modman") ||
                (package.m_Namespace == "Kesomannen" && package.m_Name == "GaleModManager") ||
                (package.m_Namespace == "NachosChipeados" && package.m_Name == "VanillaPlus") ||
                (package.m_Namespace == "ZhuangFangyi" && package.m_Name == "VanillaPlusCN");
            if (package.m_Deprecated || externalInstall)
                continue;
            fetched.push_back(CModBrowserData::Convert(package));
        }
    }
    else
    {
        uint64_t gameId = m_GameId.load(std::memory_order_acquire);
        if (!gameId && !m_Client.ResolveGameId(TITANFALL_2_SLUG, gameId, error, options))
            return false;
        m_GameId.store(gameId, std::memory_order_release);
        ModWorkshopListQuery query;
        query.gameId = gameId;
        query.limit = REMOTE_PAGE_BATCH_SIZE;
        query.sort = "published_at";
        std::unordered_set<uint64_t> seen;
        for (;; ++query.page)
        {
            ModWorkshopPage page;
            if (!m_Client.ListMods(query, page, error, options))
                return false;
            if (page.metadata.lastPage > 200 || page.metadata.total > 10000 || page.metadata.currentPage != query.page)
            {
                error = {ModRequestErrorCode::ResponseTooLarge, 0, 0, {}, "ModWorkshop catalog exceeds bounded pagination"};
                return false;
            }
            for (const auto& entry : page.entries)
                if (seen.insert(entry.id).second)
                    fetched.push_back(CModBrowserData::Convert(entry));
            if (query.page >= page.metadata.lastPage)
                break;
        }
    }
    if (options.isCancelled && options.isCancelled())
        return false;
    cache.entries = std::move(fetched);
    cache.valid = true;
    cache.fetchedAt = std::chrono::steady_clock::now();
    entries.reserve(entries.size() + cache.entries.size());
    for (const auto& entry : cache.entries)
        entries.push_back(&entry);
    return true;
}

bool CModBrowserService::LoadDetails(std::string_view id, ModBrowserDetails& details, ModRequestError& error, const ModRequestOptions& options)
{
    ModSource source;
    std::string packageId;
    uint64_t modId;
    if (!CModBrowserData::Parse(id, source, packageId, modId))
    {
        error = {ModRequestErrorCode::InvalidArgument, 0, 0, {}, "Invalid source-qualified mod ID"};
        return false;
    }
    if (source == ModSource::ModWorkshop)
    {
        ModWorkshopDetails remote;
        if (!m_Client.GetMod(modId, remote, error, options))
            return false;
        static_cast<ModBrowserEntry&>(details) = CModBrowserData::Convert(remote);
        if (remote.selectedFile)
        {
            details.selectedFileSize = remote.selectedFile->size;
            details.selectedFileUpdatedAt = remote.selectedFile->updatedAt;
            details.version = remote.selectedFile->version.empty() ? remote.version : remote.selectedFile->version;
        }
        for (const auto& dependency : remote.dependencies)
            details.dependencies.push_back(!dependency.name.empty() ? dependency.name
                                           : dependency.modId       ? "modworkshop:" + std::to_string(*dependency.modId)
                                                                    : dependency.url);
    }
    else
    {
        std::string owner, name;
        CThunderstoreClient::ParsePackageId(packageId, owner, name);
        CThunderstoreClient::PackageDetails remote;
        if (!CThunderstoreClient::FetchPackageDetails(owner, name, remote, options, &error) ||
            !CThunderstoreClient::FetchReadme(remote, error, options))
            return false;
        static_cast<ModBrowserEntry&>(details) = CModBrowserData::Convert(remote);
        details.selectedFileSize = remote.m_FileSize;
        details.selectedFileUpdatedAt = remote.m_UpdatedAt;
        details.dependencies = std::move(remote.m_Dependencies);
    }
    ApplyInventory(details, *GetInventorySnapshot());
    return true;
}

void CModBrowserService::NotifyPage(uint64_t generation)
{
    PageChangedCallback callback;
    {
        std::scoped_lock lock(m_CallbackMutex);
        callback = m_PageChanged;
    }
    if (callback)
        RunInMainThread([callback = std::move(callback), generation] { callback(generation); });
}

void CModBrowserService::NotifyDetails(const std::string& id)
{
    DetailsChangedCallback callback;
    {
        std::scoped_lock lock(m_CallbackMutex);
        callback = m_DetailsChanged;
    }
    if (callback)
        RunInMainThread([callback = std::move(callback), id] { callback(id); });
}

void CModBrowserService::NotifyInventory(uint64_t generation, ModInventoryUpdateStage stage)
{
    UpdatesChangedCallback callback;
    {
        std::scoped_lock lock(m_CallbackMutex);
        callback = m_UpdatesChanged;
    }
    if (!callback)
        return;
    const std::shared_ptr<const ModInventorySnapshot> snapshot = CModInventory::Get().GetSnapshot();
    const int updateCount = snapshot ? snapshot->updateCount : 0;
    RunInMainThread([callback = std::move(callback), generation, updateCount, stage] { callback(generation, updateCount, stage); });
}

void CModBrowserService::PublishPage(std::shared_ptr<const ModBrowserPageSnapshot> snapshot)
{
    const uint64_t generation = snapshot->generation;
    {
        std::scoped_lock lock(m_PageMutex);
        if (snapshot->generation != m_PageGeneration.load(std::memory_order_acquire) || m_Stopped.load(std::memory_order_acquire))
            return;
        m_PageSnapshot = snapshot;
    }
    NotifyPage(generation);
}

void CModBrowserService::PublishDetails(std::shared_ptr<const ModBrowserDetailsSnapshot> snapshot)
{
    const std::string id = snapshot->id;
    {
        std::scoped_lock lock(m_DetailsMutex);
        if (snapshot->generation != m_DetailsGeneration.load(std::memory_order_acquire) || m_Stopped.load(std::memory_order_acquire))
            return;
        m_DetailsSnapshot = std::move(snapshot);
    }
    NotifyDetails(id);
}

void CModBrowserService::CachePage(uint64_t generation, const std::string& key, std::shared_ptr<const ModBrowserPage> page)
{
    std::scoped_lock lock(m_PageMutex);
    if (generation != m_PageGeneration.load(std::memory_order_acquire))
        return;
    m_PageCache.insert_or_assign(key, std::move(page));
    std::erase(m_PageCacheOrder, key);
    m_PageCacheOrder.push_back(key);
    while (m_PageCache.size() > MAX_PAGE_CACHE_ENTRIES && !m_PageCacheOrder.empty())
    {
        std::string oldest = std::move(m_PageCacheOrder.front());
        m_PageCacheOrder.pop_front();
        if (std::ranges::find(m_PageCacheOrder, oldest) == m_PageCacheOrder.end())
            m_PageCache.erase(oldest);
    }
}

void CModBrowserService::CacheDetails(uint64_t generation, const std::string& id, std::shared_ptr<const ModBrowserDetails> details)
{
    std::scoped_lock lock(m_DetailsMutex);
    if (generation != m_DetailsGeneration.load(std::memory_order_acquire))
        return;
    m_DetailsCache.insert_or_assign(id, CachedDetails{std::move(details), std::chrono::steady_clock::now()});
    std::erase(m_DetailsCacheOrder, id);
    m_DetailsCacheOrder.push_back(id);
    while (m_DetailsCache.size() > MAX_DETAILS_CACHE_ENTRIES && !m_DetailsCacheOrder.empty())
    {
        const std::string oldest = m_DetailsCacheOrder.front();
        m_DetailsCacheOrder.pop_front();
        if (std::ranges::find(m_DetailsCacheOrder, oldest) == m_DetailsCacheOrder.end())
            m_DetailsCache.erase(oldest);
    }
}

void CModBrowserService::EnsurePageWorker()
{
    std::scoped_lock lock(m_PageRequestMutex);
    if (!m_PageWorker.joinable() && !m_Stopped.load(std::memory_order_acquire))
        m_PageWorker = std::thread([this] { RunPageWorker(); });
}

void CModBrowserService::EnsureDetailsWorker()
{
    std::scoped_lock lock(m_DetailsRequestMutex);
    if (!m_DetailsWorker.joinable() && !m_Stopped.load(std::memory_order_acquire))
        m_DetailsWorker = std::thread([this] { RunDetailsWorker(); });
}

void CModBrowserService::EnsureInventoryWorker()
{
    std::scoped_lock lock(m_InventoryRequestMutex);
    if (!m_InventoryWorker.joinable() && !m_Stopped.load(std::memory_order_acquire))
        m_InventoryWorker = std::thread([this] { RunInventoryWorker(); });
}

void CModBrowserService::RunPageWorker()
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
        ModRequestOptions options;
        options.isCancelled = [this, generation = request.generation]
        { return m_Stopped.load(std::memory_order_acquire) || m_PageGeneration.load(std::memory_order_acquire) != generation; };
        if (options.isCancelled())
            continue;
        auto snapshot = std::make_shared<ModBrowserPageSnapshot>();
        snapshot->generation = request.generation;
        snapshot->search = request.search;
        snapshot->sort = request.sort;
        snapshot->requestedPage = request.page;
        auto page = std::make_shared<ModBrowserPage>();
        std::vector<const ModBrowserEntry*> entries;
        bool success = true;
        if (request.source != ModSource::Thunderstore)
            success = LoadCatalog(ModSource::ModWorkshop, request.forceRefresh, entries, snapshot->error, options);
        if (success && request.source != ModSource::ModWorkshop)
            success = LoadCatalog(ModSource::Thunderstore, request.forceRefresh, entries, snapshot->error, options);
        if (options.isCancelled())
            continue;
        if (!success)
        {
            snapshot->state = ModBrowserLoadState::Failed;
            snapshot->page = page;
            PublishPage(std::move(snapshot));
            continue;
        }
        const std::string search = CModBrowserData::Lower(request.search);
        struct RankedEntry
        {
            size_t index;
            std::string name;
            int relevance;
            uint64_t random;
        };
        std::vector<RankedEntry> ranked;
        ranked.reserve(entries.size());
        const auto randomEpoch = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now().time_since_epoch()).count() / 5;
        std::mt19937_64 random(static_cast<uint64_t>(randomEpoch));
        for (size_t i = 0; i < entries.size(); ++i)
        {
            const auto& entry = *entries[i];
            const std::string_view packageId = std::string_view(entry.id).substr(entry.id.find(':') + 1);
            const auto tracked = std::ranges::find_if(request.inventory->packages, [&](const auto& package)
            { return package.source == entry.source && package.packageId == packageId; });
            const bool installed = tracked != request.inventory->packages.end();
            if ((request.filter == 0 && installed) || (request.filter == 1 && !installed) ||
                (request.filter == 2 && (!installed || tracked->updateState != ModUpdateState::UpdateAvailable)))
                continue;
            std::string name = CModBrowserData::Lower(entry.name);
            const bool titleMatch = name.find(search) != std::string::npos;
            const bool authorMatch = !search.empty() && CModBrowserData::Lower(entry.author).find(search) != std::string::npos;
            if (!titleMatch && !authorMatch && CModBrowserData::Lower(entry.shortDescription).find(search) == std::string::npos)
                continue;
            const int relevance = search.empty() ? 0 : name == search ? 4 : name.starts_with(search) ? 3 : titleMatch ? 2 : authorMatch ? 1 : 0;
            ranked.push_back({i, std::move(name), relevance, request.sort == "random" ? random() : 0});
        }
        std::ranges::sort(ranked, [&](const RankedEntry& left, const RankedEntry& right)
        {
            const auto& a = *entries[left.index];
            const auto& b = *entries[right.index];
            const auto& sort = request.sort;
            if (sort == "name" && left.name != right.name)
                return left.name < right.name;
            if (sort == "downloads" && a.downloads != b.downloads)
                return a.downloads > b.downloads;
            if (sort == "likes" && a.likes != b.likes)
                return a.likes > b.likes;
            if (sort == "views" && a.views != b.views)
                return a.views > b.views;
            if (sort == "score" && a.score != b.score)
                return a.score > b.score;
            if (sort == "weekly_score" && a.weeklyScore != b.weeklyScore)
                return a.weeklyScore > b.weeklyScore;
            if (sort == "daily_score" && a.dailyScore != b.dailyScore)
                return a.dailyScore > b.dailyScore;
            if (sort == "best_match" && left.relevance != right.relevance)
                return left.relevance > right.relevance;
            if (sort == "random" && left.random != right.random)
                return left.random < right.random;
            if (sort == "published_at" && a.publishedAt != b.publishedAt)
                return a.publishedAt > b.publishedAt;
            if ((sort == "updated" || sort == "bumped_at") && a.updatedAt != b.updatedAt)
                return a.updatedAt > b.updatedAt;
            if (left.name != right.name)
                return left.name < right.name;
            return a.id < b.id;
        });
        page->metadata.total = static_cast<int>(ranked.size());
        page->metadata.perPage = PAGE_BATCH_SIZE;
        page->metadata.lastPage = std::max(1, (page->metadata.total + PAGE_BATCH_SIZE - 1) / PAGE_BATCH_SIZE);
        page->metadata.currentPage = std::min(request.page, page->metadata.lastPage);
        const size_t begin = static_cast<size_t>(page->metadata.currentPage - 1) * PAGE_BATCH_SIZE;
        const size_t end = std::min(begin + PAGE_BATCH_SIZE, ranked.size());
        page->entries.reserve(end - begin);
        for (size_t i = begin; i < end; ++i)
        {
            page->entries.push_back(*entries[ranked[i].index]);
            ApplyInventory(page->entries.back(), *request.inventory);
        }
        if (options.isCancelled())
            continue;
        CachePage(request.generation, request.cacheKey, page);
        snapshot->page = std::move(page);
        snapshot->state = ModBrowserLoadState::Ready;
        PublishPage(std::move(snapshot));
    }
}

void CModBrowserService::RunDetailsWorker()
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

        ModRequestOptions options;
        options.isCancelled = [this, generation = request.generation]
        { return m_Stopped.load(std::memory_order_acquire) || m_DetailsGeneration.load(std::memory_order_acquire) != generation; };

        ModBrowserDetails details;
        ModRequestError error;
        if (!LoadDetails(request.id, details, error, options))
        {
            if (options.isCancelled())
                continue;
            auto snapshot = std::make_shared<ModBrowserDetailsSnapshot>();
            snapshot->generation = request.generation;
            snapshot->id = request.id;
            snapshot->state = error.code == ModRequestErrorCode::Cancelled ? ModBrowserLoadState::Cancelled : ModBrowserLoadState::Failed;
            snapshot->error = std::move(error);
            PublishDetails(std::move(snapshot));
            continue;
        }
        if (options.isCancelled())
            continue;

        auto immutableDetails = std::make_shared<const ModBrowserDetails>(std::move(details));
        CacheDetails(request.generation, request.id, immutableDetails);
        auto snapshot = std::make_shared<ModBrowserDetailsSnapshot>();
        snapshot->generation = request.generation;
        snapshot->id = request.id;
        snapshot->state = ModBrowserLoadState::Ready;
        snapshot->details = std::move(immutableDetails);
        PublishDetails(std::move(snapshot));
    }
}

void CModBrowserService::RunInventoryWorker()
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

        CModInventory::Get().RefreshLocal();
        if (m_InventoryGeneration.load(std::memory_order_acquire) != request.generation)
            continue;
        NotifyInventory(request.generation,
                        request.checkRemote ? ModInventoryUpdateStage::LocalCompleteRemotePending : ModInventoryUpdateStage::LocalComplete);
        if (!request.checkRemote)
            continue;

        ModRequestOptions options;
        options.isCancelled = [this, generation = request.generation]
        { return m_Stopped.load(std::memory_order_acquire) || m_InventoryGeneration.load(std::memory_order_acquire) != generation; };
        CModInventory::Get().CheckForUpdates(m_Client, options);
        if (!options.isCancelled())
            NotifyInventory(request.generation, ModInventoryUpdateStage::RemoteComplete);
    }
}

CModBrowserService::CModBrowserService()
{
    auto initialPage = std::make_shared<ModBrowserPageSnapshot>();
    initialPage->page = std::make_shared<ModBrowserPage>();
    m_PageSnapshot = std::move(initialPage);

    auto initialDetails = std::make_shared<ModBrowserDetailsSnapshot>();
    m_DetailsSnapshot = std::move(initialDetails);
}

uint64_t CModBrowserService::RequestPage(std::string search, std::string sort, int page, int filter, ModSource source, bool forceRefresh)
{
    if (m_Stopped.load(std::memory_order_acquire))
        return 0;
    if (search.size() > 150)
        search.resize(150);
    page = std::max(1, page);
    const uint64_t generation = m_PageGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
    const auto inventory = GetInventorySnapshot();
    const auto epoch = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now().time_since_epoch()).count() / 5;
    const std::string cacheKey =
        std::format("{}:{}:{}:{}:{}:{}:{}:{}", static_cast<int>(source), filter, page, inventory->generation, epoch, search.size(), search, sort);
    auto snapshot = std::make_shared<ModBrowserPageSnapshot>();
    snapshot->generation = generation;
    snapshot->search = search;
    snapshot->sort = sort;
    snapshot->requestedPage = page;
    snapshot->page = std::make_shared<ModBrowserPage>();
    static constexpr std::array<std::string_view, 12> sorts = {"updated", "bumped_at",    "published_at", "downloads",  "likes",  "views",
                                                               "score",   "weekly_score", "daily_score",  "best_match", "random", "name"};
    if (filter < 0 || filter > 2 || source != ModSource::Unknown && source != ModSource::ModWorkshop && source != ModSource::Thunderstore ||
        std::ranges::find(sorts, sort) == sorts.end())
    {
        snapshot->state = ModBrowserLoadState::Failed;
        snapshot->error = {ModRequestErrorCode::InvalidArgument, 0, 0, {}, "Invalid mod browser filter, source or sort"};
        PublishPage(std::move(snapshot));
        return generation;
    }
    {
        std::scoped_lock lock(m_PageMutex);
        if (forceRefresh)
        {
            m_PageCache.clear();
            m_PageCacheOrder.clear();
        }
        else if (const auto found = m_PageCache.find(cacheKey); found != m_PageCache.end())
        {
            snapshot->page = found->second;
            snapshot->fromCache = true;
        }
    }
    if (snapshot->fromCache)
    {
        snapshot->state = ModBrowserLoadState::Ready;
        PublishPage(std::move(snapshot));
        return generation;
    }
    snapshot->state = ModBrowserLoadState::Loading;
    PublishPage(std::move(snapshot));
    EnsurePageWorker();
    {
        std::scoped_lock lock(m_PageRequestMutex);
        m_PendingPageRequest = PageRequest{generation, cacheKey, std::move(search), std::move(sort), page, filter, source, forceRefresh, inventory};
    }
    m_PageRequestChanged.notify_one();
    return generation;
}

uint64_t CModBrowserService::RequestDetails(std::string id, bool forceRefresh)
{
    if (m_Stopped.load(std::memory_order_acquire))
        return 0;
    const uint64_t generation = m_DetailsGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (forceRefresh)
    {
        std::scoped_lock lock(m_DetailsMutex);
        m_DetailsCache.erase(id);
        std::erase(m_DetailsCacheOrder, id);
    }
    if (!forceRefresh)
    {
        std::shared_ptr<const ModBrowserDetails> cached;
        {
            std::scoped_lock lock(m_DetailsMutex);
            const auto found = m_DetailsCache.find(id);
            if (found != m_DetailsCache.end() && std::chrono::steady_clock::now() - found->second.fetchedAt < std::chrono::minutes(5))
                cached = found->second.details;
        }
        if (cached)
        {
            auto snapshot = std::make_shared<ModBrowserDetailsSnapshot>();
            snapshot->generation = generation;
            snapshot->id = id;
            snapshot->state = ModBrowserLoadState::Ready;
            auto currentDetails = std::make_shared<ModBrowserDetails>(*cached);
            ApplyInventory(*currentDetails, *GetInventorySnapshot());
            snapshot->details = std::move(currentDetails);
            snapshot->fromCache = true;
            PublishDetails(std::move(snapshot));
            return generation;
        }
    }

    auto loading = std::make_shared<ModBrowserDetailsSnapshot>();
    loading->generation = generation;
    loading->id = id;
    loading->state = ModBrowserLoadState::Loading;
    PublishDetails(std::move(loading));

    EnsureDetailsWorker();
    {
        std::scoped_lock lock(m_DetailsRequestMutex);
        m_PendingDetailsRequest = DetailsRequest{generation, std::move(id)};
    }
    m_DetailsRequestChanged.notify_one();
    return generation;
}

uint64_t CModBrowserService::RefreshTrackedMods(bool checkRemote)
{
    if (m_Stopped.load(std::memory_order_acquire))
        return 0;
    {
        std::scoped_lock lock(m_PageMutex, m_DetailsMutex);
        m_PageCache.clear();
        m_PageCacheOrder.clear();
        m_DetailsCache.clear();
        m_DetailsCacheOrder.clear();
    }
    const uint64_t generation = m_InventoryGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
    EnsureInventoryWorker();
    {
        std::scoped_lock lock(m_InventoryRequestMutex);
        m_PendingInventoryRequest = InventoryRequest{generation, checkRemote};
    }
    m_InventoryRequestChanged.notify_one();
    return generation;
}

void CModBrowserService::CancelPageRequest()
{
    const uint64_t generation = m_PageGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
    std::shared_ptr<const ModBrowserPageSnapshot> current = GetPageSnapshot();
    if (!current || current->state != ModBrowserLoadState::Loading)
        return;
    auto cancelled = std::make_shared<ModBrowserPageSnapshot>(*current);
    cancelled->generation = generation;
    cancelled->state = ModBrowserLoadState::Cancelled;
    cancelled->error = {ModRequestErrorCode::Cancelled, 0, 0, {}, "Mod browser page request cancelled"};
    PublishPage(std::move(cancelled));
}

void CModBrowserService::CancelDetailsRequest()
{
    const uint64_t generation = m_DetailsGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
    std::shared_ptr<const ModBrowserDetailsSnapshot> current = GetDetailsSnapshot();
    if (!current || current->state != ModBrowserLoadState::Loading)
        return;
    auto cancelled = std::make_shared<ModBrowserDetailsSnapshot>(*current);
    cancelled->generation = generation;
    cancelled->state = ModBrowserLoadState::Cancelled;
    cancelled->error = {ModRequestErrorCode::Cancelled, 0, 0, {}, "Mod browser details request cancelled"};
    PublishDetails(std::move(cancelled));
}

std::shared_ptr<const ModBrowserPageSnapshot> CModBrowserService::GetPageSnapshot() const
{
    std::scoped_lock lock(m_PageMutex);
    return m_PageSnapshot;
}

std::shared_ptr<const ModBrowserDetailsSnapshot> CModBrowserService::GetDetailsSnapshot() const
{
    std::scoped_lock lock(m_DetailsMutex);
    return m_DetailsSnapshot;
}

std::shared_ptr<const ModInventorySnapshot> CModBrowserService::GetInventorySnapshot() const
{
    return CModInventory::Get().GetSnapshot();
}

void CModBrowserService::SetPageChangedCallback(PageChangedCallback callback)
{
    std::scoped_lock lock(m_CallbackMutex);
    m_PageChanged = std::move(callback);
}

void CModBrowserService::SetDetailsChangedCallback(DetailsChangedCallback callback)
{
    std::scoped_lock lock(m_CallbackMutex);
    m_DetailsChanged = std::move(callback);
}

void CModBrowserService::SetUpdatesChangedCallback(UpdatesChangedCallback callback)
{
    std::scoped_lock lock(m_CallbackMutex);
    m_UpdatesChanged = std::move(callback);
}

void CModBrowserService::ClearCallbacks()
{
    std::scoped_lock lock(m_CallbackMutex);
    m_PageChanged = {};
    m_DetailsChanged = {};
    m_UpdatesChanged = {};
}

void CModBrowserService::Shutdown()
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
}
