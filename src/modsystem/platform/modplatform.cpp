#include "modplatform.h"

#include "modsystem/modmanager.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <format>
#include <fstream>
#include <sstream>

std::string CModPlatform::CurrentTimestamp()
{
	SYSTEMTIME time{};
	GetSystemTime(&time);
	return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond,
	                   time.wMilliseconds);
}

fs::path CModPlatform::ResolvePackageRoot(const fs::path& path)
{
	if (const std::optional<fs::path> root = FindContainingPackageRoot(path))
		return *root;
	return path.lexically_normal();
}

std::string CModPlatform::Trim(std::string value)
{
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
		value.erase(value.begin());
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
		value.pop_back();
	return value;
}

bool CModPlatform::IsSha256(std::string_view value)
{
	return value.size() == 64 && std::ranges::all_of(value, [](unsigned char character) { return std::isxdigit(character) != 0; });
}

bool CModPlatform::WriteTextAtomically(const fs::path& destination, std::string_view contents, std::string& errorMessage)
{
	std::error_code error;
	fs::create_directories(destination.parent_path(), error);
	if (error)
	{
		errorMessage = std::format("Failed creating package directory: {}", error.message());
		return false;
	}

	static std::atomic<uint64_t> s_TemporaryFileSequence = 0;
	fs::path temporary = destination;
	temporary += std::format(L".tmp.{}.{}", GetCurrentProcessId(), s_TemporaryFileSequence.fetch_add(1, std::memory_order_relaxed));
	{
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		if (!output.is_open())
		{
            errorMessage = fmt::format("Failed opening temporary file '{}'", temporary);
            return false;
		}
		output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
		output.flush();
		if (!output.good())
		{
            errorMessage = fmt::format("Failed writing temporary file '{}'", temporary);
            output.close();
			fs::remove(temporary, error);
			return false;
		}
	}

	if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
        errorMessage = fmt::format("Failed committing '{}': Windows error {}", destination, GetLastError());
        fs::remove(temporary, error);
		return false;
	}
	return true;
}

class CModPlatformJson final
{
public:
	static const rapidjson::Value* FindMember(const rapidjson::Value& object, const char* name)
	{
		if (!object.IsObject())
			return nullptr;
		const auto member = object.FindMember(name);
		return member == object.MemberEnd() ? nullptr : &member->value;
	}

	static std::string ReadString(const rapidjson::Value& object, const char* name)
	{
		const rapidjson::Value* value = CModPlatformJson::FindMember(object, name);
		return value && value->IsString() ? std::string(value->GetString(), value->GetStringLength()) : std::string();
	}

	static std::optional<uint64_t> ReadUint64(const rapidjson::Value& object, const char* name)
	{
		const rapidjson::Value* value = CModPlatformJson::FindMember(object, name);
		if (!value)
			return std::nullopt;
		if (value->IsUint64())
			return value->GetUint64();
		if (value->IsInt64() && value->GetInt64() >= 0)
			return static_cast<uint64_t>(value->GetInt64());
		return std::nullopt;
	}

	static void AddString(rapidjson::Value& object, const char* name, const std::string& value, rapidjson::Document::AllocatorType& allocator)
	{
		rapidjson::Value stringValue;
		stringValue.SetString(value.data(), static_cast<rapidjson::SizeType>(value.size()), allocator);
		object.AddMember(rapidjson::StringRef(name), std::move(stringValue), allocator);
	}
};

std::optional<fs::path> CModPlatform::FindContainingPackageRoot(const fs::path& modDir)
{
	const fs::path packagesRoot = GetPackageFolderPath().lexically_normal();
    if (!ModPaths::IsAtOrBelow(modDir, packagesRoot) || ModPaths::Equal(modDir, packagesRoot))
        return std::nullopt;

	const fs::path relative = modDir.lexically_normal().lexically_relative(packagesRoot);
	if (relative.empty() || relative == ".")
		return std::nullopt;
	const auto first = relative.begin();
	if (first == relative.end() || *first == "..")
		return std::nullopt;
	return packagesRoot / *first;
}

