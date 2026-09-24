//===========================================================================//
//
// Purpose: NVIDIA Reflex utilities
//
//===========================================================================//
#include "geforce/reflex.h"

#include "materialsystem/cmaterialsystem.h"

#include <cmath>
#include <limits>
#include <pclstats.h>

static std::atomic_bool s_LowLatencySDKEnabled = false;
static bool s_LowLatencyAvailable = false;

bool s_ReflexParametersDirty = true;
IUnknown* s_ReflexRequestedDevice = nullptr;
NV_SET_SLEEP_MODE_PARAMS s_ReflexRequestedParams = {};
ULONGLONG s_ReflexRetryAt = 0;

std::atomic<NvAPI_Status> s_ReflexModeUpdateStatus = NVAPI_OK;
std::atomic<NvAPI_Status> s_ReflexSleepStatus = NVAPI_OK;
std::atomic<NvAPI_Status> s_ReflexMarkerStatus = NVAPI_OK;

// True if the PCL stats system was initialized.
std::atomic_bool g_PCLStatsAvailable = false;

bool GeForce_CanUseLowLatencySDK()
{
    if (!s_LowLatencySDKEnabled.load(std::memory_order_relaxed))
        return false;

    IMaterialSystem* const materialSystem = MaterialSystem();
    return materialSystem && materialSystem->GetCurrentAdapterVendorID() == NVIDIA_VENDOR_ID;
}

void GeForce_ReportStatus(const char* const operation, const NvAPI_Status status, std::atomic<NvAPI_Status>& previousStatus)
{
    if (previousStatus.load(std::memory_order_relaxed) == status || previousStatus.exchange(status, std::memory_order_relaxed) == status)
        return;

    if (status == NVAPI_OK)
    {
        spdlog::info("NVIDIA Reflex {} recovered", operation);
        return;
    }

    NvAPI_ShortString error = {};
    NvAPI_GetErrorMessage(status, error);
    spdlog::warn("NVIDIA Reflex {} failed: {} ({})", operation, error, static_cast<int>(status));
}

//-----------------------------------------------------------------------------
// Purpose: enable/disable low latency SDK
// Input  : enable -
//-----------------------------------------------------------------------------
void GeForce_EnableLowLatencySDK(const bool enable)
{
    if (s_LowLatencySDKEnabled.exchange(enable, std::memory_order_relaxed) != enable)
        s_ReflexParametersDirty = true;
}

//-----------------------------------------------------------------------------
// Purpose: whether we should run the low latency SDK
//-----------------------------------------------------------------------------
bool GeForce_IsLowLatencySDKAvailable()
{
    return GeForce_CanUseLowLatencySDK() && s_LowLatencyAvailable;
}

//-----------------------------------------------------------------------------
// Purpose: reset configuration; capability is established by SetSleepMode
//-----------------------------------------------------------------------------
void GeForce_InitLowLatencySDK()
{
    s_LowLatencyAvailable = false;
    s_ReflexParametersDirty = true;
    s_ReflexRequestedDevice = nullptr;
    s_ReflexRetryAt = 0;
    s_ReflexModeUpdateStatus.store(NVAPI_OK, std::memory_order_relaxed);
    s_ReflexSleepStatus.store(NVAPI_OK, std::memory_order_relaxed);
    s_ReflexMarkerStatus.store(NVAPI_OK, std::memory_order_relaxed);
}

//-----------------------------------------------------------------------------
// Purpose: shutdown the low latency SDK
//-----------------------------------------------------------------------------
void GeForce_ShutdownLowLatencySDK()
{
    GeForce_EnableLowLatencySDK(false);
    GeForce_InitLowLatencySDK();
}

//-----------------------------------------------------------------------------
// Purpose: mark the parameters as out-of-date; force update next frame
//-----------------------------------------------------------------------------
void GeForce_MarkLowLatencyParametersOutOfDate()
{
    s_ReflexParametersDirty = true;
}

