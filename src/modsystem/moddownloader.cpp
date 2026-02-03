#include "moddownloader.h"
#include "engine/netmessages.h"
#include "util/utils.h"
#include "config/profile.h"
#include "engine/r2engine.h"
#include "core/tier0.h"
#include "modsystem/platform/modworkshop.h"
#include "modsystem/platform/modplatform.h"
#include "modsystem/platform/thunderstore.h"
#include "modsystem/modshellext.h"

#include <rapidjson/fwd.h>
#include <rapidjson/writer.h>
#include <rapidjson/error/en.h>
#include <mz_strm_mem.h>
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <compat/unzip.h>
#include <compat/zip.h>
#include <thread>
#include <future>
#include <bcrypt.h>
#include <winternl.h>
#include <fstream>
#include <cctype>

ConVar* Cvar_allow_mod_auto_download = nullptr;

ModDownloader* g_pModDownloader = nullptr;

ModDownloader::ModDownloader()
{
	spdlog::info("Mod downloader initialized");
	modState.state = NOT_FOUND;

	// Initialise mods list URI
	char* clachar = strstr(GetCommandLineA(), CUSTOM_MODS_URL_FLAG);
	if (clachar)
	{
		std::string url;
		size_t iFlagStringLength = strlen(CUSTOM_MODS_URL_FLAG);
		std::string cla = std::string(clachar);
		if (strncmp(cla.substr(iFlagStringLength, 1).c_str(), "\"", 1))
		{
			size_t space = cla.find(" ");
			url = cla.substr(iFlagStringLength, space - iFlagStringLength);
		}
		else
		{
			std::string quote = "\"";
			size_t quote1 = cla.find(quote);
			size_t quote2 = (cla.substr(quote1 + 1)).find(quote);
			url = cla.substr(quote1 + 1, quote2);
		}
		spdlog::info("Found custom verified mods URL in command line argument: {}", url);
		modsListUrl = strdup(url.c_str());
	}
	else
	{
		spdlog::info("Custom verified mods URL not found in command line arguments, using default URL.");
		modsListUrl = strdup(DEFAULT_MODS_LIST_URL);
	}

	if (auto pending = Mod_TakePendingWorkshopDownload())
		QueueWorkshopDownload(*pending);
}

size_t WriteToString(void* ptr, size_t size, size_t count, void* stream)
{
	((std::string*)stream)->append((char*)ptr, 0, size * count);
	return size * count;
}

void ModDownloader::FetchModsListFromAPI()
{
	modState.state = MANIFEST_FETCHING;

	std::thread requestThread(
		[this]()
		{
			CURLcode result;
			CURL* easyhandle;
			rapidjson::Document verifiedModsJson;
			std::string url = modsListUrl;

			// Empty verified mods manifesto
			verifiedMods = {};

			curl_global_init(CURL_GLOBAL_ALL);
			easyhandle = curl_easy_init();
			std::string readBuffer;

			// Fetching mods list from GitHub repository
			curl_easy_setopt(easyhandle, CURLOPT_CUSTOMREQUEST, "GET");
			curl_easy_setopt(easyhandle, CURLOPT_TIMEOUT, 30L);
			curl_easy_setopt(easyhandle, CURLOPT_URL, url.c_str());
			curl_easy_setopt(easyhandle, CURLOPT_FAILONERROR, 1L);
			curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &readBuffer);
			curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, WriteToString);
			result = curl_easy_perform(easyhandle);
			ScopeGuard cleanup(
				[&]
				{
					curl_easy_cleanup(easyhandle);
					modState.state = DONE;
				});

			if (result == CURLcode::CURLE_OK)
			{
				spdlog::info("Mods list successfully fetched.");
			}
			else
			{
				spdlog::error("Fetching mods list failed: {}", curl_easy_strerror(result));
				return;
			}

			// Load mods list into local state
			spdlog::info("Loading mods configuration...");
			verifiedModsJson.Parse(readBuffer);
			for (auto i = verifiedModsJson.MemberBegin(); i != verifiedModsJson.MemberEnd(); ++i)
			{
				// Format testing
				if (!i->value.HasMember("Repository") || !i->value.HasMember("Versions"))
				{
					spdlog::warn("Verified mods manifesto format is unrecognized, skipping loading.");
					return;
				}

				std::string name = i->name.GetString();
				std::unordered_map<std::string, VerifiedModVersion> modVersions;

				rapidjson::Value& versions = i->value["Versions"];
				assert(versions.IsArray());
				for (auto& attribute : versions.GetArray())
				{
					assert(attribute.IsObject());
					// Format testing
					if (!attribute.HasMember("Version") || !attribute.HasMember("Checksum") || !attribute.HasMember("DownloadLink") ||
						!attribute.HasMember("Platform"))
					{
						spdlog::warn("Verified mods manifesto format is unrecognized, skipping loading.");
						return;
					}

					std::string version = attribute["Version"].GetString();
					std::string checksum = attribute["Checksum"].GetString();
					std::string downloadLink = attribute["DownloadLink"].GetString();
					std::string platformValue = attribute["Platform"].GetString();
					ModSource platform = ResolvePlatform(platformValue);
					modVersions.insert({version, {.checksum = checksum, .downloadLink = downloadLink, .platform = platform}});
				}

				VerifiedModDetails modConfig = {.versions = modVersions};
				verifiedMods.insert({name, modConfig});
				spdlog::info("==> Loaded configuration for mod \"" + name + "\"");
			}

			spdlog::info("Done loading verified mods list.");
		});
	requestThread.detach();
}

size_t WriteData(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
	size_t written;
	written = fwrite(ptr, size, nmemb, stream);
	return written;
}

