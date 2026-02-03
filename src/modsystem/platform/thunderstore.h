#pragma once

#include <string>

struct ThunderstorePackageDetails
{
	std::string downloadUrl;
	std::string version;
	std::string name;
	std::string namespaceName;
};

bool Thunderstore_ParsePackageUrl(const std::string& url, std::string& outNamespace, std::string& outPackage);

bool Thunderstore_FetchPackageDetails(
	const std::string& namespaceName,
	const std::string& packageName,
	ThunderstorePackageDetails& out);
