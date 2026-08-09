#include "client/cdll_client_int.h"
#include "toolframework/itoolentity.h"
#include "core/tier1.h"

ON_DLL_LOAD("client.dll", ClientClientTools, [](CModule module)
{
	g_pClientTools = Sys_GetFactoryPtr("client.dll", VCLIENTTOOLS_INTERFACE_VERSION).RCast<IClientTools*>();
})
