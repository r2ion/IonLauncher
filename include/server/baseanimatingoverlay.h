#pragma once

#include "server/baseanimating.h"

class CBaseAnimatingOverlay : public CBaseAnimating
{
  public:
    ServerClass* GetServerClass() override = 0; // 3
    ServerDataMap* GetDataDescMap() override = 0; // 5
    void Spawn() override = 0; // 23
    void OnRestore() override = 0; // 40
    const CBaseAnimatingOverlay* GetBaseAnimatingOverlayConst() const override = 0; // 50
    CBaseAnimatingOverlay* GetBaseAnimatingOverlay() override = 0; // 51
    void StudioFrameAdvance() override = 0; // 247
    void DispatchAnimEvents(CBaseAnimating* eventHandler) override = 0; // 257

  protected:
    void AccumulateLayers(IBoneSetup* boneSetup, Vector3D* positions, Quaternion* rotations, Vector3D* scales, float currentTime) override = 0; // 261

    std::byte m_Reserved0EB8[0x188];
};

static_assert(sizeof(CBaseAnimatingOverlay) == 0x1040);