int ModDownloader::ModFetchingProgressCallback(
	void* ptr, curl_off_t totalDownloadSize, curl_off_t finishedDownloadSize, curl_off_t totalToUpload, curl_off_t nowUploaded)
{
	NOTE_UNUSED(totalToUpload);
	NOTE_UNUSED(nowUploaded);

	// Abort download
	ModDownloader* instance = static_cast<ModDownloader*>(ptr);
	if (instance->modState.state == ABORTED)
	{
		return 1;
	}

	if (totalDownloadSize != 0 && finishedDownloadSize != 0)
	{
		ModDownloader* instance = static_cast<ModDownloader*>(ptr);
		auto currentDownloadProgress = roundf(static_cast<float>(finishedDownloadSize) / totalDownloadSize * 100);
		instance->modState.progress = finishedDownloadSize;
		instance->modState.total = totalDownloadSize;
		instance->modState.ratio = currentDownloadProgress;
	}

	return 0;
}

std::string ModDownloader::GetModArchiveName(std::string url)
{
	std::string name = fs::path(url).filename().generic_string();
	std::string::size_type charIndex = name.find("?");

	// Thunderstore format
	if (std::string::npos == charIndex)
	{
		return name;
	}

	// ModWorkshop format (removing the "?filename=" part)
	return name.substr(charIndex + 10);
}

std::string ModDownloader::SanitizeFolderComponent(std::string value)
{
	for (char& c : value)
	{
		if (c == ' ' || c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
			c = '_';
		else if (std::iscntrl(static_cast<unsigned char>(c)))
			c = '_';
	}

	if (value.empty())
		value = "Unknown";

	return value;
}

bool ModDownloader::BuildThunderstoreDownload(
	const std::string& dependencyName,
	const std::string& dependencyUrl,
	PendingModDownload& outDownload)
{
	std::string namespaceName;
	std::string packageName;
	if (!Thunderstore_ParsePackageUrl(dependencyUrl, namespaceName, packageName))
	{
		spdlog::error("Unsupported offsite dependency URL '{}', aborting install.", dependencyUrl);
		return false;
	}

	ThunderstorePackageDetails tsDetails;
	if (!Thunderstore_FetchPackageDetails(namespaceName, packageName, tsDetails))
	{
		spdlog::error("Failed fetching Thunderstore details for dependency '{}' ({}), aborting install.", dependencyName, dependencyUrl);
		return false;
	}

	std::string depName = !tsDetails.name.empty() ? tsDetails.name : dependencyName;
	if (depName.empty() || tsDetails.version.empty() || tsDetails.downloadUrl.empty())
	{
		spdlog::error("Thunderstore dependency details missing fields for '{}', aborting install.", dependencyUrl);
		return false;
	}

	std::string folderName = SanitizeFolderComponent(tsDetails.namespaceName) + "-" +
		SanitizeFolderComponent(depName) + "-" +
		SanitizeFolderComponent(tsDetails.version);

	VerifiedModVersion depVersion = {};
	depVersion.platform = ModSource::Thunderstore;
	depVersion.checksum = "";
	depVersion.downloadLink = tsDetails.downloadUrl;

	outDownload = {
		.name = depName,
		.version = tsDetails.version,
		.versionInfo = depVersion,
		.destinationDir = GetPackageFolderPath() / folderName,
		.managedId = tsDetails.namespaceName + " " + depName};

	return true;
}

std::tuple<fs::path, bool> ModDownloader::FetchModFromDistantStore(std::string_view modName, VerifiedModVersion version)
{
	std::string url = version.downloadLink;

	// Download destination
	spdlog::info(std::format("Fetching mod archive from {}", url));
	std::string archiveName = ModDownloader::GetModArchiveName(url);
	std::filesystem::path downloadPath = std::filesystem::temp_directory_path() / archiveName;
	spdlog::info(std::format("Downloading archive to {}", downloadPath.generic_string()));

	// Update state
	modState.state = DOWNLOADING;

	// Download the actual archive
	bool success = false;
	FILE* fp = fopen(downloadPath.generic_string().c_str(), "wb");
	if (!fp)
	{
		spdlog::error("Failed opening destination file for download: {}", downloadPath.generic_string());
		modState.state = FAILED_WRITING_TO_DISK;
		return {downloadPath, false};
	}
	CURLcode result;
	CURL* easyhandle;
	easyhandle = curl_easy_init();
	if (!easyhandle)
	{
		spdlog::error("Failed initializing curl for download.");
		fclose(fp);
		modState.state = MOD_FETCHING_FAILED;
		return {downloadPath, false};
	}

	curl_easy_setopt(easyhandle, CURLOPT_URL, url.data());
	curl_easy_setopt(easyhandle, CURLOPT_FAILONERROR, 1L);

	// abort if slower than 30 bytes/sec during 10 seconds
	curl_easy_setopt(easyhandle, CURLOPT_LOW_SPEED_TIME, 10L);
	curl_easy_setopt(easyhandle, CURLOPT_LOW_SPEED_LIMIT, 30L);

	curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, fp);
	curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, WriteData);
	curl_easy_setopt(easyhandle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(easyhandle, CURLOPT_XFERINFOFUNCTION, ModDownloader::ModFetchingProgressCallback);
	curl_easy_setopt(easyhandle, CURLOPT_XFERINFODATA, this);
	result = curl_easy_perform(easyhandle);
	ScopeGuard cleanup(
		[&]
		{
			if (easyhandle)
				curl_easy_cleanup(easyhandle);
			if (fp)
				fclose(fp);
		});

	if (result == CURLcode::CURLE_OK)
	{
		spdlog::info("Mod archive successfully fetched.");
		success = true;
	}
	else
	{
		spdlog::error("Fetching mod archive failed: {}", curl_easy_strerror(result));
	}

	return {downloadPath, success};
}

