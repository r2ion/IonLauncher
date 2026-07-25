#include "modsystem/platform/thunderstore.h"

#include <curl/curl.h>
#include <rapidjson/document.h>
#include <algorithm>
#include <cctype>
#include <string_view>

bool CThunderstoreClient::IsSafePackageComponent(std::string_view value)
{
	return !value.empty() && value.size() <= 128 && std::ranges::all_of(value, [](unsigned char character) {
		return std::isalnum(character) != 0 || character == '_' || character == '-' || character == '.';
	});
}
bool CThunderstoreClient::IsSafeDependencyString(std::string_view value)
{
	return !value.empty() && value.size() <= 240 && value.find('-') != std::string_view::npos &&
	       std::ranges::all_of(value, [](unsigned char character)
	{ return std::isalnum(character) != 0 || character == '_' || character == '-' || character == '.'; });
}

size_t CThunderstoreClient::WriteToString(void* data, size_t size, size_t count, void* stream)
{
	const size_t byteCount = size * count;
	std::string& output = *static_cast<std::string*>(stream);
	if (byteCount > MAX_RESPONSE_BYTES - std::min(output.size(), MAX_RESPONSE_BYTES))
		return 0;
	output.append(static_cast<const char*>(data), byteCount);
	return byteCount;
}

bool CThunderstoreClient::ParsePackageUrl(const std::string& url, std::string& outNamespace, std::string& outPackage)
{
	outNamespace.clear();
	outPackage.clear();

	const size_t markerPosition = url.find(PACKAGE_MARKER);
	if (markerPosition == std::string::npos)
		return false;

	std::string_view remainder(url.data() + markerPosition + PACKAGE_MARKER.size(), url.size() - markerPosition - PACKAGE_MARKER.size());
	const size_t suffixPosition = remainder.find_first_of("?#");
	if (suffixPosition != std::string_view::npos)
		remainder = remainder.substr(0, suffixPosition);
	while (!remainder.empty() && remainder.back() == '/')
		remainder.remove_suffix(1);

	const size_t separator = remainder.find('/');
	if (separator == std::string_view::npos || remainder.find('/', separator + 1) != std::string_view::npos)
		return false;

	const std::string_view namespaceName = remainder.substr(0, separator);
	const std::string_view packageName = remainder.substr(separator + 1);
	if (!IsSafePackageComponent(namespaceName) || !IsSafePackageComponent(packageName))
		return false;

	outNamespace.assign(namespaceName);
	outPackage.assign(packageName);
	return true;
}

std::string CThunderstoreClient::BuildDownloadUrl(std::string_view dependencyString)
{
	if (!IsSafeDependencyString(dependencyString))
		return {};
	std::string url;
	url.reserve(CDN_PACKAGE_BASE.size() + dependencyString.size() + 4);
	url.append(CDN_PACKAGE_BASE).append(dependencyString).append(".zip");
	return url;
}

bool CThunderstoreClient::FetchPackageDetails(
	const std::string& namespaceName,
	const std::string& packageName, PackageDetails& out)
{
	out = {};
	if (!IsSafePackageComponent(namespaceName) || !IsSafePackageComponent(packageName))
		return false;

	const std::string url = "https://thunderstore.io/api/experimental/package/" + namespaceName + "/" + packageName + "/";
	CURL* easyHandle = curl_easy_init();
	if (!easyHandle)
		return false;

	std::string response;
	curl_easy_setopt(easyHandle, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(easyHandle, CURLOPT_CONNECTTIMEOUT, 15L);
	curl_easy_setopt(easyHandle, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(easyHandle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(easyHandle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(easyHandle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(easyHandle, CURLOPT_USERAGENT, "NorthstarLauncher/1.0");
	curl_easy_setopt(easyHandle, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(easyHandle, CURLOPT_WRITEFUNCTION, WriteToString);
	const CURLcode result = curl_easy_perform(easyHandle);
	curl_easy_cleanup(easyHandle);

	if (result != CURLE_OK)
		return false;

	rapidjson::Document document;
	document.Parse(response);
	if (!document.IsObject())
		return false;

	if (document.HasMember("name") && document["name"].IsString())
		out.m_Name = document["name"].GetString();
	if (document.HasMember("namespace") && document["namespace"].IsString())
		out.m_Namespace = document["namespace"].GetString();

	if (!document.HasMember("latest") || !document["latest"].IsObject())
		return false;

	const rapidjson::Value& latest = document["latest"];
	if (latest.HasMember("download_url") && latest["download_url"].IsString())
		out.m_DownloadUrl = latest["download_url"].GetString();
	if (latest.HasMember("icon") && latest["icon"].IsString())
		out.m_IconUrl = latest["icon"].GetString();
	if (latest.HasMember("version_number") && latest["version_number"].IsString())
		out.m_Version = latest["version_number"].GetString();
	if (out.m_Name.empty() && latest.HasMember("name") && latest["name"].IsString())
		out.m_Name = latest["name"].GetString();
	if (out.m_Namespace.empty())
		out.m_Namespace = namespaceName;

	if (!IsSafePackageComponent(out.m_Namespace) || !IsSafePackageComponent(out.m_Name) || !IsSafePackageComponent(out.m_Version))
		return false;

	const std::string dependencyString = out.m_Namespace + "-" + out.m_Name + "-" + out.m_Version;
	const std::string directUrl = BuildDownloadUrl(dependencyString);
	if (!directUrl.empty())
		out.m_DownloadUrl = directUrl;
	return !out.m_DownloadUrl.empty();
}
