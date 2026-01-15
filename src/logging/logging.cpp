#include "logging.h"
#include "core/convar/convar.h"
#include "core/convar/concommand.h"
#include "config/profile.h"
#include "core/tier0.h"
#include "util/version.h"
#include "client/r2client.h"
#include "dedicated/dedicated.h"

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/dist_sink.h>
#include <winternl.h>
#include <cstdlib>
#include <deque>
#include <iomanip>
#include <sstream>

static std::mutex g_LoggersMutex;
static std::vector<std::shared_ptr<spdlog::logger>> g_Loggers {};
static std::shared_ptr<spdlog::sinks::dist_sink_mt> g_DispatchSink;

namespace
{
	class RecentLogRingBufferSink final : public spdlog::sinks::base_sink<std::mutex>
	{
	public:
		explicit RecentLogRingBufferSink(size_t capacity, size_t maxLineBytes = 4096)
			: m_Capacity(capacity)
			, m_MaxLineBytes(maxLineBytes)
		{
			if (m_Capacity == 0)
				m_Capacity = 1;
			if (m_MaxLineBytes == 0)
				m_MaxLineBytes = 1;
		}

		std::vector<std::string> GetLastLines(size_t maxLines)
		{
			std::lock_guard<std::mutex> lock(this->mutex_);
			if (maxLines == 0 || m_Lines.empty())
				return {};

			const size_t count = std::min(maxLines, m_Lines.size());
			std::vector<std::string> out;
			out.reserve(count);
			const size_t start = m_Lines.size() - count;
			for (size_t i = start; i < m_Lines.size(); i++)
				out.emplace_back(m_Lines[i]);
			return out;
		}

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override
		{
			spdlog::memory_buf_t formatted;
			spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);

			std::string line = fmt::to_string(formatted);
			while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
				line.pop_back();
			if (line.size() > m_MaxLineBytes)
				line.resize(m_MaxLineBytes);

			m_Lines.emplace_back(std::move(line));
			if (m_Lines.size() > m_Capacity)
				m_Lines.pop_front();
		}

		void flush_() override {}

	private:
		size_t m_Capacity;
		size_t m_MaxLineBytes;
		std::deque<std::string> m_Lines;
	};

	static std::shared_ptr<RecentLogRingBufferSink> g_RecentLogSink;
}

namespace NS::log
{
	std::mutex g_LoggerColorMutex;
	std::map<std::string, std::string> g_LoggerAnsiColors;
	std::map<std::string, SourceColor> g_LoggerSourceColors;

	void RegisterLoggerColors(const std::string& loggerName, const Color& color)
	{
		std::scoped_lock lock(g_LoggerColorMutex);
		g_LoggerAnsiColors[loggerName] = Color(color).ToANSIColor();
		g_LoggerSourceColors[loggerName] = Color(color).ToSourceColor();
	}

	const std::string& GetAnsiColorForLoggerName(std::string_view loggerName)
	{
		static const std::string kDefault = "\033[39;49m";
		std::scoped_lock lock(g_LoggerColorMutex);
		auto it = g_LoggerAnsiColors.find(std::string(loggerName));
		if (it != g_LoggerAnsiColors.end())
			return it->second;
		return kDefault;
	}

	SourceColor GetSourceColorForLoggerName(std::string_view loggerName)
	{
		std::scoped_lock lock(g_LoggerColorMutex);
		auto it = g_LoggerSourceColors.find(std::string(loggerName));
		if (it != g_LoggerSourceColors.end())
			return it->second;
		return SourceColor(255, 255, 255, 255);
	}
};

namespace NS::log
{
	std::shared_ptr<spdlog::logger> SCRIPT_UI;
	std::shared_ptr<spdlog::logger> SCRIPT_CL;
	std::shared_ptr<spdlog::logger> SCRIPT_SV;

	std::shared_ptr<spdlog::logger> NATIVE_UI;
	std::shared_ptr<spdlog::logger> NATIVE_CL;
	std::shared_ptr<spdlog::logger> NATIVE_SV;
	std::shared_ptr<spdlog::logger> NATIVE_EN;
	std::shared_ptr<spdlog::logger> EOS;

	std::shared_ptr<spdlog::logger> fs;
	std::shared_ptr<spdlog::logger> rpak;
	std::shared_ptr<spdlog::logger> echo;

	std::shared_ptr<spdlog::logger> NORTHSTAR;
	std::shared_ptr<spdlog::logger> PLUGINSYS;
}; // namespace NS::log

static void EnsureAsyncThreadPoolInitialized()
{
	if (spdlog::thread_pool())
		return;

	spdlog::init_thread_pool(8192, 1);
}

static void EnsureDispatchSinkInitialized()
{
	if (g_DispatchSink)
		return;

	g_DispatchSink = std::make_shared<spdlog::sinks::dist_sink_mt>();
}

