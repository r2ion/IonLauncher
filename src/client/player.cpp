#include "client/player.h"
#include "tier0/callbacks.h"

#include "client/baseviewmodel.h"
#include "client/cdll_client_int.h"
#include "client/input.h"
#include "client/prediction.h"
#include "client/weapon_parse.h"
#include "client/weaponx.h"
#include "engine/cdll_int.h"
#include "engine/client/clientstate.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"
#include "util/utils.h"

#include <algorithm>
#include <cmath>

DECLARE_MODULE(ClientPlayer)

thread_local C_Player* g_pPredictionFovPlayer;
thread_local C_Player* g_pCrosshairQueryPlayer;

C_Player_GetLocalPlayer_t C_Player__GetLocalPlayer;
C_Player_GetLocalPlayer_t C_Player__GetLocalViewPlayer;
C_Player_IsMantling_t C_Player__IsMantling;
C_Player_GetActiveWeapon_t C_Player__GetActiveWeapon;
C_Player_GetViewModel_t C_Player__GetViewModel;
C_Player_GetZoomFraction_t C_Player__GetZoomFraction;
C_Player_GetZoomOutDuration_t C_Player__GetZoomOutDuration;
C_Player_GetAimDirection_t C_Player__GetAimDirection;
ConVar** g_pPlayerInterpolationQuery;


C_Player* C_Player::GetLocalViewPlayer(const int splitScreenSlot)
{
    return C_Player__GetLocalViewPlayer(splitScreenSlot);
}

C_Player* C_Player::GetLocalPlayer(const int splitScreenSlot)
{
    return C_Player__GetLocalPlayer(splitScreenSlot);
}

bool C_Player::IsMantling() const
{
    return C_Player__IsMantling(this);
}

C_WeaponX* C_Player::GetActiveWeapon()
{
    return C_Player__GetActiveWeapon(this);
}

C_BaseViewModel* C_Player::GetViewModel()
{
    return C_Player__GetViewModel(this);
}

float C_Player::GetZoomFraction()
{
    return C_Player__GetZoomFraction(this);
}

float C_Player::GetZoomOutDuration()
{
    return C_Player__GetZoomOutDuration(this);
}

Vector3D C_Player::GetAimDirection()
{
    Vector3D direction;
    C_Player__GetAimDirection(this, &direction);
    return direction;
}

ON_DLL_LOAD_CLIENT("client.dll", ClientPlayerMethods, [](CModule module)
{
    g_pPlayerInterpolationQuery = module.Offset(0x11C10B8).RCast<decltype(g_pPlayerInterpolationQuery)>();
    C_Player__GetLocalViewPlayer = module.Offset(0x14EF00).RCast<C_Player_GetLocalPlayer_t>();
    C_Player__GetLocalPlayer = module.Offset(0x14EF40).RCast<C_Player_GetLocalPlayer_t>();
    C_Player__IsMantling = module.Offset(0x9E0B0).RCast<C_Player_IsMantling_t>();
    C_Player__GetActiveWeapon = module.Offset(0xB19C0).RCast<decltype(C_Player__GetActiveWeapon)>();
    C_Player__GetViewModel = module.Offset(0x14FFA0).RCast<decltype(C_Player__GetViewModel)>();
    C_Player__GetZoomFraction = module.Offset(0x2C8440).RCast<decltype(C_Player__GetZoomFraction)>();
    C_Player__GetZoomOutDuration = module.Offset(0x2C8570).RCast<decltype(C_Player__GetZoomOutDuration)>();
    C_Player__GetAimDirection = module.Offset(0x2C56B0).RCast<decltype(C_Player__GetAimDirection)>();
    DISPATCH_MODULE(ClientPlayer)
})


