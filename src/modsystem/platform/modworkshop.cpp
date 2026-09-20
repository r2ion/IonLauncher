#include "modsystem/platform/modworkshop.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <utility>

class CModWorkshopClient::CJson final
{
  public:
    static const rapidjson::Value* FindMember(const rapidjson::Value& object, const char* name);
    static std::string ReadString(const rapidjson::Value& object, const char* name);
    static bool ReadBool(const rapidjson::Value& object, const char* name, bool fallback = false);
    static std::optional<uint64_t> ReadUint64(const rapidjson::Value& object, const char* name);
    static int ReadInt(const rapidjson::Value& object, const char* name, int fallback);
    static bool ParseCatalogEntry(const rapidjson::Value& value, ModWorkshopCatalogEntry& entry, std::string& parseError);
    static bool ParseSelectedFile(const rapidjson::Value& value, ModWorkshopSelectedFile& selectedFile, std::string& parseError);
    static bool Parse(std::string_view text, rapidjson::Document& document, ModRequestError& error);
};

std::string CModWorkshopClient::UrlEncode(std::string_view value)
{
    std::string encoded;
    encoded.reserve(value.size());
    for (const unsigned char character : value)
    {
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') ||
            character == '-' || character == '_' || character == '.' || character == '~')
        {
            encoded.push_back(static_cast<char>(character));
        }
        else
        {
            encoded.push_back('%');
            encoded.push_back(URL_HEX[character >> 4]);
            encoded.push_back(URL_HEX[character & 0xF]);
        }
    }
    return encoded;
}

bool CModWorkshopClient::IsSafeStorageFile(std::string_view file)
{
    if (file.empty() || file.size() > 255)
        return false;
    return std::ranges::all_of(file, [](unsigned char character)
    { return std::isalnum(character) || character == '.' || character == '_' || character == '-'; });
}

const rapidjson::Value* CModWorkshopClient::CJson::FindMember(const rapidjson::Value& object, const char* name)
{
    if (!object.IsObject())
        return nullptr;
    const auto member = object.FindMember(name);
    return member != object.MemberEnd() ? &member->value : nullptr;
}

std::string CModWorkshopClient::CJson::ReadString(const rapidjson::Value& object, const char* name)
{
    const rapidjson::Value* value = FindMember(object, name);
    if (!value || !value->IsString())
        return {};
    return std::string(value->GetString(), value->GetStringLength());
}

bool CModWorkshopClient::CJson::ReadBool(const rapidjson::Value& object, const char* name, bool fallback)
{
    const rapidjson::Value* value = FindMember(object, name);
    return value && value->IsBool() ? value->GetBool() : fallback;
}

std::optional<uint64_t> CModWorkshopClient::CJson::ReadUint64(const rapidjson::Value& object, const char* name)
{
    const rapidjson::Value* value = FindMember(object, name);
    if (!value || value->IsNull())
        return std::nullopt;
    if (value->IsUint64())
        return value->GetUint64();
    if (value->IsInt64() && value->GetInt64() >= 0)
        return static_cast<uint64_t>(value->GetInt64());
    if (!value->IsString())
        return std::nullopt;

    uint64_t result = 0;
    const std::string_view text(value->GetString(), value->GetStringLength());
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (error != std::errc() || end != text.data() + text.size())
        return std::nullopt;

    return result;
}

int CModWorkshopClient::CJson::ReadInt(const rapidjson::Value& object, const char* name, int fallback)
{
    const rapidjson::Value* value = FindMember(object, name);
    return value && value->IsInt() ? value->GetInt() : fallback;
}

