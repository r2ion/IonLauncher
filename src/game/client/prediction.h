#pragma once

#include "iprediction.h"
#include <cstddef>

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

	void MarkScriptPredictionError() { m_nPreviousAckHadErrors = 1; }
    std::byte m_Reserved0008[0xD4];
    int m_nPreviousAckHadErrors;
};

static_assert(offsetof(CPrediction, m_nPreviousAckHadErrors) == 0xDC);

using CPrediction_InPrediction_t = bool (*)(const CPrediction*);

extern CPrediction* g_pClientSidePrediction;
