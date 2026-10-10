#include "client/prediction.h"
#include "client/baseentity.h"
#include "client/baseviewmodel.h"
#include "client/baseanimatingoverlay.h"
#include "client/cdll_client_int.h"
#include "client/input.h"
#include "client/player.h"
#include "client/weaponx.h"
#include "core/tier1.h"
#include "engine/cdll_int.h"
#include "engine/client/clientstate.h"
#include "engine/r2engine.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"
#include "tier1/cvar.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

DECLARE_MODULE(ClientPredictionHooks)

CPrediction_InPrediction_t CPrediction__InPrediction;
CPrediction* g_pClientSidePrediction;
extern ConVar** cl_interpolate;


bool CPrediction::CanInterpolateRenderState() const
{
    if (!g_pVanillaCompatibility->GetVanillaCompatibility() || !g_pInput || !(*cl_interpolate)->GetBool())
        return false;
    const CClientState* const client = GetBaseLocalClient();
    if (!client || client->m_nSignonState != eSignonState::FULL || client->m_nDeltaTick < 0 || client->IsPaused() || client->m_bIsWatchingReplay ||
        !client->m_bCanProcessSignedOnLocalClientInput || g_pEngineClient->IsPlayingDemo() || g_pEngineClient->IsPlayingTimeDemo())
        return false;
    static const ConVar* const host_timescale = g_pCVar->FindVar("host_timescale");
    return host_timescale->GetFloat() == 1.0f && g_pEngineClient->IsMoveOneCmdPerClientFrameEnabled();
}


void CPrediction::RestoreRenderState(C_BaseEntity* target)
{
    auto& render = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState;
    if (!target && render.remoteSequencesActive)
    {
        for (RemoteSequencePresentation_t& entry : render.remoteSequences)
        {
            if (!entry.applied)
                continue;
            if (C_BaseAnimating* const entity = entry.handle.Get())
                entity->RestoreRemoteSequencePresentation();
            else
                entry.applied = false;
        }
        render.remoteSequencesActive = false;
    }
    C_BaseViewModel* const viewModel = render.viewModel.entity.Get();
    if (viewModel && (!target || target == viewModel))
        viewModel->RestoreRenderPresentation();
    else if (!target)
        render.viewModel.active = false;
    if (!target || target == render.predictedPlayer.Get())
        render.cameraTimeActive = false;
    for (PredictedRenderEntity_t& entry : render.predictedEntities)
    {
        if (!entry.changed)
            continue;
        C_BaseEntity* const entity = entry.handle.Get();
        if (target && entity != target)
            continue;
        if (entity)
        {
            const PredictedRenderSample_t& saved = entry.saved;
            entity->m_localOrigin = saved.origin;
            entity->m_localAngles = saved.angles;
            entity->m_currentFrame.viewOffset = saved.viewOffset;
            if (entry.changed & 2)
            {
                C_BaseAnimating* const animating = entity->GetBaseAnimating();
                entity->m_currentFrame.animCycle = saved.cycle;
                animating->m_flEstIkOffset = saved.ikOffset;
                animating->m_currentFrameBaseAnimating.animPlaybackRate = saved.animation.animPlaybackRate;
                std::memcpy(animating->m_currentFrameBaseAnimating.m_flPoseParameters, saved.animation.m_flPoseParameters,
                            sizeof(saved.animation.m_flPoseParameters));
            }
            if (entry.changedLayers)
            {
                AnimationOverlayData& layers = entity->GetBaseAnimatingOverlay()->m_currentFrameAnimationOverlay;
                for (int i = 0; i < 8; ++i)
                {
                    if (!(entry.changedLayers & (1u << i)))
                        continue;
                    layers.animOverlayWeight[i] = saved.overlays.animOverlayWeight[i];
                    layers.animOverlayPlaybackRate[i] = saved.overlays.animOverlayPlaybackRate[i];
                    layers.animOverlayAnimTime[i] = saved.overlays.animOverlayAnimTime[i];
                    layers.animOverlayFadeInDuration[i] = saved.overlays.animOverlayFadeInDuration[i];
                    layers.animOverlayFadeOutDuration[i] = saved.overlays.animOverlayFadeOutDuration[i];
                    layers.animOverlayCycle[i] = saved.overlays.animOverlayCycle[i];
                }
            }
            if (entry.changed & 4)
            {
                C_Player* const player = entity->MyPlayerPointer();
                player->m_currentFramePlayer.timeBase = saved.player.timeBase;
                player->m_currentFramePlayer.m_flHullHeight = saved.player.m_flHullHeight;
                player->m_currentFramePlayer.m_traversalAnimProgress = saved.player.m_traversalAnimProgress;
                player->m_currentFramePlayer.m_sprintTiltFrac = saved.player.m_sprintTiltFrac;
                std::memcpy(&player->m_currentFrameLocalPlayer.m_viewConeAngleMin, &saved.localPlayer, sizeof(LocalPlayerData));
            }
            if (entry.changed & 8)
                std::memcpy(entity->MyWeaponXPointer()->m_smartAmmo.currentFrameSmartAmmoFractions, saved.smartAmmoFractions,
                            sizeof(saved.smartAmmoFractions));
            entity->InvalidatePhysicsRecursive(15);
        }
        entry.changed = 0;
        entry.changedLayers = 0;
    }
}

