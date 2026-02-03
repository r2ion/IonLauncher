#include "thunderstore.h"

#include <rapidjson/document.h>
#include <curl/curl.h>
#include <string_view>

namespace
{
	size_t WriteToString(void* ptr, size_t size, size_t count, void* stream)
	{
		((std::string*)stream)->append((char*)ptr, 0, size * count);
		return size * count;
	}
}

bool Thunderstore_ParsePackageUrl(const std::string& url, std::string& outNamespace, std::string& outPackage)
{
	constexpr std::string_view marker = "thunderstore.io/package/";
	auto markerPos = url.find(marker);
	if (markerPos == std::string::npos)
		return false;

	std::string rest = url.substr(markerPos + marker.size());
	auto endPos = rest.find_first_of("?#");
	if (endPos != std::string::npos)
		rest = rest.substr(0, endPos);

	if (!rest.empty() && rest.back() == '/')
		rest.pop_back();

	auto slashPos = rest.find('/');
	if (slashPos == std::string::npos)
		return false;

	outNamespace = rest.substr(0, slashPos);
	outPackage = rest.substr(slashPos + 1);

	return !outNamespace.empty() && !outPackage.empty();
}

bool Thunderstore_FetchPackageDetails(
	const std::string& namespaceName,
	const std::string& packageName,
	ThunderstorePackageDetails& out)
{
	std::string url = "https://thunderstore.io/api/experimental/package/" + namespaceName + "/" + packageName + "/";
	CURL* easyhandle = curl_easy_init();
	if (!easyhandle)
		return false;

	std::string readBuffer;
	curl_easy_setopt(easyhandle, CURLOPT_CUSTOMREQUEST, "GET");
	curl_easy_setopt(easyhandle, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(easyhandle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(easyhandle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(easyhandle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(easyhandle, CURLOPT_USERAGENT, "NorthstarLauncher/1.0");
	curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &readBuffer);
	curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, WriteToString);
	CURLcode result = curl_easy_perform(easyhandle);
	curl_easy_cleanup(easyhandle);

	if (result != CURLcode::CURLE_OK)
		return false;

	rapidjson::Document doc;
	doc.Parse(readBuffer);
	if (!doc.IsObject())
		return false;

	if (doc.HasMember("name") && doc["name"].IsString())
		out.name = doc["name"].GetString();
	if (doc.HasMember("namespace") && doc["namespace"].IsString())
		out.namespaceName = doc["namespace"].GetString();

	if (doc.HasMember("latest") && doc["latest"].IsObject())
	{
		const auto& latest = doc["latest"];
		if (latest.HasMember("download_url") && latest["download_url"].IsString())
			out.downloadUrl = latest["download_url"].GetString();
		if (latest.HasMember("version_number") && latest["version_number"].IsString())
			out.version = latest["version_number"].GetString();
		if (out.name.empty() && latest.HasMember("name") && latest["name"].IsString())
			out.name = latest["name"].GetString();
		if (out.namespaceName.empty())
			out.namespaceName = namespaceName;
	}
	else if (doc.HasMember("versions") && doc["versions"].IsArray())
	{
		const auto versions = doc["versions"].GetArray();
		if (!versions.Empty())
		{
			const auto& latest = versions[0];
			if (latest.IsObject())
			{
				if (latest.HasMember("download_url") && latest["download_url"].IsString())
					out.downloadUrl = latest["download_url"].GetString();
				if (latest.HasMember("version_number") && latest["version_number"].IsString())
					out.version = latest["version_number"].GetString();
				if (out.name.empty() && latest.HasMember("name") && latest["name"].IsString())
					out.name = latest["name"].GetString();
				if (out.namespaceName.empty())
					out.namespaceName = namespaceName;
			}
		}
	}

		if (!out.version.empty() && !out.namespaceName.empty())
		{
			const std::string resolvedName = !out.name.empty() ? out.name : packageName;
			if (!resolvedName.empty())
			{
				out.downloadUrl = "https://gcdn.thunderstore.io/live/repository/packages/" +
					out.namespaceName + "-" + resolvedName + "-" + out.version + ".zip";
			}
		}

	return !out.downloadUrl.empty() && !out.version.empty();
}
