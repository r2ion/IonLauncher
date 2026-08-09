#pragma once

#include "logging/logging.h"
#include "GameUI/GameConsole.h"
#include <spdlog/sinks/base_sink.h>
#include <map>

extern CGameConsole* g_pGameConsole;


// spdlog logger
class SourceConsoleSink : public spdlog::sinks::base_sink<std::mutex>
{
private:
	std::map<spdlog::level::level_enum, SourceColor> m_LogColours = {
		{spdlog::level::trace, NS::Colors::TRACE.ToSourceColor()},
		{spdlog::level::debug, NS::Colors::DEBUG.ToSourceColor()},
		{spdlog::level::info, NS::Colors::INFO.ToSourceColor()},
		{spdlog::level::warn, NS::Colors::WARN.ToSourceColor()},
		{spdlog::level::err, NS::Colors::ERR.ToSourceColor()},
		{spdlog::level::critical, NS::Colors::CRIT.ToSourceColor()},
		{spdlog::level::off, NS::Colors::OFF.ToSourceColor()}};

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override;
	void flush_() override;
};

void InitialiseConsoleOnInterfaceCreation();