bool C_Player::CanPresentLocalPlayer() const
{
    const CClientState* const client = GetBaseLocalClient();
    return this == C_Player::GetLocalPlayer() && this == C_Player::GetLocalViewPlayer() && m_lifeState == 0 &&
           GetPredictable() && client && client->m_nSignonState == eSignonState::FULL && client->m_nDeltaTick >= 0 && !client->IsPaused() &&
           !client->m_bIsWatchingReplay && client->m_bCanProcessSignedOnLocalClientInput && !g_pEngineClient->IsPlayingDemo() &&
           !g_pEngineClient->IsPlayingTimeDemo();
}

void C_Player::UpdateZoomPresentation()
{
    CClientState* const client = GetBaseLocalClient();
    auto& zoom = client->GetClientStateExtended()->m_RenderState.zoom;
    if (!CanPresentLocalPlayer())
    {
        zoom = {};
        return;
    }
    C_WeaponX* const weapon = GetActiveWeapon();
    if (!weapon)
    {
        zoom = {};
        return;
    }

    CGlobalVarsBase* const globals = g_pClientGlobals;
    const int serverCount = client->m_nServerCount;
    const bool reset = zoom.player.Get() != this || zoom.weapon.Get() != weapon ||
                       zoom.serverCount != serverCount || DidEntityTeleport();
    if (!reset && zoom.frame == globals->framecount)
        return;

    const float exactTime = globals->latestPredictedTime;
    float cameraTime;
    if (g_pClientSidePrediction->GetRenderCameraTime(this, cameraTime))
        globals->latestPredictedTime = cameraTime;
    const float nativeFraction = GetZoomFraction();
    globals->latestPredictedTime = exactTime;
    const float duration = m_bZooming ? weapon->m_modVars.zoomTimeIn : GetZoomOutDuration();
    const float delta = globals->absoluteframetime;
    if (reset || !(delta >= 0.0f) || !std::isfinite(delta) || !std::isfinite(duration))
        zoom.fraction = nativeFraction;
    else if (duration <= 0.0f)
        zoom.fraction = m_bZooming ? 1.0 : 0.0;
    else
        zoom.fraction = std::clamp(zoom.fraction + (m_bZooming ? 1.0 : -1.0) * static_cast<double>(delta) / duration, 0.0, 1.0);
    zoom.player = this;
    zoom.weapon = weapon;
    zoom.serverCount = serverCount;
    zoom.frame = globals->framecount;
    zoom.active = true;
}

DECLARE_HOOK(C_Player_GetZoomFraction, client.dll + 0x2C8440, [](auto& hook, C_Player* self) -> float
{
    if (g_pPredictionFovPlayer == self)
        return 0.0f;
    const auto& zoom = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState.zoom;
    return zoom.active && self == zoom.player.Get() && !g_pClientSidePrediction->InPrediction()
               ? static_cast<float>(zoom.fraction)
               : hook.Original(self);
})

DECLARE_HOOK(C_Player_GetPredictionFOV, client.dll + 0x2C5E00, [](auto& hook, C_Player* self, bool* isOverride) -> float
{
    C_Player* const previousPlayer = g_pPredictionFovPlayer;
    const ScopeGuard restorePlayer([previousPlayer] { g_pPredictionFovPlayer = previousPlayer; });
    if (self->CanPresentLocalPlayer())
        g_pPredictionFovPlayer = self;
    return hook.Original(self, isOverride);
})

C_Player* C_Player::SetCrosshairQueryPlayer(C_Player* player)
{
    C_Player* const previous = g_pCrosshairQueryPlayer;
    g_pCrosshairQueryPlayer = player;
    return previous;
}


DECLARE_HOOK(C_Player_GetAimDirection, client.dll + 0x2C56B0, [](auto& hook, C_Player* self, Vector3D* direction) -> Vector3D*
{
    const auto& crosshair = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState.crosshair;
    if (self == g_pCrosshairQueryPlayer && crosshair.active && self == crosshair.player.Get() &&
        crosshair.frame == g_pClientGlobals->framecount && !g_pClientSidePrediction->InPrediction())
    {
        *direction = crosshair.direction;
        return direction;
    }
    return hook.Original(self, direction);
})


