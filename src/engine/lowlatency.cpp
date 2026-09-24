#include "cdll_int.h"
#include "core/tier0.h"
#include "core/tier1.h"
#include "geforce/reflex.h"
#include "materialsystem/cmaterialsystem.h"
#include "radeon/antilag.h"
#include "tier1/cvar.h"
#include "windows/id3dx.h"

#include <atomic>
#include <dxgi.h>
#include <memory>
#include <pclstats.h>

PCLSTATS_DEFINE()

ConVar* Cvar_fps_max = nullptr;
ConVar* Cvar_fps_max_low_latency = nullptr;
ConVar* Cvar_gfx_nvnUseLowLatency = nullptr;
ConVar* Cvar_gfx_nvnUseLowLatencyBoost = nullptr;
ConVar* Cvar_gfx_nvnUseMarkersToOptimize = nullptr;
ConVar* Cvar_gfx_ffxUseLowLatency = nullptr;

bool b_UseLowLatency = false;
bool b_MaterialSystemInitialized = false;
bool b_PresentHookInstalled = false;
std::atomic_bool b_PresentMarkersEnabled = false;

DECLARE_MODULE(LowLatencyHooks)

NvU64 LowLatency_GetCurrentFrameCount()
{
    IMaterialSystem* const materialSystem = MaterialSystem();
    return materialSystem ? static_cast<NvU64>(materialSystem->GetCurrentFrameCount()) : 0;
}

NvU64 LowLatency_GetSubmittedFrameID()
{
    const NvU64 currentFrame = LowLatency_GetCurrentFrameCount();
    return currentFrame > 0 ? currentFrame - 1 : 0;
}

float NormalizeFrameRate(const float fpsMax)
{
    if (fpsMax != -1.0f)
        return fpsMax;

    IMaterialSystem* const materialSystem = MaterialSystem();
    if (!Cvar_fps_max || Cvar_fps_max->GetFloat() != 0.0f || !materialSystem)
        return 0.0f;

    const MaterialVideoMode_t* const videoMode = materialSystem->GetCurrentVideoMode();
    return videoMode && videoMode->refreshRate > 0 ? static_cast<float>(videoMode->refreshRate) : 0.0f;
}

void LowLatency_UpdateParameters()
{
    ID3D11Device* const device = D3D11Device();
    if (!device || !Cvar_fps_max_low_latency || !Cvar_gfx_nvnUseLowLatency || !Cvar_gfx_nvnUseLowLatencyBoost || !Cvar_gfx_nvnUseMarkersToOptimize)
        return;

    const float fpsMax = NormalizeFrameRate(Cvar_fps_max_low_latency->GetFloat());
    GeForce_UpdateLowLatencyParameters(device, Cvar_gfx_nvnUseLowLatency->GetBool(), Cvar_gfx_nvnUseLowLatencyBoost->GetBool(),
                                       Cvar_gfx_nvnUseMarkersToOptimize->GetBool(), fpsMax);
}

void LowLatency_RunFrame()
{
    if (!b_UseLowLatency || !b_MaterialSystemInitialized || !Cvar_fps_max_low_latency)
        return;

    if (GeForce_IsLowLatencySDKAvailable())
    {
        if (GeForce_HasPendingLowLatencyParameterUpdates())
            LowLatency_UpdateParameters();

        GeForce_RunLowLatencyFrame(D3D11Device());
    }

    if (Radeon_IsLowLatencySDKAvailable())
    {
        const float maxFps = NormalizeFrameRate(Cvar_fps_max_low_latency->GetFloat());
        Radeon_RunLowLatencyFrame(static_cast<unsigned int>(maxFps));
    }
}

void GFX_NVN_Changed_f(IConVar*, const char*, float, ChangeUserData_t)
{
    GeForce_MarkLowLatencyParametersOutOfDate();
}

void GFX_FFX_Changed_f(IConVar* const conVar, const char*, float, ChangeUserData_t)
{
    const ConVar* const conVarRef = g_pCVar->FindVar(conVar->GetName());
    Radeon_EnableLowLatencySDK(b_UseLowLatency && conVarRef && conVarRef->GetBool());
}

