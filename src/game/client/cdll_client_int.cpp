#include "game/client/cdll_client_int.h"
#include "cdll_int.h"
#include "game/client/c_player.h"
#include "game/client/prediction.h"
#include "engine/client/clientstate.h"
#include "tier0/hooks.h"
#include "game/client/IGameClientExports.h"
#include "logging/sourceconsole.h"

DECLARE_MODULE(ClientDllHooks)

CGlobalVarsBase* g_pClientGlobals = nullptr;

using CreateInterface_t = void* (*)(const char*, int*);

DECLARE_HOOK(CHLClient_Init, client.dll + 0x18EF00,
             [](auto& hook, IBaseClientDLL* self, CreateInterface_t appSystemFactory, CGlobalVarsBase* globals) -> bool
{
    g_pClientGlobals = globals;
    return hook.Original(self, appSystemFactory, globals);
})

DECLARE_HOOK(CHLClient_FrameStageNotify, client.dll + 0x1900A0, [](auto& hook, IBaseClientDLL* self, ClientFrameStage_t stage)
{
    auto& render = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState;
    if (stage != FRAME_RENDER_END)
    {
        render.zoom.active = false;
        render.crosshair.active = false;
        g_pClientSidePrediction->RestoreRenderState();
    }
    if (stage == FRAME_START)
    {
        g_pClientSidePrediction->OnFrameStart();
        C_Player* const player = C_Player::GetLocalPlayer();
        if (!player || !player->CanPresentLocalPlayer())
            render.zoom = {};
    }
    render.inRenderStart = stage == FRAME_RENDER_START;
    hook.Original(self, stage);
    render.inRenderStart = false;
    if (stage == FRAME_RENDER_END)
    {
        render.zoom.active = false;
        render.crosshair.active = false;
        g_pClientSidePrediction->RestoreRenderState();
    }
})


ON_DLL_LOAD_CLIENT("client.dll", ClientDll, [](CModule)
{
    DISPATCH_HOOK(ClientDllHooks, CHLClient_Init)
    DISPATCH_HOOK(ClientDllHooks, CHLClient_FrameStageNotify)
})

DECLARE_HOOK_PROC(ClientCreateInterface, client.dll, CreateInterface, [](auto& hook, const char* pName, int* pReturnCode) -> void*
{
	void* ret = hook.Original(pName, pReturnCode);
	spdlog::info("CreateInterface CLIENT {}", pName);

	if (!strcmp(pName, GAMECLIENTEXPORTS_INTERFACE_VERSION))
		InitialiseConsoleOnInterfaceCreation();

	return ret;
})

ON_DLL_LOAD("client.dll", ClientInterface, [](CModule)
{
	DISPATCH_HOOK(ClientDllHooks, ClientCreateInterface)
})
