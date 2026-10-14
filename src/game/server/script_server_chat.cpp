#include "game/server/script_server_chat.h"
#include "game/server/gameinterface.h"
#include "game/server/player.h"
#include "game/server/recipientfilter.h"
#include "game/server/util_server.h"
#include "util/utils.h"
#include "vscript/languages/squirrel_re/squirrel.h"


void ChatSendMessage(unsigned int playerIndex, const char* text, bool isTeam)
{
    CServerGameDLL::OnReceivedSayTextMessage(g_pServerGameDLL, (playerIndex + 1) & CUSTOM_MESSAGE_INDEX_MASK, text, isTeam);
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

    char sendText[256];
    sendText[0] = (char)messageType;
    strncpy_s(sendText + 1, 255, text, 254);

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

