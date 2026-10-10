#include "client/baseanimating.h"
#include "client/player.h"
#include "client/prediction.h"
#include "engine/cdll_int.h"
#include "engine/client/clientstate.h"
#include "studio.h"
#include <cmath>
#include <cstring>
#include "client/cdll_client_int.h"
#include "core/tier1.h"
#include "engine/r2engine.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"

#include <limits>

DECLARE_MODULE(ClientBaseAnimating)

C_BaseAnimating_GetModelPtr_t C_BaseAnimating__GetModelPtr;
C_BaseAnimating_GetSequenceCycleRate_t C_BaseAnimating__GetSequenceCycleRate;
C_BaseAnimating_GetSequenceCycleRateForModel_t C_BaseAnimating__GetSequenceCycleRateForModel;
C_BaseAnimating_IsValidSequence_t C_BaseAnimating__IsValidSequence;
C_BaseAnimating_IsSequenceLooping_t C_BaseAnimating__IsSequenceLooping;
C_BaseAnimating_ClampCycle_t C_BaseAnimating__ClampCycle;
C_BaseAnimating_SequenceDuration_t C_BaseAnimating__SequenceDuration;
C_BaseAnimating_GetLastVisibleCycle_t C_BaseAnimating__GetLastVisibleCycle;
C_BaseAnimating_SetCycle_t C_BaseAnimating__SetCycle;
C_BaseAnimating_GetSequenceGroundSpeed_t C_BaseAnimating__GetSequenceGroundSpeed;
C_BaseAnimating_ClearRagdoll_t C_BaseAnimating__ClearRagdoll;
C_BaseAnimating_UpdateAnimationCycle_t C_BaseAnimating__UpdateAnimationCycle;
datamap_t* PredictedAnimEventData__m_PredMap;
ConVar* sv_teststepsimulation;


datamap_t* PredictedAnimEventData::GetPredDescMap()
{
    return PredictedAnimEventData__m_PredMap;
}
const datamap_t* PredictedAnimEventData::GetPredDescMap() const
{
    return PredictedAnimEventData__m_PredMap;
}

CStudioHdr* C_BaseAnimating::GetModelPtr() const
{
    return C_BaseAnimating__GetModelPtr(this);
}
float C_BaseAnimating::GetSequenceCycleRate(int sequence)
{
    return C_BaseAnimating__GetSequenceCycleRate(this, sequence);
}
float C_BaseAnimating::GetSequenceCycleRateForModel(const CStudioHdr* hdr, int sequence) const
{
    return C_BaseAnimating__GetSequenceCycleRateForModel(this, hdr, sequence);
}
bool C_BaseAnimating::IsValidSequence(int sequence) const
{
    return C_BaseAnimating__IsValidSequence(this, sequence);
}
bool C_BaseAnimating::IsSequenceLooping(const CStudioHdr* hdr, int sequence) const
{
    return C_BaseAnimating__IsSequenceLooping(this, hdr, sequence);
}
float C_BaseAnimating::ClampCycle(float cycle, bool isLooping)
{
    return C_BaseAnimating__ClampCycle(cycle, isLooping);
}
float C_BaseAnimating::SequenceDuration(int sequence)
{
    return C_BaseAnimating__SequenceDuration(this, sequence);
}
void C_BaseAnimating::SetCycle(float cycle)
{
    C_BaseAnimating__SetCycle(this, cycle);
}
float C_BaseAnimating::GetLastVisibleCycle(const CStudioHdr* hdr, int sequence) const
{
    return C_BaseAnimating__GetLastVisibleCycle(this, hdr, sequence);
}

float C_BaseAnimating::GetSequenceGroundSpeed(const CStudioHdr* hdr, int sequence)
{
    return C_BaseAnimating__GetSequenceGroundSpeed(this, hdr, sequence);
}

void C_BaseAnimating::ClearRagdoll()
{
    C_BaseAnimating__ClearRagdoll(this);
}