bool CModWorkshopClient::CJson::ParseCatalogEntry(const rapidjson::Value& value, ModWorkshopCatalogEntry& entry, std::string& parseError)
{
    if (!value.IsObject())
    {
        parseError = "catalog entry is not an object";
        return false;
    }

    const std::optional<uint64_t> id = ReadUint64(value, "id");
    entry.name = ReadString(value, "name");
    if (!id || *id == 0 || entry.name.empty())
    {
        parseError = "catalog entry is missing a valid id or name";
        return false;
    }

    entry = {};
    entry.id = *id;
    entry.name = ReadString(value, "name");
    entry.shortDescription = ReadString(value, "short_desc");
    entry.description = ReadString(value, "desc");
    entry.version = ReadString(value, "version");
    entry.visibility = ReadString(value, "visibility");
    entry.publishedAt = ReadString(value, "published_at");
    entry.bumpedAt = ReadString(value, "bumped_at");
    entry.updatedAt = ReadString(value, "updated_at");
    entry.downloads = ReadUint64(value, "downloads").value_or(0);
    entry.likes = ReadUint64(value, "likes").value_or(0);
    entry.views = ReadUint64(value, "views").value_or(0);
    if (const auto* score = FindMember(value, "score"); score && score->IsNumber())
        entry.score = score->GetDouble();
    if (const auto* score = FindMember(value, "weekly_score"); score && score->IsNumber())
        entry.weeklyScore = score->GetDouble();
    if (const auto* score = FindMember(value, "daily_score"); score && score->IsNumber())
        entry.dailyScore = score->GetDouble();
    entry.selectedFileId = ReadUint64(value, "download_id");
    entry.hasDownload = ReadBool(value, "has_download");
    entry.approved = ReadBool(value, "approved");
    entry.suspended = ReadBool(value, "suspended");
    entry.disableModManagers = ReadBool(value, "disable_mod_managers");

    if (const rapidjson::Value* user = FindMember(value, "user"); user && user->IsObject())
        entry.author = ReadString(*user, "name");

    if (const rapidjson::Value* thumbnail = FindMember(value, "thumbnail"); thumbnail && thumbnail->IsObject())
    {
        const std::optional<uint64_t> thumbnailId = ReadUint64(*thumbnail, "id");
        const std::string file = ReadString(*thumbnail, "file");
        if (thumbnailId && *thumbnailId != 0 && IsSafeStorageFile(file))
        {
            entry.thumbnail = ModWorkshopThumbnail{.id = *thumbnailId,
                                                   .file = file,
                                                   .updatedAt = ReadString(*thumbnail, "updated_at"),
                                                   .hasThumbnail = ReadBool(*thumbnail, "has_thumb")};
        }
    }
    return true;
}

bool CModWorkshopClient::CJson::ParseSelectedFile(const rapidjson::Value& value, ModWorkshopSelectedFile& selectedFile, std::string& parseError)
{
    const std::optional<uint64_t> id = ReadUint64(value, "id");
    const std::string downloadUrl = ReadString(value, "download_url");
    if (!id || *id == 0 || downloadUrl.empty() || !downloadUrl.starts_with("https://"))
    {
        parseError = "selected ModWorkshop file is missing a valid id or HTTPS download URL";
        return false;
    }

    selectedFile = {};
    selectedFile.id = *id;
    selectedFile.name = ReadString(value, "name");
    selectedFile.file = ReadString(value, "file");
    selectedFile.type = ReadString(value, "type");
    selectedFile.version = ReadString(value, "version");
    selectedFile.updatedAt = ReadString(value, "updated_at");
    selectedFile.downloadUrl = downloadUrl;
    selectedFile.size = ReadUint64(value, "size").value_or(0);
    return true;
}

bool CModWorkshopClient::CJson::Parse(std::string_view text, rapidjson::Document& document, ModRequestError& error)
{
    document.Parse(text.data(), text.size());
    if (!document.HasParseError())
        return true;

    error.code = ModRequestErrorCode::InvalidResponse;
    error.message =
        std::format("Invalid ModWorkshop JSON: {} at byte {}", rapidjson::GetParseError_En(document.GetParseError()), document.GetErrorOffset());
    return false;
}

CModWorkshopClient::CModWorkshopClient(std::string apiBaseUrl) : m_ApiBaseUrl(std::move(apiBaseUrl))
{
    while (!m_ApiBaseUrl.empty() && m_ApiBaseUrl.back() == '/')
        m_ApiBaseUrl.pop_back();
}