void CPrediction::CaptureCommittedPrediction(int commandNumber)
{
    const CClientState* const client = GetBaseLocalClient();
    auto& render = client->GetClientStateExtended()->m_RenderState;
    C_Player* const localPlayer = C_Player::GetLocalPlayer();
    const CUserCmd* const command = g_pInput->GetUserCmd(0, commandNumber);
    if (!localPlayer || localPlayer != C_Player::GetLocalViewPlayer() || !command || !(command->frameTime > 0.0f))
        return;
    if (render.predictedServerCount != client->m_nServerCount || render.predictedPlayer.Get() != localPlayer)
    {
        render.predictedEntities.clear();
        render.predictedServerCount = client->m_nServerCount;
        render.predictedPlayer = localPlayer;
    }
    std::erase_if(render.predictedEntities, [client](const PredictedRenderEntity_t& entry)
    {
        C_BaseEntity* const entity = entry.handle.Get();
        return !entity || !entity->GetPredictable() || entry.samples[entry.latest].commandNumber < client->m_nOutgoingCommandNumber - 2;
    });

    const CPredictableList* const predictables = GetPredictables(0);
    const int count = predictables->GetPredictableCount();
    render.predictedEntities.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        C_BaseEntity* const entity = predictables->GetPredictable(i);
        if (!entity || !entity->GetPredictable() || entity->IsViewModel() || (entity != localPlayer && entity->GetPredictionOwner() != localPlayer))
            continue;
        const CHandle<C_BaseEntity> handle(entity);
        auto found = std::find_if(render.predictedEntities.begin(), render.predictedEntities.end(),
                                  [handle](const PredictedRenderEntity_t& entry) { return entry.handle.ToInt() == handle.ToInt(); });
        if (found == render.predictedEntities.end())
        {
            render.predictedEntities.emplace_back();
            found = render.predictedEntities.end() - 1;
            found->handle = handle;
        }
        PredictedRenderEntity_t& entry = *found;
        int target = entry.latest;
        if (entry.samples[target].commandNumber != commandNumber)
        {
            if (entry.samples[target].commandNumber + 1 == commandNumber)
                target = entry.latest ^= 1;
            else if (entry.samples[target ^ 1].commandNumber == commandNumber)
                target ^= 1;
            else
                entry.samples[target ^ 1].commandNumber = -1;
        }
        PredictedRenderSample_t& sample = entry.samples[target];
        entity->CapturePredictedRenderSample(sample);
        sample.commandNumber = commandNumber;
        sample.frameTime = command->frameTime;
        if (entity->DidEntityTeleport())
            entry.samples[target ^ 1].commandNumber = -1;
    }
}

DECLARE_HOOK(CPrediction_SelectFirstCommand, client.dll + 0x2D61A0,
             [](auto& hook, CPrediction* self, unsigned int splitScreenSlot, C_Player* player, bool receivedNewWorldUpdate,
                int incomingAcknowledged) -> int
{
    self->RestoreRenderState();
    const int firstCommand = hook.Original(self, splitScreenSlot, player, receivedNewWorldUpdate, incomingAcknowledged);
    if (receivedNewWorldUpdate && splitScreenSlot == 0 && self->CanInterpolateRenderState())
    {
        const int outgoing = GetBaseLocalClient()->m_nOutgoingCommandNumber;
        if (firstCommand > 0 && firstCommand - 1 >= outgoing - 1 && firstCommand - 1 <= outgoing)
        {
            self->CaptureCommittedPrediction(firstCommand - 1);
        }
    }
    return firstCommand;
})

