#include "core/convar/convar.h"
#include "core/convar/concommand.h"
#include "localchatwriter.h"

// note: isIngameChat is an int64 because the whole register the arg is stored in needs to be 0'd out to work
// if isIngameChat is false, we use network chat instead
void(__fastcall* ClientSayText)(void* a1, const char* message, uint64_t isIngameChat, bool isTeamChat);

static void hClientSayText(void* a1, const char* message, uint64_t isIngameChat, bool isTeamChat)
{
	SQRESULT result = g_pSquirrel[ScriptContext::CLIENT]->Call("NS_PreSendMessage", message, (bool)isIngameChat, (bool)isTeamChat);
	if (result == SQRESULT_ERROR)
	{
		ClientSayText(a1, message, isIngameChat, isTeamChat);
	}
}

ADD_SQFUNC("void", NSSendMessage, "string message, bool isIngame, bool isTeam", "", ScriptContext::CLIENT)
{
	const char* message = g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1);
	bool isIngame = g_pSquirrel[ScriptContext::CLIENT]->getbool(sqvm, 2);
	bool isTeam = g_pSquirrel[ScriptContext::CLIENT]->getbool(sqvm, 3);

	ClientSayText(nullptr, message, isIngame, isTeam);

	return SQRESULT_NULL;
}

void ConCommand_say(const CCommand& args)
{
	if (args.ArgC() >= 2)
		ClientSayText(nullptr, args.ArgS(), true, false);
}

void ConCommand_say_team(const CCommand& args)
{
	if (args.ArgC() >= 2)
		ClientSayText(nullptr, args.ArgS(), true, true);
}

void ConCommand_log(const CCommand& args)
{
	if (args.ArgC() >= 2)
	{
		LocalChatWriter(LocalChatWriter::GameContext).WriteLine(args.ArgS());
	}
}

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", ClientChatCommand, ConCommand, [](CModule module)
{
	ClientSayText =
		module.Offset(0x54780).RCast<void(__fastcall*)(void* a1, const char* message, uint64_t isIngameChat, bool isTeamChat)>();
	HookAttach(&(PVOID&)ClientSayText, (PVOID)hClientSayText);
	RegisterConCommand("say", ConCommand_say, "Enters a message in public chat", FCVAR_CLIENTDLL);
	RegisterConCommand("say_team", ConCommand_say_team, "Enters a message in team chat", FCVAR_CLIENTDLL);
	RegisterConCommand("log", ConCommand_log, "Log a message to the local chat window", FCVAR_CLIENTDLL);
})
