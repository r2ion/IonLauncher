#include "modworkshop.h"

std::optional<fs::path> ModWorkshop_FindRootDir(unzFile file, const unz_global_info64& archiveInfo)
{
	unzGoToFirstFile(file);
	for (uint64_t i = 0; i < archiveInfo.number_entry; ++i)
	{
		char zipFilename[256];
		unz_file_info64 fileInfo;
		int status = unzGetCurrentFileInfo64(file, &fileInfo, zipFilename, sizeof(zipFilename), NULL, 0, NULL, 0);
		if (status != UNZ_OK)
			return std::nullopt;

		fs::path filePath = zipFilename;
		if (filePath.has_filename() && filePath.filename() == "mod.json")
		{
			if (filePath.has_parent_path())
				return filePath.parent_path() / "";
			return fs::path();
		}

		if ((i + 1) < archiveInfo.number_entry)
		{
			status = unzGoToNextFile(file);
			if (status != UNZ_OK)
				return std::nullopt;
		}
	}

	return std::nullopt;
}

std::optional<std::string> ModWorkshop_TryParseInstallId(const std::string& uri)
{
	constexpr std::string_view prefix = "r2ns://mws/install/";
	if (!uri.starts_with(prefix))
		return std::nullopt;

	std::string id = uri.substr(prefix.size());
	if (id.empty())
		return std::nullopt;

	return id;
}