void CPrediction::InterpolateRenderState()
{
    if (!CanInterpolateRenderState())
        return;
    const CClientState* const client = GetBaseLocalClient();
    auto& render = client->GetClientStateExtended()->m_RenderState;
    if (render.predictedServerCount != client->m_nServerCount || render.predictedPlayer.Get() != C_Player::GetLocalPlayer())
        return;
    const float pendingTime = g_pInput->GetExtraMouseSampleTime();
    for (PredictedRenderEntity_t& entry : render.predictedEntities)
    {
        C_BaseEntity* const entity = entry.handle.Get();
        const PredictedRenderSample_t& first = entry.samples[entry.latest ^ 1];
        const PredictedRenderSample_t& second = entry.samples[entry.latest];
        if (!entity || !entity->GetPredictable() || second.commandNumber != client->m_nOutgoingCommandNumber ||
            first.commandNumber + 1 != second.commandNumber || first.commandNumber < 0 || !(second.frameTime >= 0.005f) ||
            first.parent.ToInt() != second.parent.ToInt() || second.parent.ToInt() != entity->m_pMoveParent.ToInt() ||
            first.modelIndex != second.modelIndex || second.modelIndex != entity->m_currentFrame.modelIndex)
            continue;
        C_Player* const player = entity->MyPlayerPointer();
        if (player && player->m_bLerpMissingParent)
            continue;
        const float fraction = std::clamp(1.0f - (0.005f - pendingTime) / second.frameTime, 0.0f, 1.0f);
        if (fraction == 1.0f)
            continue;
        const auto lerp = [fraction](const auto& a, const auto& b) { return (b - a) * fraction + a; };
        entity->CapturePredictedRenderSample(entry.saved);
        entry.changed = 1;
        entity->m_localOrigin = lerp(first.origin, second.origin);
        entity->m_currentFrame.viewOffset = lerp(first.viewOffset, second.viewOffset);
        if (first.angles != second.angles)
        {
            alignas(16) Quaternion a, b, rotation;
            AngleQuaternion(first.angles, a);
            AngleQuaternion(second.angles, b);
            QuaternionSlerp(a, b, fraction, rotation);
            QuaternionAngles(rotation, entity->m_localAngles);
        }
        if (C_BaseAnimating* const animating = entity->GetBaseAnimating())
        {
            CStudioHdr* const hdr = animating->GetModelPtr();
            const auto& a = first.animation;
            const auto& b = second.animation;
            if (hdr && !animating->m_bClientSideAnimation && !animating->m_pRagdoll && a.animSequence == b.animSequence &&
                a.animSequenceParity == b.animSequenceParity && b.animSequence == animating->GetSequence())
            {
                entry.changed |= 2;
                auto& current = animating->m_currentFrameBaseAnimating;
                bool looping[24];
                const int poseCount = std::min(hdr->GetNumPoseParameters(), 24);
                for (int i = 0; i < poseCount; ++i)
                    looping[i] = hdr->pPoseParameter(i).loop != 0.0f;
                InterpolatePoseParameters(fraction, poseCount, a.m_flPoseParameters, b.m_flPoseParameters, looping, current.m_flPoseParameters);
                current.animPlaybackRate = lerp(a.animPlaybackRate, b.animPlaybackRate);
                animating->m_flEstIkOffset = lerp(first.ikOffset, second.ikOffset);
                entity->m_currentFrame.animCycle =
                    animating->InterpolateCycle(first.cycle, second.cycle, fraction, 0.0f, second.frameTime, b.animSequence);
            }
        }
        if (C_BaseAnimatingOverlay* const overlay = entity->GetBaseAnimatingOverlay())
        {
            auto& layers = overlay->m_currentFrameAnimationOverlay;
            const auto& a = first.overlays;
            const auto& b = second.overlays;
            for (int i = 0; i < std::min(overlay->m_AnimOverlayCount, 8); ++i)
            {
                if (!layers.animOverlayIsActive[i] || !a.animOverlayIsActive[i] || !b.animOverlayIsActive[i] ||
                    a.animOverlaySequence[i] != b.animOverlaySequence[i] || a.animOverlayModelIndex[i] != b.animOverlayModelIndex[i] ||
                    b.animOverlaySequence[i] != layers.animOverlaySequence[i] || !overlay->GetModelPtr())
                    continue;
                entry.changedLayers |= 1u << i;
                layers.animOverlayWeight[i] = lerp(a.animOverlayWeight[i], b.animOverlayWeight[i]);
                layers.animOverlayPlaybackRate[i] = lerp(a.animOverlayPlaybackRate[i], b.animOverlayPlaybackRate[i]);
                layers.animOverlayAnimTime[i] = lerp(a.animOverlayAnimTime[i], b.animOverlayAnimTime[i]);
                layers.animOverlayFadeInDuration[i] = lerp(a.animOverlayFadeInDuration[i], b.animOverlayFadeInDuration[i]);
                layers.animOverlayFadeOutDuration[i] = lerp(a.animOverlayFadeOutDuration[i], b.animOverlayFadeOutDuration[i]);
                layers.animOverlayCycle[i] =
                    overlay->InterpolateCycle(a.animOverlayCycle[i], b.animOverlayCycle[i], fraction, 0.0f, second.frameTime, b.animOverlaySequence[i]);
            }
        }
        if (player)
        {
            entry.changed |= 4;
            auto& current = player->m_currentFramePlayer;
            const auto& a = first.player;
            const auto& b = second.player;
            current.timeBase = lerp(a.timeBase, b.timeBase);
            if (player == render.predictedPlayer.Get())
            {
                render.cameraTime = current.timeBase;
                render.cameraTimeActive = true;
            }
            current.m_flHullHeight = lerp(a.m_flHullHeight, b.m_flHullHeight);
            current.m_traversalAnimProgress = lerp(a.m_traversalAnimProgress, b.m_traversalAnimProgress);
            current.m_sprintTiltFrac = lerp(a.m_sprintTiltFrac, b.m_sprintTiltFrac);
            auto& local = player->m_currentFrameLocalPlayer;
            const auto& from = first.localPlayer;
            const auto& to = second.localPlayer;
            if (first.viewConeParity == second.viewConeParity)
            {
                for (int axis = 0; axis < 3; ++axis)
                {
                    local.m_viewConeAngleMin[axis] = LerpAngle(fraction, from.m_viewConeAngleMin[axis], to.m_viewConeAngleMin[axis]);
                    local.m_viewConeAngleMax[axis] = LerpAngle(fraction, from.m_viewConeAngleMax[axis], to.m_viewConeAngleMax[axis]);
                }
            }
            local.m_stepSmoothingOffset = lerp(from.m_stepSmoothingOffset, to.m_stepSmoothingOffset);
            for (int axis = 0; axis < 3; ++axis)
            {
                local.m_vecPunchBase_Angle[axis] = LerpAngle(fraction, from.m_vecPunchBase_Angle[axis], to.m_vecPunchBase_Angle[axis]);
                local.m_vecPunchBase_AngleVel[axis] = LerpAngle(fraction, from.m_vecPunchBase_AngleVel[axis], to.m_vecPunchBase_AngleVel[axis]);
                local.m_vecPunchWeapon_Angle[axis] = LerpAngle(fraction, from.m_vecPunchWeapon_Angle[axis], to.m_vecPunchWeapon_Angle[axis]);
                local.m_vecPunchWeapon_AngleVel[axis] = LerpAngle(fraction, from.m_vecPunchWeapon_AngleVel[axis], to.m_vecPunchWeapon_AngleVel[axis]);
            }
        }
        if (C_WeaponX* const weapon = entity->MyWeaponXPointer())
        {
            entry.changed |= 8;
            for (int i = 0; i < 8; ++i)
                weapon->m_smartAmmo.currentFrameSmartAmmoFractions[i] = lerp(first.smartAmmoFractions[i], second.smartAmmoFractions[i]);
        }
        entity->InvalidatePhysicsRecursive(15);
    }
}

