#ifndef GFSDK_REFLEX_H
#define GFSDK_REFLEX_H

#include <atomic>
#include <nvapi.h>
#include <unknwn.h>

extern std::atomic_bool g_PCLStatsAvailable;

void GeForce_EnableLowLatencySDK(bool enable);
bool GeForce_IsLowLatencySDKAvailable();

void GeForce_InitLowLatencySDK();
void GeForce_ShutdownLowLatencySDK();

void GeForce_MarkLowLatencyParametersOutOfDate();

void GeForce_UpdateLowLatencyParameters(IUnknown* device, bool useLowLatencyMode, bool useLowLatencyBoost, bool useMarkersToOptimize,
                                        float maxFramesPerSecond);

void GeForce_RunLowLatencyFrame(IUnknown* device);

void GeForce_SetLatencyMarker(IUnknown* device, NV_LATENCY_MARKER_TYPE markerType, NvU64 frameID);

#endif // GFSDK_REFLEX_H