bool ModDownloader::IsModLegit(fs::path modPath, std::string_view expectedChecksum)
{
	if (strstr(GetCommandLineA(), VERIFICATION_FLAG))
	{
		spdlog::info("Bypassing mod verification due to flag set up.");
		return true;
	}

	if( expectedChecksum.empty() )
	{
		spdlog::warn("No expected checksum provided, skipping verification.");
		return true;
	}

	// Update state
	modState.state = CHECKSUMING;

	NTSTATUS status;
	BCRYPT_ALG_HANDLE algorithmHandle = NULL;
	BCRYPT_HASH_HANDLE hashHandle = NULL;
	std::vector<uint8_t> hash;
	DWORD hashLength = 0;
	DWORD resultLength = 0;
	std::stringstream ss;

	constexpr size_t bufferSize {1 << 12};
	std::vector<char> buffer(bufferSize, '\0');
	std::ifstream fp(modPath.generic_string(), std::ios::binary);

	ScopeGuard cleanup(
		[&]
		{
			if (NULL != hashHandle)
			{
				BCryptDestroyHash(hashHandle); // Handle to hash/MAC object which needs to be destroyed
			}

			if (NULL != algorithmHandle)
			{
				BCryptCloseAlgorithmProvider(
					algorithmHandle, // Handle to the algorithm provider which needs to be closed
					0); // Flags
			}
		});

	// Open an algorithm handle
	// This sample passes BCRYPT_HASH_REUSABLE_FLAG with BCryptAlgorithmProvider(...) to load a provider which supports reusable hash
	status = BCryptOpenAlgorithmProvider(
		&algorithmHandle, // Alg Handle pointer
		BCRYPT_SHA256_ALGORITHM, // Cryptographic Algorithm name (null terminated unicode string)
		NULL, // Provider name; if null, the default provider is loaded
		BCRYPT_HASH_REUSABLE_FLAG); // Flags; Loads a provider which supports reusable hash
	if (!NT_SUCCESS(status))
	{
		modState.state = MOD_CORRUPTED;
		return false;
	}

	// Obtain the length of the hash
	status = BCryptGetProperty(
		algorithmHandle, // Handle to a CNG object
		BCRYPT_HASH_LENGTH, // Property name (null terminated unicode string)
		(PBYTE)&hashLength, // Address of the output buffer which recieves the property value
		sizeof(hashLength), // Size of the buffer in bytes
		&resultLength, // Number of bytes that were copied into the buffer
		0); // Flags
	if (!NT_SUCCESS(status))
	{
		modState.state = MOD_CORRUPTED;
		return false;
	}

	// Create a hash handle
	status = BCryptCreateHash(
		algorithmHandle, // Handle to an algorithm provider
		&hashHandle, // A pointer to a hash handle - can be a hash or hmac object
		NULL, // Pointer to the buffer that recieves the hash/hmac object
		0, // Size of the buffer in bytes
		NULL, // A pointer to a key to use for the hash or MAC
		0, // Size of the key in bytes
		0); // Flags
	if (!NT_SUCCESS(status))
	{
		modState.state = MOD_CORRUPTED;
		return false;
	}

	// Hash archive content
	if (!fp.is_open())
	{
		spdlog::error("Unable to open archive.");
		modState.state = FAILED_READING_ARCHIVE;
		return false;
	}
	fp.seekg(0, fp.beg);
	while (fp.good())
	{
		fp.read(buffer.data(), bufferSize);
		std::streamsize bytesRead = fp.gcount();
		if (bytesRead > 0)
		{
			status = BCryptHashData(hashHandle, (PBYTE)buffer.data(), bytesRead, 0);
			if (!NT_SUCCESS(status))
			{
				modState.state = MOD_CORRUPTED;
				return false;
			}
		}
	}

	hash = std::vector<uint8_t>(hashLength);

	// Obtain the hash of the message(s) into the hash buffer
	status = BCryptFinishHash(
		hashHandle, // Handle to the hash or MAC object
		hash.data(), // A pointer to a buffer that receives the hash or MAC value
		hashLength, // Size of the buffer in bytes
		0); // Flags
	if (!NT_SUCCESS(status))
	{
		modState.state = MOD_CORRUPTED;
		return false;
	}

	// Convert hash to string using bytes raw values
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < hashLength; i++)
    {
        ss << std::hex << std::setw(2) << static_cast<int>(hash.data()[i]);
    }

    std::string computedHash = ss.str();

    spdlog::info("Expected checksum: {}", expectedChecksum);
    spdlog::info("Computed checksum: {}", computedHash);

    return std::string(expectedChecksum) == computedHash;
}

bool ModDownloader::IsModAuthorized(std::string_view modName, std::string_view modVersion)
{
	if (strstr(GetCommandLineA(), VERIFICATION_FLAG))
	{
		spdlog::info("Bypassing mod verification due to flag set up.");
		return true;
	}

	if (!verifiedMods.contains(modName.data()))
	{
		return false;
	}

	std::unordered_map<std::string, VerifiedModVersion> versions = verifiedMods[modName.data()].versions;
	return versions.count(modVersion.data()) != 0;
}

int GetModArchiveSize(unzFile file, unz_global_info64 info)
{
	int totalSize = 0;

	for (int i = 0; i < info.number_entry; i++)
	{
		char zipFilename[256];
		unz_file_info64 fileInfo;
		unzGetCurrentFileInfo64(file, &fileInfo, zipFilename, sizeof(zipFilename), NULL, 0, NULL, 0);

		totalSize += fileInfo.uncompressed_size;

		if ((i + 1) < info.number_entry)
		{
			unzGoToNextFile(file);
		}
	}

	// Reset file pointer for archive extraction
	unzGoToFirstFile(file);

	return totalSize;
}

