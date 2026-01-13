#include "engine/r2engine.h"
#include "server/auth/serverauthentication.h"

Cbuf_GetCurrentPlayerType Cbuf_GetCurrentPlayer;
Cbuf_AddTextType Cbuf_AddText;
Cbuf_ExecuteType Cbuf_Execute;

bool (*CCommand__Tokenize)(CCommand& self, const char* pCommandString, cmd_source_t commandSource);
// void (*_Cmd_Exec_f)(const CCommand& arg, bool bOnlyIfExists, bool bUseWhiteLists);

CEngine* g_pEngine;

server_state_t* g_pServerState;

char* g_pModName =
	nullptr; // we cant set this up here atm since we dont have an offset to it in engine, instead we store it in IsRespawnMod

CGlobalVars* g_pGlobals;

ON_DLL_LOAD("engine.dll", R2Engine, (CModule module))
{
	Cbuf_GetCurrentPlayer = module.Offset(0x120630).RCast<Cbuf_GetCurrentPlayerType>();
	Cbuf_AddText = module.Offset(0x1203B0).RCast<Cbuf_AddTextType>();
	Cbuf_Execute = module.Offset(0x1204B0).RCast<Cbuf_ExecuteType>();

	CCommand__Tokenize = module.Offset(0x418380).RCast<bool (*)(CCommand&, const char*, cmd_source_t)>();
	//_Cmd_Exec_f = module.Offset(0x1232C0).RCast<void (*)(const CCommand&, bool, bool)>();

	g_pEngine = module.Offset(0x7D70C8).Deref().RCast<CEngine*>();

	g_pServerState = module.Offset(0x12A53D48).RCast<server_state_t*>();

	g_pGlobals = module.Offset(0x7C6F70).RCast<CGlobalVars*>();
}
