#include "tier1/convar.h"
#include "core/tier0.h"
#include "modsystem/modmanager.h"
#include "r2engine.h"
#include "shared/misccommands.h"
#include "util/printcommands.h"
#include "util/printmaps.h"
#include "client/ckf.h"
#include "eos/eos_network.h"

DECLARE_MODULE(HostHooks)

DECLARE_HOOK(Host_Init, engine.dll + 0x155EA0, [](auto& hook, bool bDedicated)
{
	spdlog::info("Host_Init()");
	hook.Original(bDedicated);
	FixupCvarFlags();
	// need to initialise these after host_init since they do stuff to preexisting concommands/convars without being client/server specific
	InitialiseCommandPrint();
	InitialiseMapsPrint();
	// client/server autoexecs on necessary platforms
	// dedi needs autoexec_ns_server on boot, while non-dedi will run it on on listen server start
	if (bDedicated)
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "exec autoexec_ns_dedicatedserver", cmd_source_t::kCommandSrcCode);
	else
	{
		eos::Initialize();
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "exec autoexec_ns_client", cmd_source_t::kCommandSrcCode);
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "exec autoexec_ns_listenserver", cmd_source_t::kCommandSrcCode);
		FindBinds();
	}
})

ON_DLL_LOAD("engine.dll", Host_Init, [](CModule module)
{
	DISPATCH_MODULE(HostHooks)
})
