#include "game/client/hud_basechat.h"
#include "common/chat.h"
#include "util/utils.h"
#include "vscript/languages/squirrel_re/squirrel.h"

CBaseHudChat** CBaseHudChat::allHuds;

bool CBaseHudChat::ProcessMessage(const char* message, int inboxId, bool isTeam, bool isDead)
{
	int senderId = inboxId & CUSTOM_MESSAGE_INDEX_MASK;
	bool isAnonymous = senderId == 0;
	bool isCustom = isAnonymous || (inboxId & CUSTOM_MESSAGE_INDEX_BIT);

	// Type is set to 0 for non-custom messages, custom messages have a type encoded as the first byte
	int type = 0;
	const char* payload = message;
	if (isCustom)
	{
		type = message[0];
		payload = message + 1;
	}

	RemoveAsciiControlSequences(const_cast<char*>(message), true);

	SQRESULT result = g_pSquirrel[ScriptContext::CLIENT]->Call(
		"CHudChat_ProcessMessageStartThread", static_cast<int>(senderId) - 1, payload, isTeam, isDead, type);
	return result != SQRESULT_ERROR;
}

DECLARE_MODULE(ClientChatHooks)

DECLARE_HOOK(CBaseHudChat::AddGameLine, client.dll + 0x22E580, [](auto& hook, CBaseHudChat* self, const char* message, int inboxId, bool isTeam, bool isDead)
{
	// This hook is called for each HUD, but we only want our logic to run once.
	if (self != *CBaseHudChat::allHuds)
		return;

	if (!self->ProcessMessage(message, inboxId, isTeam, isDead))
		for (CBaseHudChat* hud = *CBaseHudChat::allHuds; hud != NULL; hud = hud->next)
			hook.Original(hud, message, inboxId, isTeam, isDead);
})

ON_DLL_LOAD_CLIENT("client.dll", ClientChatHooks, [](CModule module)
{
	CBaseHudChat::allHuds = module.Offset(0x11BA9E8).RCast<CBaseHudChat**>();
	DISPATCH_MODULE(ClientChatHooks)
})
