#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

enum class ModRequestErrorCode
{
    None,
    InvalidArgument,
    Cancelled,
    Network,
    Http,
    RateLimited,
    ResponseTooLarge,
    InvalidResponse,
};

struct ModRequestError
{
    ModRequestErrorCode code = ModRequestErrorCode::None;
    long httpStatus = 0;
    int transportCode = 0;
    std::chrono::seconds retryAfter{};
    std::string message;
};

struct ModRequestOptions
{
    size_t maxResponseBytes = 4 * 1024 * 1024;
    long connectTimeoutSeconds = 10;
    long timeoutSeconds = 30;
    std::function<bool()> isCancelled;
    std::function<void(uint64_t, uint64_t)> progress;
};

class CModHttpClient final
{
  public:
    static bool GetBytes(std::string_view url, std::vector<uint8_t>& bytes, ModRequestError& error, const ModRequestOptions& options = {});
    static bool GetFile(std::string_view url, const std::filesystem::path& destination, uint64_t& downloadedBytes, ModRequestError& error,
                        const ModRequestOptions& options = {});

  private:
    class CRequest;
};
