//===========================================================================//
//
// Purpose: NVIDIA Reflex utilities
//
//===========================================================================//
#include "geforce/reflex.h"

#include "materialsystem/cmaterialsystem.h"

#include <cmath>
#include <pclstats.h>

static bool b_LowLatencySDKEnabled = false;
static bool b_LowLatencyAvailable = false;

// If false, the system will call 'NvAPI_D3D_SetSleepMode' to update the parameters.
bool b_ReflexModeInfoUpToDate = false;

// This is 'NVAPI_OK' if the call to 'NvAPI_D3D_SetSleepMode' was successful.
// If not, the Low Latency SDK will not run.
NvAPI_Status s_ReflexModeUpdateStatus = NvAPI_Status::NVAPI_OK;

// True if the PCL stats system was initialized.
bool g_PCLStatsAvailable = false;

//-----------------------------------------------------------------------------
// Purpose: enable/disable low latency SDK
// Input  : enable -
//-----------------------------------------------------------------------------
void GeForce_EnableLowLatencySDK(const bool enable)
{
    b_LowLatencySDKEnabled = enable;
}

//-----------------------------------------------------------------------------
// Purpose: whether we should run the low latency SDK
//-----------------------------------------------------------------------------
bool GeForce_IsLowLatencySDKAvailable()
{
    if (!b_LowLatencySDKEnabled || !b_LowLatencyAvailable)
        return false;

    IMaterialSystem* const materialSystem = MaterialSystem();
    // Only run on NVIDIA display drivers; AMD and Intel are not
    // supported by NVIDIA Reflex.
    return materialSystem && materialSystem->GetCurrentAdapterVendorID() == NVIDIA_VENDOR_ID;
}

//-----------------------------------------------------------------------------
// Purpose: initialize the low latency SDK
//-----------------------------------------------------------------------------
bool GeForce_InitLowLatencySDK()
{
    b_LowLatencyAvailable = true;
    b_ReflexModeInfoUpToDate = false;
    s_ReflexModeUpdateStatus = NvAPI_Status::NVAPI_OK;
    return b_LowLatencyAvailable;
}

//-----------------------------------------------------------------------------
// Purpose: shutdown the low latency SDK
//-----------------------------------------------------------------------------
void GeForce_ShutdownLowLatencySDK()
{
    b_LowLatencyAvailable = false;
}

//-----------------------------------------------------------------------------
// Purpose: mark the parameters as out-of-date; force update next frame
//-----------------------------------------------------------------------------
void GeForce_MarkLowLatencyParametersOutOfDate()
{
    b_ReflexModeInfoUpToDate = false;
}

//-----------------------------------------------------------------------------
// Purpose: mark the parameters as up-to-date
//-----------------------------------------------------------------------------
static void GeForce_MarkLowLatencyParametersUpToDate()
{
    b_ReflexModeInfoUpToDate = true;
}

//-----------------------------------------------------------------------------
// Purpose: has the user requested any changes to the low latency parameters?
//-----------------------------------------------------------------------------
bool GeForce_HasPendingLowLatencyParameterUpdates()
{
    return !b_ReflexModeInfoUpToDate;
}

//-----------------------------------------------------------------------------
// Purpose: returns whether the call to 'NvAPI_D3D_SetSleepMode' was successful
//-----------------------------------------------------------------------------
static bool GeForce_ParameterUpdateWasSuccessful()
{
    return s_ReflexModeUpdateStatus == NvAPI_Status::NVAPI_OK;
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
    if (!device || !std::isfinite(maxFramesPerSecond))
        return;

    NV_SET_SLEEP_MODE_PARAMS params = {};
    params.version = NV_SET_SLEEP_MODE_PARAMS_VER1;

    params.bLowLatencyMode = useLowLatencyMode;
    params.bLowLatencyBoost = useLowLatencyMode && useLowLatencyBoost;
    params.minimumIntervalUs = maxFramesPerSecond > 0 ? static_cast<NvU32>((1000.0f / maxFramesPerSecond) * 1000.0f) : 0;
    params.bUseMarkersToOptimize = useMarkersToOptimize;

    s_ReflexModeUpdateStatus = NvAPI_D3D_SetSleepMode(device, &params);
    GeForce_MarkLowLatencyParametersUpToDate();
}

//-----------------------------------------------------------------------------
// Purpose: runs a frame of the low latency sdk
// Input  : *device -
//-----------------------------------------------------------------------------
void GeForce_RunLowLatencyFrame(IUnknown* const device)
{
    if (device && GeForce_ParameterUpdateWasSuccessful())
        NvAPI_D3D_Sleep(device);
}

//-----------------------------------------------------------------------------
// Purpose: sets the latency marker
// Input  : *device    -
//          markerType -
//          frameID    -
//-----------------------------------------------------------------------------
void GeForce_SetLatencyMarker(IUnknown* const device, const NV_LATENCY_MARKER_TYPE markerType, const NvU64 frameID)
{
    if (device && GeForce_ParameterUpdateWasSuccessful() && GeForce_IsLowLatencySDKAvailable())
    {
        NV_LATENCY_MARKER_PARAMS params = {};
        params.version = NV_LATENCY_MARKER_PARAMS_VER1;
        params.frameID = frameID;
        params.markerType = markerType;

        NvAPI_D3D_SetLatencyMarker(device, &params);
    }

    if (g_PCLStatsAvailable)
    {
        // PCLStats runs separately and is supported on non-NVIDIA hardware.
        PCLSTATS_MARKER(markerType, frameID);
    }
}
