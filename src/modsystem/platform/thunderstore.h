#pragma once

#include <cstddef>
#include <string>
#include <string_view>

class CThunderstoreClient final
{
public:
	struct PackageDetails
	{
		std::string m_DownloadUrl;
		std::string m_IconUrl;
		std::string m_Version;
		std::string m_Name;
		std::string m_Namespace;
	};

	static bool ParsePackageUrl(const std::string& url, std::string& outNamespace, std::string& outPackage);
	static std::string BuildDownloadUrl(std::string_view dependencyString);
	static bool FetchPackageDetails(
	const std::string& namespaceName,
	const std::string& packageName, PackageDetails& out);

private:
	static constexpr size_t MAX_RESPONSE_BYTES = 4 * 1024 * 1024;
	static constexpr std::string_view PACKAGE_MARKER = "thunderstore.io/package/";
	static constexpr std::string_view CDN_PACKAGE_BASE = "https://gcdn.thunderstore.io/live/repository/packages/";

	static bool IsSafePackageComponent(std::string_view value);
	static bool IsSafeDependencyString(std::string_view value);
	static size_t WriteToString(void* data, size_t size, size_t count, void* stream);
};
