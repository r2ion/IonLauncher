#include "core/convar/convar.h"
#include "sourceconsole.h"
#include "core/tier1.h"
#include "core/convar/concommand.h"
#include "util/printcommands.h"
#include "client/r2client.h"
#include "tier0/module.h"
#include <core/tier0.h>

CGameConsole* g_pGameConsole;

// Tier0 console functions
typedef void (*ConColorMsgType)(const Color& clr, const char* pszFormat, ...);
typedef void (*ConMsgType)(const char* pszFormat, ...);

static ConColorMsgType ConColorMsg = nullptr;
static ConMsgType ConMsg = nullptr;

void ConCommand_toggleconsole(const CCommand& arg)
{
	NOTE_UNUSED(arg);
	if (g_pGameConsole->IsConsoleVisible())
		g_pGameConsole->Hide();
	else
		g_pGameConsole->Activate();
}

void ConCommand_showconsole(const CCommand& arg)
{
	NOTE_UNUSED(arg);
	g_pGameConsole->Activate();
}

void ConCommand_hideconsole(const CCommand& arg)
{
	NOTE_UNUSED(arg);
	g_pGameConsole->Hide();
}

void SourceConsoleSink::sink_it_(const spdlog::details::log_msg& msg)
{
	if (!g_pGameConsole->m_bInitialized || !ConColorMsg || !ConMsg)
		return;

	spdlog::memory_buf_t formatted;
	spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);

	// get message string
	std::string str = fmt::to_string(formatted);

	SourceColor levelColor = m_LogColours[msg.level];
	std::string name {msg.logger_name.begin(), msg.logger_name.end()};
	SourceColor loggerColor = NS::log::GetSourceColorForLoggerName(name);

	CClientState* client = GetBaseLocalClient();

	// Convert SourceColor to Color for tier0 functions
	Color loggerColorConverted(loggerColor.R, loggerColor.G, loggerColor.B, loggerColor.A);
	Color levelColorConverted(levelColor.R, levelColor.G, levelColor.B, levelColor.A);

	// set time to be not the logger color
	if (msg.level != spdlog::level::info)
		ConColorMsg(levelColorConverted, "");
	else
		ConMsg("");

	ConColorMsg(loggerColorConverted, "[%s] ", name.c_str());

	if (msg.level != spdlog::level::info)
		ConColorMsg(levelColorConverted, "%s", str.c_str());
	else
		ConMsg("%s", str.c_str());
}

void SourceConsoleSink::flush_() {}

// clang-format off
HOOK(OnCommandSubmittedHook, OnCommandSubmitted,
void, __fastcall, (CConsoleDialog* consoleDialog, const char* pCommand))
// clang-format on
{
	if (ConMsg)
		ConMsg("] %s\n", pCommand);

	TryPrintCvarHelpForCommand(pCommand);

	OnCommandSubmitted(consoleDialog, pCommand);
}

// called from sourceinterface.cpp in client createinterface hooks, on GameClientExports001
void InitialiseConsoleOnInterfaceCreation()
{
	g_pGameConsole->Initialize();
	// hook OnCommandSubmitted so we print inputted commands
	OnCommandSubmittedHook.Dispatch((LPVOID)g_pGameConsole->m_pConsole->m_vtable->OnCommandSubmitted);
	CCommandLine* cmdLine = CommandLine();

	if (cmdLine)
	{
		// Check if we should NOT activate
		bool hasForceStartupMenu = cmdLine->FindParm("-forcestartupmenu") != 0;
		bool hasHideConsole = cmdLine->FindParm("-hideconsole") != 0;

		if (!hasForceStartupMenu && !hasHideConsole)
		{
			// Check if we have any of the activation flags
			bool hasToConsole = cmdLine->FindParm("-toconsole") != 0;
			bool hasConsole = cmdLine->FindParm("-console") != 0;
			bool hasRpt = cmdLine->FindParm("-rpt") != 0;
			bool hasAllowDebug = cmdLine->FindParm("-allowdebug") != 0;

			if (hasToConsole || hasConsole || hasRpt || hasAllowDebug)
			{
				spdlog::info("Activating GameConsole based on command line flags");
				g_pGameConsole->Activate();
			}
		}
	}
	auto consoleSink = std::make_shared<SourceConsoleSink>();
	if (g_bSpdLog_UseAnsiColor)
		consoleSink->set_pattern("%v"); // no need to include the level in the game console, the text colour signifies it anyway
	else
		consoleSink->set_pattern("[%n] [%l] %v"); // no colour, so we should show the level for colourblind people
	RegisterSink(consoleSink);
}

ON_DLL_LOAD_CLIENT("tier0.dll", Tier0ConsoleFunctions, [](CModule module)
{
	ConColorMsg = module.Offset(0xE1E0).RCast<ConColorMsgType>();
	ConMsg = module.Offset(0xE290).RCast<ConMsgType>();
})

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", SourceConsole, ConCommand, [](CModule module)
{
	g_pGameConsole = Sys_GetFactoryPtr("client.dll", "GameConsole004").RCast<CGameConsole*>();

	RegisterConCommand("toggleconsole", ConCommand_toggleconsole, "Show/hide the console.", FCVAR_DONTRECORD);
	RegisterConCommand("showconsole", ConCommand_showconsole, "Show the console.", FCVAR_DONTRECORD);
	RegisterConCommand("hideconsole", ConCommand_hideconsole, "Hide the console.", FCVAR_DONTRECORD);
})