void C_BaseAnimating::UpdateAnimationCycle(float currentTime)
{
    C_BaseAnimating__UpdateAnimationCycle(this, currentTime);
}
float C_BaseAnimating::InterpolateCycle(float startCycle, float endCycle, float fraction, float startTime, float endTime,
                                      int sequence) const
{
    const bool looping = IsSequenceLooping(GetModelPtr(), sequence);
    if (looping && endTime < startTime)
    {
        if (endCycle > startCycle)
            startCycle += 1.0f;
    }
    else if (looping && startCycle > endCycle)
        endCycle += 1.0f;

    const float cycle = (endCycle - startCycle) * fraction + startCycle;
    return looping ? static_cast<float>(static_cast<double>(cycle) - static_cast<int>(cycle)) : cycle;
}

void C_BaseAnimating::UpdateRemoteSequencePresentation()
{
    const CClientState* const client = GetBaseLocalClient();
    const CGlobalVarsBase* const globals = g_pClientGlobals;
    if (!client || client->m_nSignonState != eSignonState::FULL || client->IsPaused() || client->m_bIsWatchingReplay ||
        !C_BaseEntity::IsInterpolationEnabled() || g_pEngineClient->IsPlayingDemo() || g_pEngineClient->IsPlayingTimeDemo() ||
        !std::isfinite(globals->curtime))
        return;
    ClientRenderState_t& render = client->GetClientStateExtended()->m_RenderState;
    auto& entries = render.remoteSequences;

    C_BaseEntityIterator iterator;
    while (C_BaseEntity* const base = iterator.Next())
    {
        if (!base->IsNPC() || base->IsClientOnly() || base->GetPredictable() || base->m_bDormant)
            continue;
        C_BaseAnimating* const entity = base->GetBaseAnimating();
        if (!entity || entity->m_bClientSideAnimation || entity->m_pRagdoll || entity->m_killRagdoll || entity->SkipsAnimationData() ||
            entity->m_currentFrameBaseAnimating.animFrozen)
            continue;
        CStudioHdr* const hdr = entity->GetModelPtr();
        const CBaseHandle handle = entity->GetRefEHandle();
        if (!hdr || !handle.IsValid() || handle.GetEntryIndex() >= 0x4000)
            continue;

        const std::size_t index = handle.GetEntryIndex();
        if (index >= entries.size())
            entries.resize(index + 1);
        RemoteSequencePresentation_t& entry = entries[index];
        if (entry.applied)
            continue;
        const bool reset = entry.handle.ToInt() != handle.ToInt() || entry.serverCount != client->m_nServerCount ||
                           entry.frame + 1 != globals->framecount || entry.model != hdr->GetRenderHdr() || entry.parent != entity->m_pMoveParent ||
                           globals->curtime < entry.rendered.m_prevUpdateTime || entity->DidEntityTeleport();
        entry.saved = entity->m_SequenceTransitioner;
        if (reset)
            entry.rendered = entry.saved;
        entity->m_SequenceTransitioner = entry.rendered;

        // The replicated outgoing layer starts at the previous server update,
        // a full tick before its sequence becomes visible. Derive render history
        // from the preceding rendered cycle instead; keep native fade metadata,
        // parity/no-interp/model gates, layer limit and expiration semantics.
        C_SequenceTransitioner& transition = entity->m_SequenceTransitioner;
        if (!reset)
            transition.CheckForSequenceChange();
        transition.m_prevSequenceParity = entity->m_currentFrameBaseAnimating.animSequenceParity;
        transition.SequenceTransitioner_UpdateCurrent(globals->curtime);
        transition.m_prevUpdateTime = globals->curtime;
        transition.CalcWeights(globals->curtime);
        entry.handle = entity;
        entry.parent = entity->m_pMoveParent;
        entry.model = hdr->GetRenderHdr();
        entry.serverCount = client->m_nServerCount;
        entry.frame = globals->framecount;
        entry.applied = true;
        render.remoteSequencesActive = true;
    }
}