DECLARE_HOOK(C_Player_GetCrosshairPosition, client.dll + 0x14CF60, [](auto& hook, C_Player* self, float* x, float* y) -> std::intptr_t
{
    C_Player* const previousPlayer = g_pCrosshairQueryPlayer;
    const ScopeGuard restorePlayer([previousPlayer] { g_pCrosshairQueryPlayer = previousPlayer; });
    g_pCrosshairQueryPlayer = self;
    return hook.Original(self, x, y);
})

DECLARE_HOOK(C_Player_CalcView, client.dll + 0x143CA0, [](auto& hook, C_Player* self, Vector3D* origin, QAngle* angles, float* fieldOfView)
{
    auto& render = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState;
    const auto captureCrosshair = [&]
    {
        if (!render.inRenderStart || !self || !self->CanPresentLocalPlayer() || !g_pInput || g_pClientSidePrediction->InPrediction())
            return;
        const bool sampledInput = g_pInput->GetExtraMouseSampleTime() > 0.0f;
        float cameraTime;
        if (!sampledInput && !g_pClientSidePrediction->GetRenderCameraTime(self, cameraTime))
            return;

        const QAngle attackAngles = self->m_attackAngles;
        const ScopeGuard restoreAngles([self, attackAngles] { self->m_attackAngles = attackAngles; });
        if (sampledInput)
            self->m_attackAngles = self->m_lastUCmdAttackAngles;
        auto& crosshair = render.crosshair;
        crosshair.direction = self->GetAimDirection();
        crosshair.player = self;
        crosshair.frame = g_pClientGlobals->framecount;
        crosshair.active = true;
    };
    float cameraTime;
    if (!render.inRenderStart || !g_pClientSidePrediction->GetRenderCameraTime(self, cameraTime) || self != C_Player::GetLocalViewPlayer())
    {
        hook.Original(self, origin, angles, fieldOfView);
        captureCrosshair();
        return;
    }

    CGlobalVarsBase* const globals = g_pClientGlobals;
    const float exactTime = globals->latestPredictedTime;
    const ScopeGuard restoreTime([globals, exactTime] { globals->latestPredictedTime = exactTime; });
    globals->latestPredictedTime = cameraTime;
    hook.Original(self, origin, angles, fieldOfView);
    captureCrosshair();
})

DECLARE_HOOK(C_Player_GetViewModelFOV, client.dll + 0x2C8050, [](auto& hook, C_Player* self, float* viewmodelFOV, float* fieldOfViewOverride)
{
    const auto& render = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState;
    float cameraTime;
    if (!render.inRenderStart || !g_pClientSidePrediction->GetRenderCameraTime(self, cameraTime) || self != C_Player::GetLocalViewPlayer())
    {
        hook.Original(self, viewmodelFOV, fieldOfViewOverride);
        return;
    }

    CGlobalVarsBase* const globals = g_pClientGlobals;
    const float exactTime = globals->latestPredictedTime;
    const ScopeGuard restoreTime([globals, exactTime] { globals->latestPredictedTime = exactTime; });
    globals->latestPredictedTime = cameraTime;
    hook.Original(self, viewmodelFOV, fieldOfViewOverride);
})


