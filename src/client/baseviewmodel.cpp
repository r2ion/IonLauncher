#include "client/baseviewmodel.h"
#include "client/animation.h"
#include "engine/client/clientstate.h"

#include <algorithm>
#include <cstring>


void C_BaseViewModel::RestoreRenderPresentation()
{
    auto& viewModel = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState.viewModel;
    if (!viewModel.active || viewModel.entity.Get() != this)
        return;
    m_currentFrame.animCycle = viewModel.cycle;
    m_currentFrameBaseAnimating.animPlaybackRate = viewModel.playbackRate;
    std::memcpy(m_currentFrameAnimationOverlay.animOverlayCycle, viewModel.overlayCycles, sizeof(viewModel.overlayCycles));
    InvalidatePhysicsRecursive(8);
    viewModel.active = false;
}

void C_BaseViewModel::UpdateRenderPresentation(float cameraTime)
{
    auto& viewModel = GetBaseLocalClient()->GetClientStateExtended()->m_RenderState.viewModel;
    if (viewModel.active || !GetPredictable() || m_bClientSideAnimation || m_animActive || !GetModelPtr() ||
        !IsValidSequence(GetSequence()))
        return;

    const AnimatingData& animation = m_currentFrameBaseAnimating;
    AnimationOverlayData& layers = m_currentFrameAnimationOverlay;
    viewModel.entity = this;
    viewModel.cycle = GetCycle();
    viewModel.playbackRate = GetPlaybackRate();
    std::memcpy(viewModel.overlayCycles, layers.animOverlayCycle, sizeof(viewModel.overlayCycles));
    viewModel.active = true;

    SetCycle(CalculateCycleForAnimationStateAtTime(this, cameraTime, animation.animStartCycle,
                                                animation.animStartTime, animation.animPlaybackRate, animation.animSequence,
                                                animation.animFrozen, nullptr));
    for (int i = 0; i < std::min(m_AnimOverlayCount, 8); ++i)
    {
        if (!layers.animOverlayIsActive[i] || !IsValidSequence(layers.animOverlaySequence[i]))
            continue;
        AnimOverlay_SetCycle(
            i, CalculateCycleForAnimationStateAtTime(this, cameraTime, layers.animOverlayStartCycle[i],
                                                    layers.animOverlayStartTime[i], layers.animOverlayPlaybackRate[i],
                                                    layers.animOverlaySequence[i], animation.animFrozen, nullptr));
    }
    PredictionFrame_PreAnim();
}
