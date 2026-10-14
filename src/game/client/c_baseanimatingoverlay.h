#pragma once

#include "game/client/c_baseanimating.h"
#include "game/client/c_animation_layer.h"

class C_BaseAnimatingOverlay : public C_BaseAnimating
{
  public:
    int InterpolateFieldsInternal(float currentTime, const SingleSnapshotValues* secondSnapshot, float secondSnapshotTime) override; // 110
    void SetLayerActive(int layer, bool active);
    bool IsLayerActive(int layer) const;
    void SetLayerModelIndex(int layer, int modelIndex);
    void AnimOverlay_SetSequence(int layer, int sequence);
    void AnimOverlay_SetOrder(int layer, int order);
    void AnimOverlay_SetWeight(int layer, float weight);
    void AnimOverlay_SetPlaybackRate(int layer, float playbackRate);
    void AnimOverlay_SetAnimTime(int layer, float animTime);
    void AnimOverlay_SetFadeInDuration(int layer, float duration);
    void AnimOverlay_SetFadeOutDuration(int layer, float duration);
    void AnimOverlay_SetCycle(int layer, float cycle);

    int m_AnimOverlayCount;
    C_AnimationLayer m_AnimOverlay[8];
    std::byte m_Padding12F8[0x24];
    AnimationOverlayData m_currentFrameAnimationOverlay;
};

static_assert(sizeof(AnimationOverlayData) == 0x170);