int C_Player::InterpolateFieldsInternal(float currentTime, const SingleSnapshotValues* secondSnapshot, float secondSnapshotTime)
{
    const bool previousAngles = m_shouldInterpolateAngles;
    const bool previousOrigin = m_shouldInterpolateOrigin;
    m_shouldInterpolateAngles = !m_bLerpMissingParent;
    m_shouldInterpolateOrigin = !m_bLerpMissingParent;
    const int status = C_BaseAnimatingOverlay::InterpolateFieldsInternal(currentTime, secondSnapshot, secondSnapshotTime);
    m_shouldInterpolateOrigin = previousOrigin;
    m_shouldInterpolateAngles = previousAngles;
    if (!status)
        return status;

    const SingleSnapshotValues* const first = m_lerpData.currentSnap;
    const SingleSnapshotValues* const second = m_lerpData.futureSnap;
    const PlayerData& firstPlayer = *first->playerData;
    const PlayerData& secondPlayer = *second->playerData;
    const LocalPlayerData& firstLocal = *first->localPlayerData;
    const LocalPlayerData& secondLocal = *second->localPlayerData;
    auto& player = m_currentFramePlayer;
    auto& local = m_currentFrameLocalPlayer;
    const float fraction = GetInterpolationFraction(currentTime, secondSnapshotTime, secondSnapshot);
    const auto lerp = [fraction](float a, float b) { return (b - a) * fraction + a; };
    const CGlobalVarsBase* const globals = g_pClientGlobals;
    player.timeBase = ((secondPlayer.timeBase - firstPlayer.timeBase) * globals->snapLerp + firstPlayer.timeBase) + globals->replayDelay;
    if (m_viewConeParity == m_previousViewConeParity && first->moveParent.ToInt() == second->moveParent.ToInt())
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            local.m_viewConeAngleMin[axis] = LerpAngle(fraction, firstLocal.m_viewConeAngleMin[axis], secondLocal.m_viewConeAngleMin[axis]);
            local.m_viewConeAngleMax[axis] = LerpAngle(fraction, firstLocal.m_viewConeAngleMax[axis], secondLocal.m_viewConeAngleMax[axis]);
        }
    }
    local.m_stepSmoothingOffset = (secondLocal.m_stepSmoothingOffset - firstLocal.m_stepSmoothingOffset) * fraction + firstLocal.m_stepSmoothingOffset;
    player.m_flHullHeight = lerp(firstPlayer.m_flHullHeight, secondPlayer.m_flHullHeight);
    player.m_traversalAnimProgress = lerp(firstPlayer.m_traversalAnimProgress, secondPlayer.m_traversalAnimProgress);
    player.m_sprintTiltFrac = lerp(firstPlayer.m_sprintTiltFrac, secondPlayer.m_sprintTiltFrac);
    for (int axis = 0; axis < 3; ++axis)
        player.m_angEyeAngles[axis] = LerpAngle(fraction, firstPlayer.m_angEyeAngles[axis], secondPlayer.m_angEyeAngles[axis]);
    if ((*g_pPlayerInterpolationQuery)->GetBool())
        g_pEngineClient->GetLocalClientFlag263();
    for (int axis = 0; axis < 3; ++axis)
    {
        local.m_vecPunchBase_Angle[axis] = LerpAngle(fraction, firstLocal.m_vecPunchBase_Angle[axis], secondLocal.m_vecPunchBase_Angle[axis]);
        local.m_vecPunchBase_AngleVel[axis] = LerpAngle(fraction, firstLocal.m_vecPunchBase_AngleVel[axis], secondLocal.m_vecPunchBase_AngleVel[axis]);
        local.m_vecPunchWeapon_Angle[axis] = LerpAngle(fraction, firstLocal.m_vecPunchWeapon_Angle[axis], secondLocal.m_vecPunchWeapon_Angle[axis]);
        local.m_vecPunchWeapon_AngleVel[axis] = LerpAngle(fraction, firstLocal.m_vecPunchWeapon_AngleVel[axis], secondLocal.m_vecPunchWeapon_AngleVel[axis]);
    }
    m_currentFrame.viewOffset = (second->viewOffset - first->viewOffset) * fraction + first->viewOffset;
    return status;
}

DECLARE_HOOK(C_Player_InterpolateFieldsInternal, client.dll + 0x156E50,
             [](auto& hook, C_Player* self, float currentTime, const SingleSnapshotValues* secondSnapshot, float secondSnapshotTime) -> int
{ return self->C_Player::InterpolateFieldsInternal(currentTime, secondSnapshot, secondSnapshotTime); })