DECLARE_HOOK_ABSOLUTE(IDXGISwapChain__Present, 0,
                      ([](auto& hook, IDXGISwapChain* const swapChain, const UINT syncInterval, const UINT flags) -> HRESULT
{
    if (!b_PresentMarkersEnabled.load(std::memory_order_relaxed) || swapChain != DXGISwapChain() || (flags & DXGI_PRESENT_TEST))
        return hook.Original(swapChain, syncInterval, flags);

    const NvU64 frameID = LowLatency_GetSubmittedFrameID();
    GeForce_SetLatencyMarker(D3D11Device(), RENDERSUBMIT_END, frameID);
    GeForce_SetLatencyMarker(D3D11Device(), PRESENT_START, frameID);
    const HRESULT result = hook.Original(swapChain, syncInterval, flags);
    GeForce_SetLatencyMarker(D3D11Device(), PRESENT_END, frameID);
    return result;
}))

static void LowLatency_InstallPresentHook()
{
    if (b_PresentHookInstalled)
    {
        b_PresentMarkersEnabled.store(true, std::memory_order_relaxed);
        return;
    }

    IDXGISwapChain* const swapChain = DXGISwapChain();
    if (!swapChain)
        return;

    void** const vtable = *reinterpret_cast<void***>(swapChain);
    if (!vtable || !vtable[8])
        return;

    const std::shared_ptr<HookSys::LambdaHookBase> presentHook = LowLatencyHooks.FindHook("IDXGISwapChain__Present");
    if (!presentHook)
    {
        spdlog::error("Unable to find the IDXGISwapChain::Present low-latency hook");
        return;
    }

    presentHook->ConfigureAbsoluteAddress(reinterpret_cast<uintptr_t>(vtable[8]));
    b_PresentHookInstalled = presentHook->Dispatch();
    b_PresentMarkersEnabled.store(b_PresentHookInstalled, std::memory_order_relaxed);
}

DECLARE_HOOK(CEngine__Frame, engine.dll + 0x1C8650,
             ([](auto& hook, void* const self) -> bool
{
    const bool result = hook.Original(self);
    if (result)
        LowLatency_RunFrame();
    return result;
}))

DECLARE_HOOK(Host_CountRealTimePackets, engine.dll + 0x109710,
             ([](auto& hook)
{
    hook.Original();
    GeForce_SetLatencyMarker(D3D11Device(), SIMULATION_START, LowLatency_GetCurrentFrameCount());
}))

DECLARE_HOOK(CClient_FrameStageNotify, client.dll + 0x1900A0,
             ([](auto& hook, void* const self, const ClientFrameStage_t stage)
{
    hook.Original(self, stage);
    if (stage == FRAME_RENDER_END)
        GeForce_SetLatencyMarker(D3D11Device(), SIMULATION_END, LowLatency_GetSubmittedFrameID());
}))

DECLARE_HOOK(CMaterialSystem__Init, materialsystem_dx11.dll + 0x61000,
             ([](auto& hook, void* const self) -> InitReturnVal_t
{
    b_UseLowLatency = !CommandLine()->CheckParm("-gfx_disableLowLatency");
    GeForce_EnableLowLatencySDK(b_UseLowLatency);
    Radeon_EnableLowLatencySDK(b_UseLowLatency && Cvar_gfx_ffxUseLowLatency && Cvar_gfx_ffxUseLowLatency->GetBool());

    if (b_UseLowLatency)
    {
        GeForce_InitLowLatencySDK();
        Radeon_InitLowLatencySDK();

        PCLSTATS_INIT(0);
        g_PCLStatsAvailable = true;
    }

    const InitReturnVal_t result = hook.Original(self);
    b_MaterialSystemInitialized = result == INIT_OK;

    if (b_UseLowLatency && b_MaterialSystemInitialized)
    {
        spdlog::info("Low-latency SDKs initialized (NVIDIA Reflex: {}, AMD Anti-Lag 2: {}, PCLStats: available)",
                     GeForce_IsLowLatencySDKAvailable() ? "available" : "unavailable",
                     Radeon_IsLowLatencySDKAvailable() ? "available" : "unavailable");
    }
    else if (!b_UseLowLatency)
    {
        spdlog::info("Low-latency SDKs disabled by -gfx_disableLowLatency");
    }

    return result;
}))

DECLARE_HOOK(CMaterialSystem__Shutdown, materialsystem_dx11.dll + 0x6A630,
             ([](auto& hook, void* const self) -> int
{
    b_PresentMarkersEnabled.store(false, std::memory_order_relaxed);
    b_MaterialSystemInitialized = false;

    if (b_UseLowLatency)
    {
        if (g_PCLStatsAvailable)
        {
            PCLSTATS_SHUTDOWN();
            g_PCLStatsAvailable = false;
        }

        Radeon_ShutdownLowLatencySDK();
        GeForce_ShutdownLowLatencySDK();
    }

    return hook.Original(self);
}))

