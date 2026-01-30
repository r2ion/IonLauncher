#include "modplatform.h"

#include <fstream>

fs::path Mod_GetManagedMarkerPath(const fs::path& modDir, ModSource platform)
{
	switch (platform)
	{
	case ModSource::ModWorkshop:
		return modDir / kModWorkshopMarkerFile;
	case ModSource::Thunderstore:
		return modDir / kThunderstoreMarkerFile;
	default:
		return modDir;
	}
}

ModSource Mod_GetManagedSourceForPath(const fs::path& modDir)
{
	if (fs::exists(modDir / kModWorkshopMarkerFile))
		return ModSource::ModWorkshop;
	if (fs::exists(modDir / kThunderstoreMarkerFile))
		return ModSource::Thunderstore;
	return ModSource::Unmanaged;
}

std::optional<std::string> Mod_TryReadManagedId(const fs::path& modDir, ModSource platform)
{
	const fs::path markerPath = Mod_GetManagedMarkerPath(modDir, platform);
	if (!fs::exists(markerPath))
		return std::nullopt;

	std::ifstream file(markerPath);
	if (!file.is_open())
		return std::nullopt;

	std::string value;
	std::getline(file, value);
	if (value.empty())
		return std::nullopt;

	return value;
}

bool Mod_WriteManagedMarker(const fs::path& modDir, ModSource platform, std::string_view id)
{
	const fs::path markerPath = Mod_GetManagedMarkerPath(modDir, platform);
	if (markerPath == modDir)
		return false;

	std::ofstream file(markerPath, std::ios::trunc);
	if (!file.is_open())
		return false;

	file << id;
	return file.good();
}
