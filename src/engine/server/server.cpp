#include "engine/server/server.h"

CClientExtended CServer::sm_ClientsExtended[MAX_PLAYERS];
CServer* g_pServer;

ON_DLL_LOAD("engine.dll", EngineServerSdk, [](CModule module) { g_pServer = module.Offset(0x12A53D40).RCast<CServer*>(); })
