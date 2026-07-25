#include "modsystem/modinstaller.h"

#include "config/profile.h"
#include "core/tier0.h"
#include "modsystem/modmanager.h"
#include "modsystem/modworkshop_inventory.h"
#include "modsystem/platform/modplatform.h"
#include "modsystem/platform/modworkshop.h"
#include "modsystem/platform/thunderstore.h"
#include "tier0/frametask.h"

#include <compat/unzip.h>
#include <rapidjson/document.h>

#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <format>
#include <future>
#include <iomanip>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

std::string CModInstallService::SanitizeFolderComponent(std::string value)
{
	for (char& character : value)
	{
		if (character == ' ' || character == '\\' || character == '/' || character == ':' || character == '*' || character == '?' ||
		    character == '"' || character == '<' || character == '>' || character == '|' || std::iscntrl(static_cast<unsigned char>(character)))
		{
			character = '_';
		}
	}
	while (!value.empty() && (value.back() == ' ' || value.back() == '.'))
		value.pop_back();
	if (value.empty())
		value = "Unknown";
	if (value.size() > 80)
		value.resize(80);
	return value;
}

std::string CModInstallService::IconFilenameForPath(std::string_view path)
{
	const size_t suffix = path.find_first_of("?#");
	if (suffix != std::string_view::npos)
		path = path.substr(0, suffix);
	const size_t separator = path.find_last_of("/\\");
	const size_t extension = path.find_last_of('.');
	if (extension == std::string_view::npos || (separator != std::string_view::npos && extension < separator))
		return {};
	std::string filename = "icon";
	filename.append(path.substr(extension));
	std::ranges::transform(filename, filename.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	return std::ranges::find(MOD_ICON_FILENAMES, filename) != MOD_ICON_FILENAMES.end() ? filename : std::string();
}

bool CModInstallService::HasPackageIcon(const fs::path& packageRoot)
{
	for (const std::string_view filename : MOD_ICON_FILENAMES)
	{
		std::error_code error;
		if (fs::is_regular_file(packageRoot / filename, error) && !error)
			return true;
	}
	return false;
}

std::string CModInstallService::LowerAscii(std::string value)
{
	std::ranges::transform(value, value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	return value;
}

bool CModInstallService::NormalizeAbsolutePath(const fs::path& path, fs::path& normalized)
{
	std::error_code error;
	normalized = fs::absolute(path, error).lexically_normal();
	return !error && !normalized.empty();
}

std::string CModInstallService::PathKey(const fs::path& path)
{
	return LowerAscii(path.generic_string());
}

bool CModInstallService::TryGetDirectChildRoot(const fs::path& path, const fs::path& allowedRoot, fs::path& directChild)
{
	fs::path normalizedPath;
	fs::path normalizedAllowedRoot;
	if (!NormalizeAbsolutePath(path, normalizedPath) || !NormalizeAbsolutePath(allowedRoot, normalizedAllowedRoot))
		return false;

	const std::string pathKey = PathKey(normalizedPath);
	std::string rootPrefix = PathKey(normalizedAllowedRoot);
	if (!rootPrefix.ends_with('/'))
		rootPrefix.push_back('/');
	if (!pathKey.starts_with(rootPrefix))
		return false;

	const std::string_view relative(pathKey.data() + rootPrefix.size(), pathKey.size() - rootPrefix.size());
	const size_t separator = relative.find('/');
	const std::string_view firstComponent = relative.substr(0, separator);
	if (firstComponent.empty() || firstComponent == "." || firstComponent == "..")
		return false;

	directChild = (normalizedAllowedRoot / fs::u8path(std::string(firstComponent))).lexically_normal();
	return PathKey(directChild.parent_path()) == PathKey(normalizedAllowedRoot);
}

bool CModInstallService::TryDeriveRemovalRoot(const Mod& mod, fs::path& deletionRoot)
{
	const std::array<fs::path, 3> allowedRoots = {
	    GetModFolderPath(),
	    GetPackageFolderPath(),
	    GetRemoteModFolderPath(),
	};
	for (const fs::path& allowedRoot : allowedRoots)
	{
		fs::path candidate;
		if (!TryGetDirectChildRoot(mod.m_ModDirectory, allowedRoot, candidate))
			continue;
		if (!mod.m_PackageDirectory.empty())
		{
			fs::path normalizedPackageRoot;
			if (!NormalizeAbsolutePath(mod.m_PackageDirectory, normalizedPackageRoot) || PathKey(normalizedPackageRoot) != PathKey(candidate))
			{
				return false;
			}
		}
		deletionRoot = std::move(candidate);
		return true;
	}
	return false;
}

bool CModInstallService::ValidateRemovalRoot(const fs::path& deletionRoot)
{
	fs::path normalizedDeletionRoot;
	if (!NormalizeAbsolutePath(deletionRoot, normalizedDeletionRoot))
		return false;

	bool directChild = false;
	for (const fs::path& allowedRoot : {GetModFolderPath(), GetPackageFolderPath(), GetRemoteModFolderPath()})
	{
		fs::path candidate;
		if (TryGetDirectChildRoot(normalizedDeletionRoot, allowedRoot, candidate) && PathKey(candidate) == PathKey(normalizedDeletionRoot))
		{
			directChild = true;
			break;
		}
	}
	if (!directChild)
		return false;

	std::error_code error;
	if (!fs::is_directory(normalizedDeletionRoot, error) || error)
		return false;
	const DWORD attributes = GetFileAttributesW(normalizedDeletionRoot.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

uint64_t CModInstallService::ParseManagedModId(const Mod& mod)
{
	if (mod.m_Source != ModSource::ModWorkshop || !mod.m_ManagedId || mod.m_ManagedId->empty())
		return 0;
	uint64_t modId = 0;
	const std::string_view text = *mod.m_ManagedId;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), modId);
	return error == std::errc() && end == text.data() + text.size() ? modId : 0;
}

bool CModInstallService::InspectInstalledRemoval(int modIndex, InstalledRemovalTarget& target) const
{
	target = {};
	if (!g_pModManager || modIndex < 0 || static_cast<size_t>(modIndex) >= g_pModManager->m_LoadedMods.size())
		return false;

	const Mod& matched = g_pModManager->m_LoadedMods[modIndex];
	if (!TryDeriveRemovalRoot(matched, target.root))
		return false;

	target.name = matched.Name;
	target.version = matched.Version;
	target.managedModId = ParseManagedModId(matched);
	bool containsCoreMod = false;
	for (const Mod& mod : g_pModManager->m_LoadedMods)
	{
		fs::path modRoot;
		if (!TryDeriveRemovalRoot(mod, modRoot) || PathKey(modRoot) != PathKey(target.root))
			continue;
		++target.deleteModCount;
		containsCoreMod = containsCoreMod || Mod::IsCoreModName(mod.Name);
	}
	target.canDelete = !containsCoreMod && !Mod::IsCoreModName(matched.Name) && ValidateRemovalRoot(target.root);
	return true;
}

bool CModInstallService::IsReservedWindowsName(std::string_view component)
{
	const size_t extension = component.find('.');
	const std::string base = LowerAscii(std::string(component.substr(0, extension)));
	if (base == "con" || base == "prn" || base == "aux" || base == "nul")
		return true;
	if (base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt")) && base[3] >= '1' && base[3] <= '9')
	{
		return true;
	}
	return false;
}

bool CModInstallService::NormalizeArchivePath(std::string_view rawName, std::string& normalized, bool& directory, std::string& errorMessage)
{
	if (rawName.empty() || rawName.size() > 1024 || rawName.find('\0') != std::string_view::npos)
	{
		errorMessage = "Archive contains an empty, overlong, or NUL-containing path";
		return false;
	}

	normalized.assign(rawName);
	std::ranges::replace(normalized, '\\', '/');
	directory = normalized.back() == '/';
	while (!normalized.empty() && normalized.back() == '/')
		normalized.pop_back();
	if (normalized.empty() || normalized.front() == '/' || normalized.find(':') != std::string::npos)
	{
		errorMessage = std::format("Archive contains an unsafe absolute or ADS path: {}", rawName);
		return false;
	}

	size_t start = 0;
	while (start < normalized.size())
	{
		const size_t end = normalized.find('/', start);
		const std::string_view component =
		    std::string_view(normalized).substr(start, end == std::string::npos ? normalized.size() - start : end - start);
		if (component.empty() || component == "." || component == ".." || component.size() > 255 || component.back() == ' ' ||
		    component.back() == '.' || IsReservedWindowsName(component))
		{
			errorMessage = std::format("Archive contains an unsafe path component: {}", rawName);
			return false;
		}
		for (const unsigned char character : component)
		{
			if (character < 0x20 || character == '<' || character == '>' || character == '"' || character == '|' || character == '?' ||
			    character == '*')
			{
				errorMessage = std::format("Archive contains an invalid Windows path: {}", rawName);
				return false;
			}
		}
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	return true;
}

bool CModInstallService::IsSupportedManifestPath(std::string_view relativePath)
{
	if (relativePath == "mod.json")
		return true;
	if (!relativePath.starts_with("mods/"))
		return false;
	const size_t modNameEnd = relativePath.find('/', 5);
	return modNameEnd != std::string_view::npos && modNameEnd > 5 && relativePath.substr(modNameEnd + 1) == "mod.json";
}

bool CModInstallService::ContainsSupportedManifest(std::span<const ArchiveEntry> entries, std::string_view prefix)
{
	for (const ArchiveEntry& entry : entries)
	{
		if (entry.directory)
			continue;
		const std::string path = entry.relativePath.generic_string();
		if (path.starts_with(prefix) && IsSupportedManifestPath(std::string_view(path).substr(prefix.size())))
			return true;
	}
	return false;
}

bool CModInstallService::PathMatchesKey(const fs::path& path, std::string_view key)
{
	fs::path normalized;
	return NormalizeAbsolutePath(path, normalized) && PathKey(normalized) == key;
}

bool CModInstallService::InspectArchive(const fs::path& archivePath, std::vector<ArchiveEntry>& entries, std::string& errorMessage)
{
	entries.clear();
	unzFile archive = unzOpen64(archivePath.string().c_str());
	if (!archive)
	{
		errorMessage = "Downloaded file is not a readable ZIP archive";
		return false;
	}

	unz_global_info64 globalInfo{};
	if (unzGetGlobalInfo64(archive, &globalInfo) != UNZ_OK || globalInfo.number_entry == 0 || globalInfo.number_entry > MAX_ARCHIVE_ENTRIES ||
	    unzGoToFirstFile(archive) != UNZ_OK)
	{
		unzClose(archive);
		errorMessage = "Archive has an invalid or excessive entry count";
		return false;
	}

	entries.reserve(static_cast<size_t>(globalInfo.number_entry));
	uint64_t totalExpanded = 0;
	for (uint64_t index = 0; index < globalInfo.number_entry; ++index)
	{
		unz_file_info64 fileInfo{};
		if (unzGetCurrentFileInfo64(archive, &fileInfo, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK || fileInfo.size_filename == 0 ||
		    fileInfo.size_filename > 1024)
		{
			errorMessage = "Archive contains unreadable entry metadata";
			unzClose(archive);
			return false;
		}
		std::vector<char> filename(static_cast<size_t>(fileInfo.size_filename) + 1, '\0');
		if (unzGetCurrentFileInfo64(archive, &fileInfo, filename.data(), static_cast<unsigned long>(filename.size()), nullptr, 0, nullptr, 0) !=
		    UNZ_OK)
		{
			errorMessage = "Archive entry name could not be read";
			unzClose(archive);
			return false;
		}

		const std::string_view rawName(filename.data(), fileInfo.size_filename);
		if (rawName.find('\0') != std::string_view::npos || (fileInfo.flag & 1) != 0)
		{
			errorMessage = "Encrypted or NUL-containing archive entries are not supported";
			unzClose(archive);
			return false;
		}
		const uint32_t unixMode = static_cast<uint32_t>(fileInfo.external_fa >> 16);
		if ((unixMode & 0170000) == 0120000)
		{
			errorMessage = "Archive symlinks are not allowed";
			unzClose(archive);
			return false;
		}
		if (fileInfo.uncompressed_size > MAX_ENTRY_BYTES || totalExpanded > MAX_EXPANDED_BYTES - fileInfo.uncompressed_size)
		{
			errorMessage = "Archive expanded size exceeds the safety limit";
			unzClose(archive);
			return false;
		}
		totalExpanded += fileInfo.uncompressed_size;

		ArchiveEntry entry;
		entry.archiveName.assign(rawName);
		entry.compressedSize = fileInfo.compressed_size;
		entry.uncompressedSize = fileInfo.uncompressed_size;
		std::string normalized;
		if (!NormalizeArchivePath(rawName, normalized, entry.directory, errorMessage))
		{
			unzClose(archive);
			return false;
		}
		entry.directory = entry.directory || (fileInfo.external_fa & FILE_ATTRIBUTE_DIRECTORY) != 0;
		entry.relativePath = fs::u8path(normalized);
		entries.push_back(std::move(entry));

		if (index + 1 < globalInfo.number_entry && unzGoToNextFile(archive) != UNZ_OK)
		{
			errorMessage = "Archive directory ended unexpectedly";
			unzClose(archive);
			return false;
		}
	}
	unzClose(archive);

	std::string rootPrefix;
	if (!ContainsSupportedManifest(entries, ""))
	{
		const std::string firstPath = entries.front().relativePath.generic_string();
		const size_t separator = firstPath.find('/');
		const std::string firstComponent = firstPath.substr(0, separator);
		if (firstComponent.empty())
		{
			errorMessage = "Archive does not contain a supported mod layout";
			return false;
		}
		rootPrefix = firstComponent + "/";
		for (const ArchiveEntry& entry : entries)
		{
			const std::string path = entry.relativePath.generic_string();
			if (path != firstComponent && !path.starts_with(rootPrefix))
			{
				errorMessage = "Archive has files outside its package root";
				return false;
			}
		}
		if (!ContainsSupportedManifest(entries, rootPrefix))
		{
			errorMessage = "Archive does not contain root mod.json or mods/<name>/mod.json";
			return false;
		}
	}

	std::unordered_set<std::string> occupiedPaths;
	std::unordered_set<std::string> filePaths;
	for (ArchiveEntry& entry : entries)
	{
		std::string path = entry.relativePath.generic_string();
		if (!rootPrefix.empty())
		{
			if (path == rootPrefix.substr(0, rootPrefix.size() - 1))
			{
				if (!entry.directory)
				{
					errorMessage = "Archive package root is a file";
					return false;
				}
				entry.relativePath.clear();
				continue;
			}
			path.erase(0, rootPrefix.size());
			entry.relativePath = fs::u8path(path);
		}
		if (path.empty())
			continue;
		const std::string canonical = LowerAscii(path);
		if (!occupiedPaths.insert(canonical).second)
		{
			errorMessage = std::format("Archive contains duplicate path '{}'", path);
			return false;
		}
		if (!entry.directory)
			filePaths.insert(canonical);
	}
	for (const std::string& path : occupiedPaths)
	{
		size_t separator = path.find('/');
		while (separator != std::string::npos)
		{
			if (filePaths.contains(path.substr(0, separator)))
			{
				errorMessage = "Archive contains a file/directory path collision";
				return false;
			}
			separator = path.find('/', separator + 1);
		}
	}
	return true;
}

bool CModInstallService::ExtractArchive(const fs::path& archivePath, const fs::path& stagingRoot, std::span<const ArchiveEntry> entries,
                                        const std::function<bool()>& cancelled, const std::function<void(uint64_t, uint64_t)>& progress,
                                        std::string& errorMessage)
{
	std::error_code filesystemError;
	fs::create_directories(stagingRoot, filesystemError);
	if (filesystemError)
	{
		errorMessage = std::format("Failed creating staging directory: {}", filesystemError.message());
		return false;
	}

	unzFile archive = unzOpen64(archivePath.string().c_str());
	if (!archive || unzGoToFirstFile(archive) != UNZ_OK)
	{
		if (archive)
			unzClose(archive);
		errorMessage = "Failed reopening validated archive";
		return false;
	}

	uint64_t totalBytes = 0;
	for (const ArchiveEntry& entry : entries)
		totalBytes += entry.uncompressedSize;
	uint64_t extractedBytes = 0;
	std::vector<uint8_t> buffer(EXTRACTION_BUFFER_SIZE);

	for (size_t index = 0; index < entries.size(); ++index)
	{
		if (cancelled())
		{
			errorMessage = "Installation cancelled while staging";
			unzClose(archive);
			return false;
		}
		const ArchiveEntry& entry = entries[index];
		if (entry.relativePath.empty())
		{
			if (index + 1 < entries.size())
				unzGoToNextFile(archive);
			continue;
		}

		const fs::path destination = stagingRoot / entry.relativePath;
		if (entry.directory)
		{
			fs::create_directories(destination, filesystemError);
			if (filesystemError)
			{
				errorMessage = std::format("Failed creating staged directory: {}", filesystemError.message());
				unzClose(archive);
				return false;
			}
		}
		else
		{
			fs::create_directories(destination.parent_path(), filesystemError);
			if (filesystemError || unzOpenCurrentFile(archive) != UNZ_OK)
			{
				errorMessage = "Failed opening archive entry for staging";
				unzClose(archive);
				return false;
			}

			FILE* output = _wfopen(destination.c_str(), L"wb");
			if (!output)
			{
				unzCloseCurrentFile(archive);
				unzClose(archive);
				errorMessage = std::format("Failed creating staged file '{}'", destination.string());
				return false;
			}

			uint64_t entryBytes = 0;
			bool extractionFailed = false;
			for (;;)
			{
				if (cancelled())
				{
					errorMessage = "Installation cancelled while staging";
					extractionFailed = true;
					break;
				}
				const int bytesRead = unzReadCurrentFile(archive, buffer.data(), static_cast<unsigned int>(buffer.size()));
				if (bytesRead < 0)
				{
					errorMessage = "Archive decompression failed";
					extractionFailed = true;
					break;
				}
				if (bytesRead == 0)
					break;
				if (std::fwrite(buffer.data(), 1, static_cast<size_t>(bytesRead), output) != static_cast<size_t>(bytesRead))
				{
					errorMessage = "Failed writing staged archive content";
					extractionFailed = true;
					break;
				}
				entryBytes += static_cast<uint64_t>(bytesRead);
				extractedBytes += static_cast<uint64_t>(bytesRead);
				progress(extractedBytes, totalBytes);
			}
			const bool closeFailed = std::fclose(output) != 0;
			const int archiveCloseResult = unzCloseCurrentFile(archive);
			if (extractionFailed || closeFailed || archiveCloseResult != UNZ_OK || entryBytes != entry.uncompressedSize)
			{
				if (errorMessage.empty())
					errorMessage = "Archive CRC, size, or staged file flush validation failed";
				unzClose(archive);
				return false;
			}
		}

		if (index + 1 < entries.size() && unzGoToNextFile(archive) != UNZ_OK)
		{
			errorMessage = "Archive directory ended unexpectedly during staging";
			unzClose(archive);
			return false;
		}
	}
	unzClose(archive);
	return true;
}

bool CModInstallService::ReadStagedManifest(const fs::path& manifestPath, ModWorkshopContainedMod& containedMod, std::string& errorMessage)
{
	std::error_code filesystemError;
	const uintmax_t size = fs::file_size(manifestPath, filesystemError);
	if (filesystemError || size == 0 || size > MAX_MANIFEST_BYTES)
	{
		errorMessage = std::format("Invalid staged manifest '{}'", manifestPath.string());
		return false;
	}
	std::ifstream input(manifestPath, std::ios::binary);
	std::string contents(static_cast<size_t>(size), '\0');
	input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
	if (!input)
	{
		errorMessage = std::format("Failed reading staged manifest '{}'", manifestPath.string());
		return false;
	}

	rapidjson::Document document;
	document.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(contents.data(), contents.size());
	if (document.HasParseError() || !document.IsObject())
	{
		errorMessage = std::format("Staged manifest '{}' is invalid JSON", manifestPath.string());
		return false;
	}
	const auto name = document.FindMember("Name");
	if (name == document.MemberEnd() || !name->value.IsString() || name->value.GetStringLength() == 0)
	{
		errorMessage = std::format("Staged manifest '{}' has no valid Name", manifestPath.string());
		return false;
	}
	containedMod.name.assign(name->value.GetString(), name->value.GetStringLength());
	const auto version = document.FindMember("Version");
	if (version != document.MemberEnd() && version->value.IsString())
		containedMod.version.assign(version->value.GetString(), version->value.GetStringLength());
	return true;
}

bool CModInstallService::ValidateStagedPackage(const fs::path& stagingRoot, std::vector<ModWorkshopContainedMod>& containedMods,
                                               std::string& errorMessage)
{
	containedMods.clear();
	std::error_code filesystemError;
	if (fs::is_regular_file(stagingRoot / "mod.json", filesystemError))
	{
		ModWorkshopContainedMod mod;
		if (!ReadStagedManifest(stagingRoot / "mod.json", mod, errorMessage))
			return false;
		containedMods.push_back(std::move(mod));
	}

	const fs::path modsRoot = stagingRoot / "mods";
	filesystemError.clear();
	if (fs::is_directory(modsRoot, filesystemError))
	{
		for (const fs::directory_entry& entry : fs::directory_iterator(modsRoot, filesystemError))
		{
			if (filesystemError)
			{
				errorMessage = std::format("Failed enumerating staged mods: {}", filesystemError.message());
				return false;
			}
			if (!entry.is_directory(filesystemError) || !fs::is_regular_file(entry.path() / "mod.json", filesystemError))
			{
				continue;
			}
			ModWorkshopContainedMod mod;
			if (!ReadStagedManifest(entry.path() / "mod.json", mod, errorMessage))
				return false;
			containedMods.push_back(std::move(mod));
		}
	}
	if (containedMods.empty())
	{
		errorMessage = "Staged package contains no loadable mod.json";
		return false;
	}
	return true;
}

bool CModInstallService::ComputeSha256(const fs::path& filePath, std::string& hashText, std::string& errorMessage)
{
	BCRYPT_ALG_HANDLE algorithm = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_HASH_REUSABLE_FLAG)) ||
	    !BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0)))
	{
		if (algorithm)
			BCryptCloseAlgorithmProvider(algorithm, 0);
		errorMessage = "Failed initializing SHA-256";
		return false;
	}

	std::ifstream input(filePath, std::ios::binary);
	std::vector<uint8_t> buffer(EXTRACTION_BUFFER_SIZE);
	while (input)
	{
		input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
		const std::streamsize count = input.gcount();
		if (count > 0 && !BCRYPT_SUCCESS(BCryptHashData(hash, buffer.data(), static_cast<ULONG>(count), 0)))
		{
			BCryptDestroyHash(hash);
			BCryptCloseAlgorithmProvider(algorithm, 0);
			errorMessage = "Failed hashing downloaded archive";
			return false;
		}
	}
	if (!input.eof())
	{
		BCryptDestroyHash(hash);
		BCryptCloseAlgorithmProvider(algorithm, 0);
		errorMessage = "Failed reading downloaded archive for hashing";
		return false;
	}

	std::array<uint8_t, 32> digest{};
	const NTSTATUS result = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
	BCryptDestroyHash(hash);
	BCryptCloseAlgorithmProvider(algorithm, 0);
	if (!BCRYPT_SUCCESS(result))
	{
		errorMessage = "Failed finalizing archive SHA-256";
		return false;
	}

	std::ostringstream output;
	output << std::hex << std::setfill('0');
	for (const uint8_t byte : digest)
		output << std::setw(2) << static_cast<unsigned int>(byte);
	hashText = output.str();
	return true;
}

