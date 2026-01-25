#include "core/convar/convar.h"
#include "sourceconsole.h"
#include "core/tier1.h"
#include "core/convar/concommand.h"
#include "util/printcommands.h"
#include "client/r2client.h"

CGameConsole* g_pGameConsole;

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
	if (!g_pGameConsole->m_bInitialized)
		return;

	spdlog::memory_buf_t formatted;
	spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);

	// get message string
	std::string str = fmt::to_string(formatted);

	SourceColor levelColor = m_LogColours[msg.level];
	std::string name {msg.logger_name.begin(), msg.logger_name.end()};
	SourceColor loggerColor = NS::log::GetSourceColorForLoggerName(name);

	CClientState* client = GetBaseLocalClient();

	g_pGameConsole->m_pConsole->m_pConsolePanel->ColorPrint(loggerColor, ("[" + name + "]").c_str());
	g_pGameConsole->m_pConsole->m_pConsolePanel->Print(" ");

    if(client->m_nSignonState >= eSignonState::CONNECTED)
    {
        const std::string uptimeStr = fmt::format("{:.3f}", client->m_flServerUptime);
        g_pGameConsole->m_pConsole->m_pConsolePanel->Print(("[" + uptimeStr + "]").c_str());
        g_pGameConsole->m_pConsole->m_pConsolePanel->Print(" ");
    }

	if (msg.level != spdlog::level::info)
		g_pGameConsole->m_pConsole->m_pConsolePanel->ColorPrint(levelColor, str.c_str());
	else
		g_pGameConsole->m_pConsole->m_pConsolePanel->Print(str.c_str());
}

void SourceConsoleSink::flush_() {}

// clang-format off
HOOK(OnCommandSubmittedHook, OnCommandSubmitted,
void, __fastcall, (CConsoleDialog* consoleDialog, const char* pCommand))
// clang-format on
{
	consoleDialog->m_pConsolePanel->Print("] ");
	consoleDialog->m_pConsolePanel->Print(pCommand);
	consoleDialog->m_pConsolePanel->Print("\n");

	TryPrintCvarHelpForCommand(pCommand);

	OnCommandSubmitted(consoleDialog, pCommand);
}

// called from sourceinterface.cpp in client createinterface hooks, on GameClientExports001
void InitialiseConsoleOnInterfaceCreation()
{
	g_pGameConsole->Initialize();
	// hook OnCommandSubmitted so we print inputted commands
	OnCommandSubmittedHook.Dispatch((LPVOID)g_pGameConsole->m_pConsole->m_vtable->OnCommandSubmitted);

	auto consoleSink = std::make_shared<SourceConsoleSink>();
	if (g_bSpdLog_UseAnsiColor)
		consoleSink->set_pattern("%v"); // no need to include the level in the game console, the text colour signifies it anyway
	else
		consoleSink->set_pattern("[%n] [%l] %v"); // no colour, so we should show the level for colourblind people
	RegisterSink(consoleSink);
}

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", SourceConsole, ConCommand, [](CModule module)
{
	g_pGameConsole = Sys_GetFactoryPtr("client.dll", "GameConsole004").RCast<CGameConsole*>();

	RegisterConCommand("toggleconsole", ConCommand_toggleconsole, "Show/hide the console.", FCVAR_DONTRECORD);
	RegisterConCommand("showconsole", ConCommand_showconsole, "Show the console.", FCVAR_DONTRECORD);
	RegisterConCommand("hideconsole", ConCommand_hideconsole, "Hide the console.", FCVAR_DONTRECORD);
})
