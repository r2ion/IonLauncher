#include "game/client/IGameClientExports.h"

#include "logging/sourceconsole.h"

DECLARE_MODULE(SourceInterfaceHooks)

// really wanted to do a modular callback system here but honestly couldn't be bothered so hardcoding stuff for now: todo later

DECLARE_HOOK_PROC(ClientCreateInterface, client.dll, CreateInterface, [](auto& hook, const char* pName, int* pReturnCode) -> void*
{
	void* ret = hook.Original(pName, pReturnCode);
	spdlog::info("CreateInterface CLIENT {}", pName);

	if (!strcmp(pName, GAMECLIENTEXPORTS_INTERFACE_VERSION))
		InitialiseConsoleOnInterfaceCreation();

	return ret;
})

DECLARE_HOOK_PROC(ServerCreateInterface, server.dll, CreateInterface, [](auto& hook, const char* pName, int* pReturnCode) -> void*
{
	void* ret = hook.Original(pName, pReturnCode);
	spdlog::info("CreateInterface SERVER {}", pName);

	return ret;
})

DECLARE_HOOK_PROC(EngineCreateInterface, engine.dll, CreateInterface, [](auto& hook, const char* pName, int* pReturnCode) -> void*
{
	void* ret = hook.Original(pName, pReturnCode);
	spdlog::info("CreateInterface ENGINE {}", pName);

	return ret;
})

ON_DLL_LOAD("client.dll", ClientInterface, [](CModule module)
{
	DISPATCH_HOOK(SourceInterfaceHooks, ClientCreateInterface)
})

ON_DLL_LOAD("server.dll", ServerInterface, [](CModule module)
{
	DISPATCH_HOOK(SourceInterfaceHooks, ServerCreateInterface)
})

ON_DLL_LOAD("engine.dll", EngineInterface, [](CModule module)
{
	DISPATCH_HOOK(SourceInterfaceHooks, EngineCreateInterface)
})
