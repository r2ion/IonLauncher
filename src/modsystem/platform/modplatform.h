#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <vector>

#include "modsystem/mod.h"

namespace fs = std::filesystem;

inline constexpr const char* MODWORKSHOP_MARKER_FILE = ".mws_id";
inline constexpr const char* THUNDERSTORE_MARKER_FILE = ".ts_id";
inline constexpr const char* MODWORKSHOP_STATE_FILE = ".mws_state.json";
inline constexpr int MODWORKSHOP_STATE_SCHEMA = 1;

struct ModWorkshopContainedMod
{
	std::string name;
	std::string version;
};

struct ModWorkshopPackageState
{
	int schema = MODWORKSHOP_STATE_SCHEMA;
	uint64_t modId = 0;
	uint64_t selectedFileId = 0;
	uint64_t downloadSize = 0;
	std::string selectedFileVersion;
	std::string selectedFileUpdatedAt;
	std::string remoteModUpdatedAt;
	std::string sha256;
	std::string installedAt;
	std::vector<ModWorkshopContainedMod> containedMods;
};

class CModPlatform final
{
public:
	static std::string CurrentTimestamp();
	static std::optional<fs::path> FindContainingPackageRoot(const fs::path& modDirectory);
	static ModSource GetManagedSourceForPath(const fs::path& modDirectory);
	static std::optional<std::string> TryReadManagedId(const fs::path& modDirectory, ModSource platform);
	static bool WriteManagedMarker(const fs::path& packageRoot, ModSource platform, std::string_view id);
	static fs::path GetManagedMarkerPath(const fs::path& packageRoot, ModSource platform);
	static bool ReadWorkshopPackageState(const fs::path& packageRoot, ModWorkshopPackageState& state, std::string& errorMessage);
	static bool WriteWorkshopPackageState(const fs::path& packageRoot, const ModWorkshopPackageState& state, std::string& errorMessage);

private:
	static constexpr size_t MAX_MARKER_BYTES = 128;
	static constexpr size_t MAX_STATE_BYTES = 256 * 1024;

	static std::string NormalizeForComparison(const fs::path& path);
	static bool IsPathUnder(const fs::path& candidate, const fs::path& root);
	static fs::path ResolvePackageRoot(const fs::path& path);
	static std::string Trim(std::string value);
	static bool IsSha256(std::string_view value);
	static bool WriteTextAtomically(const fs::path& destination, std::string_view contents, std::string& errorMessage);
};
