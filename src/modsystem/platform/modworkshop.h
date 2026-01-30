#pragma once

#include <filesystem>
#include <optional>

#include <compat/unzip.h>

namespace fs = std::filesystem;

std::optional<fs::path> ModWorkshop_FindRootDir(unzFile file, const unz_global_info64& archiveInfo);
std::optional<std::string> ModWorkshop_TryParseInstallId(const std::string& uri);