void ModDownloader::ExtractMod(fs::path modPath, fs::path destinationPath, ModSource platform)
{
	unzFile file;

	file = unzOpen(modPath.generic_string().c_str());
	ScopeGuard cleanup(
		[&]
		{
			if (unzClose(file) != MZ_OK)
			{
				spdlog::error("Failed closing mod archive after extraction.");
			}
		});

	if (file == NULL)
	{
		spdlog::error("Cannot open archive located at {}.", modPath.generic_string());
		modState.state = FAILED_READING_ARCHIVE;
		return;
	}

	unz_global_info64 gi;
	int status;
	status = unzGetGlobalInfo64(file, &gi);
	if (status != UNZ_OK)
	{
		spdlog::error("Failed getting information from archive (error code: {})", status);
		modState.state = FAILED_READING_ARCHIVE;
		return;
	}

	// Update state
	modState.state = EXTRACTING;
	modState.total = GetModArchiveSize(file, gi);
	modState.progress = 0;

	// extracts the file in the archive at zipFilename to fileDestination on disk
	auto extractFile = [&](fs::path fileDestination, char* zipFilename) -> bool
	{
		std::error_code ec;
		spdlog::info("=> {}", fileDestination.generic_string());

		// Create parent directory if needed
		if (!std::filesystem::exists(fileDestination.parent_path()))
		{
			spdlog::info("Parent directory does not exist, creating it.", fileDestination.generic_string());
			if (!std::filesystem::create_directories(fileDestination.parent_path(), ec) && ec.value() != 0)
			{
				spdlog::error("Parent directory ({}) creation failed.", fileDestination.parent_path().generic_string());
				modState.state = FAILED_WRITING_TO_DISK;
				return false;
			}
		}

		// If current file is a directory, create directory...
		if (fileDestination.generic_string().back() == '/')
		{
			// Create directory
			if (!std::filesystem::create_directory(fileDestination, ec) && ec.value() != 0)
			{
				spdlog::error("Directory creation failed: {}", ec.message());
				modState.state = FAILED_WRITING_TO_DISK;
				return false;
			}
		}
		// ...else create file
		else
		{
			// Ensure file is in zip archive
			if (unzLocateFile(file, zipFilename, 0) != UNZ_OK)
			{
				spdlog::error("File \"{}\" was not found in archive.", zipFilename);
				modState.state = FAILED_READING_ARCHIVE;
				return false;
			}

			// Create file
			const int bufferSize = 8192;
			void* buffer;
			int err = UNZ_OK;
			FILE* fout = NULL;

			// Open zip file to prepare its extraction
			status = unzOpenCurrentFile(file);
			if (status != UNZ_OK)
			{
				spdlog::error("Could not open file {} from archive.", zipFilename);
				modState.state = FAILED_READING_ARCHIVE;
				return false;
			}

			// Create destination file
			fout = fopen(fileDestination.generic_string().c_str(), "wb");
			if (fout == NULL)
			{
				spdlog::error("Failed creating destination file.");
				modState.state = FAILED_WRITING_TO_DISK;
				return false;
			}

			// Allocate memory for buffer
			buffer = (void*)malloc(bufferSize);
			if (buffer == NULL)
			{
				spdlog::error("Error while allocating memory.");
				modState.state = FAILED_WRITING_TO_DISK;
				return false;
			}

			// Extract file to destination
			do
			{
				err = unzReadCurrentFile(file, buffer, bufferSize);
				if (err < 0)
				{
					spdlog::error("error {} with zipfile in unzReadCurrentFile", err);
					break;
				}
				if (err > 0)
				{
					if (fwrite(buffer, (unsigned)err, 1, fout) != 1)
					{
						spdlog::error("error in writing extracted file\n");
						err = UNZ_ERRNO;
						break;
					}
				}

				// Update extraction stats
				modState.progress += bufferSize;
				modState.ratio = roundf(static_cast<float>(modState.progress) / modState.total * 100);
			} while (err > 0);

			if (err != UNZ_OK)
			{
				spdlog::error("An error occurred during file extraction (code: {})", err);
				modState.state = FAILED_WRITING_TO_DISK;
				return false;
			}
			err = unzCloseCurrentFile(file);
			if (err != UNZ_OK)
			{
				spdlog::error("error {} with zipfile in unzCloseCurrentFile", err);
			}

			// Cleanup
			if (fout)
				fclose(fout);
		}

		return true;
	};

	// the folder that contains the mod.json. all other files are considered relative to this
	fs::path rootDir = "";

	// We don't know how to extract mods from unknown platforms
	if (platform == ModSource::Unknown)
	{
		spdlog::error("Failed extracting mod from unknown platform.");
		modState.state = UNKNOWN_PLATFORM;
		return;
	}
	else if (platform == ModSource::ModWorkshop)
	{
		if (auto foundRootDir = ModWorkshop_FindRootDir(file, gi))
			rootDir = *foundRootDir;
	}

	unzGoToFirstFile(file);
	for (uint64_t i = 0; i < gi.number_entry; i++)
	{
		char zipFilename[256];
		unz_file_info64 fileInfo;
		status = unzGetCurrentFileInfo64(file, &fileInfo, zipFilename, sizeof(zipFilename), NULL, 0, NULL, 0);

		// Get the destination path, correcting for rootDir
		fs::path zipFilePath = zipFilename;
		fs::path relativePath = zipFilePath.lexically_relative(rootDir);
		// don't try to do anything with our root directory
		if (zipFilePath.compare(rootDir))
		{
			fs::path fileDestination = destinationPath / relativePath;

			// Extract file
			if (!extractFile(fileDestination, zipFilename))
				return;

			// Abort mod extraction if needed
			if (modState.state == ABORTED)
			{
				spdlog::info("User cancelled mod installation, aborting mod extraction.");
				return;
			}
		}

		// Go to next file
		if ((i + 1) < gi.number_entry)
		{
			status = unzGoToNextFile(file);
			if (status != UNZ_OK)
			{
				spdlog::error("Error while browsing archive files (error code: {}).", status);
				return;
			}
		}
	}

	// Mod extraction went fine
	modState.state = DONE;
}

void ModDownloader::DownloadMod(std::string modName, std::string modVersion)
{
	if (IsDownloadInProgress())
	{
		spdlog::warn("Download already in progress, ignoring request for {} {}", modName, modVersion);
		return;
	}

	// Check if mod can be auto-downloaded
	if (!IsModAuthorized(std::string_view(modName), std::string_view(modVersion)))
	{
		spdlog::warn("Tried to download a mod that is not verified, aborting.");
		modState.state = ABORTED;
		return;
	}

	// Remove old versions of this mod before downloading the new version
	for (auto& mod : g_pModManager->m_LoadedMods)
	{
		if (mod.Name == modName && mod.Version != modVersion && mod.IsRemote())
		{
			spdlog::info("Removing old version {} of mod {} before downloading version {}", mod.Version, modName, modVersion);
			g_pModManager->DeleteRemoteMod(mod.Name.c_str(), mod.Version.c_str());
		}
	}

	VerifiedModVersion fullVersion = verifiedMods[modName].versions[modVersion];
	StartDownloadThread(modName, modVersion, fullVersion, std::nullopt, std::nullopt, {});
}

