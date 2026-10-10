#include "client/cdll_client_int.h"

#include "cdll_int.h"
#include "client/baseanimating.h"
#include "client/baseviewmodel.h"
#include "client/player.h"
#include "client/prediction.h"
#include "engine/client/clientstate.h"
#include "tier0/hooks.h"

DECLARE_MODULE(ClientDllHooks)

CGlobalVarsBase* g_pClientGlobals = nullptr;

using CreateInterface_t = void* (*)(const char*, int*);

DECLARE_HOOK(CClient_Init, client.dll + 0x18EF00,
             [](auto& hook, IBaseClientDLL* self, CreateInterface_t appSystemFactory, CGlobalVarsBase* globals) -> bool
{
    g_pClientGlobals = globals;
    return hook.Original(self, appSystemFactory, globals);
})

DECLARE_HOOK(CClient_FrameStageNotify, client.dll + 0x1900A0, [](auto& hook, IBaseClientDLL* self, ClientFrameStage_t stage)
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

DECLARE_HOOK(C_BaseAnimating_UpdateClientSideAnimations, client.dll + 0x100AC0, [](auto& hook)
{
    if (GetBaseLocalClient()->GetClientStateExtended()->m_RenderState.inRenderStart)
    {
        g_pClientSidePrediction->InterpolateRenderState();
        auto& render = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState;
        C_Player* const player = C_Player::GetLocalPlayer();
        if (player)
            player->UpdateZoomPresentation();
        else
            render.zoom = {};
        float cameraTime;
        if (!render.viewModel.active && player && g_pClientSidePrediction->GetRenderCameraTime(player, cameraTime) &&
            player->CanPresentLocalPlayer())
        {
            if (C_BaseViewModel* const viewModel = player->GetViewModel())
                viewModel->UpdateRenderPresentation(cameraTime);
        }
        C_BaseAnimating::UpdateRemoteSequencePresentation();
    }
    hook.Original();
})

ON_DLL_LOAD_CLIENT("client.dll", ClientDll, [](CModule)
{
    DISPATCH_MODULE(ClientDllHooks)
})
