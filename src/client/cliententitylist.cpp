#include "cliententitylist.h"
#include "core/tier1.h"

IClientEntityList* g_pClientEntityList;

ON_DLL_LOAD("client.dll", ClientEntityList, [](CModule module)
{
	g_pClientEntityList = Sys_GetFactoryPtr("client.dll", "VClientEntityList003").RCast<IClientEntityList*>();
})
