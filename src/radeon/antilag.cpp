//===========================================================================//
//
// Purpose: AMD Anti-Lag 2 utilities
//
//===========================================================================//
#include "radeon/antilag.h"

#include "materialsystem/cmaterialsystem.h"

#include <d3d11.h>
#include <ffx_antilag2_dx11.h>

static bool s_LowLatencySDKEnabled = false;

// This will be true if the call to 'AMD::AntiLag2DX11::Initialize' succeeds.
static bool s_LowLatencyAvailable = false;

AMD::AntiLag2DX11::Context s_LowLatencyContext = {};
HRESULT s_LowLatencyUpdateStatus = S_OK;

//-----------------------------------------------------------------------------
// Purpose: enable/disable low latency SDK
// Input  : enable -
//-----------------------------------------------------------------------------
void Radeon_EnableLowLatencySDK(const bool enable)
{
    s_LowLatencySDKEnabled = enable;
}

//-----------------------------------------------------------------------------
// Purpose: whether we should run the low latency SDK
//-----------------------------------------------------------------------------
bool Radeon_IsLowLatencySDKAvailable()
{
    // NOTE: don't check on s_LowLatencySDKEnabled here as this needs to be
    // provided to the driver itself.
    if (!s_LowLatencyAvailable)
        return false;

    IMaterialSystem* const materialSystem = MaterialSystem();
    // Only run on AMD display drivers; NVIDIA and Intel are not
    // supported by AMD Anti-Lag 2.
    return materialSystem && materialSystem->GetCurrentAdapterVendorID() == AMD_VENDOR_ID;
}

//-----------------------------------------------------------------------------
// Purpose: initialize the low latency SDK
//-----------------------------------------------------------------------------
bool Radeon_InitLowLatencySDK()
{
    Radeon_ShutdownLowLatencySDK();

    const HRESULT result = AMD::AntiLag2DX11::Initialize(&s_LowLatencyContext);
    s_LowLatencyAvailable = result == S_OK;
    if (s_LowLatencyAvailable)
        spdlog::info("AMD Anti-Lag 2 initialized (HRESULT 0x{:08X})", static_cast<unsigned long>(result));
    else
        spdlog::warn("AMD Anti-Lag 2 initialization failed (HRESULT 0x{:08X})", static_cast<unsigned long>(result));

    return s_LowLatencyAvailable;
}

//-----------------------------------------------------------------------------
// Purpose: shutdown the low latency SDK
//-----------------------------------------------------------------------------
void Radeon_ShutdownLowLatencySDK()
{
    s_LowLatencyAvailable = false;
    AMD::AntiLag2DX11::DeInitialize(&s_LowLatencyContext);
    s_LowLatencyContext = {};
    s_LowLatencyUpdateStatus = S_OK;
}

//-----------------------------------------------------------------------------
// Purpose: runs a frame of the low latency sdk
// Input  : maxFPS -
//-----------------------------------------------------------------------------
void Radeon_RunLowLatencyFrame(const unsigned int maxFPS)
{
    if (!Radeon_IsLowLatencySDKAvailable())
        return;

    const HRESULT result = AMD::AntiLag2DX11::Update(&s_LowLatencyContext, s_LowLatencySDKEnabled, maxFPS);
    if (result == s_LowLatencyUpdateStatus)
        return;

    s_LowLatencyUpdateStatus = result;
    if (SUCCEEDED(result))
        spdlog::info("AMD Anti-Lag 2 Update recovered (HRESULT 0x{:08X})", static_cast<unsigned long>(result));
    else
        spdlog::warn("AMD Anti-Lag 2 Update failed (HRESULT 0x{:08X})", static_cast<unsigned long>(result));
}
