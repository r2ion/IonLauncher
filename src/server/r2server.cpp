#include "r2server.h"
#include <mutex>

CBaseEntity* (*Server_GetEntityByIndex)(int index);
CServer* g_pServer = nullptr;
CClientExtended CServer::sm_ClientsExtended[MAX_PLAYERS];

static uint8_t* s_pAimAssistAdspullClassesInitialized;

DECLARE_MODULE(R2ServerHooks)

// genuinely no fucking idea why this spontaneously became an issue
DECLARE_HOOK(AimAssistAdspullClassInit, server.dll + 0x6CC300, [](auto& hook) -> __int64
{
	static std::mutex s_AimAssistAdspullClassInitMutex;

    const std::lock_guard lock(s_AimAssistAdspullClassInitMutex);
    if (*s_pAimAssistAdspullClassesInitialized)
        return 0;

    return hook.Original();
})

ON_DLL_LOAD("server.dll", R2GameServer, [](CModule module)
{
    Server_GetEntityByIndex = module.Offset(0xFB820).RCast<CBaseEntity* (*)(int)>();
    s_pAimAssistAdspullClassesInitialized = module.Offset(0x160B477).RCast<uint8_t*>();
    DISPATCH_MODULE(R2ServerHooks)
    // FF 15 42 90 42 00
    //  Remove call to Error for Geo bug: bullet trace ended at (%f, %f, %f), which is outside the max map coord:%i.
    module.Offset(0x43D4D8).NoOP(6);
})

ON_DLL_LOAD("engine.dll", R2EngineServer, [](CModule module)
{
	g_pServer = module.Offset(0x12A53D40).RCast<CServer*>();
})