void CModInstallService::Notify()
{
	OperationChangedCallback callback;
	{
		std::scoped_lock lock(m_CallbackMutex);
		callback = m_OperationChanged;
	}
	if (callback)
		RunInMainThread([callback = std::move(callback)] { callback(); });
}

void CModInstallService::Publish(std::shared_ptr<ModInstallOperationSnapshot> next)
{
	{
		std::scoped_lock lock(m_SnapshotMutex);
		m_Snapshot = std::move(next);
	}
	Notify();
}

std::shared_ptr<const ModInstallOperationSnapshot> CModInstallService::GetSnapshot() const
{
	std::scoped_lock lock(m_SnapshotMutex);
	return m_Snapshot;
}

void CModInstallService::Transition(ModInstallOperationState state, std::string message, std::string name, std::string version)
{
	auto next = std::make_shared<ModInstallOperationSnapshot>(*GetSnapshot());
	next->state = state;
	next->message = std::move(message);
	if (!name.empty())
		next->name = std::move(name);
	if (!version.empty())
		next->version = std::move(version);
	next->progress = 0;
	next->total = 0;
	next->ratio = 0.0f;
	Publish(std::move(next));
}

void CModInstallService::ReportProgress(uint64_t progressValue, uint64_t totalValue)
{
	const auto now = std::chrono::steady_clock::now();
	if (progressValue != totalValue && now - m_LastProgressPublish < std::chrono::milliseconds(75))
	{
		return;
	}
	m_LastProgressPublish = now;
	auto next = std::make_shared<ModInstallOperationSnapshot>(*GetSnapshot());
	next->progress = progressValue;
	next->total = totalValue;
	next->ratio = totalValue != 0 ? static_cast<float>(static_cast<double>(progressValue) / totalValue) : 0.0f;
	Publish(std::move(next));
}