fs::path CModPlatform::GetManagedMarkerPath(const fs::path& packageRoot, ModSource platform)
{
	const fs::path root = ResolvePackageRoot(packageRoot);
	switch (platform)
	{
	case ModSource::ModWorkshop:
		return root / MODWORKSHOP_MARKER_FILE;
	case ModSource::Thunderstore:
		return root / THUNDERSTORE_MARKER_FILE;
	default:
		return {};
	}
}

ModSource CModPlatform::GetManagedSourceForPath(const fs::path& modDir)
{
	const fs::path root = ResolvePackageRoot(modDir);
	std::error_code error;
	if (fs::is_regular_file(root / MODWORKSHOP_MARKER_FILE, error))
		return ModSource::ModWorkshop;
	error.clear();
	if (fs::is_regular_file(root / THUNDERSTORE_MARKER_FILE, error))
		return ModSource::Thunderstore;
	return ModSource::Unmanaged;
}

std::optional<std::string> CModPlatform::TryReadManagedId(const fs::path& modDir, ModSource platform)
{
	const fs::path markerPath = GetManagedMarkerPath(modDir, platform);
	if (markerPath.empty())
		return std::nullopt;

	std::error_code error;
	const uintmax_t fileSize = fs::file_size(markerPath, error);
	if (error || fileSize == 0 || fileSize > MAX_MARKER_BYTES)
		return std::nullopt;

	std::ifstream file(markerPath, std::ios::binary);
	if (!file.is_open())
		return std::nullopt;

	std::string value(static_cast<size_t>(fileSize), '\0');
	file.read(value.data(), static_cast<std::streamsize>(value.size()));
	value = Trim(std::move(value));
	return value.empty() ? std::nullopt : std::optional<std::string>(std::move(value));
}

bool CModPlatform::WriteManagedMarker(const fs::path& packageRoot, ModSource platform, std::string_view id)
{
	const fs::path markerPath = GetManagedMarkerPath(packageRoot, platform);
	if (markerPath.empty() || id.empty() || id.size() > MAX_MARKER_BYTES)
		return false;

	std::string errorMessage;
	return WriteTextAtomically(markerPath, id, errorMessage);
}

