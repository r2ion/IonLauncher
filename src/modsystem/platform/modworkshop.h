#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <compat/unzip.h>

namespace fs = std::filesystem;

std::optional<fs::path> ModWorkshop_FindRootDir(unzFile file, const unz_global_info64& archiveInfo);
std::optional<std::string> ModWorkshop_TryParseInstallId(const std::string& uri);
std::string ModWorkshop_BuildDetailsUrl(const std::string& id);
std::string ModWorkshop_BuildDownloadUrl(const std::string& id);

struct ModWorkshopDetails
{
	std::string downloadUrl;
	std::string author;
	std::string name;
	std::string version;
};

bool ModWorkshop_TryParseDetails(const std::string& json, ModWorkshopDetails& out);
bool ModWorkshop_FetchDetails(const std::string& id, ModWorkshopDetails& out);