DECLARE_HOOK(DoF_UpdateMainViewParams, materialsystem_dx11.dll + 0x38CE0,
             ([](auto& hook, const float scalar)
{
    if (b_UseLowLatency)
        GeForce_SetLatencyMarker(D3D11Device(), RENDERSUBMIT_START, LowLatency_GetCurrentFrameCount());

    hook.Original(scalar);
}))

DECLARE_HOOK(CMaterialSystem__BeginFrame, materialsystem_dx11.dll + 0x5A200,
             ([](auto& hook, void* const self, const float frameTime)
{
    if (b_UseLowLatency)
        LowLatency_InstallPresentHook();

    hook.Original(self, frameTime);
}))

DECLARE_HOOK(CInputSystem__WindowProc, inputsystem.dll + 0x8B80,
             ([](auto& hook, void* const unused, HWND const window, const UINT message, const WPARAM wParam, const LPARAM lParam) -> LRESULT
{
    if (g_PCLStatsAvailable && PCLSTATS_IS_PING_MSG_ID(message))
        GeForce_SetLatencyMarker(D3D11Device(), PC_LATENCY_PING, LowLatency_GetCurrentFrameCount());

    return hook.Original(unused, window, message, wParam, lParam);
}))

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", LowLatencyEngine, ConVar, [](CModule module)
{
    NOTE_UNUSED(module);

    Cvar_fps_max = g_pCVar->FindVar("fps_max");
    Cvar_fps_max_low_latency = new ConVar("fps_max_low_latency", "0", FCVAR_RELEASE,
                                          "Frame rate limiter using Low Latency SDK. -1 indicates the use of desktop refresh. 0 is disabled.", true,
                                          -1.0f, true, 295.0f, GFX_NVN_Changed_f);
    Cvar_gfx_nvnUseLowLatency = new ConVar("gfx_nvnUseLowLatency", "1", FCVAR_RELEASE | FCVAR_ARCHIVE, "Enables NVIDIA Reflex Low Latency SDK.",
                                           false, 0.0f, false, 0.0f, GFX_NVN_Changed_f);
    Cvar_gfx_nvnUseLowLatencyBoost = new ConVar("gfx_nvnUseLowLatencyBoost", "0", FCVAR_RELEASE | FCVAR_ARCHIVE,
                                                "Enables NVIDIA Reflex Low Latency Boost.", false, 0.0f, false, 0.0f, GFX_NVN_Changed_f);

    // NOTE: defaulted to 0 as it causes rubber banding on some hardware.
    Cvar_gfx_nvnUseMarkersToOptimize = new ConVar("gfx_nvnUseMarkersToOptimize", "0", FCVAR_RELEASE,
                                                  "Use NVIDIA Reflex Low Latency markers to optimize (requires Low Latency Boost to be enabled).",
                                                  false, 0.0f, false, 0.0f, GFX_NVN_Changed_f);
    Cvar_gfx_ffxUseLowLatency = new ConVar("gfx_ffxUseLowLatency", "1", FCVAR_RELEASE | FCVAR_ARCHIVE, "Enables AMD Anti-Lag 2 Low Latency SDK.",
                                           false, 0.0f, false, 0.0f, GFX_FFX_Changed_f);

    LowLatencyHooks.DispatchForModule("engine.dll");
})

ON_DLL_LOAD_CLIENT("client.dll", LowLatencyClient, [](CModule module)
{
    NOTE_UNUSED(module);
    LowLatencyHooks.DispatchForModule("client.dll");
})

ON_DLL_LOAD_CLIENT_RELIESON("materialsystem_dx11.dll", LowLatencyMaterialSystem, D3D11, [](CModule module)
{
    NOTE_UNUSED(module);
    g_pMaterialSystem = Sys_GetFactoryPtr("materialsystem_dx11.dll", MATERIAL_SYSTEM_INTERFACE_VERSION).RCast<IMaterialSystem*>();
    if (!g_pMaterialSystem)
        spdlog::error("Unable to acquire {} for low-latency integration", MATERIAL_SYSTEM_INTERFACE_VERSION);

    LowLatencyHooks.DispatchForModule("materialsystem_dx11.dll");
})

ON_DLL_LOAD_CLIENT("inputsystem.dll", LowLatencyInputSystem, [](CModule module)
{
    NOTE_UNUSED(module);
    LowLatencyHooks.DispatchForModule("inputsystem.dll");
})
