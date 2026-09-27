#include "client/input.h"
#include "client/iprediction.h"
#include "client/player.h"
#include "game/shared/usercmd.h"
#include "engine/cdll_int.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"
#include "tier1/cvar.h"

#include <algorithm>
#include <cstdint>

DECLARE_MODULE(ClientInputHooks)


kbutton_t* s_pMovementKeys[8];
kbutton_t* s_pStrafe;

static float s_flExtraSampleTime;
static float s_flExtraForwardMove;
static float s_flExtraSideMove;
static float s_flCommandFrameTime;
static bool s_bExtraMouseSample;
static bool s_bCreatingMove;
static bool s_bRestoreCommandTime;

CInput* g_pInput;
ConVar* cl_move_use_dt;



void CInput::SetInputSampleTime(float frameTime)
{
    Joystick_SetSampleTime(frameTime);
    IN_SetSampleTime(frameTime);
}

void CInput::ResetExtraMouseSamples()
{
    s_flExtraSampleTime = 0.0f;
    s_flExtraForwardMove = 0.0f;
    s_flExtraSideMove = 0.0f;
}


DECLARE_HOOK(CInput_ExtraMouseSample, client.dll + 0x2554C0, [](auto& hook, CInput* self, float frameTime)
{
    if (g_pEngineClient->IsPlayingDemo())
    {
        hook.Original(self, frameTime);
        return;
    }

    if (frameTime <= 0.0f)
        return;

    if (s_flExtraSampleTime > 0.0f && !g_pEngineClient->IsPlayingDemo())
    {
        QAngle angles{};
        g_pEngineClient->GetViewAngles(&angles);
        g_pClientSidePrediction->SetViewAngles(angles);
    }

    const int keyCount = (s_pStrafe->state & 1) != 0 ? 8 : 6;
    int savedKeyStates[8];
    for (int i = 0; i < keyCount; ++i)
        savedKeyStates[i] = s_pMovementKeys[i]->state;

    const bool wasExtraMouseSample = s_bExtraMouseSample;
    s_bExtraMouseSample = true;
    self->SetInputSampleTime(frameTime);
    hook.Original(self, frameTime);
    s_bExtraMouseSample = wasExtraMouseSample;

    for (int i = 0; i < keyCount; ++i)
        s_pMovementKeys[i]->state = savedKeyStates[i];

    s_flExtraSampleTime += frameTime;
})

DECLARE_HOOK(CInput_GetButtonBits, client.dll + 0x251620,
             [](auto& hook, CInput* self, bool resetState) -> std::uint32_t { return hook.Original(self, resetState && !s_bExtraMouseSample); })

DECLARE_HOOK(CInput_ApplyMouse, client.dll + 0x2567B0,
             [](auto& hook, CInput* self, int splitScreenSlot, QAngle* angles, CUserCmd* command, float mouseX, float mouseY)
{
    if (!s_bExtraMouseSample)
    {
        hook.Original(self, splitScreenSlot, angles, command, mouseX, mouseY);
        return;
    }

    const float forwardMove = command->forwardmove;
    const float sideMove = command->sidemove;
    hook.Original(self, splitScreenSlot, angles, command, mouseX, mouseY);

    s_flExtraForwardMove += command->forwardmove - forwardMove;
    s_flExtraSideMove += command->sidemove - sideMove;
})