void C_BaseAnimating::RestoreRemoteSequencePresentation()
{
    auto& render = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState;
    const CBaseHandle handle = GetRefEHandle();
    const std::size_t index = handle.GetEntryIndex();
    if (!render.remoteSequencesActive || index >= render.remoteSequences.size())
        return;
    RemoteSequencePresentation_t& entry = render.remoteSequences[index];
    if (!entry.applied || entry.handle.ToInt() != handle.ToInt())
        return;
    if (C_BaseAnimating* const entity = entry.handle.Get())
    {
        entry.rendered = entity->m_SequenceTransitioner;
        entity->m_SequenceTransitioner = entry.saved;
        entity->InvalidatePhysicsRecursive(8);
    }
    entry.applied = false;
}

DECLARE_HOOK(C_BaseAnimating_StandardBlendingRulesLOD, client.dll + 0xF4780, [](auto& hook, C_BaseAnimating* self, unsigned char* flags) -> std::intptr_t
{
    const std::intptr_t result = hook.Original(self, flags);
    if (!flags[2])
        return result;
    const ClientRenderState_t& render = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState;
    if (!render.remoteSequencesActive)
        return result;
    const auto& entries = render.remoteSequences;
    const CBaseHandle handle = self->GetRefEHandle();
    const std::size_t index = handle.GetEntryIndex();
    if (index < entries.size())
    {
        const RemoteSequencePresentation_t& entry = entries[index];
        const C_SequenceTransitioner& transition = self->m_SequenceTransitioner;
        if (entry.applied && entry.handle.ToInt() == handle.ToInt() && transition.m_sequenceTransitionerLayerCount > 0 &&
            transition.m_sequenceTransitionerLayers[transition.m_sequenceTransitionerLayerCount - 1].GetWeight() > 0.0f)
        {
            // Only retain a live base-sequence fade. Keep native IK, autoplay and
            // overlay LOD flags; dropping this layer would make the sequence snap.
            flags[2] = 0;
        }
    }
    return result;
})

