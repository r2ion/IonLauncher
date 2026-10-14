#pragma once

class C_BaseAnimatingOverlay;

struct C_AnimationLayer
{
    void* __vftable;
    C_BaseAnimatingOverlay* m_animationLayerOwner;
    int m_layerIndex;
    unsigned int m_nInvalidatePhysicsBits;
};

static_assert(sizeof(C_AnimationLayer) == 0x18);
