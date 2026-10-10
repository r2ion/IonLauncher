#pragma once

#include "client/iprediction.h"

class C_BaseEntity;
class C_Player;

class CPrediction : public IPrediction
{
  public:
    bool InPrediction() const;

    bool CanInterpolateRenderState() const;
    void OnFrameStart();
    void InterpolateRenderState();
    void RestoreRenderState(C_BaseEntity* target = nullptr);
    bool GetRenderCameraTime(const C_Player* player, float& time) const;
    void CaptureCommittedPrediction(int commandNumber);
};

using CPrediction_InPrediction_t = bool (*)(const CPrediction*);

extern CPrediction* g_pClientSidePrediction;
