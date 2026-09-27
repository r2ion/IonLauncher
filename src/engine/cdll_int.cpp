#include "cdll_int.h"
#include "core/tier1.h"
#include "engine/cdll_int.h"

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
