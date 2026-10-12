#include "player_info.h"

DECLARE_MODULE(PlayerInfoHooks)

DECLARE_HOOK(GetPlayerInfo, engine.dll + 0x52660, [](auto& hook, void* self, int clientIndex, player_info_t* pInfo) -> bool
{
    if (!pInfo)
        return false;
    const bool result = hook.Original(self, clientIndex, pInfo);
    if (pInfo)
    {
        for (char* p = pInfo->m_ClanTag; *p; ++p)
            if (*p == '\n')
                *p = ' ';
    }
	return result;
})


ON_DLL_LOAD("engine.dll", EnginePlayerInfoHooks, [](CModule module)
{
	DISPATCH_MODULE(PlayerInfoHooks)
});
