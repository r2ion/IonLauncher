#include "client/input.h"
#include "client/cdll_client_int.h"
#include "client/prediction.h"
#include "client/player.h"
#include "engine/cdll_int.h"
#include "engine/client/clientstate.h"
#include "engine/r2engine.h"
#include "game/shared/usercmd.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"
#include "tier1/cvar.h"

#include <algorithm>
#include <cstdint>

DECLARE_MODULE(ClientInputHooks)


kbutton_t* g_pMovementKeys[8];
kbutton_t* in_strafe;


CInput* g_pInput;
ConVar* cl_move_use_dt;



void CInput::SetInputSampleTime(float frameTime)
{
    Joystick_SetSampleTime(frameTime);
    IN_SetSampleTime(frameTime);
}

void CInput::ResetExtraMouseSamples()
{
    auto& input = *GetBaseLocalClient()->GetClientStateExtended();
    input.m_flExtraMouseSampleTime = 0.0f;
    input.m_flExtraForwardMove = 0.0f;
    input.m_flExtraSideMove = 0.0f;
}

float CInput::GetExtraMouseSampleTime() const
{
    return GetBaseLocalClient()->GetClientStateExtended()->m_flExtraMouseSampleTime;
}

DECLARE_HOOK(CInput_ExtraMouseSample, client.dll + 0x2554C0, [](auto& hook, CInput* self, float frameTime)
{
    auto& input = *GetBaseLocalClient()->GetClientStateExtended();
    if (g_pEngineClient->IsPlayingDemo())
    {
        hook.Original(self, frameTime);
        return;
    }

    if (frameTime <= 0.0f)
        return;

    if (input.m_flExtraMouseSampleTime > 0.0f && !g_pEngineClient->IsPlayingDemo())
    {
        QAngle angles{};
        g_pEngineClient->GetViewAngles(&angles);
        g_pClientSidePrediction->SetViewAngles(angles);
    }

    const int keyCount = (in_strafe->state & 1) != 0 ? 8 : 6;
    int savedKeyStates[8];
    for (int i = 0; i < keyCount; ++i)
        savedKeyStates[i] = g_pMovementKeys[i]->state;

    const bool wasExtraMouseSample = input.m_bInExtraMouseSample;
    input.m_bInExtraMouseSample = true;
    self->SetInputSampleTime(frameTime);
    hook.Original(self, frameTime);
    input.m_bInExtraMouseSample = wasExtraMouseSample;

    for (int i = 0; i < keyCount; ++i)
        g_pMovementKeys[i]->state = savedKeyStates[i];

    input.m_flExtraMouseSampleTime += frameTime;
})

DECLARE_HOOK(CInput_GetButtonBits, client.dll + 0x251620,
             [](auto& hook, CInput* self, bool resetState) -> std::uint32_t
{
    return hook.Original(self, resetState && !GetBaseLocalClient()->GetClientStateExtended()->m_bInExtraMouseSample);
})

DECLARE_HOOK(CInput_ApplyMouse, client.dll + 0x2567B0,
             [](auto& hook, CInput* self, int splitScreenSlot, QAngle* angles, CUserCmd* command, float mouseX, float mouseY)
{
    auto& input = *GetBaseLocalClient()->GetClientStateExtended();
    if (!input.m_bInExtraMouseSample)
    {
        hook.Original(self, splitScreenSlot, angles, command, mouseX, mouseY);
        return;
    }

    const float forwardMove = command->forwardmove;
    const float sideMove = command->sidemove;
    hook.Original(self, splitScreenSlot, angles, command, mouseX, mouseY);

    input.m_flExtraForwardMove += command->forwardmove - forwardMove;
    input.m_flExtraSideMove += command->sidemove - sideMove;
})