bool ModDownloader::DownloadModInternal(const PendingModDownload& download)
{
	modState.state = DOWNLOADING;
	modState.name = download.name;
	modState.version = download.version;
	modState.progress = 0;
	modState.total = 0;
	modState.ratio = 0.0f;

	std::string name;
	fs::path archiveLocation;
	fs::path modDirectory;

	ScopeGuard cleanup(
		[&]
		{
			// Remove downloaded archive
			try
			{
				remove(archiveLocation);
			}
			catch (const std::exception& a)
			{
				spdlog::error("Error while removing downloaded archive: {}", a.what());
			}

			// Remove mod if auto-download process failed
			if (modState.state != DONE)
			{
				try
				{
					remove_all(modDirectory);
				}
				catch (const std::exception& e)
				{
					spdlog::error("Error while removing downloaded mod: {}", e.what());
				}
			}
		});

	const std::tuple<fs::path, bool> downloadResult = FetchModFromDistantStore(std::string_view(download.name), download.versionInfo);
	archiveLocation = get<0>(downloadResult);
	bool downloadSuccessful = get<1>(downloadResult);

	if (!downloadSuccessful)
	{
		spdlog::error("Something went wrong while fetching archive, aborting.");
		if (modState.state != ABORTED)
			modState.state = MOD_FETCHING_FAILED;
		return false;
	}

	if (!IsModLegit(archiveLocation, std::string_view(download.versionInfo.checksum)))
	{
		spdlog::warn("Archive hash does not match expected checksum, aborting.");
		modState.state = MOD_CORRUPTED;
		return false;
	}

	if (download.destinationDir)
	{
		modDirectory = *download.destinationDir;
	}
	else
	{
		name = archiveLocation.filename().string();
		name = name.substr(0, name.length() - 4);
		modDirectory = GetRemoteModFolderPath() / name;
	}

	ExtractMod(archiveLocation, modDirectory, download.versionInfo.platform);

	if (download.managedId && modState.state == DONE)
		Mod_WriteManagedMarker(modDirectory, download.versionInfo.platform, *download.managedId);

	return modState.state == DONE;
}

bool ModDownloader::IsModInstalled(std::string_view modName) const
{
	if (!g_pModManager)
		return false;

	for (const auto& mod : g_pModManager->m_LoadedMods)
	{
		if (mod.Name == modName)
			return true;
	}

	return false;
}

bool ModDownloader::StartDownloadThread(
	std::string modName,
	std::string modVersion,
	const VerifiedModVersion& version,
	const std::optional<fs::path>& destinationDir,
	const std::optional<std::string>& managedId,
	std::vector<PendingModDownload> dependencies)
{
	bool expected = false;
	if (!m_bDownloadThreadRunning.compare_exchange_strong(expected, true))
	{
		spdlog::warn("Download thread already running, skipping start for {} {}", modName, modVersion);
		return false;
	}

	NotifyDownloadStarted();

	std::thread requestThread(
		[this, modName, modVersion, version, destinationDir, managedId, dependencies = std::move(dependencies)]() mutable
		{
			ScopeGuard cleanup(
				[&]
				{
					m_bDownloadThreadRunning = false;
					NotifyDownloadStopped();
				});

			for (const auto& dependency : dependencies)
			{
				if (!DownloadModInternal(dependency))
				{
					if (modState.state == ABORTED)
						return;
					spdlog::warn("Dependency download failed for {} v{}, continuing with remaining downloads.", dependency.name, dependency.version);
				}
			}

			PendingModDownload mainDownload = {
				.name = modName,
				.version = modVersion,
				.versionInfo = version,
				.destinationDir = destinationDir,
				.managedId = managedId};
			DownloadModInternal(mainDownload);
		});

	requestThread.detach();
	return true;
}

void ModDownloader::QueueWorkshopDownload(std::string id)
{
	if (id.empty())
		return;

	m_PendingWorkshopId = std::move(id);

	if (!m_bDownloadReady)
	{
		spdlog::info("Mod download readiness not set; enabling to handle ModWorkshop install.");
		SetDownloadReady(true);
		return;
	}

	if (m_bDownloadReady && !IsDownloadInProgress())
		StartPendingWorkshopDownload();
}

void ModDownloader::SetDownloadReady(bool ready)
{
	m_bDownloadReady = ready;
	if (m_bDownloadReady && !IsDownloadInProgress())
		StartPendingWorkshopDownload();
}