int C_BaseAnimating::InterpolateFieldsInternal(float currentTime, const SingleSnapshotValues* secondSnapshot,
                                              float secondSnapshotTime)
{
    if (m_pRagdoll)
    {
        if (m_killRagdoll)
            ClearRagdoll();
        return 0;
    }
    if (m_killRagdoll)
        return 0;

    const float previousCycle = GetCycle();
    const Vector3D previousOrigin = m_localOrigin;
    const QAngle previousAngles = m_localAngles;
    const float fraction = GetInterpolationFraction(currentTime, secondSnapshotTime, secondSnapshot);
    const int status = C_BaseEntity::InterpolateFieldsInternal(currentTime, secondSnapshot, secondSnapshotTime);
    if (status)
    {
        CStudioHdr* const hdr = GetModelPtr();
        const int count = hdr && !m_bClientSideAnimation ? hdr->GetNumPoseParameters() : 0;
        if (count > 0)
        {
            bool looping[32];
            float previousPoseParameters[24];
            const std::size_t poseBytes = count * sizeof(float);
            float* const pose = m_currentFrameBaseAnimating.m_flPoseParameters;
            for (int i = 0; i < count; ++i)
                looping[i] = hdr->pPoseParameter(i).loop != 0.0f;
            std::memmove(previousPoseParameters, pose, poseBytes);
            if (!SkipsAnimationData())
            {
                const SingleSnapshotValues* const firstSnapshot = m_lerpData.currentSnap;
                const float* const firstPose = firstSnapshot->animatingData->m_flPoseParameters;
                if (firstSnapshot->modelIndex == secondSnapshot->modelIndex)
                    InterpolatePoseParameters(fraction, count, firstPose, secondSnapshot->animatingData->m_flPoseParameters, looping, pose);
                else
                    std::memmove(pose, firstPose, poseBytes);
                if (std::memcmp(previousPoseParameters, pose, poseBytes) != 0)
                    InvalidatePhysicsRecursive(8);
            }
        }

        if (!SkipsAnimationData())
        {
            const AnimatingData* const first = m_lerpData.currentSnap->animatingData;
            const AnimatingData* const second = secondSnapshot->animatingData;
            m_flEstIkOffset = (second->m_flEstIkOffset - first->m_flEstIkOffset) * fraction + first->m_flEstIkOffset;
            const bool predictable = GetPredictable();
            const bool remotePlayer = !predictable && IsPlayer();
            if (hdr && (!m_bClientSideAnimation || remotePlayer))
            {
                const int firstSequence = IsValidSequence(first->animSequence) ? first->animSequence : 0;
                const int secondSequence = IsValidSequence(second->animSequence) ? second->animSequence : 0;
                const bool matchingSequence = !second->animFrozen && m_lerpData.currentSnap->modelIndex == secondSnapshot->modelIndex &&
                                              firstSequence == secondSequence && first->animSequenceParity == second->animSequenceParity;
                float firstTime = g_pClientGlobals->currentSnapTime;
                if (const C_Player* const owner = predictable ? GetPredictionOwner() : nullptr)
                {
                    const CGlobalVarsBase* const globals = g_pClientGlobals;
                    firstTime = globals->replayDelay + owner->m_lerpData.currentSnap->playerData->timeBase;
                    const SingleSnapshotValues* const ownerSecond =
                        secondSnapshot == m_lerpData.futureSnap ? owner->m_lerpData.futureSnap : owner->m_lerpData.lastSnap;
                    secondSnapshotTime = globals->replayDelay + ownerSecond->playerData->timeBase;
                }
                const float firstCycle = CalculateCycleForAnimationStateAtTime(this, firstTime, first->animStartCycle, first->animStartTime,
                                                                                 first->animPlaybackRate, firstSequence, first->animFrozen, nullptr);
                const AnimatingData* const cycleEnd = matchingSequence ? second : first;
                const float secondCycle = CalculateCycleForAnimationStateAtTime(
                    this, secondSnapshotTime, cycleEnd->animStartCycle, cycleEnd->animStartTime, cycleEnd->animPlaybackRate,
                    matchingSequence ? secondSequence : firstSequence, cycleEnd->animFrozen, nullptr);
                SetCycle(InterpolateCycle(firstCycle, secondCycle, fraction, firstTime, secondSnapshotTime, firstSequence));
            }
        }
        m_SequenceTransitioner.m_prevUpdateTime = currentTime;
    }
    UpdateAnimationCycle(currentTime);
    CheckInterpolatedTransformChanges(previousOrigin, previousAngles, GetCycle() != previousCycle ? 8 : 0);
    return status;
}

DECLARE_HOOK(C_BaseAnimating_InterpolateFieldsInternal, client.dll + 0xF9940,
             [](auto&, C_BaseAnimating* self, float currentTime, const SingleSnapshotValues* secondSnapshot, float secondSnapshotTime) -> int
{ return self->C_BaseAnimating::InterpolateFieldsInternal(currentTime, secondSnapshot, secondSnapshotTime); })

DECLARE_HOOK(C_BaseAnimating_Release, client.dll + 0xFD700, [](auto& hook, IClientNetworkable* self)
{
    C_BaseAnimating* const entity = static_cast<C_BaseAnimating*>(self->GetIClientUnknown()->GetBaseEntity());
    g_pClientSidePrediction->RestoreRenderState(entity);
    entity->RestoreRemoteSequencePresentation();
    hook.Original(self);
})

