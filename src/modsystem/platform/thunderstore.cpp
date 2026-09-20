#include "modsystem/platform/thunderstore.h"
#include <algorithm>
#include <cctype>
#include <format>
#include <rapidjson/document.h>

bool CThunderstoreClient::IsNewerVersion(std::string_view remote, std::string_view installed)
{
    if (!IsSafeVersion(remote) || !IsSafeVersion(installed))
        return false;
    for (int component = 0; component < 3; ++component)
    {
        const size_t remoteEnd = remote.find('.');
        const size_t installedEnd = installed.find('.');
        auto a = remote.substr(0, remoteEnd);
        auto b = installed.substr(0, installedEnd);
        while (a.size() > 1 && a.front() == '0')
            a.remove_prefix(1);
        while (b.size() > 1 && b.front() == '0')
            b.remove_prefix(1);
        if (a.size() != b.size())
            return a.size() > b.size();
        if (a != b)
            return a > b;
        if (component < 2)
        {
            remote.remove_prefix(remoteEnd + 1);
            installed.remove_prefix(installedEnd + 1);
        }
    }
    return false;
}

class CThunderstoreClient::CJson final
{
  public:
    static std::string String(const rapidjson::Value& value, const char* key)
    {
        const auto member = value.FindMember(key);
        return member != value.MemberEnd() && member->value.IsString() ? std::string(member->value.GetString(), member->value.GetStringLength())
                                                                       : std::string();
    }
    static uint64_t Number(const rapidjson::Value& value, const char* key)
    {
        const auto member = value.FindMember(key);
        return member != value.MemberEnd() && member->value.IsUint64() ? member->value.GetUint64() : 0;
    }
    static bool Boolean(const rapidjson::Value& value, const char* key)
    {
        const auto member = value.FindMember(key);
        return member != value.MemberEnd() && member->value.IsBool() && member->value.GetBool();
    }
    static bool Get(const std::string& url, rapidjson::Document& document, ModRequestError& error, const ModRequestOptions& options)
    {
        std::vector<uint8_t> bytes;
        if (!CModHttpClient::GetBytes(url, bytes, error, options))
        {
            error.message = "Thunderstore: " + error.message;
            return false;
        }
        document.Parse(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        if (document.HasParseError())
        {
            error = {ModRequestErrorCode::InvalidResponse, 0, 0, {}, "Invalid Thunderstore JSON"};
            return false;
        }
        return true;
    }
    static bool Version(const rapidjson::Value& value, PackageDetails& package, ModRequestError& error)
    {
        if (!value.IsObject())
            return Invalid(error);
        package.m_Version = String(value, "version_number");
        package.m_Description = String(value, "description");
        package.m_IconUrl = String(value, "icon");
        package.m_DownloadUrl = String(value, "download_url");
        package.m_FileSize = Number(value, "file_size");
        package.m_Downloads = Number(value, "downloads");
        package.m_Active = Boolean(value, "is_active");
        package.m_CreatedAt = String(value, "date_created");
        if (!IsSafePackageComponent(package.m_Namespace) || !IsSafePackageComponent(package.m_Name) || !IsSafeVersion(package.m_Version) ||
            String(value, "full_name") != package.m_Namespace + "-" + package.m_Name + "-" + package.m_Version ||
            !package.m_DownloadUrl.starts_with("https://"))
            return Invalid(error);
        const auto dependencies = value.FindMember("dependencies");
        if (dependencies == value.MemberEnd() || !dependencies->value.IsArray() || dependencies->value.Size() > 512)
            return Invalid(error);
        for (const auto& dependency : dependencies->value.GetArray())
        {
            if (!dependency.IsString() || BuildDownloadUrl(dependency.GetString()).empty())
                return Invalid(error);
            package.m_Dependencies.emplace_back(dependency.GetString(), dependency.GetStringLength());
        }
        return true;
    }
    static bool Invalid(ModRequestError& error)
    {
        error = {ModRequestErrorCode::InvalidResponse, 0, 0, {}, "Invalid Thunderstore package identity or metadata"};
        return false;
    }
};

bool CThunderstoreClient::IsSafePackageComponent(std::string_view value)
{
    return !value.empty() && value.size() <= 128 && std::ranges::all_of(value, [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    });
}

bool CThunderstoreClient::IsSafeVersion(std::string_view value)
{
    if (value.empty() || value.size() > 64)
        return false;
    int dots = 0;
    bool digit = false;
    for (char c : value)
    {
        if (c >= '0' && c <= '9')
            digit = true;
        else if (c == '.' && digit)
        {
            ++dots;
            digit = false;
        }
        else
            return false;
    }
    return dots == 2 && digit;
}

bool CThunderstoreClient::ParsePackageId(std::string_view id, std::string& outNamespace, std::string& outPackage)
{
    outNamespace.clear();
    outPackage.clear();
    const size_t separator = id.find('-');
    if (separator == std::string_view::npos || !IsSafePackageComponent(id.substr(0, separator)) || !IsSafePackageComponent(id.substr(separator + 1)))
        return false;
    outNamespace = id.substr(0, separator);
    outPackage = id.substr(separator + 1);
    return true;
}

bool CThunderstoreClient::ParsePackageUrl(const std::string& url, std::string& outNamespace, std::string& outPackage)
{
    outNamespace.clear();
    outPackage.clear();
    std::string_view path(url);
    if (path.starts_with("https://thunderstore.io/package/"))
        path.remove_prefix(std::string_view("https://thunderstore.io/package/").size());
    else if (path.starts_with("https://thunderstore.io/c/northstar/p/"))
        path.remove_prefix(std::string_view("https://thunderstore.io/c/northstar/p/").size());
    else if (path.starts_with("https://northstar.thunderstore.io/package/"))
        path.remove_prefix(std::string_view("https://northstar.thunderstore.io/package/").size());
    else
        return false;
    path = path.substr(0, path.find_first_of("?#"));
    if (path.ends_with('/'))
        path.remove_suffix(1);
    const size_t separator = path.find('/');
    if (separator == std::string_view::npos)
        return false;
    return ParsePackageId(std::string(path.substr(0, separator)) + "-" + std::string(path.substr(separator + 1)), outNamespace, outPackage);
}

std::string CThunderstoreClient::BuildDownloadUrl(std::string_view dependencyString)
{
    const size_t separator = dependencyString.rfind('-');
    std::string owner, name;
    if (separator == std::string_view::npos || !ParsePackageId(dependencyString.substr(0, separator), owner, name) ||
        !IsSafeVersion(dependencyString.substr(separator + 1)))
        return {};
    return std::string(CDN_PACKAGE_BASE) + std::string(dependencyString) + ".zip";
}

bool CThunderstoreClient::FetchPackageDetails(const std::string& namespaceName, const std::string& packageName, PackageDetails& out,
                                              const ModRequestOptions& options, ModRequestError* outputError)
{
    out = {};
    ModRequestError localError;
    ModRequestError& error = outputError ? *outputError : localError;
    error = {};
    if (!IsSafePackageComponent(namespaceName) || !IsSafePackageComponent(packageName))
    {
        error = {ModRequestErrorCode::InvalidArgument, 0, 0, {}, "Invalid Thunderstore package ID"};
        return false;
    }
    rapidjson::Document document;
    if (!CJson::Get(std::string(API_BASE) + namespaceName + "/" + packageName + "/", document, error, options))
        return false;
    if (!document.IsObject() || !document.HasMember("latest") || !document["latest"].IsObject())
        return CJson::Invalid(error);
    out.m_Name = CJson::String(document, "name");
    out.m_Namespace = CJson::String(document, "namespace");
    if (out.m_Name != packageName || out.m_Namespace != namespaceName)
        return CJson::Invalid(error);
    out.m_PageUrl = "https://thunderstore.io/c/northstar/p/" + out.m_Namespace + "/" + out.m_Name + "/";
    out.m_UpdatedAt = CJson::String(document, "date_updated");
    out.m_Deprecated = CJson::Boolean(document, "is_deprecated");
    out.m_Rating = CJson::Number(document, "rating_score");
    return CJson::Version(document["latest"], out, error);
}

bool CThunderstoreClient::FetchPackageVersion(const std::string& namespaceName, const std::string& packageName, const std::string& version,
                                              PackageDetails& out, const ModRequestOptions& options, ModRequestError* outputError)
{
    out = {};
    ModRequestError localError;
    ModRequestError& error = outputError ? *outputError : localError;
    error = {};
    if (!IsSafePackageComponent(namespaceName) || !IsSafePackageComponent(packageName) || !IsSafeVersion(version))
    {
        error = {ModRequestErrorCode::InvalidArgument, 0, 0, {}, "Invalid Thunderstore version ID"};
        return false;
    }
    rapidjson::Document document;
    if (!CJson::Get(std::string(API_BASE) + namespaceName + "/" + packageName + "/" + version + "/", document, error, options))
        return false;
    out.m_Namespace = namespaceName;
    out.m_Name = packageName;
    if (!CJson::Version(document, out, error))
        return false;
    if (out.m_Version != version)
        return CJson::Invalid(error);
    out.m_UpdatedAt = out.m_CreatedAt;
    out.m_PageUrl = "https://thunderstore.io/c/northstar/p/" + namespaceName + "/" + packageName + "/";
    return true;
}

bool CThunderstoreClient::FetchReadme(PackageDetails& package, ModRequestError& error, const ModRequestOptions& options)
{
    if (!IsSafePackageComponent(package.m_Namespace) || !IsSafePackageComponent(package.m_Name) || !IsSafeVersion(package.m_Version))
        return CJson::Invalid(error);
    rapidjson::Document document;
    if (!CJson::Get(std::string(API_BASE) + package.m_Namespace + "/" + package.m_Name + "/" + package.m_Version + "/readme/", document, error,
                    options))
        return false;
    if (!document.IsObject() || !document.HasMember("markdown") || !document["markdown"].IsString())
        return CJson::Invalid(error);
    package.m_Readme = CJson::String(document, "markdown");
    return true;
}

bool CThunderstoreClient::FetchCatalog(std::vector<PackageDetails>& packages, ModRequestError& error, const ModRequestOptions& options)
{
    packages.clear();
    rapidjson::Document document;
    if (!CJson::Get("https://thunderstore.io/c/northstar/api/v1/package/", document, error, options))
        return false;
    if (!document.IsArray() || document.Size() > 10000)
        return CJson::Invalid(error);
    packages.reserve(document.Size());
    for (const auto& value : document.GetArray())
    {
        if (options.isCancelled && options.isCancelled())
        {
            error = {ModRequestErrorCode::Cancelled, 0, 0, {}, "Thunderstore catalog cancelled"};
            return false;
        }
        if (!value.IsObject() || !value.HasMember("versions") || !value["versions"].IsArray() || value["versions"].Empty())
            return CJson::Invalid(error);
        PackageDetails package;
        package.m_Name = CJson::String(value, "name");
        package.m_Namespace = CJson::String(value, "owner");
        if (CJson::String(value, "full_name") != package.m_Namespace + "-" + package.m_Name || !CJson::Version(value["versions"][0], package, error))
            return CJson::Invalid(error);
        package.m_Downloads = 0;
        for (const auto& version : value["versions"].GetArray())
        {
            if (!version.IsObject())
                return CJson::Invalid(error);
            package.m_Downloads += CJson::Number(version, "downloads");
        }
        package.m_Rating = CJson::Number(value, "rating_score");
        package.m_Deprecated = CJson::Boolean(value, "is_deprecated");
        package.m_UpdatedAt = CJson::String(value, "date_updated");
        package.m_CreatedAt = CJson::String(value, "date_created");
        package.m_PageUrl = "https://thunderstore.io/c/northstar/p/" + package.m_Namespace + "/" + package.m_Name + "/";
        packages.push_back(std::move(package));
    }
    return true;
}