bool CModInstallService::IsCancelled() const
{
	return m_Stopped.load(std::memory_order_acquire) || m_CancelRequested.load(std::memory_order_acquire);
}

bool CModInstallService::RunOnMainThreadAndWait(std::function<bool()> function, bool& result)
{
	if (ThreadInMainThread())
	{
		result = function();
		return true;
	}
	auto task = std::make_shared<std::packaged_task<bool()>>(std::move(function));
	std::future<bool> future = task->get_future();
	RunInMainThread([task] { (*task)(); });
	if (future.wait_for(MAIN_THREAD_TIMEOUT) != std::future_status::ready)
		return false;
	result = future.get();
	return true;
}

bool CModInstallService::CaptureRuntimeState(const std::vector<PackagePlan>& plans, std::unordered_set<std::string>& installedModNames,
                                             std::unordered_map<std::string, bool>& enabledStates, std::string& errorMessage)
{
	std::vector<fs::path> oldRoots;
	for (const PackagePlan& plan : plans)
	{
		if (plan.oldRoot)
			oldRoots.push_back(plan.oldRoot->lexically_normal());
	}
	auto namesResult = std::make_shared<std::unordered_set<std::string>>();
	auto enabledResult = std::make_shared<std::unordered_map<std::string, bool>>();
	bool callbackResult = false;
	const bool completed = RunOnMainThreadAndWait([oldRoots = std::move(oldRoots), namesResult, enabledResult]
	{
		if (!g_pModManager)
			return false;
		for (const Mod& mod : g_pModManager->m_LoadedMods)
			namesResult->insert(mod.Name);
		*enabledResult = g_pModManager->CaptureEnabledStatesForPackages(oldRoots);
		return true;
	}, callbackResult);
	if (!completed || !callbackResult)
	{
		errorMessage = completed ? "Mod manager is unavailable" : "Timed out reading mod state";
		return false;
	}
	installedModNames = std::move(*namesResult);
	enabledStates = std::move(*enabledResult);
	return true;
}

