#pragma once

#include "modsystem/platform/modhttp.h"

class CThunderstoreClient final
{
  public:
    struct PackageDetails
    {
        std::string m_DownloadUrl, m_IconUrl, m_Version, m_Name, m_Namespace;
        std::string m_Description, m_Readme, m_PageUrl, m_UpdatedAt, m_CreatedAt;
        std::vector<std::string> m_Dependencies;
        uint64_t m_FileSize = 0, m_Downloads = 0, m_Rating = 0;
        bool m_Active = false, m_Deprecated = false;
    };

    static bool ParsePackageUrl(const std::string& url, std::string& outNamespace, std::string& outPackage);
    static bool ParsePackageId(std::string_view id, std::string& outNamespace, std::string& outPackage);
    static std::string BuildDownloadUrl(std::string_view dependencyString);
    static bool IsNewerVersion(std::string_view remote, std::string_view installed);
    static bool FetchPackageDetails(const std::string& namespaceName, const std::string& packageName, PackageDetails& out,
                                    const ModRequestOptions& options = {}, ModRequestError* error = nullptr);
    static bool FetchPackageVersion(const std::string& namespaceName, const std::string& packageName, const std::string& version, PackageDetails& out,
                                    const ModRequestOptions& options = {}, ModRequestError* error = nullptr);
    static bool FetchReadme(PackageDetails& package, ModRequestError& error, const ModRequestOptions& options = {});
    static bool FetchCatalog(std::vector<PackageDetails>& packages, ModRequestError& error, const ModRequestOptions& options = {});

  private:
    class CJson;
    static bool IsSafePackageComponent(std::string_view value);
    static bool IsSafeVersion(std::string_view value);
    static constexpr std::string_view API_BASE = "https://thunderstore.io/api/experimental/package/";
    static constexpr std::string_view CDN_PACKAGE_BASE = "https://gcdn.thunderstore.io/live/repository/packages/";
};