DECLARE_HOOK(CPrediction_StorePredictionResults, client.dll + 0x2D9830,
             [](auto& hook, CPrediction* self, unsigned int splitScreenSlot, int commandNumber, int predictedFrame) -> std::intptr_t
{
    self->RestoreRenderState();
    const std::intptr_t result = hook.Original(self, splitScreenSlot, commandNumber, predictedFrame);
    if (splitScreenSlot == 0 && predictedFrame != -1 && self->CanInterpolateRenderState())
    {
        const int outgoing = GetBaseLocalClient()->m_nOutgoingCommandNumber;
        if (commandNumber >= outgoing - 1 && commandNumber <= outgoing)
            self->CaptureCommittedPrediction(commandNumber);
    }
    return result;
})

void CPrediction::OnFrameStart()
{
    auto& render = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState;
    if (!CanInterpolateRenderState())
        render.predictedEntities.clear();
}

bool CPrediction::GetRenderCameraTime(const C_Player* player, float& time) const
{
    const auto& render = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState;
    if (!render.cameraTimeActive || player != render.predictedPlayer.Get())
        return false;
    time = render.cameraTime;
    return true;
}


bool CPrediction::InPrediction() const
{
    return CPrediction__InPrediction(this);
}


ON_DLL_LOAD_CLIENT("client.dll", ClientPrediction, [](CModule module)
{
    g_pClientSidePrediction = Sys_GetFactoryPtr("client.dll", VCLIENT_PREDICTION_INTERFACE_VERSION).RCast<CPrediction*>();
    CPrediction__InPrediction = module.Offset(0x2D6960).RCast<CPrediction_InPrediction_t>();
    DISPATCH_MODULE(ClientPredictionHooks)
})