bool CModInstallService::ResolveUnmanagedReplacements(uint64_t generation, std::vector<PackagePlan>& plans,
                                                      std::unordered_map<std::string, bool>& enabledStates, std::string& errorMessage)
{

	std::unordered_map<std::string, size_t> planByModName;
	for (size_t planIndex = 0; planIndex < plans.size(); ++planIndex)
	{
		for (const ModWorkshopContainedMod& containedMod : plans[planIndex].containedMods)
			planByModName.try_emplace(containedMod.name, planIndex);
	}
	if (planByModName.empty())
		return true;

	auto scan = std::make_shared<ConflictScan>();
	scan->rootsByPlan.resize(plans.size());
	bool callbackResult = false;
	const bool completed = RunOnMainThreadAndWait([scan, planByModName = std::move(planByModName)]
	{
		if (!g_pModManager)
		{
			scan->error = "Mod manager is unavailable";
			return false;
		}

		std::unordered_set<std::string> stagedNames;
		for (const auto& [name, planIndex] : planByModName)
		{
			NOTE_UNUSED(planIndex);
			stagedNames.insert(name);
		}

		std::unordered_map<std::string, size_t> rootOwners;
		std::unordered_set<std::string> conflictingNames;
		for (const Mod& mod : g_pModManager->m_LoadedMods)
		{
			const auto plan = planByModName.find(mod.Name);
			if (plan == planByModName.end() || mod.m_Source == ModSource::Remote || mod.m_Source == ModSource::ModWorkshop)
				continue;
			if (Mod::IsCoreModName(mod.Name))
			{
				scan->error = std::format("Core mod '{}' cannot be replaced", mod.Name);
				return false;
			}

			fs::path root;
			if (!TryDeriveRemovalRoot(mod, root) || !ValidateRemovalRoot(root))
			{
				scan->error = std::format("The existing '{}' installation cannot be replaced safely", mod.Name);
				return false;
			}

			fs::path remoteRoot;
			if (TryGetDirectChildRoot(root, GetRemoteModFolderPath(), remoteRoot) && PathKey(remoteRoot) == PathKey(root))
				continue;

			const std::string rootKey = PathKey(root);
			const auto [owner, inserted] = rootOwners.try_emplace(rootKey, plan->second);
			if (inserted)
			{
				scan->roots.push_back(root);
				scan->rootsByPlan[owner->second].push_back(root);
			}
			conflictingNames.insert(mod.Name);
		}

		if (scan->roots.empty())
			return true;

		std::unordered_set<std::string> replacementRootKeys;
		for (const fs::path& root : scan->roots)
			replacementRootKeys.insert(PathKey(root));
		for (const Mod& mod : g_pModManager->m_LoadedMods)
		{
			fs::path root;
			if (!TryDeriveRemovalRoot(mod, root) || !replacementRootKeys.contains(PathKey(root)))
				continue;
			if (!stagedNames.contains(mod.Name))
			{
				scan->error = std::format("Package containing '{}' also contains unrelated mod '{}'; automatic replacement was cancelled",
				                          *conflictingNames.begin(), mod.Name);
				return false;
			}
		}

		scan->names.assign(conflictingNames.begin(), conflictingNames.end());
		std::ranges::sort(scan->names);
		scan->enabledStates = g_pModManager->CaptureEnabledStatesForPackages(scan->roots);
		return true;
	}, callbackResult);
	if (!completed || !callbackResult)
	{
		errorMessage = completed ? (scan->error.empty() ? "Could not inspect existing mod packages" : scan->error)
		                         : "Timed out inspecting existing mod packages";
		return false;
	}
	if (scan->roots.empty())
		return true;

	std::string names;
	for (const std::string& name : scan->names)
	{
		if (!names.empty())
			names += ", ";
		names += name;
	}

	{
		std::scoped_lock lock(m_MigrationMutex);
		m_MigrationGeneration = generation;
		m_MigrationDecision = MigrationDecision::Pending;
	}
	Transition(ModInstallOperationState::AwaitingMigration, "#MWS_MIGRATION_MESSAGE", names);

	MigrationDecision decision = MigrationDecision::None;
	{
		std::unique_lock lock(m_MigrationMutex);
		m_MigrationChanged.wait(lock, [this]
		{
			return m_MigrationDecision != MigrationDecision::Pending || m_CancelRequested.load(std::memory_order_acquire) ||
			       m_Stopped.load(std::memory_order_acquire);
		});
		decision = m_MigrationDecision;
		m_MigrationDecision = MigrationDecision::None;
		m_MigrationGeneration = 0;
	}
	if (decision != MigrationDecision::Accepted)
	{
		m_CancelRequested.store(true, std::memory_order_release);
		errorMessage = "Existing mod replacement was cancelled";
		return false;
	}

	for (size_t planIndex = 0; planIndex < plans.size(); ++planIndex)
		plans[planIndex].replacementRoots = std::move(scan->rootsByPlan[planIndex]);
	enabledStates.insert(scan->enabledStates.begin(), scan->enabledStates.end());
	return true;
}

bool CModInstallService::UnloadRuntimeForFilesystemMutation(std::string& errorMessage)
{
	Transition(ModInstallOperationState::Reloading, "#MWS_OPERATION_RELOADING");
	bool unloadResult = false;
	const bool completed = RunOnMainThreadAndWait([] { return g_pModManager && g_pModManager->UnloadModsForFilesystemMutation(); }, unloadResult);
	if (!completed || !unloadResult)
	{
		errorMessage = completed ? "Failed unloading mod assets; removal was cancelled" : "Timed out while unloading mod assets";
		return false;
	}
	return true;
}

