#include "game/client/c_player.h"
#include "tier0/callbacks.h"

#include "engine/client/cdll_engine_int.h"
#include "engine/client/clientstate.h"
#include "game/client/c_baseviewmodel.h"
#include "game/client/cdll_client_int.h"
#include "game/client/input.h"
#include "game/client/prediction.h"
#include "game/shared/r1/c_weapon_x.h"
#include "game/shared/weapon_parse.h"
#include "tier0/hooks.h"
#include "tier0/vanilla.h"
#include "tier1/convar.h"
#include "util/utils.h"
#include "vscript/languages/squirrel_re/squirrel.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>

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

ConVar* viewangle_debug;

C_PlayerExtended C_Player::sm_PlayerExtended;

C_PlayerExtended* C_Player::GetPlayerExtended()
{
    return &sm_PlayerExtended;
}

C_PlayerExtended::C_PlayerExtended()
{
    m_WeaponPrediction.Clear();
}

void C_PlayerExtended::Reset()
{
    auto& prediction = m_WeaponPrediction;
    if (prediction.retired)
        return;
    if (prediction.player)
        RetireWeaponPredictionOwner(prediction);
    prediction.Clear();
    for (auto& snapshot : m_Authoritative)
        snapshot.command = -1;
    m_nNextAuthoritative = 0;
}

void C_PlayerExtended::ClearClientState()
{
    C_Player::sm_PlayerExtended.Reset();
}

bool C_PlayerExtended::SelectBaseline(C_Player* player, const bool receivedNewWorldUpdate, const int acknowledgedCommand)
{
    return SelectWeaponPredictionBaseline(player, receivedNewWorldUpdate, acknowledgedCommand);
}

void C_PlayerExtended::RestoreBaseline(const int commandNumber)
{
    RestoreWeaponPredictionBaseline(m_WeaponPrediction, commandNumber);
}

void C_PlayerExtended::StoreResults(const int commandNumber)
{
    StoreWeaponPredictionResults(m_WeaponPrediction, commandNumber);
}

bool C_Player::UsesWeaponPrediction() const
{
    const CClientState* const client = GetBaseLocalClient ? GetBaseLocalClient() : nullptr;
    return IsScriptWeaponPredictionEnabled() && !g_pVanillaCompatibility->GetVanillaCompatibility() && client &&
           client->IsConnectedToExtendedServer() && this == C_Player::GetLocalPlayer();
}

PredictedMovementState C_Player::GetPredictedMovementState()
{
    PredictedMovementState state;
    for (int i = 0; i < PREDICTED_MOVEMENT_SCALE_COUNT; ++i)
        state.scales[i] = GetPredictedMovementScale(static_cast<PredictedMovementScale>(i));
    for (int i = 0; i < PREDICTED_MOVEMENT_FLAG_COUNT; ++i)
        state.flags[i] = GetPredictedMovementFlag(static_cast<PredictedMovementFlag>(i));
    return state;
}

void C_Player::RestorePredictedMovementState(const PredictedMovementState& state)
{
    for (int i = 0; i < PREDICTED_MOVEMENT_SCALE_COUNT; ++i)
        SetPredictedMovementScale(static_cast<PredictedMovementScale>(i), state.scales[i]);
    for (int i = 0; i < PREDICTED_MOVEMENT_FLAG_COUNT; ++i)
        SetPredictedMovementFlag(static_cast<PredictedMovementFlag>(i), state.flags[i]);
}

float C_Player::GetPredictedMovementScale(const PredictedMovementScale setting) const
{
    switch (setting)
    {
    case PredictedMovementScale::Gravity:
        return m_flGravity;
    case PredictedMovementScale::AirSpeed:
        return m_airSpeed;
    case PredictedMovementScale::AirAcceleration:
        return m_airAcceleration;
    case PredictedMovementScale::GroundFriction:
        return m_groundFrictionScale;
    case PredictedMovementScale::WallrunFriction:
        return m_wallrunFrictionScale;
    case PredictedMovementScale::PowerRegenRate:
        return m_powerRegenRateScale;
    default:
        assert(false);
        return 0.0f;
    }
}

