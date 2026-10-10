#include "client/baseanimatingoverlay.h"
#include "client/player.h"
#include "client/cdll_client_int.h"
#include "core/tier1.h"
#include "tier0/hooks.h"

DECLARE_MODULE(ClientBaseAnimatingOverlay)

using C_BaseAnimatingOverlay_SetLayerActive_t = void (*)(C_BaseAnimatingOverlay*, int, bool);
C_BaseAnimatingOverlay_SetLayerActive_t C_BaseAnimatingOverlay__SetLayerActive;
using C_BaseAnimatingOverlay_IsLayerActive_t = bool (*)(C_BaseAnimatingOverlay*, int);
C_BaseAnimatingOverlay_IsLayerActive_t C_BaseAnimatingOverlay__IsLayerActive;
using C_BaseAnimatingOverlay_AnimOverlay_SetSequence_t = void (*)(C_BaseAnimatingOverlay*, int, int);
C_BaseAnimatingOverlay_AnimOverlay_SetSequence_t C_BaseAnimatingOverlay__AnimOverlay_SetSequence;
using C_BaseAnimatingOverlay_SetLayerModelIndex_t = void (*)(C_BaseAnimatingOverlay*, int, int);
C_BaseAnimatingOverlay_SetLayerModelIndex_t C_BaseAnimatingOverlay__SetLayerModelIndex;
using C_BaseAnimatingOverlay_AnimOverlay_SetOrder_t = void (*)(C_BaseAnimatingOverlay*, int, int);
C_BaseAnimatingOverlay_AnimOverlay_SetOrder_t C_BaseAnimatingOverlay__AnimOverlay_SetOrder;
using C_BaseAnimatingOverlay_AnimOverlay_SetWeight_t = void (*)(C_BaseAnimatingOverlay*, int, float);
C_BaseAnimatingOverlay_AnimOverlay_SetWeight_t C_BaseAnimatingOverlay__AnimOverlay_SetWeight;
using C_BaseAnimatingOverlay_AnimOverlay_SetPlaybackRate_t = void (*)(C_BaseAnimatingOverlay*, int, float);
C_BaseAnimatingOverlay_AnimOverlay_SetPlaybackRate_t C_BaseAnimatingOverlay__AnimOverlay_SetPlaybackRate;
using C_BaseAnimatingOverlay_AnimOverlay_SetAnimTime_t = void (*)(C_BaseAnimatingOverlay*, int, float);
C_BaseAnimatingOverlay_AnimOverlay_SetAnimTime_t C_BaseAnimatingOverlay__AnimOverlay_SetAnimTime;
using C_BaseAnimatingOverlay_AnimOverlay_SetFadeInDuration_t = void (*)(C_BaseAnimatingOverlay*, int, float);
C_BaseAnimatingOverlay_AnimOverlay_SetFadeInDuration_t C_BaseAnimatingOverlay__AnimOverlay_SetFadeInDuration;
using C_BaseAnimatingOverlay_AnimOverlay_SetFadeOutDuration_t = void (*)(C_BaseAnimatingOverlay*, int, float);
C_BaseAnimatingOverlay_AnimOverlay_SetFadeOutDuration_t C_BaseAnimatingOverlay__AnimOverlay_SetFadeOutDuration;
using C_BaseAnimatingOverlay_AnimOverlay_SetCycle_t = void (*)(C_BaseAnimatingOverlay*, int, float);
C_BaseAnimatingOverlay_AnimOverlay_SetCycle_t C_BaseAnimatingOverlay__AnimOverlay_SetCycle;

void C_BaseAnimatingOverlay::SetLayerActive(int layer, bool active)
{
    C_BaseAnimatingOverlay__SetLayerActive(this, layer, active);
}

bool C_BaseAnimatingOverlay::IsLayerActive(int layer) const
{
    return C_BaseAnimatingOverlay__IsLayerActive(const_cast<C_BaseAnimatingOverlay*>(this), layer);
}

void C_BaseAnimatingOverlay::SetLayerModelIndex(int layer, int modelIndex)
{
    C_BaseAnimatingOverlay__SetLayerModelIndex(this, layer, modelIndex);
}

void C_BaseAnimatingOverlay::AnimOverlay_SetSequence(int layer, int sequence)
{
    C_BaseAnimatingOverlay__AnimOverlay_SetSequence(this, layer, sequence);
}

void C_BaseAnimatingOverlay::AnimOverlay_SetOrder(int layer, int order)
{
    C_BaseAnimatingOverlay__AnimOverlay_SetOrder(this, layer, order);
}

