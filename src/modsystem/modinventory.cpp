#include "modsystem/modinventory.h"
#include "config/profile.h"
#include "modsystem/platform/thunderstore.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <rapidjson/document.h>
#include <utility>

std::optional<uint64_t> CModInventory::ParseId(std::string_view text)
{
    uint64_t id = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), id);
    if (text.empty() || error != std::errc() || end != text.data() + text.size() || id == 0)
        return std::nullopt;
    return id;
}

void CModInventory::ReadContainedMod(const fs::path& modDirectory, std::vector<ModContainedMod>& mods)
{
    const fs::path manifestPath = modDirectory / "mod.json";
    std::error_code error;
    const uintmax_t size = fs::file_size(manifestPath, error);
    if (error || size == 0 || size > MAX_MANIFEST_BYTES)
        return;

    std::ifstream input(manifestPath, std::ios::binary);
    if (!input.is_open())
        return;
    std::string contents(static_cast<size_t>(size), '\0');
    input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!input)
        return;

    rapidjson::Document document;
    document.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(contents.data(), contents.size());
    if (document.HasParseError() || !document.IsObject())
        return;
    const auto name = document.FindMember("Name");
    if (name == document.MemberEnd() || !name->value.IsString() || name->value.GetStringLength() == 0)
        return;
    const auto version = document.FindMember("Version");
    mods.push_back({.name = std::string(name->value.GetString(), name->value.GetStringLength()),
                    .version = version != document.MemberEnd() && version->value.IsString()
                                   ? std::string(version->value.GetString(), version->value.GetStringLength())
                                   : std::string()});
}

std::vector<ModContainedMod> CModInventory::DiscoverContainedMods(const fs::path& packageRoot)
{
    std::vector<ModContainedMod> mods;
    ReadContainedMod(packageRoot, mods);

    std::error_code error;
    const fs::path modsDirectory = packageRoot / "mods";
    if (fs::is_directory(modsDirectory, error))
    {
        for (const fs::directory_entry& entry : fs::directory_iterator(modsDirectory, error))
        {
            if (error)
                break;
            if (entry.is_directory(error))
                ReadContainedMod(entry.path(), mods);
        }
    }
    return mods;
}

CModInventory::CModInventory() : m_Snapshot(std::make_shared<ModInventorySnapshot>())
{
}

void CModInventory::Publish(std::shared_ptr<ModInventorySnapshot> snapshot)
{
    std::scoped_lock lock(m_Mutex);
    snapshot->generation = ++m_Generation;
    m_Snapshot = std::move(snapshot);
}

bool CModInventory::PublishIfCurrent(const std::shared_ptr<const ModInventorySnapshot>& expected, std::shared_ptr<ModInventorySnapshot> snapshot)
{
    std::scoped_lock lock(m_Mutex);
    if (m_Snapshot != expected)
        return false;
    snapshot->generation = ++m_Generation;
    m_Snapshot = std::move(snapshot);
    return true;
}