DECLARE_HOOK(C_BaseAnimating_ClientFrameAdvance, client.dll + 0xF0AD0, [](auto&, C_BaseAnimating* self)
{
    const CStudioHdr* const hdr = self->GetModelPtr();
    if (!hdr || self->m_animPaused)
        return;

    const float currentTime = g_pClientGlobals->curtime;
    const bool animActive = self->m_animActive;
    float interval;
    if (animActive)
    {
        interval = currentTime - self->m_animRelativeData.m_animBlendBeginTime;
        if (sv_teststepsimulation->GetBool())
            interval -= 0.1f;
        interval = interval > 0.0f ? interval : 0.0f;
    }
    else
    {
        interval = currentTime - self->m_flAnimTime;
        if (interval <= 0.0f)
            return;
        if (self->m_flAnimTime == 0.0f)
            interval = 0.0f;
    }

    float cycle = self->GetSequenceCycleRate(self->GetSequence()) * interval;
    cycle *= self->GetPlaybackRate();
    if (!animActive)
        cycle = self->GetCycle() + cycle;

    const float previousCycle = self->GetCycle();
    const float previousAnimTime = self->m_flAnimTime;
    self->m_flAnimTime = currentTime;
    self->m_prevClientAnimTime = previousAnimTime;
    self->m_prevClientCycle = previousCycle;

    if (cycle < 0.0f || cycle >= 1.0f)
    {
        if (self->IsSequenceLooping(hdr, self->GetSequence()))
        {
            const double wideCycle = cycle;
            const int wholeCycle = wideCycle >= std::numeric_limits<int>::min() && wideCycle <= std::numeric_limits<int>::max()
                                       ? static_cast<int>(wideCycle)
                                       : std::numeric_limits<int>::min();
            cycle = static_cast<float>(wideCycle - wholeCycle);
        }
        else
        {
            cycle = cycle < 0.0f ? 0.0f : 1.0f;
        }
        self->m_bSequenceFinished = true;
    }
    else if (cycle > self->GetLastVisibleCycle(hdr, self->GetSequence()))
    {
        self->m_bSequenceFinished = true;
    }

    self->SetCycle(cycle);
    self->InvalidatePhysicsRecursive(8u);
    self->m_flGroundSpeed = self->GetSequenceGroundSpeed(hdr, self->GetSequence());
})

ON_DLL_LOAD_CLIENT("client.dll", BaseAnimatingMethods, [](CModule module)
{
    C_BaseAnimating__GetModelPtr = module.Offset(0x88900).RCast<decltype(C_BaseAnimating__GetModelPtr)>();
    C_BaseAnimating__GetSequenceCycleRate = module.Offset(0xAB4B0).RCast<decltype(C_BaseAnimating__GetSequenceCycleRate)>();
    C_BaseAnimating__GetSequenceCycleRateForModel = module.Offset(0xAB530).RCast<decltype(C_BaseAnimating__GetSequenceCycleRateForModel)>();
    C_BaseAnimating__IsValidSequence = module.Offset(0xFA4F0).RCast<decltype(C_BaseAnimating__IsValidSequence)>();
    C_BaseAnimating__IsSequenceLooping = module.Offset(0xAC710).RCast<decltype(C_BaseAnimating__IsSequenceLooping)>();
    C_BaseAnimating__ClampCycle = module.Offset(0xAA260).RCast<decltype(C_BaseAnimating__ClampCycle)>();
    C_BaseAnimating__SequenceDuration = module.Offset(0xADA20).RCast<decltype(C_BaseAnimating__SequenceDuration)>();
    PredictedAnimEventData__m_PredMap = module.Offset(0xAF46E0).RCast<datamap_t*>();

    C_BaseAnimating__GetLastVisibleCycle = module.Offset(0xAB310).RCast<decltype(C_BaseAnimating__GetLastVisibleCycle)>();
    C_BaseAnimating__SetCycle = module.Offset(0xADB20).RCast<decltype(C_BaseAnimating__SetCycle)>();
    C_BaseAnimating__GetSequenceGroundSpeed = module.Offset(0xF62A0).RCast<decltype(C_BaseAnimating__GetSequenceGroundSpeed)>();
    C_BaseAnimating__ClearRagdoll = module.Offset(0xF0A00).RCast<decltype(C_BaseAnimating__ClearRagdoll)>();
    C_BaseAnimating__UpdateAnimationCycle = module.Offset(0xF07A0).RCast<decltype(C_BaseAnimating__UpdateAnimationCycle)>();
    sv_teststepsimulation = module.Offset(0xBFC770).RCast<ConVar*>();
    DISPATCH_MODULE(ClientBaseAnimating)
})
