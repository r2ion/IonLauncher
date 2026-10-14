#include "cdll_int.h"
#include "tier1/interface.h"
#include "engine/client/cdll_engine_int.h"
#include "tier0/hooks.h"
IBaseClientDLL* g_ClientDLL;
IVEngineClient* g_pEngineClient;

ON_DLL_LOAD_CLIENT("engine.dll", EngineClientInterface, [](CModule)
{
	g_pEngineClient =
		Sys_GetFactoryPtr("engine.dll", VENGINE_CLIENT_INTERFACE_VERSION).RCast<IVEngineClient*>();
})

ON_DLL_LOAD_CLIENT("client.dll", ClientDLLInterface, [](CModule)
{
	g_ClientDLL = Sys_GetFactoryPtr("client.dll", CLIENT_DLL_INTERFACE_VERSION).RCast<IBaseClientDLL*>();
})

DECLARE_MODULE(PlayerInfoHooks)

DECLARE_HOOK(CEngineClient::GetPlayerInfo, engine.dll + 0x52660,
             [](auto& hook, IVEngineClient* self, int clientIndex, player_info_t* pInfo) -> bool
{
    if (!pInfo)
        return false;
    const bool result = hook.Original(self, clientIndex, pInfo);
    for (char* p = pInfo->m_ClanTag; *p; ++p)
        if (*p == '\n')
            *p = ' ';
    return result;
})

ON_DLL_LOAD("engine.dll", EnginePlayerInfoHooks, [](CModule module)
{
    DISPATCH_MODULE(PlayerInfoHooks)
});
