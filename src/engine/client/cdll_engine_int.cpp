#include "cdll_int.h"
#include "tier1/interface.h"
#include "tier1/convar.h"
#include "tier1/cvar.h"
#include "engine/client/cdll_engine_int.h"
#include "tier0/hooks.h"
#include <cstdio>

IBaseClientDLL* g_ClientDLL;
IVEngineClient* g_pEngineClient;
static ConVar* Cvar_ion_anonymize_players;
static ConVar* Cvar_ion_hide_clantag;

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
	
	if (Cvar_ion_anonymize_players && Cvar_ion_anonymize_players->GetBool())
        std::snprintf(pInfo->m_Name, sizeof(pInfo->m_Name), "player%d", clientIndex);

    if (Cvar_ion_hide_clantag && Cvar_ion_hide_clantag->GetBool())
    {
        pInfo->m_ClanTag[0] = '\0';
        return result;
    }
	
    for (char* p = pInfo->m_ClanTag; *p; ++p)
        if (*p == '\n')
            *p = ' ';
    return result;
})

ON_DLL_LOAD_RELIESON("engine.dll", EnginePlayerInfoHooks, ConVar, [](CModule module)
{
	Cvar_ion_anonymize_players = new ConVar(
        "ion_anonymize_players",
        "0",
        FCVAR_CLIENTDLL | FCVAR_ARCHIVE_PLAYERPROFILE,
        "Replace player names with player<index> in player info.");
	Cvar_ion_hide_clantag = new ConVar(
        "ion_hide_clantag",
        "0",
        FCVAR_CLIENTDLL | FCVAR_ARCHIVE_PLAYERPROFILE,
        "Hide clan tags in player info.");
    DISPATCH_MODULE(PlayerInfoHooks)
});