void CModInventory::RefreshLocal()
{
    auto snapshot = std::make_shared<ModInventorySnapshot>();
    std::error_code error;
    const fs::path packagesRoot = GetPackageFolderPath();
    if (!fs::is_directory(packagesRoot, error))
    {
        if (error && error != std::errc::no_such_file_or_directory)
            snapshot->error = std::format("Failed reading packages directory: {}", error.message());
        Publish(std::move(snapshot));
        return;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(packagesRoot, error))
    {
        if (error)
        {
            snapshot->error = std::format("Failed enumerating packages: {}", error.message());
            break;
        }
        if (!entry.is_directory(error))
            continue;

        const fs::path packageRoot = entry.path();
        const std::optional<std::string> marker = CModPlatform::TryReadManagedId(packageRoot, ModSource::ModWorkshop);
        const auto thunderstoreMarker = CModPlatform::TryReadManagedId(packageRoot, ModSource::Thunderstore);
        if (!marker && thunderstoreMarker)
        {
            ModTrackedPackage package;
            package.source = ModSource::Thunderstore;
            package.packageRoot = packageRoot;
            package.packageId = *thunderstoreMarker;
            std::ranges::replace(package.packageId, '/', '-');
            std::string owner, name;
            if (!CThunderstoreClient::ParsePackageId(package.packageId, owner, name))
            {
                package.updateState = ModUpdateState::Error;
                package.error = "Invalid .ts_id marker";
            }
            else
            {
                const fs::path manifest = packageRoot / "manifest.json";
                package.updateState = ModUpdateState::LegacyUnknown;
                const auto size = fs::file_size(manifest, error);
                if (!error && size > 0 && size <= MAX_MANIFEST_BYTES)
                {
                    std::ifstream input(manifest, std::ios::binary);
                    std::string text(static_cast<size_t>(size), '\0');
                    input.read(text.data(), static_cast<std::streamsize>(text.size()));
                    rapidjson::Document document;
                    if (input)
                        document.Parse(text.data(), text.size());
                    if (!document.HasParseError() && document.IsObject() && document.HasMember("name") && document["name"].IsString() &&
                        document["name"].GetString() == name && document.HasMember("version_number") && document["version_number"].IsString() &&
                        !CThunderstoreClient::BuildDownloadUrl(package.packageId + "-" + document["version_number"].GetString()).empty())
                        package.installedVersion = document["version_number"].GetString();
                    else
                    {
                        package.updateState = ModUpdateState::Error;
                        package.error = "Invalid Thunderstore manifest identity or version";
                    }
                }
                else if (!error || error != std::errc::no_such_file_or_directory)
                {
                    package.updateState = ModUpdateState::Error;
                    package.error = "Thunderstore manifest is unreadable or exceeds its size limit";
                }
                error.clear();
            }
            package.containedMods = DiscoverContainedMods(packageRoot);
            snapshot->packages.push_back(std::move(package));
            continue;
        }
        if (!marker)
            continue;

        ModTrackedPackage package;
        package.packageRoot = packageRoot;
        const std::optional<uint64_t> id = ParseId(*marker);
        if (!id)
        {
            package.updateState = ModUpdateState::Error;
            package.error = "Invalid .mws_id marker";
            package.containedMods = DiscoverContainedMods(packageRoot);
            snapshot->packages.push_back(std::move(package));
            continue;
        }
        package.modId = *id;
        package.packageId = std::to_string(*id);

        const bool hasStateFile = fs::is_regular_file(packageRoot / MODWORKSHOP_STATE_FILE, error);
        error.clear();
        if (hasStateFile)
        {
            ModWorkshopPackageState installedState;
            std::string stateError;
            if (!CModPlatform::ReadWorkshopPackageState(packageRoot, installedState, stateError))
            {
                package.updateState = ModUpdateState::Error;
                package.error = std::move(stateError);
            }
            else if (installedState.modId != package.modId)
            {
                package.updateState = ModUpdateState::Error;
                package.error = ".mws_id and .mws_state.json disagree";
            }
            else
            {
                package.containedMods = installedState.containedMods;
                package.installedVersion = installedState.selectedFileVersion;
                package.installedState = std::move(installedState);
                package.updateState = ModUpdateState::LegacyUnknown;
            }
        }
        else
        {
            package.updateState = ModUpdateState::LegacyUnknown;
            package.containedMods = DiscoverContainedMods(packageRoot);
        }
        if (package.containedMods.empty())
            package.containedMods = DiscoverContainedMods(packageRoot);
        snapshot->packages.push_back(std::move(package));
    }

    std::ranges::sort(snapshot->packages, [](const auto& left, const auto& right)
    {
        if (left.source != right.source)
            return left.source < right.source;
        if (left.packageId != right.packageId)
            return left.packageId < right.packageId;
        return left.packageRoot.generic_string() < right.packageRoot.generic_string();
    });
    for (size_t i = 1; i < snapshot->packages.size(); ++i)
    {
        auto& previous = snapshot->packages[i - 1];
        auto& package = snapshot->packages[i];
        if (previous.source == package.source && previous.packageId == package.packageId)
        {
            previous.updateState = package.updateState = ModUpdateState::Error;
            previous.error = package.error = "Multiple installed roots claim the same provider package";
        }
    }
    Publish(std::move(snapshot));
}

void CModInventory::RestoreCancelled(const std::shared_ptr<const ModInventorySnapshot>& current,
                                     const std::shared_ptr<const ModInventorySnapshot>& expected)
{
    auto restored = std::make_shared<ModInventorySnapshot>(*current);
    restored->checking = false;
    PublishIfCurrent(expected, std::move(restored));
}

