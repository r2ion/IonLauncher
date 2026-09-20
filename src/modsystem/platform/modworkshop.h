#pragma once

#include "modsystem/platform/modhttp.h"
#include "modsystem/platform/modplatform.h"

#include <array>
#include <compat/unzip.h>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ModWorkshopThumbnail
{
    uint64_t id = 0;
    std::string file;
    std::string updatedAt;
    bool hasThumbnail = false;
};

struct ModWorkshopCatalogEntry
{
    uint64_t id = 0;
    std::string name;
    std::string shortDescription;
    std::string description;
    std::string author;
    std::string version;
    std::string visibility;
    std::string publishedAt;
    std::string bumpedAt;
    std::string updatedAt;
    double score = 0, weeklyScore = 0, dailyScore = 0;
    uint64_t downloads = 0;
    uint64_t likes = 0;
    uint64_t views = 0;
    std::optional<uint64_t> selectedFileId;
    std::optional<ModWorkshopThumbnail> thumbnail;
    bool hasDownload = false;
    bool approved = false;
    bool suspended = false;
    bool disableModManagers = false;
};

struct ModWorkshopSelectedFile
{
    uint64_t id = 0;
    std::string name;
    std::string file;
    std::string type;
    std::string version;
    std::string updatedAt;
    std::string downloadUrl;
    uint64_t size = 0;
};

struct ModWorkshopDependency
{
    std::string name;
    std::string url;
    std::optional<uint64_t> modId;
    bool offsite = false;
    bool optional = false;
};

struct ModWorkshopDetails : ModWorkshopCatalogEntry
{
    std::optional<ModWorkshopSelectedFile> selectedFile;
    std::vector<ModWorkshopDependency> dependencies;
};

struct ModWorkshopPage
{
    std::vector<ModWorkshopCatalogEntry> entries;
    ModPageMetadata metadata;
};

struct ModWorkshopListQuery
{
    uint64_t gameId = 0;
    std::string search;
    std::string sort = "bumped_at";
    int page = 1;
    int limit = 9;
    std::vector<uint64_t> ids;
};

class CModWorkshopClient final
{
  public:
    explicit CModWorkshopClient(std::string apiBaseUrl = "https://api.modworkshop.net");

    bool ResolveGameId(std::string_view gameSlug, uint64_t& gameId, ModRequestError& error, const ModRequestOptions& options = {}) const;
    bool ListMods(const ModWorkshopListQuery& query, ModWorkshopPage& page, ModRequestError& error, const ModRequestOptions& options = {}) const;
    bool GetMod(uint64_t modId, ModWorkshopDetails& details, ModRequestError& error, const ModRequestOptions& options = {}) const;
    static std::string BuildThumbnailUrl(const ModWorkshopThumbnail& thumbnail);
    static std::string BuildModPageUrl(uint64_t modId)
    {
        return modId != 0 ? std::string(MOD_PAGE_BASE) + std::to_string(modId) : std::string();
    }
    static std::optional<fs::path> FindRootDir(unzFile file, const unz_global_info64& archiveInfo);
    static std::optional<uint64_t> TryParseInstallId(std::string_view uri);

  private:
    class CJson;

    static std::string UrlEncode(std::string_view value);
    static bool IsSafeStorageFile(std::string_view file);

    static constexpr std::string_view STORAGE_IMAGE_BASE = "https://storage.modworkshop.net/mods/images/";
    static constexpr std::string_view MOD_PAGE_BASE = "https://modworkshop.net/mod/";
    static constexpr std::string_view INSTALL_URI_PREFIX = "r2ns://mws/install/";
    static constexpr std::array<std::string_view, 10> VALID_SORTS = {
        "bumped_at", "published_at", "likes", "downloads", "views", "score", "weekly_score", "daily_score", "random", "best_match",
    };
    inline static constexpr char URL_HEX[] = "0123456789ABCDEF";

    bool GetText(std::string_view url, std::string& text, ModRequestError& error, const ModRequestOptions& options = {}) const;

    std::string m_ApiBaseUrl;
};