void C_Player::SetPredictedMovementScale(const PredictedMovementScale setting, const float value)
{
    switch (setting)
    {
    case PredictedMovementScale::Gravity:
        m_flGravity = value;
        break;
    case PredictedMovementScale::AirSpeed:
        m_airSpeed = value;
        break;
    case PredictedMovementScale::AirAcceleration:
        m_airAcceleration = value;
        break;
    case PredictedMovementScale::GroundFriction:
        m_groundFrictionScale = value;
        break;
    case PredictedMovementScale::WallrunFriction:
        m_wallrunFrictionScale = value;
        break;
    case PredictedMovementScale::PowerRegenRate:
        m_powerRegenRateScale = value;
        break;
    default:
        assert(false);
        break;
    }
}

bool C_Player::GetPredictedMovementFlag(const PredictedMovementFlag setting) const
{
    switch (setting)
    {
    case PredictedMovementFlag::DodgeDisabled:
        return (m_playerFlags & PF_DODGE_DISABLED) != 0;
    case PredictedMovementFlag::TitanDisembarkEnabled:
        return m_titanDisembarkEnabled;
    default:
        assert(false);
        return false;
    }
}

void C_Player::SetPredictedMovementFlag(const PredictedMovementFlag setting, const bool value)
{
    switch (setting)
    {
    case PredictedMovementFlag::DodgeDisabled:
        if (value)
            m_playerFlags |= PF_DODGE_DISABLED;
        else
            m_playerFlags &= ~PF_DODGE_DISABLED;
        break;
    case PredictedMovementFlag::TitanDisembarkEnabled:
        m_titanDisembarkEnabled = value;
        break;
    default:
        assert(false);
        break;
    }
}

Vector3D C_Player::GetPredictedMovementVelocity()
{
    return GetAbsVelocity();
}

void C_Player::SetPredictedMovementVelocity(const Vector3D& velocity)
{
    SetAbsVelocity(velocity);
}

void C_Player::SetPredictedDodgePower(const float power)
{
    m_flSuitPower = power;
}

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