bool ModDownloader::StartPendingWorkshopDownload()
{
	if (IsDownloadInProgress())
		return false;

	if (!m_bDownloadReady)
		return false;

	if (!m_PendingWorkshopId)
		return false;

	std::string id = std::move(*m_PendingWorkshopId);
	m_PendingWorkshopId.reset();
	NotifyDownloadStarted();
	modState.state = CHECKING_DETAILS;
	modState.name = "";
	modState.version = "";
	modState.progress = 0;
	modState.total = 0;
	modState.ratio = 0.0f;
	bool startedDownloadThread = false;
	ScopeGuard callbackCleanup(
		[&]
		{
			if (!startedDownloadThread)
				NotifyDownloadStopped();
		});

	ModWorkshopDetails details;
	if (!ModWorkshop_FetchDetails(id, details))
	{
		spdlog::error("Failed to fetch ModWorkshop details for id {}", id);
		modState.state = MOD_FETCHING_FAILED;
		return false;
	}

	if (details.version.empty() || details.name.empty() || details.downloadUrl.empty())
	{
		modState.state = MOD_FETCHING_FAILED;
		return false;
	}

	modState.name = details.name;
	modState.version = details.version;

	std::vector<PendingModDownload> dependencyDownloads;
	bool hasInvalidDependency = false;
	for (const auto& dependency : details.dependencies)
	{
		if (dependency.optional)
			continue;

		if (!dependency.name.empty() && IsModInstalled(dependency.name))
			continue;

		if (dependency.offsite)
		{
			modState.state = CHECKING_DETAILS;
			modState.name = !dependency.name.empty() ? dependency.name : dependency.url;
			modState.version.clear();
			modState.progress = 0;
			modState.total = 0;
			modState.ratio = 0.0f;

			PendingModDownload depDownload;
			if (!BuildThunderstoreDownload(dependency.name, dependency.url, depDownload))
			{
				hasInvalidDependency = true;
				break;
			}

			modState.name = depDownload.name;
			modState.version = depDownload.version;
			dependencyDownloads.push_back(std::move(depDownload));
		}
		else
		{
			if (!dependency.modId)
			{
				spdlog::error("Dependency '{}' is missing a ModWorkshop id, aborting install.", dependency.name);
				hasInvalidDependency = true;
				break;
			}

			modState.state = CHECKING_DETAILS;
			modState.name = !dependency.name.empty() ? dependency.name : *dependency.modId;
			modState.version.clear();
			modState.progress = 0;
			modState.total = 0;
			modState.ratio = 0.0f;

			ModWorkshopDetails depDetails;
			if (!ModWorkshop_FetchDetails(*dependency.modId, depDetails))
			{
				spdlog::error("Failed fetching ModWorkshop details for dependency id {}, aborting install.", *dependency.modId);
				hasInvalidDependency = true;
				break;
			}

			if (depDetails.version.empty() || depDetails.name.empty() || depDetails.downloadUrl.empty())
			{
				spdlog::error("Dependency details missing fields for id {}, aborting install.", *dependency.modId);
				hasInvalidDependency = true;
				break;
			}

			modState.name = depDetails.name;
			modState.version = depDetails.version;

			std::string folderName = SanitizeFolderComponent(depDetails.author) + "-" +
				SanitizeFolderComponent(depDetails.name) + "-" +
				SanitizeFolderComponent(depDetails.version);

			VerifiedModVersion depVersion = {};
			depVersion.platform = ModSource::ModWorkshop;
			depVersion.checksum = "";
			depVersion.downloadLink = depDetails.downloadUrl;

			dependencyDownloads.push_back({
				.name = depDetails.name,
				.version = depDetails.version,
				.versionInfo = depVersion,
				.destinationDir = GetPackageFolderPath() / folderName,
				.managedId = *dependency.modId});
		}
	}

	if (hasInvalidDependency)
	{
		modState.state = INVALID_DEPENDENCY;
		return false;
	}

	std::string folderName = SanitizeFolderComponent(details.author) + "-" +
		SanitizeFolderComponent(details.name) + "-" +
		SanitizeFolderComponent(details.version);

	VerifiedModVersion version = {};
	version.platform = ModSource::ModWorkshop;
	version.checksum = "";
	version.downloadLink = details.downloadUrl;

	startedDownloadThread = true;
	startedDownloadThread = StartDownloadThread(details.name, details.version, version, GetPackageFolderPath() / folderName, id, std::move(dependencyDownloads));
	return startedDownloadThread;
}

void ModDownloader::CancelDownload()
{
	modState.state = ABORTED;
}

