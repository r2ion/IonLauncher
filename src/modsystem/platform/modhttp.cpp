#include "modsystem/platform/modhttp.h"
#include "ns_version.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <curl/curl.h>
#include <format>
#include <limits>

class CModHttpClient::CRequest final
{
  public:
    struct ResponseBuffer
    {
        std::vector<uint8_t>* m_pBytes = nullptr;
        FILE* m_pFile = nullptr;
        size_t m_MaximumBytes = 0;
        size_t m_ReceivedBytes = 0;
        bool m_Overflowed = false;
    };

    struct HeaderState
    {
        std::chrono::seconds m_RetryAfter{};
    };

    struct ProgressState
    {
        const ModRequestOptions* m_pOptions = nullptr;
    };

    static std::string_view Trim(std::string_view value);
    static bool EqualsCaseInsensitive(std::string_view left, std::string_view right);
    static size_t WriteResponse(void* data, size_t size, size_t count, void* userData);
    static size_t ReadHeaders(char* data, size_t size, size_t count, void* userData);
    static int ReportProgress(void* userData, curl_off_t downloadTotal, curl_off_t downloaded, curl_off_t uploadTotal, curl_off_t uploaded);
    static bool PerformGet(std::string_view url, ResponseBuffer& response, ModRequestError& error, const ModRequestOptions& options);
};

std::string_view CModHttpClient::CRequest::Trim(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);
    return value;
}

bool CModHttpClient::CRequest::EqualsCaseInsensitive(std::string_view left, std::string_view right)
{
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](char a, char b)
    { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
}

size_t CModHttpClient::CRequest::WriteResponse(void* data, size_t size, size_t count, void* userData)
{
    ResponseBuffer& response = *static_cast<ResponseBuffer*>(userData);
    if ((!response.m_pBytes && !response.m_pFile) || size != 0 && count > std::numeric_limits<size_t>::max() / size)
    {
        return 0;
    }

    const size_t byteCount = size * count;
    if (byteCount > response.m_MaximumBytes || response.m_ReceivedBytes > response.m_MaximumBytes - byteCount)
    {
        response.m_Overflowed = true;
        return 0;
    }

    if (response.m_pBytes)
    {
        const auto* first = static_cast<const uint8_t*>(data);
        response.m_pBytes->insert(response.m_pBytes->end(), first, first + byteCount);
    }
    else if (std::fwrite(data, 1, byteCount, response.m_pFile) != byteCount)
    {
        return 0;
    }
    response.m_ReceivedBytes += byteCount;
    return byteCount;
}

size_t CModHttpClient::CRequest::ReadHeaders(char* data, size_t size, size_t count, void* userData)
{
    HeaderState& headers = *static_cast<HeaderState*>(userData);
    const size_t byteCount = size * count;
    const std::string_view line(data, byteCount);
    const size_t separator = line.find(':');
    if (separator == std::string_view::npos)
        return byteCount;

    const std::string_view name = Trim(line.substr(0, separator));
    const std::string_view value = Trim(line.substr(separator + 1));
    if (EqualsCaseInsensitive(name, "Retry-After"))
    {
        long seconds = 0;
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), seconds);
        if (error == std::errc() && end == value.data() + value.size() && seconds > 0)
            headers.m_RetryAfter = std::chrono::seconds(seconds);
    }
    return byteCount;
}

int CModHttpClient::CRequest::ReportProgress(void* userData, curl_off_t downloadTotal, curl_off_t downloaded, curl_off_t, curl_off_t)
{
    const ProgressState& progress = *static_cast<ProgressState*>(userData);
    if (!progress.m_pOptions)
        return 0;
    if (progress.m_pOptions->isCancelled && progress.m_pOptions->isCancelled())
        return 1;
    if (progress.m_pOptions->progress)
    {
        progress.m_pOptions->progress(downloaded > 0 ? static_cast<uint64_t>(downloaded) : 0,
                                      downloadTotal > 0 ? static_cast<uint64_t>(downloadTotal) : 0);
    }
    return 0;
}

