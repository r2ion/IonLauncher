#include "serverchathooks.h"
#include "engine/client/client.h"
#include "engine/shared/exploit_fixes/ns_limits.h"
#include "server/r2server.h"
#include "server/recipientfilter.h"
#include "server/usermessages.h"
#include "util/utils.h"
#include "vscript/languages/squirrel_re/squirrel.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

DECLARE_MODULE(ServerChatHooks)

class CServerGameDLL;

CServerGameDLL* g_pServerGameDLL;

bool bShouldCallSayTextHook = false;

using CServerGameDLL_OnReceivedSayTextMessage_Original = void(__fastcall*)(CServerGameDLL* self, unsigned int senderPlayerId, const char* text,
                                                                           bool isTeam);
static CServerGameDLL_OnReceivedSayTextMessage_Original pCServerGameDLL_OnReceivedSayTextMessage_Original = nullptr;

DECLARE_HOOK(CServerGameDLL::OnReceivedSayTextMessage, server.dll + 0x1595C0,
             [](auto& hook, CServerGameDLL* self, unsigned int senderPlayerId, const char* text, bool isTeam)
{
    RemoveAsciiControlSequences(const_cast<char*>(text), true);

    // check chat ratelimits
    if (!g_pServerLimits->CheckChatLimits(&g_pClientArray[senderPlayerId - 1]))
        return;

    SQRESULT result =
        g_pSquirrel[ScriptContext::SERVER]->Call("CServerGameDLL_ProcessMessageStartThread", static_cast<int>(senderPlayerId) - 1, text, isTeam);

    if (result == SQRESULT_ERROR)
        hook.Original(self, senderPlayerId, text, isTeam);
})

void ChatSendMessage(unsigned int playerIndex, const char* text, bool isTeam)
{
    if (!pCServerGameDLL_OnReceivedSayTextMessage_Original)
    {
        spdlog::error("ChatSendMessage called before original function pointer was set!");
        return;
    }

    pCServerGameDLL_OnReceivedSayTextMessage_Original(g_pServerGameDLL,
                                                      // Ensure the first bit isn't set, since this indicates a custom message
                                                      (playerIndex + 1) & CUSTOM_MESSAGE_INDEX_MASK, text, isTeam);
}

void ChatBroadcastMessage(int fromPlayerIndex, int toPlayerIndex, const char* text, bool isTeam, bool isDead, CustomMessageType messageType)
{
    CPlayer* toPlayer = NULL;
    if (toPlayerIndex >= 0)
    {
        toPlayer = UTIL_PlayerByIndex(toPlayerIndex + 1);
        if (toPlayer == NULL)
            return;
    }

    // Build a new string where the first byte is the message type
    char sendText[256];
    sendText[0] = (char)messageType;
    strncpy_s(sendText + 1, 255, text, 254);

    // Anonymous custom messages use playerId=0, non-anonymous ones use a player ID with the first bit set
    unsigned int fromPlayerId = fromPlayerIndex < 0 ? 0 : ((fromPlayerIndex + 1) | CUSTOM_MESSAGE_INDEX_BIT);

    CRecipientFilter filter;
    if (toPlayer == NULL)
    {
        filter.AddAllPlayers();
    }
    else
    {
        filter.AddRecipient(toPlayer);
    }
    filter.MakeReliable();

    UserMessageBegin(filter, "SayText", NOT_IN_REPLAY);
    MessageWriteByte(fromPlayerId);
    MessageWriteString(sendText);
    MessageWriteBool(isTeam);
    MessageWriteBool(isDead);
    MessageEnd();
}

ADD_SQFUNC("void", NSSendMessage, "int playerIndex, string text, bool isTeam", "", ScriptContext::SERVER)
{
    int playerIndex = g_pSquirrel[ScriptContext::SERVER]->getinteger(sqvm, 1);
    const char* text = g_pSquirrel[ScriptContext::SERVER]->getstring(sqvm, 2);
    bool isTeam = g_pSquirrel[ScriptContext::SERVER]->getbool(sqvm, 3);

    ChatSendMessage(playerIndex, text, isTeam);

    return SQRESULT_NULL;
}

ADD_SQFUNC("void", NSBroadcastMessage, "int fromPlayerIndex, int toPlayerIndex, string text, bool isTeam, bool isDead, int messageType", "",
           ScriptContext::SERVER)
{
    int fromPlayerIndex = g_pSquirrel[ScriptContext::SERVER]->getinteger(sqvm, 1);
    int toPlayerIndex = g_pSquirrel[ScriptContext::SERVER]->getinteger(sqvm, 2);
    const char* text = g_pSquirrel[ScriptContext::SERVER]->getstring(sqvm, 3);
    bool isTeam = g_pSquirrel[ScriptContext::SERVER]->getbool(sqvm, 4);
    bool isDead = g_pSquirrel[ScriptContext::SERVER]->getbool(sqvm, 5);
    int messageType = g_pSquirrel[ScriptContext::SERVER]->getinteger(sqvm, 6);

    if (messageType < 1)
    {
        g_pSquirrel[ScriptContext::SERVER]->raiseerror(sqvm, fmt::format("Invalid message type {}", messageType).c_str());
        return SQRESULT_ERROR;
    }

    ChatBroadcastMessage(fromPlayerIndex, toPlayerIndex, text, isTeam, isDead, (CustomMessageType)messageType);

    return SQRESULT_NULL;
}

ON_DLL_LOAD("engine.dll", EngineServerChatHooks, [](CModule module) { g_pServerGameDLL = module.Offset(0x13F0AA98).RCast<CServerGameDLL*>(); })

ON_DLL_LOAD_RELIESON("server.dll", ServerChatHooks, ServerSquirrel, [](CModule module)
{
    DISPATCH_MODULE(ServerChatHooks)
    pCServerGameDLL_OnReceivedSayTextMessage_Original =
        HookSys::GetOriginalFunction<CServerGameDLL_OnReceivedSayTextMessage_Original>(HookSys::FindHook("CServerGameDLL::OnReceivedSayTextMessage"));
})