DECLARE_HOOK(CInput_CreateMove, client.dll + 0x254D10, [](auto& hook, CInput* self, int sequenceNumber, float frameTime, bool active)
{
    if (s_flExtraSampleTime > 0.0f && !g_pEngineClient->IsPlayingDemo())
    {
        QAngle angles{};
        g_pEngineClient->GetViewAngles(&angles);
        g_pClientSidePrediction->SetViewAngles(angles);
    }
    const float viewFrameTime = std::max(frameTime - s_flExtraSampleTime, 0.0f);
    s_flExtraSampleTime = 0.0f;
    self->SetInputSampleTime(viewFrameTime);

    if (!active)
    {
        s_flExtraForwardMove = 0.0f;
        s_flExtraSideMove = 0.0f;
    }

    const float previousCommandFrameTime = s_flCommandFrameTime;
    const bool wasCreatingMove = s_bCreatingMove;
    const bool wasRestoringCommandTime = s_bRestoreCommandTime;
    s_flCommandFrameTime = frameTime;
    s_bCreatingMove = true;
    s_bRestoreCommandTime = viewFrameTime != frameTime;
    hook.Original(self, sequenceNumber, viewFrameTime, active);
    s_bRestoreCommandTime = wasRestoringCommandTime;
    s_bCreatingMove = wasCreatingMove;
    s_flCommandFrameTime = previousCommandFrameTime;

    s_flExtraForwardMove = 0.0f;
    s_flExtraSideMove = 0.0f;
})

DECLARE_HOOK(CPlayer_CreateMove, client.dll + 0x14A880, [](auto& hook, C_Player* self, float frameTime, CUserCmd* command, bool extraSample) -> bool
{
    if (s_bCreatingMove && !extraSample)
    {
        command->forwardmove += s_flExtraForwardMove;
        command->sidemove += s_flExtraSideMove;
        s_flExtraForwardMove = 0.0f;
        s_flExtraSideMove = 0.0f;
    }

    const bool result = hook.Original(self, frameTime, command, extraSample);
    if (s_bRestoreCommandTime && !extraSample && cl_move_use_dt->GetBool())
    {
        command->frameTime = s_flCommandFrameTime;
        self->m_LastCmd.frameTime = s_flCommandFrameTime;
    }
    return result;
})

DECLARE_HOOK(CInput_ActivateMouse, client.dll + 0x256700, [](auto& hook, CInput* self)
{
    const bool wasMouseActive = self->m_fMouseActive;
    hook.Original(self);
    if (!wasMouseActive && self->m_fMouseActive)
    {
        s_flExtraForwardMove = 0.0f;
        s_flExtraSideMove = 0.0f;
    }
})

DECLARE_HOOK(CInput_DeactivateMouse, client.dll + 0x256940, [](auto& hook, CInput* self)
{
    hook.Original(self);
    s_flExtraForwardMove = 0.0f;
    s_flExtraSideMove = 0.0f;
})

DECLARE_HOOK(CInput_ClearStates, client.dll + 0x2568F0, [](auto& hook, CInput* self)
{
    hook.Original(self);
    s_flExtraForwardMove = 0.0f;
    s_flExtraSideMove = 0.0f;
})

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", ClientInput, (R2EngineClient, ConVar, ClientPrediction), [](CModule module)
{
    g_pInput = module.Offset(0xB1D380).RCast<CInput*>();
    cl_move_use_dt = g_pCVar->FindVar("cl_move_use_dt");
    s_pStrafe = module.Offset(0x11BE110).RCast<kbutton_t*>();
    s_pMovementKeys[0] = module.Offset(0x11BE160).RCast<kbutton_t*>();
    s_pMovementKeys[1] = module.Offset(0x11BE170).RCast<kbutton_t*>();
    s_pMovementKeys[2] = module.Offset(0x11BE140).RCast<kbutton_t*>();
    s_pMovementKeys[3] = module.Offset(0x11BE150).RCast<kbutton_t*>();
    s_pMovementKeys[4] = module.Offset(0x11C14B8).RCast<kbutton_t*>();
    s_pMovementKeys[5] = module.Offset(0x11C14C8).RCast<kbutton_t*>();
    s_pMovementKeys[6] = module.Offset(0x11C1468).RCast<kbutton_t*>();
    s_pMovementKeys[7] = module.Offset(0x11C1478).RCast<kbutton_t*>();
    DISPATCH_MODULE(ClientInputHooks);
})
