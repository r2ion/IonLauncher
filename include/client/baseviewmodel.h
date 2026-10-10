#pragma once

#include "client/baseanimatingoverlay.h"

class C_BaseViewModel : public C_BaseAnimatingOverlay
{
  public:
    void UpdateRenderPresentation(float cameraTime);
    void RestoreRenderPresentation();
};