bool CModInstallService::ResolveModWorkshop(uint64_t modId, bool root, ModInstallAction action, uint64_t expectedSelectedFileId,
                                            const ModWorkshopRequestOptions& options, const std::unordered_set<std::string>& installedModNames,
                                            std::vector<PackagePlan>& plans, std::unordered_map<uint64_t, size_t>& resolved,
                                            std::unordered_set<uint64_t>& visiting, std::string& errorMessage)
{
	if (resolved.contains(modId))
		return true;
	if (!visiting.insert(modId).second)
	{
		errorMessage = "ModWorkshop dependency cycle detected";
		return false;
	}

	ModWorkshopDetails details;
	ModWorkshopError requestError;
	if (!m_Client.GetMod(modId, details, requestError, options))
	{
		errorMessage = requestError.message;
		visiting.erase(modId);
		return false;
	}
	if (!details.approved || details.suspended || details.disableModManagers || !details.hasDownload || !details.selectedFile ||
	    (!details.selectedFile->type.empty() && details.selectedFile->type != "zip") || details.selectedFile->downloadUrl.empty())
	{
		errorMessage = std::format("ModWorkshop mod {} cannot be installed by a mod manager", modId);
		visiting.erase(modId);
		return false;
	}
	if (root && expectedSelectedFileId != 0 && details.selectedFile->id != expectedSelectedFileId)
	{
		errorMessage = "The selected ModWorkshop download changed before installation";
		visiting.erase(modId);
		return false;
	}

	const std::optional<ModWorkshopTrackedPackage> installed = CModWorkshopInventory::Get().FindPackage(modId);
	if (!root && installed && installed->installedState && installed->installedState->selectedFileId == details.selectedFile->id)
	{
		visiting.erase(modId);
		return true;
	}
	if (root && action == ModInstallAction::Install && installed)
	{
		errorMessage = "Mod is already installed";
		visiting.erase(modId);
		return false;
	}
	if (root && action == ModInstallAction::Update && (!installed || !installed->installedState))
	{
		errorMessage = "Tracked selected-file metadata is required before updating";
		visiting.erase(modId);
		return false;
	}

	for (const ModWorkshopDependency& dependency : details.dependencies)
	{
		if (dependency.optional)
			continue;
		if (!dependency.name.empty() && installedModNames.contains(dependency.name) &&
		    (!dependency.modId || !CModWorkshopInventory::Get().FindPackage(*dependency.modId)))
		{
			continue;
		}
		if (dependency.offsite)
		{
			std::string namespaceName;
			std::string packageName;
			CThunderstoreClient::PackageDetails dependencyDetails;
			if (!CThunderstoreClient::ParsePackageUrl(dependency.url, namespaceName, packageName) ||
			    !CThunderstoreClient::FetchPackageDetails(namespaceName, packageName, dependencyDetails))
			{
				const std::string& dependencyLabel = dependency.name.empty() ? dependency.url : dependency.name;
				errorMessage = std::format("Failed resolving required Thunderstore dependency '{}'", dependencyLabel);
				visiting.erase(modId);
				return false;
			}
			if (IsCancelled())
			{
				errorMessage = "Installation cancelled";
				visiting.erase(modId);
				return false;
			}

			const std::string managedId = dependencyDetails.m_Namespace + "/" + dependencyDetails.m_Name;
			const bool alreadyPlanned = std::ranges::any_of(plans, [&](const PackagePlan& candidate)
			{ return candidate.source == ModSource::Thunderstore && candidate.managedId == managedId; });
			if (alreadyPlanned)
				continue;

			PackagePlan dependencyPlan;
			dependencyPlan.source = ModSource::Thunderstore;
			dependencyPlan.managedId = managedId;
			dependencyPlan.name = dependency.name.empty() ? dependencyDetails.m_Name : dependency.name;
			dependencyPlan.author = dependencyDetails.m_Namespace;
			dependencyPlan.version = dependencyDetails.m_Version;
			dependencyPlan.downloadUrl = dependencyDetails.m_DownloadUrl;
			dependencyPlan.iconFilename = IconFilenameForPath(dependencyDetails.m_IconUrl);
			if (!dependencyPlan.iconFilename.empty())
				dependencyPlan.iconUrl = dependencyDetails.m_IconUrl;
			dependencyPlan.destination = GetPackageFolderPath() / ("ts-" + SanitizeFolderComponent(dependencyDetails.m_Namespace) + "-" +
			                                                       SanitizeFolderComponent(dependencyDetails.m_Name) + "-" +
			                                                       SanitizeFolderComponent(dependencyDetails.m_Version));
			plans.push_back(std::move(dependencyPlan));
		}
		else if (!dependency.modId || !ResolveModWorkshop(*dependency.modId, false, ModInstallAction::Install, 0, options, installedModNames, plans,
		                                                  resolved, visiting, errorMessage))
		{
			if (errorMessage.empty())
				errorMessage = std::format("Required dependency '{}' has no installable id", dependency.name);
			visiting.erase(modId);
			return false;
		}
	}

	PackagePlan plan;
	plan.modId = modId;
	plan.selectedFileId = details.selectedFile->id;
	plan.expectedDownloadSize = details.selectedFile->size;
	plan.managedId = std::to_string(modId);
	plan.name = details.name;
	plan.author = details.author;
	plan.version = details.selectedFile->version.empty() ? details.version : details.selectedFile->version;
	plan.selectedFileUpdatedAt = details.selectedFile->updatedAt;
	plan.remoteModUpdatedAt = details.updatedAt;
	plan.downloadUrl = details.selectedFile->downloadUrl;
	if (details.thumbnail)
	{
		plan.iconFilename = IconFilenameForPath(details.thumbnail->file);
		if (!plan.iconFilename.empty())
		{
			ModWorkshopThumbnail original = *details.thumbnail;
			original.hasThumbnail = false;
			plan.iconUrl = CModWorkshopClient::BuildThumbnailUrl(original);
			if (details.thumbnail->hasThumbnail)
			{
				ModWorkshopThumbnail preview = *details.thumbnail;
				preview.hasThumbnail = true;
				plan.iconFallbackUrl = CModWorkshopClient::BuildThumbnailUrl(preview);
			}
		}
	}
	if (installed)
		plan.oldRoot = installed->packageRoot;
	plan.destination = GetPackageFolderPath() / ("mws-" + std::to_string(modId));
	resolved.emplace(modId, plans.size());
	plans.push_back(std::move(plan));
	visiting.erase(modId);
	return true;
}

bool CModInstallService::StagePlan(PackagePlan& plan, size_t planIndex, const fs::path& jobRoot, uint64_t totalExpectedBytes,
                                   uint64_t completedExpectedBytes, std::string& errorMessage)
{
	Transition(ModInstallOperationState::Downloading, "#MWS_OPERATION_DOWNLOADING", plan.name, plan.version);
	const fs::path downloadsRoot = jobRoot / "downloads";
	std::error_code filesystemError;
	fs::create_directories(downloadsRoot, filesystemError);
	if (filesystemError)
	{
		errorMessage = std::format("Failed creating download directory: {}", filesystemError.message());
		return false;
	}
	plan.archivePath = downloadsRoot / std::format("{}.part", planIndex);
	plan.stagingRoot = jobRoot / "staging" / std::format("package-{}", planIndex);
	plan.backupRoot = jobRoot / "backups" / std::format("package-{}", planIndex);

	ModWorkshopRequestOptions options;
	options.timeoutSeconds = 600;
	options.connectTimeoutSeconds = 15;
	if (plan.expectedDownloadSize > MAX_ARCHIVE_BYTES)
	{
		errorMessage = "Remote archive exceeds the one-gigabyte safety limit";
		return false;
	}
	const uint64_t allowance = plan.expectedDownloadSize != 0 ? std::max<uint64_t>(1024 * 1024, plan.expectedDownloadSize / 20) : MAX_ARCHIVE_BYTES;
	options.maxResponseBytes = static_cast<size_t>(
	    std::min<uint64_t>(MAX_ARCHIVE_BYTES, plan.expectedDownloadSize != 0 ? plan.expectedDownloadSize + allowance : MAX_ARCHIVE_BYTES));
	options.isCancelled = [this] { return IsCancelled(); };
	options.progress = [this, totalExpectedBytes, completedExpectedBytes](uint64_t current, uint64_t)
	{ ReportProgress(completedExpectedBytes + current, totalExpectedBytes); };

	ModWorkshopError requestError;
	if (!m_Client.GetFile(plan.downloadUrl, plan.archivePath, plan.downloadedSize, requestError, options))
	{
		errorMessage = requestError.message;
		return false;
	}
	if (plan.expectedDownloadSize != 0 && plan.downloadedSize != plan.expectedDownloadSize)
	{
		errorMessage = std::format("Downloaded size {} does not match declared size {}", plan.downloadedSize, plan.expectedDownloadSize);
		return false;
	}

	Transition(ModInstallOperationState::Validating, "#MWS_OPERATION_VALIDATING");
	if (!ComputeSha256(plan.archivePath, plan.sha256, errorMessage))
		return false;
	std::vector<ArchiveEntry> entries;
	if (!InspectArchive(plan.archivePath, entries, errorMessage))
		return false;

	Transition(ModInstallOperationState::Staging, "#MWS_OPERATION_STAGING");
	if (!ExtractArchive(plan.archivePath, plan.stagingRoot, entries, [this] { return IsCancelled(); },
	                    [this](uint64_t progressValue, uint64_t totalValue) { ReportProgress(progressValue, totalValue); }, errorMessage) ||
	    !ValidateStagedPackage(plan.stagingRoot, plan.containedMods, errorMessage))
	{
		return false;
	}

	if (!HasPackageIcon(plan.stagingRoot) && !plan.iconUrl.empty() && !plan.iconFilename.empty())
	{
		ModWorkshopRequestOptions iconOptions;
		iconOptions.timeoutSeconds = 45;
		iconOptions.connectTimeoutSeconds = 15;
		iconOptions.maxResponseBytes = MAX_ICON_BYTES;
		iconOptions.isCancelled = [this] { return IsCancelled(); };
		const fs::path iconPath = plan.stagingRoot / plan.iconFilename;
		uint64_t iconBytes = 0;
		ModWorkshopError iconError;
		bool iconDownloaded = m_Client.GetFile(plan.iconUrl, iconPath, iconBytes, iconError, iconOptions) && iconBytes != 0;
		if (!iconDownloaded && !plan.iconFallbackUrl.empty() && !IsCancelled())
			iconDownloaded = m_Client.GetFile(plan.iconFallbackUrl, iconPath, iconBytes, iconError, iconOptions) && iconBytes != 0;
		if (IsCancelled())
		{
			errorMessage = "Installation cancelled";
			return false;
		}
		if (!iconDownloaded)
			spdlog::warn("Could not cache the package icon for '{}': {}", plan.name, iconError.message);
	}

	if (!CModPlatform::WriteManagedMarker(plan.stagingRoot, plan.source, plan.managedId))
	{
		errorMessage = std::format("Failed writing staged {} marker", plan.source == ModSource::Thunderstore ? "Thunderstore" : "ModWorkshop");
		return false;
	}
	if (plan.source == ModSource::ModWorkshop)
	{
		ModWorkshopPackageState state;
		state.modId = plan.modId;
		state.selectedFileId = plan.selectedFileId;
		state.downloadSize = plan.downloadedSize;
		state.selectedFileVersion = plan.version;
		state.selectedFileUpdatedAt = plan.selectedFileUpdatedAt;
		state.remoteModUpdatedAt = plan.remoteModUpdatedAt;
		state.sha256 = plan.sha256;
		state.installedAt = CModPlatform::CurrentTimestamp();
		state.containedMods = plan.containedMods;
		if (!CModPlatform::WriteWorkshopPackageState(plan.stagingRoot, state, errorMessage))
			return false;
	}
	return true;
}