void C_BaseAnimatingOverlay::AnimOverlay_SetWeight(int layer, float weight)
{
    C_BaseAnimatingOverlay__AnimOverlay_SetWeight(this, layer, weight);
}

void C_BaseAnimatingOverlay::AnimOverlay_SetPlaybackRate(int layer, float playbackRate)
{
    C_BaseAnimatingOverlay__AnimOverlay_SetPlaybackRate(this, layer, playbackRate);
}

void C_BaseAnimatingOverlay::AnimOverlay_SetAnimTime(int layer, float animTime)
{
    C_BaseAnimatingOverlay__AnimOverlay_SetAnimTime(this, layer, animTime);
}

void C_BaseAnimatingOverlay::AnimOverlay_SetFadeInDuration(int layer, float duration)
{
    C_BaseAnimatingOverlay__AnimOverlay_SetFadeInDuration(this, layer, duration);
}

void C_BaseAnimatingOverlay::AnimOverlay_SetFadeOutDuration(int layer, float duration)
{
    C_BaseAnimatingOverlay__AnimOverlay_SetFadeOutDuration(this, layer, duration);
}

void C_BaseAnimatingOverlay::AnimOverlay_SetCycle(int layer, float cycle)
{
    C_BaseAnimatingOverlay__AnimOverlay_SetCycle(this, layer, cycle);
}

int C_BaseAnimatingOverlay::InterpolateFieldsInternal(float currentTime, const SingleSnapshotValues* secondSnapshot,
                                                     float secondSnapshotTime)
{
    const int status = C_BaseAnimating::InterpolateFieldsInternal(currentTime, secondSnapshot, secondSnapshotTime);
	// lazy load here
	GetModelPtr();
    if (status && m_pStudioHdr && !IsClientOnly() && !GetPredictable())
    {
        const int layerLimit = IsPlayer() ? 8 : 2;
        const float fraction = GetInterpolationFraction(currentTime, secondSnapshotTime, secondSnapshot);
        for (int i = 0; i < layerLimit && i < m_AnimOverlayCount; ++i)
        {
            const AnimationOverlayData* first = m_lerpData.currentSnap->animationOverlayData;
            SetLayerActive(i, first->animOverlayIsActive[i]);
            if (!IsLayerActive(i))
                continue;

            const int firstSequence = IsValidSequence(0) ? m_lerpData.currentSnap->animationOverlayData->animOverlaySequence[i] : 0;
            AnimOverlay_SetSequence(i, firstSequence);
            SetLayerModelIndex(i, m_lerpData.currentSnap->animationOverlayData->animOverlayModelIndex[i]);
            AnimOverlay_SetOrder(i, m_lerpData.currentSnap->animationOverlayData->animOverlayOrder[i]);
            first = m_lerpData.currentSnap->animationOverlayData;
            const AnimationOverlayData* const second = secondSnapshot->animationOverlayData;
            const bool sameSequence = second->animOverlaySequence[i] == firstSequence;
            const auto interpolate = [fraction, sameSequence](float start, float end)
            { return sameSequence ? (end - start) * fraction + start : start; };
            const float weight = interpolate(first->animOverlayWeight[i], second->animOverlayWeight[i]);
            const float rate = interpolate(first->animOverlayPlaybackRate[i], second->animOverlayPlaybackRate[i]);
            const float animTime = interpolate(first->animOverlayAnimTime[i], second->animOverlayAnimTime[i]);
            const float fadeIn = interpolate(first->animOverlayFadeInDuration[i], second->animOverlayFadeInDuration[i]);
            const float fadeOut = interpolate(first->animOverlayFadeOutDuration[i], second->animOverlayFadeOutDuration[i]);
            AnimOverlay_SetWeight(i, weight);
            AnimOverlay_SetPlaybackRate(i, rate);
            AnimOverlay_SetAnimTime(i, animTime);
            AnimOverlay_SetFadeInDuration(i, fadeIn);
            AnimOverlay_SetFadeOutDuration(i, fadeOut);

            const int secondSequence = IsValidSequence(0) ? secondSnapshot->animationOverlayData->animOverlaySequence[i] : 0;
            const bool matchingSequence = m_lerpData.currentSnap->modelIndex == secondSnapshot->modelIndex && firstSequence == secondSequence;
            float firstTime = g_pClientGlobals->currentSnapTime;
            float secondTime = secondSnapshotTime;
            if (const C_Player* const owner = GetPredictable() ? GetPredictionOwner() : nullptr)
            {
                const CGlobalVarsBase* const globals = g_pClientGlobals;
                firstTime = globals->replayDelay + owner->m_lerpData.currentSnap->playerData->timeBase;
                const SingleSnapshotValues* const ownerSecond =
                    secondSnapshot == m_lerpData.futureSnap ? owner->m_lerpData.futureSnap : owner->m_lerpData.lastSnap;
                secondTime = globals->replayDelay + ownerSecond->playerData->timeBase;
            }
            first = m_lerpData.currentSnap->animationOverlayData;
            const bool firstFrozen = m_lerpData.currentSnap->animatingData->animFrozen;
            const float firstCycle =
                CalculateCycleForAnimationStateAtTime(this, firstTime, first->animOverlayStartCycle[i], first->animOverlayStartTime[i],
                                                        first->animOverlayPlaybackRate[i], firstSequence, firstFrozen, nullptr);
            const AnimationOverlayData* const cycleEnd = matchingSequence ? second : first;
            const float secondCycle =
                CalculateCycleForAnimationStateAtTime(this, secondTime, cycleEnd->animOverlayStartCycle[i], cycleEnd->animOverlayStartTime[i],
                                                        cycleEnd->animOverlayPlaybackRate[i], matchingSequence ? secondSequence : firstSequence,
                                                        matchingSequence ? secondSnapshot->animatingData->animFrozen : firstFrozen, nullptr);
            AnimOverlay_SetCycle(i, InterpolateCycle(firstCycle, secondCycle, fraction, firstTime, secondTime, firstSequence));
        }
    }
    unsigned int invalidateFlags = 0;
    for (int i = 0; i < m_AnimOverlayCount; ++i)
        invalidateFlags |= m_AnimOverlay[i].m_nInvalidatePhysicsBits;
    if (invalidateFlags)
        InvalidatePhysicsRecursive(invalidateFlags);
    return status;
}