static std::shared_ptr<spdlog::logger> CreateAsyncLoggerInternal(const std::string& name)
{
	EnsureAsyncThreadPoolInitialized();
	EnsureDispatchSinkInitialized();

	if (auto existing = spdlog::get(name))
		return existing;

	std::scoped_lock lock(g_LoggersMutex);
	std::array<spdlog::sink_ptr, 1> sinks {g_DispatchSink};
	auto logger = std::make_shared<spdlog::async_logger>(
		name,
		sinks.begin(),
		sinks.end(),
		spdlog::thread_pool(),
		spdlog::async_overflow_policy::block);

	try
	{
		spdlog::register_logger(logger);
	}
	catch (...)
	{
		if (auto raced = spdlog::get(name))
			return raced;
		throw;
	}
	g_Loggers.push_back(logger);
	return logger;
}

// This needs to be called after hooks are loaded so we can access the command line args
void LogSys_CreateLogFiles()
{
	if (strstr(GetCommandLineA(), "-disablelogs"))
	{
		spdlog::default_logger()->set_level(spdlog::level::off);
	}
	else
	{
		try
		{
			// todo: might be good to delete logs that are too old
			time_t time = std::time(nullptr);
			tm currentTime = *std::localtime(&time);
			std::stringstream stream;

			stream << std::put_time(&currentTime, (GetNorthstarPrefix() + "/logs/nslog%Y-%m-%d %H-%M-%S.txt").c_str());
			auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(stream.str(), false);
			sink->set_pattern("[%Y-%m-%d] [%H:%M:%S] [%n] %v");
			RegisterSink(sink);
			spdlog::flush_on(spdlog::level::info);
		}
		catch (...)
		{
			spdlog::error("Failed creating log file!");
			MessageBoxA(
				0, "Failed creating log file! Make sure the profile directory is writable.", "Northstar Warning", MB_ICONWARNING | MB_OK);
		}
	}
}

void ExternalConsoleSink::sink_it_(const spdlog::details::log_msg& msg)
{
	spdlog::memory_buf_t formatted;
	spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);

	std::string message = fmt::to_string(formatted);
	std::string name {msg.logger_name.begin(), msg.logger_name.end()};

	std::string out;
	if (g_bSpdLog_UseAnsiColor)
		out += NS::log::GetAnsiColorForLoggerName(name);
	out += "[" + name + "]";
	if (g_bSpdLog_UseAnsiColor)
		out += default_color;
	out += " ";

	if (!IsDedicatedServer() && GetBaseLocalClient)
	{
		if (CClientState* client = GetBaseLocalClient(); client && client->m_nSignonState >= eSignonState::CONNECTED)
		{
			const std::string uptimeStr = fmt::format("{:.3f}", client->m_flServerUptime);
			out += "[" + uptimeStr + "] ";
		}
	}

	if (g_bSpdLog_UseAnsiColor && msg.level != spdlog::level::info)
		out += m_LogColours[msg.level];
	out += message;
	if (g_bSpdLog_UseAnsiColor && msg.level != spdlog::level::info)
		out += default_color;

	// print the string to the console - this is definitely bad i think
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	auto ignored = WriteConsoleA(handle, out.c_str(), (DWORD)std::strlen(out.c_str()), nullptr, nullptr);
	(void)ignored;
}

void ExternalConsoleSink::flush_()
{
	std::cout << std::flush;
}

void LogSys_InitialiseConsole()
{
	if (AllocConsole() != FALSE)
	{
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
	}

	// this if statement is adapted from r5sdk
	if (!strstr(GetCommandLineA(), "-noansiclr"))
	{
		g_bSpdLog_UseAnsiColor = true;
		DWORD dwMode = 0;
		HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);

		GetConsoleMode(hOutput, &dwMode);
		dwMode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;

		if (!SetConsoleMode(hOutput, dwMode)) // Some editions of Windows have 'VirtualTerminalLevel' disabled by default.
		{
			// If 'VirtualTerminalLevel' can't be set, just disable ANSI color, since it wouldnt work anyway.
			spdlog::warn("could not set VirtualTerminalLevel. Disabling color output");
			g_bSpdLog_UseAnsiColor = false;
		}
	}
}

void RegisterLogger(std::shared_ptr<spdlog::logger> logger)
{
	std::scoped_lock lock(g_LoggersMutex);
	try
	{
		spdlog::register_logger(logger);
	}
	catch (...)
	{
		// ignore duplicates
	}
	g_Loggers.push_back(logger);
}

void RegisterSink(spdlog::sink_ptr sink)
{
	std::scoped_lock lock(g_LoggersMutex);
	EnsureDispatchSinkInitialized();
	// Thread-safe: dist_sink_mt guards its internal sink list.
	g_DispatchSink->add_sink(std::move(sink));
};

std::shared_ptr<spdlog::logger> CreateLogger(std::string name, const Color& color)
{
	NS::log::RegisterLoggerColors(name, color);
	return CreateAsyncLoggerInternal(name);
}

