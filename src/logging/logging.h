#pragma once

#include <map>
#include <mutex>
#include <spdlog/common.h>
#include <spdlog/details/fmt_helper.h>
#include <spdlog/fmt/std.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

#include "vscript/languages/squirrel_re/squirrel.h"
#include "mathlib/color.h"

void LogSys_CreateLogFiles();
void LogSys_InitialiseLogging();
void LogSys_InitialiseConsole();
void LogSys_StartupLog();

namespace NS::log
{
	using LoggerPtr = std::shared_ptr<spdlog::logger>;

	extern std::mutex g_LoggerColorMutex;
	extern std::map<std::string, std::string> g_LoggerAnsiColors;
	extern std::map<std::string, SourceColor> g_LoggerSourceColors;

	void RegisterLoggerColors(const std::string& loggerName, const Color& color);
	const std::string& GetAnsiColorForLoggerName(std::string_view loggerName);
	SourceColor GetSourceColorForLoggerName(std::string_view loggerName);

	// Squirrel
	extern LoggerPtr SCRIPT_UI;
	extern LoggerPtr SCRIPT_CL;
	extern LoggerPtr SCRIPT_SV;

	// Native code
	extern LoggerPtr NATIVE_UI;
	extern LoggerPtr NATIVE_CL;
	extern LoggerPtr NATIVE_SV;
	extern LoggerPtr NATIVE_EN;

	// File system
	extern LoggerPtr fs;
	// RPak
	extern LoggerPtr rpak;
	// Echo
	extern LoggerPtr echo;

	extern LoggerPtr NORTHSTAR;
	extern LoggerPtr MILES;

	extern LoggerPtr PLUGINSYS;

	// p2p
	extern LoggerPtr EOS;
	extern LoggerPtr RUI;

	void FlushLoggers();

	// Returns the most recent log lines (oldest->newest). Empty if logging hasn't been initialised yet.
	std::vector<std::string> GetRecentLogLines(size_t maxLines = 200);
}; // namespace NS::log

void RegisterSink(spdlog::sink_ptr sink);
void RegisterLogger(std::shared_ptr<spdlog::logger> logger);
std::shared_ptr<spdlog::logger> CreateLogger(std::string name, const Color& color);

inline bool g_bSpdLog_UseAnsiColor = true;

// Could maybe use some different names here, idk
static const char* level_names[] {"trac", "dbug", "info", "warn", "errr", "crit", "off"};

// spdlog logger, for cool colour things
class ExternalConsoleSink : public spdlog::sinks::base_sink<std::mutex>
{
private:
	std::map<spdlog::level::level_enum, std::string> m_LogColours = {
		{spdlog::level::trace, NS::Colors::TRACE.ToANSIColor()},
		{spdlog::level::debug, NS::Colors::DEBUG.ToANSIColor()},
		{spdlog::level::info, NS::Colors::INFO.ToANSIColor()},
		{spdlog::level::warn, NS::Colors::WARN.ToANSIColor()},
		{spdlog::level::err, NS::Colors::ERR.ToANSIColor()},
		{spdlog::level::critical, NS::Colors::CRIT.ToANSIColor()},
		{spdlog::level::off, NS::Colors::OFF.ToANSIColor()}};

	std::string default_color = "\033[39;49m";

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override;
	void flush_() override;
};