DECLARE_HOOK(C_BaseAnimatingOverlay_InterpolateFieldsInternal, client.dll + 0x1033D0,
             [](auto&, C_BaseAnimatingOverlay* self, float currentTime, const SingleSnapshotValues* secondSnapshot,
                float secondSnapshotTime) -> int
{ return self->C_BaseAnimatingOverlay::InterpolateFieldsInternal(currentTime, secondSnapshot, secondSnapshotTime); })

ON_DLL_LOAD_CLIENT("client.dll", BaseAnimatingOverlayMethods, [](CModule module)
{
    C_BaseAnimatingOverlay__SetLayerActive = module.Offset(0xB01E0).RCast<decltype(C_BaseAnimatingOverlay__SetLayerActive)>();
    C_BaseAnimatingOverlay__IsLayerActive = module.Offset(0xAFFB0).RCast<decltype(C_BaseAnimatingOverlay__IsLayerActive)>();
    C_BaseAnimatingOverlay__AnimOverlay_SetSequence = module.Offset(0xB0430).RCast<decltype(C_BaseAnimatingOverlay__AnimOverlay_SetSequence)>();
    C_BaseAnimatingOverlay__SetLayerModelIndex = module.Offset(0xB0340).RCast<decltype(C_BaseAnimatingOverlay__SetLayerModelIndex)>();
    C_BaseAnimatingOverlay__AnimOverlay_SetOrder = module.Offset(0xB0390).RCast<decltype(C_BaseAnimatingOverlay__AnimOverlay_SetOrder)>();
    C_BaseAnimatingOverlay__AnimOverlay_SetWeight = module.Offset(0xB0450).RCast<decltype(C_BaseAnimatingOverlay__AnimOverlay_SetWeight)>();
    C_BaseAnimatingOverlay__AnimOverlay_SetPlaybackRate =
        module.Offset(0xB03E0).RCast<decltype(C_BaseAnimatingOverlay__AnimOverlay_SetPlaybackRate)>();
    C_BaseAnimatingOverlay__AnimOverlay_SetAnimTime = module.Offset(0xB0210).RCast<decltype(C_BaseAnimatingOverlay__AnimOverlay_SetAnimTime)>();
    C_BaseAnimatingOverlay__AnimOverlay_SetFadeInDuration =
        module.Offset(0xB02A0).RCast<decltype(C_BaseAnimatingOverlay__AnimOverlay_SetFadeInDuration)>();
    C_BaseAnimatingOverlay__AnimOverlay_SetFadeOutDuration =
        module.Offset(0xB02F0).RCast<decltype(C_BaseAnimatingOverlay__AnimOverlay_SetFadeOutDuration)>();
    C_BaseAnimatingOverlay__AnimOverlay_SetCycle = module.Offset(0xB0260).RCast<decltype(C_BaseAnimatingOverlay__AnimOverlay_SetCycle)>();
    DISPATCH_MODULE(ClientBaseAnimatingOverlay)
})