void LogSys_InitialiseLogging()
{
	EnsureAsyncThreadPoolInitialized();

	// In-memory recent-log ring buffer for crash reporting.
	{
		auto recentSink = std::make_shared<RecentLogRingBufferSink>(200);
		recentSink->set_pattern("[%Y-%m-%d] [%H:%M:%S] [%n] [%l] %v");
		g_RecentLogSink = recentSink;
		RegisterSink(std::move(recentSink));
	}

	auto consoleSink = std::make_shared<ExternalConsoleSink>();
	consoleSink->set_pattern("%v");
	RegisterSink(consoleSink);

	NS::log::NORTHSTAR = CreateLogger("NORTHSTAR", NS::Colors::NORTHSTAR);
	spdlog::set_default_logger(NS::log::NORTHSTAR);

	NS::log::SCRIPT_UI = CreateLogger("SCRIPT UI", NS::Colors::SCRIPT_UI);
	NS::log::SCRIPT_CL = CreateLogger("SCRIPT CL", NS::Colors::SCRIPT_CL);
	NS::log::SCRIPT_SV = CreateLogger("SCRIPT SV", NS::Colors::SCRIPT_SV);

	NS::log::NATIVE_UI = CreateLogger("NATIVE UI", NS::Colors::NATIVE_UI);
	NS::log::NATIVE_CL = CreateLogger("NATIVE CL", NS::Colors::NATIVE_CL);
	NS::log::NATIVE_SV = CreateLogger("NATIVE SV", NS::Colors::NATIVE_SV);
	NS::log::NATIVE_EN = CreateLogger("NATIVE EN", NS::Colors::NATIVE_ENGINE);
	NS::log::EOS = CreateLogger(" EOS P2P ", NS::Colors::EOS);

	NS::log::fs = CreateLogger("FILESYSTM", NS::Colors::FILESYSTEM);
	NS::log::rpak = CreateLogger("RPAK_FSYS", NS::Colors::RPAK);
	NS::log::echo = CreateLogger("ECHO", NS::Colors::ECHO);

	NS::log::PLUGINSYS = CreateLogger("PLUGINSYS", NS::Colors::PLUGINSYS);
}

std::vector<std::string> NS::log::GetRecentLogLines(size_t maxLines)
{
	auto sink = g_RecentLogSink;
	if (!sink)
		return {};
	return sink->GetLastLines(maxLines);
}

void NS::log::FlushLoggers()
{
	std::scoped_lock lock(g_LoggersMutex);
	for (auto& logger : g_Loggers)
		logger->flush();

	spdlog::default_logger()->flush();
}

// Wine specific functions
typedef const char*(CDECL* wine_get_host_version_type)(const char**, const char**);
wine_get_host_version_type wine_get_host_version;

typedef const char*(CDECL* wine_get_build_id_type)(void);
wine_get_build_id_type wine_get_build_id;

// Not exported Winapi methods
typedef NTSTATUS(WINAPI* RtlGetVersion_type)(PRTL_OSVERSIONINFOW);
RtlGetVersion_type RtlGetVersion;

void LogSys_StartupLog()
{
	spdlog::info("NorthstarLauncher version: {}", version);
	spdlog::info("Command line: {}", GetCommandLineA());
	spdlog::info("Using profile: {}", GetNorthstarPrefix());

	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	if (!ntdll)
	{
		// How did we get here
		spdlog::info("Operating System: Unknown");
		return;
	}

	wine_get_host_version = (wine_get_host_version_type)GetProcAddress(ntdll, "wine_get_host_version");
	if (wine_get_host_version)
	{
		// Load the rest of the functions we need
		wine_get_build_id = (wine_get_build_id_type)GetProcAddress(ntdll, "wine_get_build_id");

		const char* sysname;
		wine_get_host_version(&sysname, NULL);

		spdlog::info("Operating System: {} (Wine)", sysname);
		spdlog::info("Wine build: {}", wine_get_build_id());

		// STEAM_COMPAT_TOOL_PATHS is a colon separated lists of all compat tool paths used
		// The first one tends to be the Proton path itself
		// We extract the basename out of it to get the name used
		char* compatToolPtr = std::getenv("STEAM_COMPAT_TOOL_PATHS");
		if (compatToolPtr)
		{
			std::string_view compatToolPath(compatToolPtr);

			auto protonBasenameEnd = compatToolPath.find(":");
			if (protonBasenameEnd == std::string_view::npos)
				protonBasenameEnd = 0;
			auto protonBasenameStart = compatToolPath.rfind("/", protonBasenameEnd) + 1;
			if (protonBasenameStart == std::string_view::npos)
				protonBasenameStart = 0;

			spdlog::info("Proton build: {}", compatToolPath.substr(protonBasenameStart, protonBasenameEnd - protonBasenameStart));
		}
	}
	else
	{
		// We are real Windows (hopefully)
		const char* win_ver = "Unknown";

		RTL_OSVERSIONINFOW osvi;
		osvi.dwOSVersionInfoSize = sizeof(osvi);

		RtlGetVersion = (RtlGetVersion_type)GetProcAddress(ntdll, "RtlGetVersion");
		if (RtlGetVersion && !RtlGetVersion(&osvi))
		{
			// Version reference table
			// https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-osversioninfoa#remarks
			spdlog::info("Operating System: Windows (NT{}.{})", osvi.dwMajorVersion, osvi.dwMinorVersion);
		}
		else
		{
			spdlog::info("Operating System: Windows");
		}
	}
}
