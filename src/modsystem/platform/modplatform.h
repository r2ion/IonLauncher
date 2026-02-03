#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "modsystem/mod.h"

namespace fs = std::filesystem;

static constexpr const char* kModWorkshopMarkerFile = ".mws_id";
static constexpr const char* kThunderstoreMarkerFile = ".ts_id";

ModSource Mod_GetManagedSourceForPath(const fs::path& modDir);
std::optional<std::string> Mod_TryReadManagedId(const fs::path& modDir, ModSource platform);
bool Mod_WriteManagedMarker(const fs::path& modDir, ModSource platform, std::string_view id);
fs::path Mod_GetManagedMarkerPath(const fs::path& modDir, ModSource platform);