void ModDownloader::LoadServerModSchema()
{
	fs::path path = GetModFolderPath() / "servermodschema.json";
	if (!fs::exists(path))
	{
		spdlog::warn("Server mod schema file not found at {}, skipping loading", path.generic_string());
		return;
	}

	std::ifstream fileStream(path);
	if (!fileStream.is_open())
	{
		spdlog::error("Failed opening server mod schema file at {}, skipping loading", path.generic_string());
		return;
	}

	std::string fileContent((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
	m_Document.Parse(fileContent);

	if (m_Document.HasParseError())
	{
		spdlog::error(
			"Error parsing server mod schema file at {}: {} (offset {})",
			path.generic_string(),
			rapidjson::GetParseError_En(m_Document.GetParseError()),
			m_Document.GetErrorOffset());
		return;
	}

	spdlog::info("Successfully loaded server mod schema from {}", path.generic_string());

	ParseSchemaDocument();
}

void ModDownloader::ParseSchemaDocument()
{
	m_ParsedSchemaMods.clear();

	if (!m_Document.IsObject())
		return;

	for (auto it = m_Document.MemberBegin(); it != m_Document.MemberEnd(); ++it)
	{
		modentry_s modEntry;

		modEntry.name = it->name.GetString();
		if (!it->value.IsObject())
		{
			spdlog::error("Mod entry {} is not an object, skipping.", modEntry.name);
			continue;
		}

		if (!it->value.HasMember("Version") || !it->value["Version"].IsString())
		{
			spdlog::error("Mod entry {} does not have a valid Version field, skipping.", modEntry.name);
			continue;
		}

		modEntry.version = it->value["Version"].GetString();

		ModSource platform = ModSource::Unknown;

		if(it->value.HasMember("Platform") && it->value["Platform"].IsString())
		{
			std::string platformValue = it->value["Platform"].GetString();
			platform = ResolvePlatform(platformValue);
		}

		modEntry.platform = platform;

		switch(platform)
		{
			case ModSource::Thunderstore:
				if(!it->value.HasMember("DependencyString") || !it->value["DependencyString"].IsString())
				{
					spdlog::error("Mod entry {} does not have a valid DependencyString field for Thunderstore platform, skipping.", modEntry.name);
					continue;
				}
				modEntry.dependencyString = it->value["DependencyString"].GetString();
				break;
			case ModSource::ModWorkshop:
				spdlog::error("ModWorkshop platform is not supported in server mod schema, skipping mod {}", modEntry.name);
				continue;
				break;
			case ModSource::Unknown:
				if(!it->value.HasMember("URL") || !it->value["URL"].IsString())
				{
					spdlog::error("Mod entry {} does not have a valid URL field for Unknown platform, skipping.", modEntry.name);
					continue;
				}
				modEntry.url = it->value["URL"].GetString();
				break;
			default:
				continue;
		}

		if (!it->value.HasMember("Checksum") || !it->value["Checksum"].IsString())
		{
			spdlog::warn("Mod entry {} does not have a valid Checksum field, you should consider adding this.", modEntry.name);
			modEntry.checksum = "";
		} else
			modEntry.checksum = it->value["Checksum"].GetString();

		m_ParsedSchemaMods.push_back(modEntry);
	}
}

bool ModDownloader::SendModInfoConnectionlessPacket(netadr_t& adr, modentry_s& mod, int index, int totalMods)
{
	char buffer[512];
	bf_write msg(buffer, sizeof(buffer));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(S2C_MODDOWNLOADINFO);
	msg.WriteLong(MODDOWNLOADINFO_VERSION);

	msg.WriteLong(index);
	msg.WriteLong(totalMods);

	if(!msg.WriteString(mod.name.c_str()))
		return false;
	if(!msg.WriteString(mod.version.c_str()))
		return false;

		switch(mod.platform)
	{
			case ModSource::Thunderstore:
			msg.WriteChar('T');
			if(!msg.WriteString(mod.dependencyString.c_str()))
				return false;
			break;
			case ModSource::ModWorkshop:
			msg.WriteChar('M');
			break;
			case ModSource::Unknown:
			msg.WriteChar('U');
			if(!msg.WriteString(mod.url.c_str()))
				return false;
			break;
	}

	if(mod.checksum.empty())
		msg.WriteByte(0);
	else
		msg.WriteByte(1);

	if(!mod.checksum.empty())
		if(!msg.WriteString(mod.checksum.c_str()))
			return false;

	NET_SendPacket(nullptr, NS_SERVER, &adr, msg.GetData(), msg.GetNumBytesWritten(), nullptr, false, 0, true);

	return true;
}

bool ModDownloader::RecvModInfoConnectionlessPacket(bf_read& msg)
{
	if(!g_pModDownloader->AllowingServerModDownloads())
		return false;

	int protocolVersion = msg.ReadLong();

	int modIndex = msg.ReadLong();
	int totalMods = msg.ReadLong();

	if(g_pModDownloader->m_iTotalServerRequestedMods != totalMods)
		g_pModDownloader->m_iTotalServerRequestedMods = totalMods;

	modentry_s modEntry;

	char modName[128];
	if(!msg.ReadString(modName, sizeof(modName)))
		return false;

	modEntry.name = std::string(modName);

	char modVersion[64];
	if(!msg.ReadString(modVersion, sizeof(modVersion)))
		return false;

	modEntry.version = std::string(modVersion);

	char platformChar = msg.ReadChar();

	char dependencyOrUrl[256];

	switch(platformChar)
	{
		case 'T':
			modEntry.platform = ModSource::Thunderstore;
			if(!msg.ReadString(dependencyOrUrl, sizeof(dependencyOrUrl)))
				return false;
			modEntry.dependencyString = std::string(dependencyOrUrl);
			break;
		case 'M':
			modEntry.platform = ModSource::ModWorkshop;
			spdlog::error("Received ModWorkshop platform mod info from server, which is unsupported, skipping mod.");
			return false;
		case 'U':
			modEntry.platform = ModSource::Unknown;
			if(!msg.ReadString(dependencyOrUrl, sizeof(dependencyOrUrl)))
				return false;
			modEntry.url = std::string(dependencyOrUrl);
			break;
		default:
			spdlog::warn("Received mod info packet with unrecognized platform char {}, skipping mod.", platformChar);
			return false;
	}

	bool hasChecksum = msg.ReadByte();
	if(hasChecksum)
	{
		char checksum[128];
		if(!msg.ReadString(checksum, sizeof(checksum)))
			return false;
		modEntry.checksum = std::string(checksum);
	}
	else
		modEntry.checksum = "";

	g_pModDownloader->m_ServerRequestedMods.push_back(modEntry);

	spdlog::info("{}/{} {} v{} [{}] ({} / {})", modIndex + 1, totalMods, modEntry.name, modEntry.version, GetPlatformString(modEntry.platform),
		dependencyOrUrl, modEntry.checksum.empty() ? "no checksum" : modEntry.checksum.c_str());


	if( g_pModDownloader->verifiedMods.contains( modEntry.name ) )
	{
		if( !g_pModDownloader->verifiedMods[ modEntry.name ].versions.contains( modEntry.version ) )
		{
			VerifiedModVersion versionInfo;
			versionInfo.checksum = modEntry.checksum;
			versionInfo.platform = modEntry.platform;

			switch( modEntry.platform )
			{
				case ModSource::Thunderstore:
					versionInfo.downloadLink = "https://gcdn.thunderstore.io/live/repository/packages/" + modEntry.dependencyString + ".zip";
					break;
				case ModSource::Unknown:
					versionInfo.downloadLink = modEntry.url;
					break;
				default:
					spdlog::warn("Received mod {} v{} from server has unsupported platform for auto-download, skipping adding to verified mods.", modEntry.name, modEntry.version);
					return true;
			}

			g_pModDownloader->verifiedMods[ modEntry.name ].versions.insert( { modEntry.version, versionInfo } );
		}
	}
	else
	{
		VerifiedModDetails modDetails;
		VerifiedModVersion versionInfo;
		versionInfo.checksum = modEntry.checksum;
		versionInfo.platform = modEntry.platform;

		switch( modEntry.platform )
		{
			case ModSource::Thunderstore:
				versionInfo.downloadLink = "https://gcdn.thunderstore.io/live/repository/packages/" + modEntry.dependencyString + ".zip";
				break;
			case ModSource::Unknown:
				versionInfo.downloadLink = modEntry.url;
				break;
			default:
				spdlog::warn("Received mod {} v{} from server has unsupported platform for auto-download, skipping adding to verified mods.", modEntry.name, modEntry.version);
				return true;
		}

		modDetails.versions.insert( { modEntry.version, versionInfo } );
		g_pModDownloader->verifiedMods.insert( { modEntry.name, modDetails } );
	}

	if(g_pModDownloader->m_ServerRequestedMods.size() == static_cast<size_t>(g_pModDownloader->GetTotalServerRequestedMods()))
	{
		spdlog::info("All {} server mods received.", g_pModDownloader->GetTotalServerRequestedMods());
		g_pModDownloader->SetIsListeningForServerMods(false);
	}

	return true;
}

ON_DLL_LOAD_RELIESON("engine.dll", ModDownloader, (ConVar), [](CModule module)
{
	Cvar_allow_mod_auto_download = new ConVar(
		"allow_mod_auto_download",
		"1",
		FCVAR_ARCHIVE_PLAYERPROFILE,
		"Allows the client to automatically download required mods from the server if they are verified.");

	g_pModDownloader = new ModDownloader();
})

ADD_SQFUNC("array<RequiredModInfo>", NSGetServerRequestedMods, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->newarray(sqvm, 0);

	const auto& serverMods = g_pModDownloader->GetServerRequestedMods();
	for (size_t i = 0; i < serverMods.size(); ++i)
	{
		const auto& mod = serverMods[i];

		g_pSquirrel[context]->pushnewstructinstance(sqvm, 2);

		// name
		g_pSquirrel[context]->pushstring(sqvm, mod.name.c_str());
		g_pSquirrel[context]->sealstructslot(sqvm, 0);

		// version
		g_pSquirrel[context]->pushstring(sqvm, mod.version.c_str());
		g_pSquirrel[context]->sealstructslot(sqvm, 1);

		g_pSquirrel[context]->arrayappend(sqvm, -2);
	}

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSClearServerRequestedMods, "", "", ScriptContext::UI)
{
	g_pModDownloader->GetServerRequestedMods().clear();
	g_pModDownloader->SetTotalServerRequestedMods(0);
	g_pModDownloader->SetIsListeningForServerMods(false);

	return SQRESULT_NULL;
}

ADD_SQFUNC("void", NSAllowServerModDownloads, "", "", ScriptContext::UI)
{
	g_pModDownloader->SetIsListeningForServerMods(true);

	return SQRESULT_NULL;
}

ADD_SQFUNC("int", NSReceivedServerModInfoCount, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushinteger(sqvm, g_pModDownloader->GetServerRequestedMods().size());
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("int", NSTotalServerRequestedMods, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushinteger(sqvm, g_pModDownloader->GetTotalServerRequestedMods());
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSListeningForServerMods, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, g_pModDownloader->IsListeningForServerMods());
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSFetchVerifiedModsManifesto, "", "", ScriptContext::SERVER | ScriptContext::CLIENT | ScriptContext::UI)
{
	if (g_pModDownloader)
		g_pModDownloader->FetchModsListFromAPI();
	return SQRESULT_NULL;
}

ADD_SQFUNC(
	"bool", NSIsModDownloadable, "string name, string version", "", ScriptContext::SERVER | ScriptContext::CLIENT | ScriptContext::UI)
{
	if (!g_pModDownloader)
	{
		g_pSquirrel[context]->pushbool(sqvm, false);
		return SQRESULT_NOTNULL;
	}

	g_pSquirrel[context]->newarray(sqvm, 0);

	const SQChar* modName = g_pSquirrel[context]->getstring(sqvm, 1);
	const SQChar* modVersion = g_pSquirrel[context]->getstring(sqvm, 2);

	bool result = g_pModDownloader->IsModAuthorized(modName, modVersion);
	g_pSquirrel[context]->pushbool(sqvm, result);

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSDownloadMod, "string name, string version", "", ScriptContext::SERVER | ScriptContext::CLIENT | ScriptContext::UI)
{
	if (!g_pModDownloader)
		return SQRESULT_NULL;

	const SQChar* modName = g_pSquirrel[context]->getstring(sqvm, 1);
	const SQChar* modVersion = g_pSquirrel[context]->getstring(sqvm, 2);
	g_pModDownloader->DownloadMod(modName, modVersion);

	return SQRESULT_NULL;
}

ADD_SQFUNC("void", NSSetModDownloadReady, "", "", ScriptContext::UI)
{
	if (!g_pModDownloader)
		return SQRESULT_NULL;

	g_pModDownloader->SetDownloadReady(true);
	return SQRESULT_NULL;
}

ADD_SQFUNC("ModInstallState", NSGetModInstallState, "", "", ScriptContext::SERVER | ScriptContext::CLIENT | ScriptContext::UI)
{
	g_pSquirrel[context]->pushnewstructinstance(sqvm, 6);

	ModDownloader::MOD_STATE modState = {};
	if (g_pModDownloader)
		modState = g_pModDownloader->modState;
	else
		modState.state = ModDownloader::NOT_FOUND;

	// name
	g_pSquirrel[context]->pushstring(sqvm, modState.name.c_str());
	g_pSquirrel[context]->sealstructslot(sqvm, 0);

	// version
	g_pSquirrel[context]->pushstring(sqvm, modState.version.c_str());
	g_pSquirrel[context]->sealstructslot(sqvm, 1);

	// state
	g_pSquirrel[context]->pushinteger(sqvm, modState.state);
	g_pSquirrel[context]->sealstructslot(sqvm, 2);

	// progress
	g_pSquirrel[context]->pushinteger(sqvm, modState.progress);
	g_pSquirrel[context]->sealstructslot(sqvm, 3);

	// total
	g_pSquirrel[context]->pushinteger(sqvm, modState.total);
	g_pSquirrel[context]->sealstructslot(sqvm, 4);

	// ratio
	g_pSquirrel[context]->pushfloat(sqvm, modState.ratio);
	g_pSquirrel[context]->sealstructslot(sqvm, 5);

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSCancelModDownload, "", "", ScriptContext::SERVER | ScriptContext::CLIENT | ScriptContext::UI)
{
	if (g_pModDownloader)
		g_pModDownloader->CancelDownload();
	return SQRESULT_NULL;
}