bool CModPlatform::ReadWorkshopPackageState(const fs::path& packageRoot, ModWorkshopPackageState& state, std::string& errorMessage)
{
	state = {};
	errorMessage.clear();
	const fs::path statePath = ResolvePackageRoot(packageRoot) / MODWORKSHOP_STATE_FILE;
	std::error_code error;
	const uintmax_t fileSize = fs::file_size(statePath, error);
	if (error)
	{
		errorMessage = "ModWorkshop package state does not exist";
		return false;
	}
	if (fileSize == 0 || fileSize > MAX_STATE_BYTES)
	{
		errorMessage = "ModWorkshop package state has an invalid size";
		return false;
	}

	std::ifstream input(statePath, std::ios::binary);
	if (!input.is_open())
	{
		errorMessage = "Failed opening ModWorkshop package state";
		return false;
	}
	std::string contents(static_cast<size_t>(fileSize), '\0');
	input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
	if (!input)
	{
		errorMessage = "Failed reading ModWorkshop package state";
		return false;
	}

	rapidjson::Document document;
	document.Parse(contents.data(), contents.size());
	if (document.HasParseError() || !document.IsObject())
	{
		errorMessage = "ModWorkshop package state is not valid JSON";
		return false;
	}

	const rapidjson::Value* schema = CModPlatformJson::FindMember(document, "schema");
	const std::optional<uint64_t> modId = CModPlatformJson::ReadUint64(document, "mod_id");
	const std::optional<uint64_t> selectedFileId = CModPlatformJson::ReadUint64(document, "selected_file_id");
	const std::optional<uint64_t> downloadSize = CModPlatformJson::ReadUint64(document, "download_size");
	const std::string sha256 = CModPlatformJson::ReadString(document, "sha256");
	if (!schema || !schema->IsInt() || schema->GetInt() != MODWORKSHOP_STATE_SCHEMA || !modId || *modId == 0 || !selectedFileId ||
	    *selectedFileId == 0 || !downloadSize || !IsSha256(sha256))
	{
		errorMessage = "ModWorkshop package state is missing required verified fields";
		return false;
	}

	state.schema = schema->GetInt();
	state.modId = *modId;
	state.selectedFileId = *selectedFileId;
	state.downloadSize = *downloadSize;
	state.selectedFileVersion = CModPlatformJson::ReadString(document, "selected_file_version");
	state.selectedFileUpdatedAt = CModPlatformJson::ReadString(document, "selected_file_updated_at");
	state.remoteModUpdatedAt = CModPlatformJson::ReadString(document, "remote_mod_updated_at");
	state.sha256 = sha256;
	state.installedAt = CModPlatformJson::ReadString(document, "installed_at");

	if (const rapidjson::Value* containedMods = CModPlatformJson::FindMember(document, "contained_mods"))
	{
		if (!containedMods->IsArray())
		{
			errorMessage = "ModWorkshop contained_mods field is not an array";
			return false;
		}
		state.containedMods.reserve(containedMods->Size());
		for (const rapidjson::Value& value : containedMods->GetArray())
		{
			ModWorkshopContainedMod containedMod{.name = CModPlatformJson::ReadString(value, "name"),
			                                     .version = CModPlatformJson::ReadString(value, "version")};
			if (!value.IsObject() || containedMod.name.empty())
			{
				errorMessage = "ModWorkshop contained mod entry is malformed";
				return false;
			}
			state.containedMods.push_back(std::move(containedMod));
		}
	}
	return true;
}

bool CModPlatform::WriteWorkshopPackageState(const fs::path& packageRoot, const ModWorkshopPackageState& state, std::string& errorMessage)
{
	errorMessage.clear();
	if (state.schema != MODWORKSHOP_STATE_SCHEMA || state.modId == 0 || state.selectedFileId == 0 || !IsSha256(state.sha256))
	{
		errorMessage = "Cannot write incomplete ModWorkshop package state";
		return false;
	}

	rapidjson::Document document;
	document.SetObject();
	auto& allocator = document.GetAllocator();
	document.AddMember("schema", state.schema, allocator);
	document.AddMember("mod_id", state.modId, allocator);
	document.AddMember("selected_file_id", state.selectedFileId, allocator);
	document.AddMember("download_size", state.downloadSize, allocator);
	CModPlatformJson::AddString(document, "selected_file_version", state.selectedFileVersion, allocator);
	CModPlatformJson::AddString(document, "selected_file_updated_at", state.selectedFileUpdatedAt, allocator);
	CModPlatformJson::AddString(document, "remote_mod_updated_at", state.remoteModUpdatedAt, allocator);
	CModPlatformJson::AddString(document, "sha256", state.sha256, allocator);
	CModPlatformJson::AddString(document, "installed_at", state.installedAt, allocator);

	rapidjson::Value containedMods(rapidjson::kArrayType);
	for (const ModWorkshopContainedMod& containedMod : state.containedMods)
	{
		rapidjson::Value value(rapidjson::kObjectType);
		CModPlatformJson::AddString(value, "name", containedMod.name, allocator);
		CModPlatformJson::AddString(value, "version", containedMod.version, allocator);
		containedMods.PushBack(std::move(value), allocator);
	}
	document.AddMember("contained_mods", std::move(containedMods), allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	document.Accept(writer);
	return WriteTextAtomically(ResolvePackageRoot(packageRoot) / MODWORKSHOP_STATE_FILE, std::string_view(buffer.GetString(), buffer.GetSize()),
	                           errorMessage);
}
