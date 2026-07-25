#include "modsystem/modworkshop_inventory.h"

#include "modsystem/modmanager.h"

#include <rapidjson/document.h>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <format>
#include <unordered_map>
#include <unordered_set>

std::optional<uint64_t> CModWorkshopInventory::ParseId(std::string_view text)
{
	uint64_t id = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), id);
	if (text.empty() || error != std::errc() || end != text.data() + text.size() || id == 0)
		return std::nullopt;
	return id;
}

void CModWorkshopInventory::ReadContainedMod(const fs::path& modDirectory, std::vector<ModWorkshopContainedMod>& mods)
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

std::vector<ModWorkshopContainedMod> CModWorkshopInventory::DiscoverContainedMods(const fs::path& packageRoot)
{
	std::vector<ModWorkshopContainedMod> mods;
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

CModWorkshopInventory::CModWorkshopInventory() : m_Snapshot(std::make_shared<ModWorkshopInventorySnapshot>())
{
}

void CModWorkshopInventory::Publish(std::shared_ptr<ModWorkshopInventorySnapshot> snapshot)
{
	std::scoped_lock lock(m_Mutex);
	snapshot->generation = ++m_Generation;
	m_Snapshot = std::move(snapshot);
}

bool CModWorkshopInventory::PublishIfCurrent(const std::shared_ptr<const ModWorkshopInventorySnapshot>& expected,
                                             std::shared_ptr<ModWorkshopInventorySnapshot> snapshot)
{
	std::scoped_lock lock(m_Mutex);
	if (m_Snapshot != expected)
		return false;
	snapshot->generation = ++m_Generation;
	m_Snapshot = std::move(snapshot);
	return true;
}

void CModWorkshopInventory::RefreshLocal()
{
	auto snapshot = std::make_shared<ModWorkshopInventorySnapshot>();
	std::error_code error;
	const fs::path packagesRoot = GetPackageFolderPath();
	if (!fs::is_directory(packagesRoot, error))
	{
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
		if (!marker)
			continue;

		ModWorkshopTrackedPackage package;
		package.packageRoot = packageRoot;
		const std::optional<uint64_t> id = ParseId(*marker);
		if (!id)
		{
			package.updateState = ModWorkshopUpdateState::Error;
			package.error = "Invalid .mws_id marker";
			package.containedMods = DiscoverContainedMods(packageRoot);
			snapshot->packages.push_back(std::move(package));
			continue;
		}
		package.modId = *id;

		const bool hasStateFile = fs::is_regular_file(packageRoot / MODWORKSHOP_STATE_FILE, error);
		error.clear();
		if (hasStateFile)
		{
			ModWorkshopPackageState installedState;
			std::string stateError;
			if (!CModPlatform::ReadWorkshopPackageState(packageRoot, installedState, stateError))
			{
				package.updateState = ModWorkshopUpdateState::Error;
				package.error = std::move(stateError);
			}
			else if (installedState.modId != package.modId)
			{
				package.updateState = ModWorkshopUpdateState::Error;
				package.error = ".mws_id and .mws_state.json disagree";
			}
			else
			{
				package.containedMods = installedState.containedMods;
				package.installedState = std::move(installedState);
				package.updateState = ModWorkshopUpdateState::Checking;
			}
		}
		else
		{
			package.updateState = ModWorkshopUpdateState::LegacyUnknown;
			package.containedMods = DiscoverContainedMods(packageRoot);
		}
		if (package.containedMods.empty())
			package.containedMods = DiscoverContainedMods(packageRoot);
		snapshot->packages.push_back(std::move(package));
	}

	std::ranges::sort(snapshot->packages, [](const auto& left, const auto& right)
	{
		if (left.modId != right.modId)
			return left.modId < right.modId;
		return left.packageRoot.generic_string() < right.packageRoot.generic_string();
	});
	Publish(std::move(snapshot));
}

void CModWorkshopInventory::RestoreCancelled(const std::shared_ptr<const ModWorkshopInventorySnapshot>& current,
                                             const std::shared_ptr<const ModWorkshopInventorySnapshot>& expected)
{
	auto restored = std::make_shared<ModWorkshopInventorySnapshot>(*current);
	restored->checking = false;
	PublishIfCurrent(expected, std::move(restored));
}

bool CModWorkshopInventory::CheckForUpdates(CModWorkshopClient& client, const ModWorkshopRequestOptions& options)
{
	std::shared_ptr<const ModWorkshopInventorySnapshot> current = GetSnapshot();
	if (!current || current->packages.empty())
	{
		RefreshLocal();
		current = GetSnapshot();
	}

	auto checking = std::make_shared<ModWorkshopInventorySnapshot>(*current);
	checking->checking = true;
	checking->error.clear();
	checking->updateCount = 0;
	for (ModWorkshopTrackedPackage& package : checking->packages)
	{
		if (package.modId != 0 && package.updateState != ModWorkshopUpdateState::Error)
			package.updateState = package.installedState ? ModWorkshopUpdateState::Checking : ModWorkshopUpdateState::LegacyUnknown;
	}
	auto publishedChecking = std::make_shared<ModWorkshopInventorySnapshot>(*checking);
	if (!PublishIfCurrent(current, publishedChecking))
		return false;
	const std::shared_ptr<const ModWorkshopInventorySnapshot> expected = publishedChecking;

	uint64_t gameId = 0;
	ModWorkshopError requestError;
	if (!client.ResolveGameId(TITANFALL_2_SLUG, gameId, requestError, options))
	{
		if (requestError.code == ModWorkshopErrorCode::Cancelled)
		{
			RestoreCancelled(current, expected);
			return false;
		}
		checking->checking = false;
		checking->checkedAt = CModPlatform::CurrentTimestamp();
		checking->error = requestError.message;
		for (ModWorkshopTrackedPackage& package : checking->packages)
		{
			if (package.updateState == ModWorkshopUpdateState::Checking)
			{
				package.updateState = ModWorkshopUpdateState::Error;
				package.error = requestError.message;
			}
		}
		PublishIfCurrent(expected, std::move(checking));
		return false;
	}

	std::vector<uint64_t> ids;
	ids.reserve(checking->packages.size());
	std::unordered_set<uint64_t> uniqueIds;
	for (const ModWorkshopTrackedPackage& package : checking->packages)
	{
		if (package.modId != 0 && uniqueIds.insert(package.modId).second)
			ids.push_back(package.modId);
	}

	std::unordered_map<uint64_t, ModWorkshopCatalogEntry> remoteEntries;
	for (size_t offset = 0; offset < ids.size(); offset += UPDATE_BATCH_SIZE)
	{
		if (options.isCancelled && options.isCancelled())
		{
			RestoreCancelled(current, expected);
			return false;
		}
		ModWorkshopListQuery query;
		query.gameId = gameId;
		query.limit = static_cast<int>(UPDATE_BATCH_SIZE);
		query.page = 1;
		query.sort = "bumped_at";
		const size_t count = std::min(UPDATE_BATCH_SIZE, ids.size() - offset);
		query.ids.assign(ids.begin() + offset, ids.begin() + offset + count);
		ModWorkshopPage page;
		if (!client.ListMods(query, page, requestError, options))
		{
			if (requestError.code == ModWorkshopErrorCode::Cancelled)
			{
				RestoreCancelled(current, expected);
				return false;
			}
			checking->error = requestError.message;
			for (ModWorkshopTrackedPackage& package : checking->packages)
			{
				if (package.updateState == ModWorkshopUpdateState::Checking)
				{
					package.updateState = ModWorkshopUpdateState::Error;
					package.error = requestError.message;
				}
			}
			checking->checking = false;
			checking->checkedAt = CModPlatform::CurrentTimestamp();
			PublishIfCurrent(expected, std::move(checking));
			return false;
		}
		for (ModWorkshopCatalogEntry& entry : page.entries)
			remoteEntries.insert_or_assign(entry.id, std::move(entry));
	}

	checking->updateCount = 0;
	for (ModWorkshopTrackedPackage& package : checking->packages)
	{
		if (package.modId == 0 || package.updateState == ModWorkshopUpdateState::Error)
			continue;
		const auto found = remoteEntries.find(package.modId);
		if (found == remoteEntries.end())
		{
			package.updateState = ModWorkshopUpdateState::MissingRemote;
			package.error = "Mod is no longer returned by ModWorkshop";
			continue;
		}

		const ModWorkshopCatalogEntry& remote = found->second;
		package.remoteSelectedFileId = remote.selectedFileId;
		package.remoteVersion = remote.version;
		package.remoteUpdatedAt = remote.updatedAt;
		package.remoteThumbnail = remote.thumbnail;
		package.error.clear();
		if (remote.suspended || !remote.approved || remote.disableModManagers || !remote.hasDownload || !remote.selectedFileId)
		{
			package.updateState = ModWorkshopUpdateState::Unsupported;
			package.error = "Remote mod cannot be installed by a mod manager";
		}
		else if (!package.installedState)
		{
			package.updateState = ModWorkshopUpdateState::LegacyUnknown;
		}
		else if (package.installedState->selectedFileId == *remote.selectedFileId)
		{
			package.updateState = ModWorkshopUpdateState::Current;
		}
		else
		{
			package.updateState = ModWorkshopUpdateState::UpdateAvailable;
			++checking->updateCount;
		}
	}
	checking->checking = false;
	checking->checkedAt = CModPlatform::CurrentTimestamp();
	return PublishIfCurrent(expected, std::move(checking));
}

std::shared_ptr<const ModWorkshopInventorySnapshot> CModWorkshopInventory::GetSnapshot() const
{
	std::scoped_lock lock(m_Mutex);
	return m_Snapshot;
}

std::optional<ModWorkshopTrackedPackage> CModWorkshopInventory::FindPackage(uint64_t modId) const
{
	const std::shared_ptr<const ModWorkshopInventorySnapshot> snapshot = GetSnapshot();
	if (!snapshot)
		return std::nullopt;
	const auto found = std::ranges::find(snapshot->packages, modId, &ModWorkshopTrackedPackage::modId);
	return found != snapshot->packages.end() ? std::optional<ModWorkshopTrackedPackage>(*found) : std::nullopt;
}
