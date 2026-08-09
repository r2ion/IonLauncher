#include "tier1/convar.h"
ConVar* Cvar_rui_drawEnable;

DECLARE_MODULE(ClientRuiHooks)

DECLARE_HOOK(DrawRUIFunc, engine.dll + 0xFC500, [](auto& hook, void* a1, float* a2) -> bool
{
	if (!Cvar_rui_drawEnable->GetBool())
		return 0;

	return hook.Original(a1, a2);
})

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", RUI, ConVar, [](CModule module)
{
	DISPATCH_MODULE(ClientRuiHooks)

	Cvar_rui_drawEnable = new ConVar("rui_drawEnable", "1", FCVAR_CLIENTDLL, "Controls whether RUI should be drawn");
})
