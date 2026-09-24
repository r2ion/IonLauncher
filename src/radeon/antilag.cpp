//===========================================================================//
//
// Purpose: AMD Anti-Lag 2 utilities
//
//===========================================================================//
#include "radeon/antilag.h"

#include "materialsystem/cmaterialsystem.h"

#include <d3d11.h>
#include <ffx_antilag2_dx11.h>

bool b_LowLatencySDKEnabled = false;

// This will be true if the call to 'AMD::AntiLag2DX11::Initialize' succeeds.
bool b_LowLatencyAvailable = false;

AMD::AntiLag2DX11::Context s_LowLatencyContext = {};

//-----------------------------------------------------------------------------
// Purpose: enable/disable low latency SDK
// Input  : enable -
//-----------------------------------------------------------------------------
void Radeon_EnableLowLatencySDK(const bool enable)
{
    b_LowLatencySDKEnabled = enable;
}

//-----------------------------------------------------------------------------
// Purpose: whether we should run the low latency SDK
//-----------------------------------------------------------------------------
bool Radeon_IsLowLatencySDKAvailable()
{
    // NOTE: don't check on b_LowLatencySDKEnabled here as this needs to be
    // provided to the driver itself.
    if (!b_LowLatencyAvailable)
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
    if (AMD::AntiLag2DX11::Initialize(&s_LowLatencyContext) == S_OK)
        b_LowLatencyAvailable = true;

    return b_LowLatencyAvailable;
}

//-----------------------------------------------------------------------------
// Purpose: shutdown the low latency SDK
//-----------------------------------------------------------------------------
void Radeon_ShutdownLowLatencySDK()
{
    AMD::AntiLag2DX11::DeInitialize(&s_LowLatencyContext);
    b_LowLatencyAvailable = false;
}

//-----------------------------------------------------------------------------
// Purpose: runs a frame of the low latency sdk
// Input  : maxFPS -
//-----------------------------------------------------------------------------
void Radeon_RunLowLatencyFrame(const unsigned int maxFPS)
{
    if (Radeon_IsLowLatencySDKAvailable())
        AMD::AntiLag2DX11::Update(&s_LowLatencyContext, b_LowLatencySDKEnabled, maxFPS);
}