bool CModInstallService::ReloadAndVerify(const std::vector<PackagePlan>& plans, std::unordered_map<std::string, bool> enabledStates,
                                         std::string& errorMessage)
{
	std::vector<ExpectedPackage> expected;
	expected.reserve(plans.size());
	for (const PackagePlan& plan : plans)
	{
		ExpectedPackage package;
		package.root = plan.destination;
		for (const ModWorkshopContainedMod& mod : plan.containedMods)
			package.names.push_back(mod.name);
		expected.push_back(std::move(package));
	}

	bool reloadResult = false;
	const bool completed = RunOnMainThreadAndWait([expected = std::move(expected), enabledStates = std::move(enabledStates)]() mutable
	{
		if (!g_pModManager)
			return false;
		g_pModManager->ReloadModsWithEnabledStates(std::move(enabledStates));
		for (const ExpectedPackage& package : expected)
		{
			if (!g_pModManager->HasLoadedPackageMods(package.root, package.names))
				return false;
		}
		return true;
	}, reloadResult);
	if (!completed || !reloadResult)
	{
		errorMessage = completed ? "Reloaded mods did not contain every staged package" : "Timed out while reloading mods";
		return false;
	}
	return true;
}

bool CModInstallService::ReloadAfterRollback(std::unordered_map<std::string, bool> enabledStates, std::string& errorMessage)
{
	bool reloadResult = false;
	const bool completed = RunOnMainThreadAndWait([enabledStates = std::move(enabledStates)]() mutable
	{
		if (!g_pModManager)
			return false;
		g_pModManager->ReloadModsWithEnabledStates(std::move(enabledStates));
		return true;
	}, reloadResult);
	if (!completed || !reloadResult)
	{
		errorMessage += "; rollback restored files but mod reload failed";
		return false;
	}
	return true;
}

bool CModInstallService::CommitPlans(std::vector<PackagePlan>& plans, const std::unordered_map<std::string, bool>& enabledStates,
                                     std::string& errorMessage, bool& preserveRecoveryFiles)
{
	std::unordered_set<std::string> destinations;
	for (const PackagePlan& plan : plans)
	{
		fs::path normalizedDestination;
		if (!NormalizeAbsolutePath(plan.destination, normalizedDestination))
		{
			errorMessage = std::format("Could not normalize destination '{}'", plan.destination.string());
			return false;
		}
		const std::string destinationKey = PathKey(normalizedDestination);
		if (!destinations.insert(destinationKey).second)
		{
			errorMessage = "Two dependency packages resolve to the same destination";
			return false;
		}
		const bool ownsDestination =
		    (plan.oldRoot && PathMatchesKey(*plan.oldRoot, destinationKey)) ||
		    std::ranges::any_of(plan.replacementRoots, [&](const fs::path& root) { return PathMatchesKey(root, destinationKey); });
		std::error_code filesystemError;
		if (fs::exists(plan.destination, filesystemError) && !ownsDestination)
		{
			errorMessage = std::format("Destination '{}' already belongs to another package", plan.destination.string());
			return false;
		}
	}

	const bool replacesInstalledPackage =
	    std::ranges::any_of(plans, [](const PackagePlan& plan) { return plan.oldRoot.has_value() || !plan.replacementRoots.empty(); });
	if (replacesInstalledPackage && !UnloadRuntimeForFilesystemMutation(errorMessage))
		return false;

	Transition(ModInstallOperationState::Committing, "#MWS_OPERATION_COMMITTING");
	std::vector<CommitRecord> records;
	records.reserve(plans.size());
	std::error_code filesystemError;
	fs::create_directories(plans.front().backupRoot.parent_path(), filesystemError);
	for (PackagePlan& plan : plans)
	{
		CommitRecord record{.plan = &plan};
		if (plan.oldRoot && fs::exists(*plan.oldRoot, filesystemError))
		{
			fs::rename(*plan.oldRoot, plan.backupRoot, filesystemError);
			if (filesystemError)
			{
				errorMessage = std::format("Failed backing up installed package: {}", filesystemError.message());
				records.push_back(record);
				break;
			}
			record.oldMoved = true;
		}
		bool replacementFailed = false;
		for (size_t replacementIndex = 0; replacementIndex < plan.replacementRoots.size(); ++replacementIndex)
		{
			const fs::path& replacementRoot = plan.replacementRoots[replacementIndex];
			if (plan.oldRoot && PathKey(*plan.oldRoot) == PathKey(replacementRoot))
				continue;

			filesystemError.clear();
			if (!fs::exists(replacementRoot, filesystemError) || filesystemError)
			{
				errorMessage = std::format("Existing replacement package '{}' is no longer available", replacementRoot.string());
				replacementFailed = true;
				break;
			}

			fs::path replacementBackup = plan.backupRoot;
			replacementBackup += ".replacement-" + std::to_string(replacementIndex);
			fs::rename(replacementRoot, replacementBackup, filesystemError);
			if (filesystemError)
			{
				errorMessage = std::format("Failed backing up replacement package: {}", filesystemError.message());
				replacementFailed = true;
				break;
			}
			record.replacementsMoved.emplace_back(replacementRoot, std::move(replacementBackup));
		}
		if (replacementFailed)
		{
			records.push_back(std::move(record));
			break;
		}
		fs::rename(plan.stagingRoot, plan.destination, filesystemError);
		if (filesystemError)
		{
			errorMessage = std::format("Failed committing staged package: {}", filesystemError.message());
			records.push_back(record);
			break;
		}
		record.newPlaced = true;
		records.push_back(record);
	}

	bool commitComplete = records.size() == plans.size() && std::ranges::all_of(records, [](const CommitRecord& record) { return record.newPlaced; });
	if (commitComplete)
	{
		Transition(ModInstallOperationState::Reloading, "#MWS_OPERATION_RELOADING");
		commitComplete = ReloadAndVerify(plans, enabledStates, errorMessage);
	}
	if (commitComplete)
	{
		for (const CommitRecord& record : records)
		{
			if (record.oldMoved)
				fs::remove_all(record.plan->backupRoot, filesystemError);
			for (const auto& [original, backup] : record.replacementsMoved)
			{
				NOTE_UNUSED(original);
				fs::remove_all(backup, filesystemError);
			}
		}
		return true;
	}

	bool rollbackSucceeded = true;
	for (auto record = records.rbegin(); record != records.rend(); ++record)
	{
		filesystemError.clear();
		if (record->newPlaced)
			fs::remove_all(record->plan->destination, filesystemError);
		if (filesystemError)
			rollbackSucceeded = false;
		filesystemError.clear();
		if (record->oldMoved)
			fs::rename(record->plan->backupRoot, *record->plan->oldRoot, filesystemError);
		if (filesystemError)
			rollbackSucceeded = false;
		for (auto replacement = record->replacementsMoved.rbegin(); replacement != record->replacementsMoved.rend(); ++replacement)
		{
			filesystemError.clear();
			fs::rename(replacement->second, replacement->first, filesystemError);
			if (filesystemError)
				rollbackSucceeded = false;
		}
	}
	if (rollbackSucceeded)
		rollbackSucceeded = ReloadAfterRollback(enabledStates, errorMessage);
	if (!rollbackSucceeded)
	{
		preserveRecoveryFiles = true;
		errorMessage += "; automatic rollback was incomplete; recovery files were preserved";
	}
	return false;
}