bool CModInventory::CheckForUpdates(CModWorkshopClient& client, const ModRequestOptions& options)
{
    const auto current = GetSnapshot();
    auto result = std::make_shared<ModInventorySnapshot>(*current);
    result->checking = true;
    result->error.clear();
    result->updateCount = 0;
    for (auto& package : result->packages)
        if (package.updateState != ModUpdateState::Error)
            package.updateState = ModUpdateState::Checking;
    auto checking = std::make_shared<ModInventorySnapshot>(*result);
    if (!PublishIfCurrent(current, checking))
        return false;
    bool success = true;
    for (auto& package : result->packages)
    {
        if (options.isCancelled && options.isCancelled())
        {
            RestoreCancelled(current, checking);
            return false;
        }
        if (package.updateState == ModUpdateState::Error)
            continue;
        ModRequestError error;
        bool fetched = false;
        bool supported = false;
        bool knownVersion = false;
        bool sameVersion = false;
        if (package.source == ModSource::ModWorkshop)
        {
            ModWorkshopDetails remote;
            fetched = client.GetMod(package.modId, remote, error, options);
            if (fetched)
            {
                package.remoteSelectedFileId = remote.selectedFileId;
                package.remoteVersion = remote.selectedFile ? remote.selectedFile->version : remote.version;
                package.remoteUpdatedAt = remote.updatedAt;
                package.remoteThumbnail = remote.thumbnail;
                package.remoteThumbnailUrl = remote.thumbnail ? CModWorkshopClient::BuildThumbnailUrl(*remote.thumbnail) : std::string();
                supported =
                    remote.approved && !remote.suspended && !remote.disableModManagers && remote.hasDownload && remote.selectedFileId.has_value();
                knownVersion = package.installedState.has_value();
                sameVersion = knownVersion && remote.selectedFileId && package.installedState->selectedFileId == *remote.selectedFileId;
            }
        }
        else if (package.source == ModSource::Thunderstore)
        {
            std::string owner, name;
            CThunderstoreClient::PackageDetails remote;
            fetched = CThunderstoreClient::ParsePackageId(package.packageId, owner, name) &&
                      CThunderstoreClient::FetchPackageDetails(owner, name, remote, options, &error);
            if (fetched)
            {
                package.remoteVersion = remote.m_Version;
                package.remoteUpdatedAt = remote.m_UpdatedAt;
                package.remoteThumbnailUrl = remote.m_IconUrl;
                supported = remote.m_Active && !remote.m_Deprecated;
                knownVersion = !package.installedVersion.empty();
                sameVersion = knownVersion && !CThunderstoreClient::IsNewerVersion(remote.m_Version, package.installedVersion);
            }
        }
        if (!fetched)
        {
            if (error.code == ModRequestErrorCode::Cancelled)
            {
                RestoreCancelled(current, checking);
                return false;
            }
            package.updateState = error.httpStatus == 404 ? ModUpdateState::MissingRemote : ModUpdateState::Error;
            package.error = error.message;
            result->error = error.message;
            success = false;
            continue;
        }
        package.error.clear();
        package.updateState = !supported      ? ModUpdateState::Unsupported
                              : !knownVersion ? ModUpdateState::LegacyUnknown
                              : sameVersion   ? ModUpdateState::Current
                                              : ModUpdateState::UpdateAvailable;
        if (package.updateState == ModUpdateState::UpdateAvailable)
            ++result->updateCount;
    }
    if (options.isCancelled && options.isCancelled())
    {
        RestoreCancelled(current, checking);
        return false;
    }
    result->checking = false;
    result->checkedAt = CModPlatform::CurrentTimestamp();
    return PublishIfCurrent(checking, std::move(result)) && success;
}

std::shared_ptr<const ModInventorySnapshot> CModInventory::GetSnapshot() const
{
    std::scoped_lock lock(m_Mutex);
    return m_Snapshot;
}

std::optional<ModTrackedPackage> CModInventory::FindPackage(uint64_t modId) const
{
    return modId ? FindPackage(ModSource::ModWorkshop, std::to_string(modId)) : std::nullopt;
}

std::optional<ModTrackedPackage> CModInventory::FindPackage(ModSource source, std::string_view packageId) const
{
    const auto snapshot = GetSnapshot();
    const ModTrackedPackage* found = nullptr;
    for (const auto& package : snapshot->packages)
    {
        if (package.source != source || package.packageId != packageId)
            continue;
        if (found)
            return std::nullopt;
        found = &package;
    }
    return found ? std::optional<ModTrackedPackage>(*found) : std::nullopt;
}
