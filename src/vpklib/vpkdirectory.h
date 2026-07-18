#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

bool VPKDirectory_GetFileList(
	const std::filesystem::path& directoryFile, std::string_view extension, std::vector<std::string>& entries);
