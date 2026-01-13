#include "r2server.h"

CBaseEntity* (*Server_GetEntityByIndex)(int index);
CServer* g_pServer = nullptr;
CClientExtended CServer::sm_ClientsExtended[MAX_PLAYERS];

ON_DLL_LOAD("server.dll", R2GameServer, (CModule module))
{
	Server_GetEntityByIndex = module.Offset(0xFB820).RCast<CBaseEntity* (*)(int)>();
}

ON_DLL_LOAD("engine.dll", R2EngineServer, (CModule module))
{
	g_pServer = module.Offset(0x12A53D40).RCast<CServer*>();
}