bool CModHttpClient::CRequest::PerformGet(std::string_view url, ResponseBuffer& response, ModRequestError& error, const ModRequestOptions& options)
{
    error = {};
    if (!url.starts_with("https://"))
    {
        error.code = ModRequestErrorCode::InvalidArgument;
        error.message = "Mod requests require an HTTPS URL";
        return false;
    }
    if (options.maxResponseBytes == 0 || options.timeoutSeconds <= 0 || options.connectTimeoutSeconds <= 0)
    {
        error.code = ModRequestErrorCode::InvalidArgument;
        error.message = "Mod request limits must be positive";
        return false;
    }
    if (options.isCancelled && options.isCancelled())
    {
        error.code = ModRequestErrorCode::Cancelled;
        error.message = "Mod request cancelled";
        return false;
    }

    CURL* easy = curl_easy_init();
    if (!easy)
    {
        error.code = ModRequestErrorCode::Network;
        error.message = "Failed creating a libcurl request";
        return false;
    }

    response.m_MaximumBytes = options.maxResponseBytes;
    HeaderState headers;
    ProgressState progress{&options};
    const std::string ownedUrl(url);
    curl_easy_setopt(easy, CURLOPT_URL, ownedUrl.c_str());
    curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(easy, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, options.connectTimeoutSeconds);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT, options.timeoutSeconds);
    curl_easy_setopt(easy, CURLOPT_LOW_SPEED_TIME, 15L);
    curl_easy_setopt(easy, CURLOPT_LOW_SPEED_LIMIT, 64L);
    curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_USERAGENT, "Northstar/" NORTHSTAR_VERSION_STR " (+https://northstar.tf)");
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, WriteResponse);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, ReadHeaders);
    curl_easy_setopt(easy, CURLOPT_HEADERDATA, &headers);
    curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(easy, CURLOPT_XFERINFOFUNCTION, ReportProgress);
    curl_easy_setopt(easy, CURLOPT_XFERINFODATA, &progress);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(easy, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#endif

    const CURLcode result = curl_easy_perform(easy);
    long httpStatus = 0;
    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &httpStatus);
    curl_easy_cleanup(easy);

    error.httpStatus = httpStatus;
    error.transportCode = static_cast<int>(result);
    error.retryAfter = headers.m_RetryAfter;
    if (response.m_Overflowed)
    {
        error.code = ModRequestErrorCode::ResponseTooLarge;
        error.message = std::format("Mod response exceeded the {} byte limit", options.maxResponseBytes);
        if (response.m_pBytes)
            response.m_pBytes->clear();
        return false;
    }
    if (result == CURLE_ABORTED_BY_CALLBACK || options.isCancelled && options.isCancelled())
    {
        error.code = ModRequestErrorCode::Cancelled;
        error.message = "Mod request cancelled";
        if (response.m_pBytes)
            response.m_pBytes->clear();
        return false;
    }
    if (result != CURLE_OK)
    {
        error.code = ModRequestErrorCode::Network;
        error.message = std::format("Mod request failed: {}", curl_easy_strerror(result));
        if (response.m_pBytes)
            response.m_pBytes->clear();
        return false;
    }
    if (httpStatus == 429)
    {
        error.code = ModRequestErrorCode::RateLimited;
        error.message = "Mod rate limit reached";
        return false;
    }
    if (httpStatus < 200 || httpStatus >= 300)
    {
        error.code = ModRequestErrorCode::Http;
        error.message = std::format("Mod returned HTTP {}", httpStatus);
        return false;
    }
    return true;
}

bool CModHttpClient::GetBytes(std::string_view url, std::vector<uint8_t>& bytes, ModRequestError& error, const ModRequestOptions& options)
{
    bytes.clear();
    CRequest::ResponseBuffer response;
    response.m_pBytes = &bytes;
    return CRequest::PerformGet(url, response, error, options);
}

bool CModHttpClient::GetFile(std::string_view url, const std::filesystem::path& destination, uint64_t& downloadedBytes, ModRequestError& error,
                             const ModRequestOptions& options)
{
    downloadedBytes = 0;
    FILE* file = _wfopen(destination.c_str(), L"wb");
    if (!file)
    {
        error = {ModRequestErrorCode::InvalidArgument, 0, 0, {}, std::format("Failed opening download destination '{}'", destination.string())};
        return false;
    }

    CRequest::ResponseBuffer response;
    response.m_pFile = file;
    const bool success = CRequest::PerformGet(url, response, error, options);
    const bool flushSucceeded = std::fflush(file) == 0;
    std::fclose(file);
    if (!success || !flushSucceeded)
    {
        std::error_code ignored;
        std::filesystem::remove(destination, ignored);
        if (success)
        {
            error.code = ModRequestErrorCode::Network;
            error.message = "Failed flushing downloaded file";
        }
        return false;
    }
    downloadedBytes = response.m_ReceivedBytes;
    return true;
}