bool CModInstallService::ExecuteInstalledRemove(const InstallRequest& request, std::string& errorMessage, bool& preserveRecoveryFiles)
{
	if (request.installedRemovalRoot.empty() || !ValidateRemovalRoot(request.installedRemovalRoot))
	{
		errorMessage = "Installed mod removal root is missing or no longer safe";
		return false;
	}
	if (IsCancelled())
		return false;

	PackagePlan plan;
	plan.modId = request.modId;
	plan.name = request.installedRemovalName;
	plan.version = request.installedRemovalVersion;
	plan.oldRoot = request.installedRemovalRoot;
	plan.destination = request.installedRemovalRoot;
	const fs::path jobRoot =
	    fs::path(GetNorthstarPrefix()) / "cache" / "modworkshop" / "jobs" / std::format("{}-{}", GetCurrentProcessId(), request.generation);
	plan.backupRoot = jobRoot / "backups" / "removed-installed-mod";

	std::vector<PackagePlan> plans{plan};
	std::unordered_set<std::string> names;
	std::unordered_map<std::string, bool> enabledStates;
	if (!CaptureRuntimeState(plans, names, enabledStates, errorMessage))
		return false;

	std::error_code filesystemError;
	fs::remove_all(jobRoot, filesystemError);
	filesystemError.clear();
	fs::create_directories(plan.backupRoot.parent_path(), filesystemError);
	if (filesystemError)
	{
		errorMessage = std::format("Failed creating rollback storage: {}", filesystemError.message());
		return false;
	}
	if (!ValidateRemovalRoot(*plan.oldRoot))
	{
		errorMessage = "Installed mod removal root changed before removal";
		return false;
	}
	if (!UnloadRuntimeForFilesystemMutation(errorMessage))
		return false;
	Transition(ModInstallOperationState::Committing, "#MWS_OPERATION_REMOVING", plan.name, plan.version);
	fs::rename(*plan.oldRoot, plan.backupRoot, filesystemError);
	if (filesystemError)
	{
		errorMessage = std::format("Failed moving installed mod to rollback storage: {}", filesystemError.message());
		ReloadAfterRollback(enabledStates, errorMessage);
		return false;
	}

	Transition(ModInstallOperationState::Reloading, "#MWS_OPERATION_RELOADING_AFTER_REMOVAL");
	bool reloadResult = false;
	const fs::path removedRoot = *plan.oldRoot;
	const bool completed = RunOnMainThreadAndWait([removedRoot]
	{
		if (!g_pModManager)
			return false;
		g_pModManager->ReloadModsWithEnabledStates({});
		for (const Mod& mod : g_pModManager->m_LoadedMods)
		{
			fs::path modRoot;
			if (TryDeriveRemovalRoot(mod, modRoot) && PathKey(modRoot) == PathKey(removedRoot))
				return false;
		}
		return true;
	}, reloadResult);
	if (completed && reloadResult)
	{
		fs::remove_all(jobRoot, filesystemError);
		return true;
	}

	errorMessage = completed ? "Reloaded mods still contain the removed package" : "Timed out while reloading mods after removal";
	filesystemError.clear();
	fs::rename(plan.backupRoot, *plan.oldRoot, filesystemError);
	if (filesystemError)
	{
		preserveRecoveryFiles = true;
		errorMessage += std::format("; rollback move failed: {}; recovery files were preserved", filesystemError.message());
		return false;
	}
	const bool rollbackReloaded = ReloadAfterRollback(enabledStates, errorMessage);
	preserveRecoveryFiles = !rollbackReloaded;
	if (!preserveRecoveryFiles)
		fs::remove_all(jobRoot, filesystemError);
	return false;
}

bool CModInstallService::ExecuteRemove(const InstallRequest& request, std::string& errorMessage)
{
	CModWorkshopInventory::Get().RefreshLocal();
	const std::optional<ModWorkshopTrackedPackage> package = CModWorkshopInventory::Get().FindPackage(request.modId);
	if (!package)
	{
		errorMessage = "Tracked ModWorkshop package is not installed";
		return false;
	}
	if (IsCancelled())
		return false;

	PackagePlan plan;
	plan.modId = request.modId;
	plan.name = package->containedMods.empty() ? std::to_string(request.modId) : package->containedMods.front().name;
	plan.oldRoot = package->packageRoot;
	plan.destination = package->packageRoot;
	const fs::path jobRoot =
	    fs::path(GetNorthstarPrefix()) / "cache" / "modworkshop" / "jobs" / std::format("{}-{}", GetCurrentProcessId(), request.generation);
	plan.backupRoot = jobRoot / "backups" / "removed-package";

	std::vector<PackagePlan> plans{plan};
	std::unordered_set<std::string> names;
	std::unordered_map<std::string, bool> enabledStates;
	if (!CaptureRuntimeState(plans, names, enabledStates, errorMessage))
		return false;

	std::error_code filesystemError;
	fs::create_directories(plan.backupRoot.parent_path(), filesystemError);
	if (filesystemError)
	{
		errorMessage = std::format("Failed creating rollback storage: {}", filesystemError.message());
		return false;
	}
	if (!ValidateRemovalRoot(*plan.oldRoot))
	{
		errorMessage = "Tracked package removal root is no longer safe";
		return false;
	}
	if (!UnloadRuntimeForFilesystemMutation(errorMessage))
		return false;
	Transition(ModInstallOperationState::Committing, "#MWS_OPERATION_REMOVING", plan.name);
	fs::rename(*plan.oldRoot, plan.backupRoot, filesystemError);
	if (filesystemError)
	{
		errorMessage = std::format("Failed moving package to rollback storage: {}", filesystemError.message());
		ReloadAfterRollback(enabledStates, errorMessage);
		return false;
	}

	Transition(ModInstallOperationState::Reloading, "#MWS_OPERATION_RELOADING_AFTER_REMOVAL");
	bool reloadResult = false;
	const bool completed = RunOnMainThreadAndWait([]
	{
		if (!g_pModManager)
			return false;
		g_pModManager->ReloadModsWithEnabledStates({});
		return true;
	}, reloadResult);
	if (!completed || !reloadResult)
	{
		fs::rename(plan.backupRoot, *plan.oldRoot, filesystemError);
		ReloadAfterRollback(enabledStates, errorMessage);
		errorMessage = "Mod reload failed; removed package was restored";
		return false;
	}
	fs::remove_all(jobRoot, filesystemError);
	return true;
}

