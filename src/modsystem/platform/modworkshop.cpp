#include "modworkshop.h"

#include <rapidjson/document.h>

#include <curl/curl.h>

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

std::string ModWorkshop_BuildDetailsUrl(const std::string& id)
{
	static constexpr std::string_view baseUrl = "https://api.modworkshop.net/mods/";
	return std::string(baseUrl) + id;
}

static size_t ModWorkshop_WriteToString(void* ptr, size_t size, size_t count, void* stream)
{
	((std::string*)stream)->append((char*)ptr, 0, size * count);
	return size * count;
}

bool ModWorkshop_TryParseDetails(const std::string& json, ModWorkshopDetails& out)
{
	rapidjson::Document doc;
	doc.Parse(json);
	if (!doc.IsObject())
		return false;

	out.dependencies.clear();

	if (!doc.HasMember("download") || !doc["download"].IsObject())
		return false;

	auto& download = doc["download"];
	if (!download.HasMember("download_url") || !download["download_url"].IsString())
		return false;

	out.downloadUrl = download["download_url"].GetString();

	if (download.HasMember("name") && download["name"].IsString())
		out.author = download["name"].GetString();

	if (doc.HasMember("user") && doc["user"].IsObject())
	{
		auto& user = doc["user"];
		if (user.HasMember("name") && user["name"].IsString())
			out.author = user["name"].GetString();
	}

	if (doc.HasMember("name") && doc["name"].IsString())
		out.name = doc["name"].GetString();

	if (download.HasMember("version") && download["version"].IsString())
		out.version = download["version"].GetString();

	if (out.version.empty() && doc.HasMember("version") && doc["version"].IsString())
		out.version = doc["version"].GetString();

	if (doc.HasMember("dependencies") && doc["dependencies"].IsArray())
	{
		for (auto& depValue : doc["dependencies"].GetArray())
		{
			if (!depValue.IsObject())
				continue;

			ModWorkshopDetails::Dependency dependency;

			if (depValue.HasMember("name") && depValue["name"].IsString())
				dependency.name = depValue["name"].GetString();

			if (depValue.HasMember("url") && depValue["url"].IsString())
				dependency.url = depValue["url"].GetString();

			if (depValue.HasMember("offsite") && depValue["offsite"].IsBool())
				dependency.offsite = depValue["offsite"].GetBool();

			if (depValue.HasMember("optional") && depValue["optional"].IsBool())
				dependency.optional = depValue["optional"].GetBool();

			if (depValue.HasMember("mod_id"))
			{
				if (depValue["mod_id"].IsString())
					dependency.modId = depValue["mod_id"].GetString();
				else if (depValue["mod_id"].IsInt())
					dependency.modId = std::to_string(depValue["mod_id"].GetInt());
				else if (depValue["mod_id"].IsUint64())
					dependency.modId = std::to_string(depValue["mod_id"].GetUint64());
			}

			if (!dependency.name.empty() || !dependency.url.empty() || dependency.modId)
				out.dependencies.push_back(std::move(dependency));
		}
	}

	return true;
}

bool ModWorkshop_FetchDetails(const std::string& id, ModWorkshopDetails& out)
{
	std::string url = ModWorkshop_BuildDetailsUrl(id);
	CURL* easyhandle = curl_easy_init();
	if (!easyhandle)
		return false;

	std::string readBuffer;
	curl_easy_setopt(easyhandle, CURLOPT_CUSTOMREQUEST, "GET");
	curl_easy_setopt(easyhandle, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(easyhandle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(easyhandle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &readBuffer);
	curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, ModWorkshop_WriteToString);
	CURLcode result = curl_easy_perform(easyhandle);
	curl_easy_cleanup(easyhandle);

	if (result != CURLcode::CURLE_OK)
		return false;

	return ModWorkshop_TryParseDetails(readBuffer, out);
}

