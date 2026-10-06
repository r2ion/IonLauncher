#pragma once

#include "client/baseanimating.h"

struct C_AnimationLayer
{
    void* __vftable;
    C_BaseAnimatingOverlay* m_animationLayerOwner;
    int m_layerIndex;
    unsigned int m_nInvalidatePhysicsBits;
};

class C_BaseAnimatingOverlay : public C_BaseAnimating
{
  public:
    int m_AnimOverlayCount;
    C_AnimationLayer m_AnimOverlay[8];
    std::byte m_Padding12F8[0x24];
    AnimationOverlayData m_currentFrameAnimationOverlay;
};

static_assert(sizeof(C_AnimationLayer) == 0x18);
static_assert(sizeof(AnimationOverlayData) == 0x170);
