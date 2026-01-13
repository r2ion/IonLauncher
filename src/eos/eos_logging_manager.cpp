#include "eos_logging_manager.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <cwchar>

#include "util/platform.h"

#include "eos_threading.h"

#include <eos_logging.h>

namespace
{

bool g_loggingInitialized = false;

bool HasCommandLineFlag(const wchar_t* flag)
{
    if (!flag)
        return false;

    // Convert wide string flag to narrow string for HasEngineCommandLineFlag
    char narrowFlag[256];
    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, narrowFlag, sizeof(narrowFlag), flag, _TRUNCATE);

    return HasEngineCommandLineFlag(narrowFlag);
}

EOS_ELogLevel DetermineLogLevel()
{
#if BUILD_DEBUG
    return EOS_ELogLevel::EOS_LOG_VeryVerbose;
#else
    if (HasCommandLineFlag(L"-eoslogverbose"))
        return EOS_ELogLevel::EOS_LOG_VeryVerbose;
    return EOS_ELogLevel::EOS_LOG_Info;
#endif
}

void EOS_CALL OnLogMessageReceived(const EOS_LogMessage* message)
{
    if (!message || !message->Message)
        return;

	switch (message->Level)
	{
		case EOS_ELogLevel::EOS_LOG_Fatal:
		case EOS_ELogLevel::EOS_LOG_Error:
			NS::log::EOS->error("[{}] {}", message->Category ? message->Category : "Unknown", message->Message);
			break;
		case EOS_ELogLevel::EOS_LOG_Warning:
			NS::log::EOS->warn("[{}] {}", message->Category ? message->Category : "Unknown", message->Message);
			break;
		case EOS_ELogLevel::EOS_LOG_Info:
			NS::log::EOS->info("[{}] {}", message->Category ? message->Category : "Unknown", message->Message);
			break;
		case EOS_ELogLevel::EOS_LOG_Verbose:
			NS::log::EOS->debug("[{}] {}", message->Category ? message->Category : "Unknown", message->Message);
			break;
		case EOS_ELogLevel::EOS_LOG_VeryVerbose:
			NS::log::EOS->trace("[{}] {}", message->Category ? message->Category : "Unknown", message->Message);
			break;
		default:
			NS::log::EOS->info("[{}] {}", message->Category ? message->Category : "Unknown", message->Message);
			break;
	}
}

} // namespace

namespace eos
{

void Logging::Initialize()
{
    if (g_loggingInitialized)
        return;

    EOS_EResult callbackResult = EOS_EResult::EOS_UnexpectedError;
    {
        SdkLock lock(GetSdkMutex());
        callbackResult = EOS_Logging_SetCallback(&OnLogMessageReceived);
    }
    if (callbackResult != EOS_EResult::EOS_Success)
    {
        NS::log::EOS->error("Failed to register logging callback ({})", static_cast<int>(callbackResult));
        return;
    }

    const EOS_ELogLevel level = DetermineLogLevel();
    EOS_EResult levelResult = EOS_EResult::EOS_UnexpectedError;
    {
        SdkLock lock(GetSdkMutex());
        levelResult = EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, level);
    }
    if (levelResult != EOS_EResult::EOS_Success)
    {
        NS::log::EOS->error("Failed to set log level ({})", static_cast<int>(levelResult));
    }

    g_loggingInitialized = true;
}

void Logging::Shutdown()
{
    if (!g_loggingInitialized)
        return;

    {
        SdkLock lock(GetSdkMutex());
        EOS_Logging_SetCallback(nullptr);
    }

    g_loggingInitialized = false;
}

} // namespace eos