//-----------------------------------------------------------------------------
// Purpose: updates the low latency parameters
// Input  : *device              -
//          useLowLatencyMode    -
//          useLowLatencyBoost   -
//          useMarkersToOptimize -
//          maxFramesPerSecond   -
//-----------------------------------------------------------------------------
void GeForce_UpdateLowLatencyParameters(IUnknown* const device, const bool useLowLatencyMode, const bool useLowLatencyBoost,
                                        const bool useMarkersToOptimize, const float maxFramesPerSecond)
{
    if (!device || !GeForce_CanUseLowLatencySDK() || !std::isfinite(maxFramesPerSecond))
        return;

    NvU32 minimumIntervalUs = 0;
    if (maxFramesPerSecond > 0)
    {
        // Keep the existing rounding for normal caps, but saturate before converting tiny FPS values.
        const float intervalUs = (1000.0f / maxFramesPerSecond) * 1000.0f;
        constexpr NvU32 maxIntervalUs = std::numeric_limits<NvU32>::max();
        minimumIntervalUs = static_cast<double>(intervalUs) >= static_cast<double>(maxIntervalUs) ? maxIntervalUs : static_cast<NvU32>(intervalUs);
    }

    const bool lowLatencyBoost = useLowLatencyMode && useLowLatencyBoost;
    const bool changed = s_ReflexParametersDirty || device != s_ReflexRequestedDevice ||
                         s_ReflexRequestedParams.bLowLatencyMode != static_cast<NvBool>(useLowLatencyMode) ||
                         s_ReflexRequestedParams.bLowLatencyBoost != static_cast<NvBool>(lowLatencyBoost) ||
                         s_ReflexRequestedParams.minimumIntervalUs != minimumIntervalUs ||
                         s_ReflexRequestedParams.bUseMarkersToOptimize != static_cast<NvBool>(useMarkersToOptimize);

    if (!changed)
    {
        const NvAPI_Status status = s_ReflexModeUpdateStatus.load(std::memory_order_relaxed);
        if (s_LowLatencyAvailable || status == NVAPI_NO_IMPLEMENTATION || status == NVAPI_NOT_SUPPORTED || GetTickCount64() < s_ReflexRetryAt)
            return;
    }

    NV_SET_SLEEP_MODE_PARAMS params = {};
    params.version = NV_SET_SLEEP_MODE_PARAMS_VER1;
    params.bLowLatencyMode = useLowLatencyMode;
    params.bLowLatencyBoost = lowLatencyBoost;
    params.minimumIntervalUs = minimumIntervalUs;
    params.bUseMarkersToOptimize = useMarkersToOptimize;

    s_ReflexRequestedDevice = device;
    s_ReflexRequestedParams = params;
    s_ReflexParametersDirty = false;

    const NvAPI_Status status = NvAPI_D3D_SetSleepMode(device, &params);
    s_LowLatencyAvailable = status == NVAPI_OK;
    s_ReflexRetryAt = s_LowLatencyAvailable ? 0 : GetTickCount64() + 1000;
    GeForce_ReportStatus("SetSleepMode", status, s_ReflexModeUpdateStatus);
}

//-----------------------------------------------------------------------------
// Purpose: runs a frame of the low latency sdk
// Input  : *device -
//-----------------------------------------------------------------------------
void GeForce_RunLowLatencyFrame(IUnknown* const device)
{
    if (device && GeForce_CanUseLowLatencySDK())
        GeForce_ReportStatus("Sleep", NvAPI_D3D_Sleep(device), s_ReflexSleepStatus);
}

//-----------------------------------------------------------------------------
// Purpose: sets the latency marker
// Input  : *device    -
//          markerType -
//          frameID    -
//-----------------------------------------------------------------------------
void GeForce_SetLatencyMarker(IUnknown* const device, const NV_LATENCY_MARKER_TYPE markerType, const NvU64 frameID)
{
    if (device && GeForce_CanUseLowLatencySDK())
    {
        NV_LATENCY_MARKER_PARAMS params = {};
        params.version = NV_LATENCY_MARKER_PARAMS_VER1;
        params.frameID = frameID;
        params.markerType = markerType;

        GeForce_ReportStatus("SetLatencyMarker", NvAPI_D3D_SetLatencyMarker(device, &params), s_ReflexMarkerStatus);
    }

    if (g_PCLStatsAvailable.load(std::memory_order_acquire))
    {
        // PCLStats runs separately and is supported on non-NVIDIA hardware.
        PCLSTATS_MARKER(markerType, frameID);
    }
}