DECLARE_HOOK(CInput_CreateMove, client.dll + 0x254D10, [](auto& hook, CInput* self, int sequenceNumber, float frameTime, bool active)
{
    auto& input = *GetBaseLocalClient()->GetClientStateExtended();
    if (input.m_flExtraMouseSampleTime > 0.0f && !g_pEngineClient->IsPlayingDemo())
    {
        QAngle angles{};
        g_pEngineClient->GetViewAngles(&angles);
        g_pClientSidePrediction->SetViewAngles(angles);
    }
    const float viewFrameTime = std::max(frameTime - input.m_flExtraMouseSampleTime, 0.0f);
    input.m_flExtraMouseSampleTime = 0.0f;
    self->SetInputSampleTime(viewFrameTime);

    if (!active)
    {
        input.m_flExtraForwardMove = 0.0f;
        input.m_flExtraSideMove = 0.0f;
    }

    const float previousCommandFrameTime = input.m_flCommandFrameTime;
    const bool wasCreatingMove = input.m_bInCreateMove;
    const bool wasRestoringCommandTime = input.m_bRestoreCommandFrameTime;
    input.m_flCommandFrameTime = frameTime;
    input.m_bInCreateMove = true;
    input.m_bRestoreCommandFrameTime = viewFrameTime != frameTime;
    hook.Original(self, sequenceNumber, viewFrameTime, active);
    input.m_bRestoreCommandFrameTime = wasRestoringCommandTime;
    input.m_bInCreateMove = wasCreatingMove;
    input.m_flCommandFrameTime = previousCommandFrameTime;

    input.m_flExtraForwardMove = 0.0f;
    input.m_flExtraSideMove = 0.0f;
})

DECLARE_HOOK(CPlayer_CreateMove, client.dll + 0x14A880, [](auto& hook, C_Player* self, float frameTime, CUserCmd* command, bool extraSample) -> bool
{
    auto& input = *GetBaseLocalClient()->GetClientStateExtended();
    if (input.m_bInExtraMouseSample && extraSample)
        command->command_time = g_pClientGlobals->lastInterpolationTime;

    if (input.m_bInCreateMove && !extraSample)
    {
        command->forwardmove += input.m_flExtraForwardMove;
        command->sidemove += input.m_flExtraSideMove;
        input.m_flExtraForwardMove = 0.0f;
        input.m_flExtraSideMove = 0.0f;
    }

    const bool result = hook.Original(self, frameTime, command, extraSample);
    if (input.m_bRestoreCommandFrameTime && !extraSample && cl_move_use_dt->GetBool())
    {
        command->frameTime = input.m_flCommandFrameTime;
        self->m_LastCmd.frameTime = input.m_flCommandFrameTime;
    }
    return result;
})

DECLARE_HOOK(CInput_ActivateMouse, client.dll + 0x256700, [](auto& hook, CInput* self)
{
    auto& input = *GetBaseLocalClient()->GetClientStateExtended();
    const bool wasMouseActive = self->m_fMouseActive;
    hook.Original(self);
    if (!wasMouseActive && self->m_fMouseActive)
    {
        input.m_flExtraForwardMove = 0.0f;
        input.m_flExtraSideMove = 0.0f;
    }
})

DECLARE_HOOK(CInput_DeactivateMouse, client.dll + 0x256940, [](auto& hook, CInput* self)
{
    auto& input = *GetBaseLocalClient()->GetClientStateExtended();
    hook.Original(self);
    input.m_flExtraForwardMove = 0.0f;
    input.m_flExtraSideMove = 0.0f;
})

DECLARE_HOOK(CInput_ClearStates, client.dll + 0x2568F0, [](auto& hook, CInput* self)
{
    auto& input = *GetBaseLocalClient()->GetClientStateExtended();
    hook.Original(self);
    input.m_flExtraForwardMove = 0.0f;
    input.m_flExtraSideMove = 0.0f;
})

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", ClientInput, (R2EngineClient, ConVar, ClientPrediction), [](CModule module)
{
    g_pInput = module.Offset(0xB1D380).RCast<CInput*>();
    cl_move_use_dt = g_pCVar->FindVar("cl_move_use_dt");
    in_strafe = module.Offset(0x11BE110).RCast<kbutton_t*>();
    g_pMovementKeys[0] = module.Offset(0x11BE160).RCast<kbutton_t*>();
    g_pMovementKeys[1] = module.Offset(0x11BE170).RCast<kbutton_t*>();
    g_pMovementKeys[2] = module.Offset(0x11BE140).RCast<kbutton_t*>();
    g_pMovementKeys[3] = module.Offset(0x11BE150).RCast<kbutton_t*>();
    g_pMovementKeys[4] = module.Offset(0x11C14B8).RCast<kbutton_t*>();
    g_pMovementKeys[5] = module.Offset(0x11C14C8).RCast<kbutton_t*>();
    g_pMovementKeys[6] = module.Offset(0x11C1468).RCast<kbutton_t*>();
    g_pMovementKeys[7] = module.Offset(0x11C1478).RCast<kbutton_t*>();
    DISPATCH_MODULE(ClientInputHooks);
})
