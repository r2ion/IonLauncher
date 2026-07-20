#include "cliententitylist.h"
#include "core/tier1.h"
#include "shared/maxplayers.h"

DECLARE_MODULE(ClientEntityListHooks)

IClientEntityList* g_pClientEntityList;

DECLARE_HOOK(IsPlayerMutedOrBlockedByClientNum, client.dll + 0x55BF90, [](auto& hook, int nClientNum) -> bool
{
	if (nClientNum < 0 || nClientNum >= GetMaxPlayers())
		return false;

	return hook.Original(nClientNum);
})

ON_DLL_LOAD("client.dll", ClientEntityList, [](CModule module)
{
	DISPATCH_MODULE(ClientEntityListHooks)
	g_pClientEntityList = Sys_GetFactoryPtr("client.dll", "VClientEntityList003").RCast<IClientEntityList*>();
})