bool CModInstallService::ExecuteInstall(const InstallRequest& request, std::string& errorMessage, bool& preserveRecoveryFiles)
{
	CModWorkshopInventory::Get().RefreshLocal();
	Transition(ModInstallOperationState::FetchingDetails, "#MWS_OPERATION_FETCHING_DETAILS");
	std::unordered_set<std::string> installedModNames;
	std::unordered_map<std::string, bool> enabledStates;
	std::vector<PackagePlan> emptyPlans;
	if (!CaptureRuntimeState(emptyPlans, installedModNames, enabledStates, errorMessage))
		return false;

	Transition(ModInstallOperationState::ResolvingDependencies, "#MWS_OPERATION_RESOLVING_DEPENDENCIES");
	ModWorkshopRequestOptions options;
	options.isCancelled = [this] { return IsCancelled(); };
	std::vector<PackagePlan> plans;
	std::unordered_map<uint64_t, size_t> resolved;
	std::unordered_set<uint64_t> visiting;
	if (!ResolveModWorkshop(request.modId, true, request.action, request.expectedSelectedFileId, options, installedModNames, plans, resolved,
	                        visiting, errorMessage))
	{
		return false;
	}
	if (plans.empty())
	{
		errorMessage = "No installable package was resolved";
		return false;
	}
	const PackagePlan& rootPlan = plans.back();
	if (request.action == ModInstallAction::Update && rootPlan.oldRoot)
	{
		const auto installed = CModWorkshopInventory::Get().FindPackage(request.modId);
		if (installed && installed->installedState && installed->installedState->selectedFileId == rootPlan.selectedFileId)
		{
			Transition(ModInstallOperationState::Done, "#MWS_OPERATION_ALREADY_CURRENT", rootPlan.name, rootPlan.version);
			return true;
		}
	}

	if (!CaptureRuntimeState(plans, installedModNames, enabledStates, errorMessage))
		return false;
	const fs::path jobRoot =
	    fs::path(GetNorthstarPrefix()) / "cache" / "modworkshop" / "jobs" / std::format("{}-{}", GetCurrentProcessId(), request.generation);
	std::error_code filesystemError;
	fs::remove_all(jobRoot, filesystemError);
	fs::create_directories(jobRoot, filesystemError);
	if (filesystemError)
	{
		errorMessage = std::format("Failed creating install job directory: {}", filesystemError.message());
		return false;
	}

	uint64_t totalExpectedBytes = 0;
	for (const PackagePlan& plan : plans)
	{
		if (totalExpectedBytes <= std::numeric_limits<uint64_t>::max() - plan.expectedDownloadSize)
			totalExpectedBytes += plan.expectedDownloadSize;
	}
	uint64_t completedExpectedBytes = 0;
	for (size_t index = 0; index < plans.size(); ++index)
	{
		if (IsCancelled() || !StagePlan(plans[index], index, jobRoot, totalExpectedBytes, completedExpectedBytes, errorMessage))
		{
			fs::remove_all(jobRoot, filesystemError);
			return false;
		}
		completedExpectedBytes += plans[index].expectedDownloadSize;
	}

	if (!ResolveUnmanagedReplacements(request.generation, plans, enabledStates, errorMessage))
	{
		fs::remove_all(jobRoot, filesystemError);
		return false;
	}

	if (IsCancelled())
	{
		fs::remove_all(jobRoot, filesystemError);
		return false;
	}
	const bool committed = CommitPlans(plans, enabledStates, errorMessage, preserveRecoveryFiles);
	if (!preserveRecoveryFiles)
		fs::remove_all(jobRoot, filesystemError);
	return committed;
}

void CModInstallService::RunWorker()
{
	for (;;)
	{
		InstallRequest request;
		{
			std::unique_lock lock(m_RequestMutex);
			m_RequestChanged.wait(lock, [this] { return m_Stopped.load(std::memory_order_acquire) || m_PendingRequest.has_value(); });
			if (m_Stopped.load(std::memory_order_acquire))
				return;
			request = *m_PendingRequest;
			m_PendingRequest.reset();
		}

		std::string errorMessage;
		bool preserveRecoveryFiles = false;
		const bool success = !request.installedRemovalRoot.empty()        ? ExecuteInstalledRemove(request, errorMessage, preserveRecoveryFiles)
		                     : request.action == ModInstallAction::Remove ? ExecuteRemove(request, errorMessage)
		                                                                  : ExecuteInstall(request, errorMessage, preserveRecoveryFiles);
		const bool cancelled = m_CancelRequested.load(std::memory_order_acquire);
		const ModInstallOperationState currentState = GetSnapshot()->state;
		if (success && currentState != ModInstallOperationState::Done)
		{
			Transition(ModInstallOperationState::Done,
			           cancelled && (currentState == ModInstallOperationState::Committing || currentState == ModInstallOperationState::Reloading)
			               ? "#MWS_OPERATION_COMPLETED_AFTER_CANCELLATION"
			               : "#MWS_OPERATION_COMPLETED");
		}
		else if (!success)
		{
			Transition(cancelled && !preserveRecoveryFiles ? ModInstallOperationState::Cancelled : ModInstallOperationState::Failed,
			           errorMessage.empty() ? (cancelled ? "#MWS_OPERATION_CANCELLED" : "#MWS_OPERATION_FAILED") : std::move(errorMessage));
		}

		m_CancelRequested.store(false, std::memory_order_release);
		{
			std::scoped_lock lock(m_RequestMutex);
			m_Busy = false;
		}
	}
}

CModInstallService::CModInstallService() : m_Snapshot(std::make_shared<ModInstallOperationSnapshot>()), m_Worker([this] { RunWorker(); })
{
}

bool CModInstallService::Request(ModInstallAction action, uint64_t modId, uint64_t expectedSelectedFileId)
{
	if (modId == 0 || m_Stopped.load(std::memory_order_acquire))
		return false;
	uint64_t generation = 0;
	{
		std::scoped_lock lock(m_RequestMutex);
		if (m_Busy || m_PendingRequest)
			return false;
		m_Busy = true;
		generation = ++m_NextGeneration;
		m_PendingRequest = InstallRequest{
		    .generation = generation,
		    .modId = modId,
		    .action = action,
		    .expectedSelectedFileId = expectedSelectedFileId,
		};
	}
	m_CancelRequested.store(false, std::memory_order_release);
	auto queued = std::make_shared<ModInstallOperationSnapshot>();
	queued->generation = generation;
	queued->modId = modId;
	queued->action = action;
	queued->state = ModInstallOperationState::Queued;
	queued->message = "#MWS_OPERATION_QUEUED";
	Publish(std::move(queued));
	m_RequestChanged.notify_one();
	return true;
}

bool CModInstallService::RequestInstalledModRemoval(int modIndex)
{
	if (modIndex < 0 || m_Stopped.load(std::memory_order_acquire) || !ThreadInMainThread())
		return false;

	InstalledRemovalTarget target;
	uint64_t generation = 0;
	{
		std::scoped_lock lock(m_RequestMutex);
		if (m_Busy || m_PendingRequest || !InspectInstalledRemoval(modIndex, target) || !target.canDelete)
			return false;

		m_Busy = true;
		generation = ++m_NextGeneration;
		InstallRequest request;
		request.generation = generation;
		request.modId = target.managedModId;
		request.action = ModInstallAction::Remove;
		request.installedRemovalRoot = target.root;
		request.installedRemovalName = target.name;
		request.installedRemovalVersion = target.version;
		m_PendingRequest = std::move(request);
	}

	m_CancelRequested.store(false, std::memory_order_release);
	auto queued = std::make_shared<ModInstallOperationSnapshot>();
	queued->generation = generation;
	queued->modId = target.managedModId;
	queued->action = ModInstallAction::Remove;
	queued->state = ModInstallOperationState::Queued;
	queued->name = std::move(target.name);
	queued->version = std::move(target.version);
	queued->message = "#MWS_OPERATION_QUEUED";
	Publish(std::move(queued));
	m_RequestChanged.notify_one();
	return true;
}

InstalledModRemovalInfo CModInstallService::GetInstalledModRemovalInfo(int modIndex) const
{
	InstalledRemovalTarget target;
	if (!InspectInstalledRemoval(modIndex, target))
		return {};
	return {
	    .canDelete = target.canDelete,
	    .deleteModCount = target.deleteModCount,
	};
}

bool CModInstallService::DecideMigration(uint64_t generation, bool accept)
{
	{
		std::scoped_lock lock(m_MigrationMutex);
		if (m_MigrationGeneration != generation || m_MigrationDecision != MigrationDecision::Pending)
			return false;
		m_MigrationDecision = accept ? MigrationDecision::Accepted : MigrationDecision::Declined;
	}
	m_MigrationChanged.notify_all();
	return true;
}

void CModInstallService::Cancel()
{
	const std::shared_ptr<const ModInstallOperationSnapshot> current = GetSnapshot();
	if (!current || current->state == ModInstallOperationState::Idle || current->state == ModInstallOperationState::Done ||
	    current->state == ModInstallOperationState::Failed || current->state == ModInstallOperationState::Cancelled)
	{
		return;
	}
	m_CancelRequested.store(true, std::memory_order_release);
	m_MigrationChanged.notify_all();
	if (current->state == ModInstallOperationState::Committing || current->state == ModInstallOperationState::Reloading)
	{
		auto deferred = std::make_shared<ModInstallOperationSnapshot>(*current);
		deferred->cancellationDeferred = true;
		deferred->message = "#MWS_CANCELLATION_DEFERRED";
		Publish(std::move(deferred));
	}
}

void CModInstallService::SetOperationChangedCallback(OperationChangedCallback callback)
{
	std::scoped_lock lock(m_CallbackMutex);
	m_OperationChanged = std::move(callback);
}

void CModInstallService::Shutdown()
{
	if (m_Stopped.exchange(true, std::memory_order_acq_rel))
		return;
	m_CancelRequested.store(true, std::memory_order_release);
	m_MigrationChanged.notify_all();
	{
		std::scoped_lock lock(m_CallbackMutex);
		m_OperationChanged = {};
	}
	m_RequestChanged.notify_all();
	if (m_Worker.joinable() && m_Worker.get_id() != std::this_thread::get_id())
		m_Worker.join();
}
