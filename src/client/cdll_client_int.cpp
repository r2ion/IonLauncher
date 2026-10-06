#include "client/cdll_client_int.h"

#include "cdll_int.h"
#include "tier0/hooks.h"

DECLARE_MODULE(ClientDllHooks)

CGlobalVars* g_pClientGlobals = nullptr;

DECLARE_HOOK(CHLClient_Init, client.dll + 0x18EF00,
             [](auto& hook, IBaseClientDLL* self, CreateInterfaceFn appSystemFactory, CGlobalVars* globals) -> bool
{
    g_pClientGlobals = globals;
    return hook.Original(self, appSystemFactory, globals);
})

ON_DLL_LOAD_CLIENT("client.dll", ClientDll, [](CModule)
{
    DISPATCH_MODULE(ClientDllHooks)
})