bool CModWorkshopClient::GetText(std::string_view url, std::string& text, ModRequestError& error, const ModRequestOptions& options) const
{
    std::vector<uint8_t> bytes;
    if (!CModHttpClient::GetBytes(url, bytes, error, options))
        return false;
    text.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

bool CModWorkshopClient::ResolveGameId(std::string_view gameSlug, uint64_t& gameId, ModRequestError& error, const ModRequestOptions& options) const
{
    gameId = 0;
    if (gameSlug.empty())
    {
        error = {ModRequestErrorCode::InvalidArgument, 0, 0, {}, "Game slug cannot be empty"};
        return false;
    }

    const std::string url = std::format("{}/games/{}", m_ApiBaseUrl, UrlEncode(gameSlug));
    std::string text;
    if (!GetText(url, text, error, options))
        return false;

    rapidjson::Document document;
    if (!CJson::Parse(text, document, error) || !document.IsObject())
    {
        if (error.code == ModRequestErrorCode::None)
        {
            error.code = ModRequestErrorCode::InvalidResponse;
            error.message = "ModWorkshop game response is not an object";
        }
        return false;
    }

    if (CJson::ReadString(document, "short_name") != gameSlug)
    {
        error.code = ModRequestErrorCode::InvalidResponse;
        error.message = std::format("ModWorkshop returned the wrong game for '{}'", gameSlug);
        return false;
    }

    const std::optional<uint64_t> id = CJson::ReadUint64(document, "id");
    if (!id || *id == 0)
    {
        error.code = ModRequestErrorCode::InvalidResponse;
        error.message = "ModWorkshop game response is missing a valid id";
        return false;
    }

    gameId = *id;
    return true;
}

bool CModWorkshopClient::ListMods(const ModWorkshopListQuery& query, ModWorkshopPage& page, ModRequestError& error,
                                  const ModRequestOptions& options) const
{
    page = {};
    if (query.gameId == 0 || query.page < 1 || query.limit < 1 || query.limit > 50)
    {
        error = {ModRequestErrorCode::InvalidArgument, 0, 0, {}, "Invalid ModWorkshop list query"};
        return false;
    }
    if (std::ranges::find(VALID_SORTS, query.sort) == VALID_SORTS.end())
    {
        error = {ModRequestErrorCode::InvalidArgument, 0, 0, {}, "Unsupported ModWorkshop sort order"};
        return false;
    }

    std::string url =
        std::format("{}/games/{}/mods?limit={}&page={}&sort={}", m_ApiBaseUrl, query.gameId, query.limit, query.page, UrlEncode(query.sort));
    if (!query.search.empty())
        url += "&query=" + UrlEncode(query.search.substr(0, 150));
    for (const uint64_t id : query.ids)
    {
        if (id != 0)
            url += std::format("&ids[]={}", id);
    }

    std::string text;
    if (!GetText(url, text, error, options))
        return false;

    rapidjson::Document document;

    if (!CJson::Parse(text, document, error) || !document.IsObject())
    {
        if (error.code == ModRequestErrorCode::None)
        {
            error.code = ModRequestErrorCode::InvalidResponse;
            error.message = "ModWorkshop catalog response is not an object";
        }
        return false;
    }

    const rapidjson::Value* data = CJson::FindMember(document, "data");
    if (!data || !data->IsArray())
    {
        error.code = ModRequestErrorCode::InvalidResponse;
        error.message = "ModWorkshop catalog response is missing its data array";
        return false;
    }

    page.entries.reserve(data->Size());
    for (const rapidjson::Value& value : data->GetArray())
    {
        ModWorkshopCatalogEntry entry;
        std::string parseError;
        if (!CJson::ParseCatalogEntry(value, entry, parseError))
        {
            error.code = ModRequestErrorCode::InvalidResponse;
            error.message = std::move(parseError);
            page = {};
            return false;
        }
        page.entries.push_back(std::move(entry));
    }

    if (const rapidjson::Value* metadata = CJson::FindMember(document, "meta"); metadata && metadata->IsObject())
    {
        page.metadata.currentPage = CJson::ReadInt(*metadata, "current_page", query.page);
        page.metadata.lastPage = CJson::ReadInt(*metadata, "last_page", page.metadata.currentPage);
        page.metadata.perPage = CJson::ReadInt(*metadata, "per_page", query.limit);
        page.metadata.total = CJson::ReadInt(*metadata, "total", static_cast<int>(page.entries.size()));
    }
    else
    {
        page.metadata.currentPage = query.page;
        page.metadata.lastPage = query.page;
        page.metadata.perPage = query.limit;
        page.metadata.total = static_cast<int>(page.entries.size());
    }
    return true;
}

bool CModWorkshopClient::GetMod(uint64_t modId, ModWorkshopDetails& details, ModRequestError& error, const ModRequestOptions& options) const
{
    details = {};
    if (modId == 0)
    {
        error = {ModRequestErrorCode::InvalidArgument, 0, 0, {}, "ModWorkshop mod id cannot be zero"};
        return false;
    }

    std::string text;
    if (!GetText(std::format("{}/mods/{}", m_ApiBaseUrl, modId), text, error, options))
        return false;

    rapidjson::Document document;
    if (!CJson::Parse(text, document, error) || !document.IsObject())
    {
        if (error.code == ModRequestErrorCode::None)
        {
            error.code = ModRequestErrorCode::InvalidResponse;
            error.message = "ModWorkshop details response is not an object";
        }
        return false;
    }

    ModWorkshopCatalogEntry catalogEntry;
    std::string parseError;
    if (!CJson::ParseCatalogEntry(document, catalogEntry, parseError))
    {
        error.code = ModRequestErrorCode::InvalidResponse;
        error.message = std::move(parseError);
        return false;
    }
    if (catalogEntry.id != modId)
    {
        error = {ModRequestErrorCode::InvalidResponse, 0, 0, {}, "ModWorkshop returned the wrong mod identity"};
        return false;
    }
    static_cast<ModWorkshopCatalogEntry&>(details) = std::move(catalogEntry);

    if (const rapidjson::Value* download = CJson::FindMember(document, "download"); download && !download->IsNull())
    {
        if (!download->IsObject())
        {
            error.code = ModRequestErrorCode::InvalidResponse;
            error.message = "Selected ModWorkshop download is not an object";
            return false;
        }
        ModWorkshopSelectedFile selectedFile;

        if (!CJson::ParseSelectedFile(*download, selectedFile, parseError))
        {
            error.code = ModRequestErrorCode::InvalidResponse;
            error.message = std::move(parseError);
            return false;
        }
        details.selectedFileId = selectedFile.id;
        details.selectedFile = std::move(selectedFile);
    }

    if (const rapidjson::Value* dependencies = CJson::FindMember(document, "dependencies"); dependencies && dependencies->IsArray())
    {
        details.dependencies.reserve(dependencies->Size());
        for (const rapidjson::Value& value : dependencies->GetArray())
        {
            if (!value.IsObject())
                continue;
            ModWorkshopDependency dependency;
            dependency.name = CJson::ReadString(value, "name");
            dependency.url = CJson::ReadString(value, "url");
            dependency.modId = CJson::ReadUint64(value, "mod_id");
            dependency.offsite = CJson::ReadBool(value, "offsite");
            dependency.optional = CJson::ReadBool(value, "optional");
            if (!dependency.name.empty() || !dependency.url.empty() || dependency.modId)
                details.dependencies.push_back(std::move(dependency));
        }
    }

    return true;
}

std::string CModWorkshopClient::BuildThumbnailUrl(const ModWorkshopThumbnail& thumbnail)
{
    if (!IsSafeStorageFile(thumbnail.file))
        return {};
    return std::string(STORAGE_IMAGE_BASE) + (thumbnail.hasThumbnail ? "thumbnail_" : "") + thumbnail.file;
}

std::optional<fs::path> CModWorkshopClient::FindRootDir(unzFile file, const unz_global_info64& archiveInfo)
{
    if (!file || unzGoToFirstFile(file) != UNZ_OK)
        return std::nullopt;

    for (uint64_t index = 0; index < archiveInfo.number_entry; ++index)
    {
        unz_file_info64 fileInfo{};
        if (unzGetCurrentFileInfo64(file, &fileInfo, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK)
            return std::nullopt;
        std::vector<char> filename(static_cast<size_t>(fileInfo.size_filename) + 1, '\0');
        if (unzGetCurrentFileInfo64(file, &fileInfo, filename.data(), static_cast<unsigned long>(filename.size()), nullptr, 0, nullptr, 0) != UNZ_OK)
        {
            return std::nullopt;
        }

        const fs::path filePath = fs::path(filename.data()).lexically_normal();
        if (filePath.has_filename() && filePath.filename() == "mod.json")
            return filePath.has_parent_path() ? filePath.parent_path() / "" : fs::path();
        if (index + 1 < archiveInfo.number_entry && unzGoToNextFile(file) != UNZ_OK)
            return std::nullopt;
    }
    return std::nullopt;
}

std::optional<uint64_t> CModWorkshopClient::TryParseInstallId(std::string_view uri)
{
    if (!uri.starts_with(INSTALL_URI_PREFIX))
        return std::nullopt;
    uri.remove_prefix(INSTALL_URI_PREFIX.size());
    if (uri.empty())
        return std::nullopt;

    uint64_t id = 0;
    const auto [end, error] = std::from_chars(uri.data(), uri.data() + uri.size(), id);

    if (error != std::errc() || end != uri.data() + uri.size() || id == 0)
        return std::nullopt;

    return id;
}