ADD_SQFUNC("string", GetUID, "entity player", "Get uid of a player", ScriptContext::CLIENT)
{
    const C_Player* const player = g_pSquirrel[ScriptContext::CLIENT]->getentity<C_Player>(sqvm, 1);
    if (!player)
    {
        spdlog::error("print_lobby_uids: Invalid player entity.");
        return SQRESULT_ERROR;
    }

    char uidBuffer[64];
    sprintf_s(uidBuffer, "%llu", player->GetPlatformUserId());
    g_pSquirrel[ScriptContext::CLIENT]->pushstring(sqvm, uidBuffer, -1);
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("vector", GetWallNormalVector, "entity", "", ScriptContext::CLIENT)
{
    auto ent = g_pSquirrel[ScriptContext::CLIENT]->getentity<uintptr_t>(sqvm, 1);
    Vector3D wallNormalVector = *(Vector3D*)(((uintptr_t)ent + 0x2BA0));

    g_pSquirrel[ScriptContext::CLIENT]->pushvector(sqvm, wallNormalVector);
    return SQRESULT_NOTNULL;
}

ON_DLL_LOAD_CLIENT("client.dll", ClientPlayerMethods, [](CModule module)
{
    viewangle_debug = module.Offset(0x11C1080).RCast<decltype(viewangle_debug)>();
    C_Player__GetLocalViewPlayer = module.Offset(0x14EF00).RCast<C_Player_GetLocalPlayer_t>();
    C_Player__GetLocalPlayer = module.Offset(0x14EF40).RCast<C_Player_GetLocalPlayer_t>();
    C_Player__IsMantling = module.Offset(0x9E0B0).RCast<C_Player_IsMantling_t>();
    C_Player__GetActiveWeapon = module.Offset(0xB19C0).RCast<decltype(C_Player__GetActiveWeapon)>();
    C_Player__GetViewModel = module.Offset(0x14FFA0).RCast<decltype(C_Player__GetViewModel)>();
    C_Player__GetZoomFraction = module.Offset(0x2C8440).RCast<decltype(C_Player__GetZoomFraction)>();
    C_Player__GetZoomOutDuration = module.Offset(0x2C8570).RCast<decltype(C_Player__GetZoomOutDuration)>();
    C_Player__GetAimDirection = module.Offset(0x2C56B0).RCast<decltype(C_Player__GetAimDirection)>();
    RegisterPlayerPredictionScriptMethods(ScriptContext::CLIENT, module.Offset(0xC217A0).RCast<ScriptClassDesc_t*>());
    DISPATCH_MODULE(ClientPlayer)
})

bool C_Player::CanPresentLocalPlayer() const
{
    const CClientState* const client = GetBaseLocalClient();
    return this == C_Player::GetLocalPlayer() && this == C_Player::GetLocalViewPlayer() && m_lifeState == 0 && GetPredictable() && client &&
           client->m_nSignonState == eSignonState::FULL && client->m_nDeltaTick >= 0 && !client->IsPaused() && !client->m_bIsWatchingReplay &&
           client->m_bCanProcessSignedOnLocalClientInput && !g_pEngineClient->IsPlayingDemo() && !g_pEngineClient->IsPlayingTimeDemo();
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
    const bool reset = zoom.player.Get() != this || zoom.weapon.Get() != weapon || zoom.serverCount != serverCount || DidEntityTeleport();
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
    return zoom.active && self == zoom.player.Get() && !g_pClientSidePrediction->InPrediction() ? static_cast<float>(zoom.fraction)
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
    if (self == g_pCrosshairQueryPlayer && crosshair.active && self == crosshair.player.Get() && crosshair.frame == g_pClientGlobals->framecount &&
        !g_pClientSidePrediction->InPrediction())
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

        const QAngle attackAngles = self->pl.a_angle;
        const ScopeGuard restoreAngles([self, attackAngles] { self->pl.a_angle = attackAngles; });
        if (sampledInput)
            self->pl.a_angle = self->m_lastUCmdAttackAngles;
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
    local.m_stepSmoothingOffset =
        (secondLocal.m_stepSmoothingOffset - firstLocal.m_stepSmoothingOffset) * fraction + firstLocal.m_stepSmoothingOffset;
    player.m_flHullHeight = lerp(firstPlayer.m_flHullHeight, secondPlayer.m_flHullHeight);
    player.m_traversalAnimProgress = lerp(firstPlayer.m_traversalAnimProgress, secondPlayer.m_traversalAnimProgress);
    player.m_sprintTiltFrac = lerp(firstPlayer.m_sprintTiltFrac, secondPlayer.m_sprintTiltFrac);
    for (int axis = 0; axis < 3; ++axis)
        player.m_angEyeAngles[axis] = LerpAngle(fraction, firstPlayer.m_angEyeAngles[axis], secondPlayer.m_angEyeAngles[axis]);
    if (viewangle_debug->GetBool())
        g_pEngineClient->IsWatchingReplay();
    for (int axis = 0; axis < 3; ++axis)
    {
        local.m_vecPunchBase_Angle[axis] = LerpAngle(fraction, firstLocal.m_vecPunchBase_Angle[axis], secondLocal.m_vecPunchBase_Angle[axis]);
        local.m_vecPunchBase_AngleVel[axis] =
            LerpAngle(fraction, firstLocal.m_vecPunchBase_AngleVel[axis], secondLocal.m_vecPunchBase_AngleVel[axis]);
        local.m_vecPunchWeapon_Angle[axis] = LerpAngle(fraction, firstLocal.m_vecPunchWeapon_Angle[axis], secondLocal.m_vecPunchWeapon_Angle[axis]);
        local.m_vecPunchWeapon_AngleVel[axis] =
            LerpAngle(fraction, firstLocal.m_vecPunchWeapon_AngleVel[axis], secondLocal.m_vecPunchWeapon_AngleVel[axis]);
    }
    m_currentFrame.viewOffset = (second->viewOffset - first->viewOffset) * fraction + first->viewOffset;
    return status;
}

DECLARE_HOOK(C_Player_InterpolateFieldsInternal, client.dll + 0x156E50,
             [](auto& hook, C_Player* self, float currentTime, const SingleSnapshotValues* secondSnapshot, float secondSnapshotTime) -> int
{ return self->C_Player::InterpolateFieldsInternal(currentTime, secondSnapshot, secondSnapshotTime); })
