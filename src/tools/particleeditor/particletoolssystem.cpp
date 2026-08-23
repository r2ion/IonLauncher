#include "particletoolssystem.h"

#include "appframework/IAppSystem.h"
#include "client/cdll_client_int.h"
#include "config/profile.h"
#include "core/convar/concommand.h"
#include "core/tier0.h"
#include "core/tier1.h"
#include "game/client/iclientrenderable.h"
#include "game/client/viewrender.h"
#include "materialsystem/imatrendercontext.h"
#include "plugins/interfaces/interface_registry.h"
#include "rtech/rui/rui_core_types.h"
#include "tier0/frametask.h"
#include "tier0/hooks.h"
#include "toolframework/itooldictionary.h"
#include "util/wininfo.h"
#include "vgui/ienginevgui.h"
#include "vgui/IInput.h"
#include "vgui/surface.h"
#include "vscript/squirrel/squirrel.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

DECLARE_MODULE(ParticleToolsFrameworkHooks)

namespace ParticleTools
{
inline constexpr char EngineToolInterfaceVersion[] = "VENGINETOOL003";
inline constexpr char ToolFrameworkInterfaceVersion[] = "VTOOLFRAMEWORKVERSION002";
inline constexpr char VGuiInputInternalInterfaceVersion[] = "VGUI_InputInternal001";
inline constexpr std::size_t EngineToolOnModeChangedSlot = 69;
inline constexpr std::size_t ToolFrameworkSwitchToToolByNameSlot = 48;
inline constexpr std::uintptr_t BuildRenderViewOffset = 0x359250;
inline constexpr int RenderViewDrawHud = 2;
inline constexpr std::uintptr_t ParticleHandleTableOffset = 0xFB6EF0;
struct ParticleEffectHandleEntry
{
    std::uint32_t m_Handle;
    std::byte m_Reserved[28];
    void* m_pEffect;
};
static_assert(offsetof(ParticleEffectHandleEntry, m_pEffect) == 32);
static_assert(sizeof(ParticleEffectHandleEntry) == 40);
inline constexpr std::uint32_t InvalidParticleHandle = 0xFFFFFFFF;
inline constexpr std::uint32_t MaximumParticleHandleIndex = 599;
static thread_local bool g_PreviewRenderViewActive = false;
static thread_local bool g_PreviewSceneCleared = false;
static std::atomic<std::uint32_t> g_PreviewParticleIndex = InvalidParticleHandle;
static std::atomic<std::uint32_t> g_PreviewParticleHandle = InvalidParticleHandle;
void* ResolvePreviewParticleEffect()
{
    const std::uint32_t handle = g_PreviewParticleHandle.load(std::memory_order_acquire);
    const std::uint32_t handleIndex = handle & 0xFFFF;
    if (handle == InvalidParticleHandle || handleIndex > MaximumParticleHandleIndex)
        return nullptr;

    const HMODULE clientModule = GetModuleHandleW(L"client.dll");
    if (!clientModule)
        return nullptr;

    const auto* handleTable = reinterpret_cast<const ParticleEffectHandleEntry*>(
        reinterpret_cast<const std::byte*>(clientModule) + ParticleHandleTableOffset);
    const ParticleEffectHandleEntry& entry = handleTable[handleIndex];
    if (entry.m_Handle != handle)
        return nullptr;
    return entry.m_pEffect;
}

} // namespace ParticleTools

DECLARE_HOOK(LoadParticleToolsFromEngineManifest, engine.dll + 0x243790, [](auto& hook, void* framework)
{
    hook.Original(framework);
    ParticleTools::GetParticleToolSystem().LoadIntoEngineToolFramework(framework, false);
})

// Keep the engine's RUI job bookkeeping intact while preventing game-owned
// HUD, cockpit, and world-space RUI from painting over the editor workspace.
DECLARE_HOOK(SuppressParticleEditorRui, engine.dll + 0xFC7A0, [](auto& hook, RuiRenderContext* context)
{
    if (!context || !ParticleTools::GetParticleToolSystem().IsEditorInputEnabled())
    {
        hook.Original(context);
        return;
    }

    const std::uint16_t ruiCount = context->ruiCount;
    context->ruiCount = 0;
    context->materialBatchCount = 0;
    hook.Original(context);
    context->ruiCount = ruiCount;
})

// Replace the player view at retail's per-frame setup boundary. Rebuilding the
// shared main view later in RenderView lets other consumers alternate cameras.
DECLARE_HOOK(OverrideParticlePreviewCamera, client.dll + 0x35A500, [](auto& hook, CViewRender* pViewRender, int splitScreenSlot)
{
    hook.Original(pViewRender, splitScreenSlot);
    if (!pViewRender)
        return;

    CViewSetup previewView;
    CViewRenderView& renderView = pViewRender->m_MainView;
    auto& toolSystem = ParticleTools::GetParticleToolSystem();
    if (!toolSystem.PreparePreviewView(pViewRender->m_PlayerViewSetup, &renderView, previewView))
        return;

    const HMODULE clientModule = GetModuleHandleW(L"client.dll");
    if (!clientModule)
        return;
    using BuildRenderViewFn = void(__fastcall*)(const CViewSetup*, CViewRenderView*);
    const auto buildRenderView =
        reinterpret_cast<BuildRenderViewFn>(reinterpret_cast<std::uintptr_t>(clientModule) + ParticleTools::BuildRenderViewOffset);
    pViewRender->m_PlayerViewSetup = previewView;
    buildRenderView(&pViewRender->m_PlayerViewSetup, &renderView);
    toolSystem.FinalizePreviewView(&renderView);
    pViewRender->SetupViewMatrices(&renderView);
})

DECLARE_HOOK(CaptureParticlePreviewIndex, client.dll + 0x1D10B0, [](auto& hook, HSQUIRRELVM sqvm) -> SQRESULT
{
    const char* effectName = sqvm && g_pSquirrel[ScriptContext::CLIENT] ? g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1) : nullptr;
    const bool isPreviewEffect = effectName && std::strstr(effectName, "__northstar_preview_") != nullptr;
    const SQRESULT result = hook.Original(sqvm);
    if (!isPreviewEffect)
        return result;

    ParticleTools::g_PreviewParticleIndex.store(ParticleTools::InvalidParticleHandle, std::memory_order_release);
    ParticleTools::g_PreviewParticleHandle.store(ParticleTools::InvalidParticleHandle, std::memory_order_release);
    if (result == SQRESULT_NOTNULL && sqvm && sqvm->_stack && sqvm->_top > 0)
    {
        const SQObject& returnValue = sqvm->_stack[sqvm->_top - 1];
        if (returnValue._Type == OT_INTEGER)
            ParticleTools::g_PreviewParticleIndex.store(static_cast<std::uint32_t>(returnValue._VAL.asInteger), std::memory_order_release);
    }
    return result;
})

DECLARE_HOOK(CaptureParticlePreviewHandle, client.dll + 0x1D65E0,
             [](auto& hook, std::uint32_t particleIndex, const void* origin, const void* angles) -> std::uint32_t
{
    const std::uint32_t handle = hook.Original(particleIndex, origin, angles);
    if (particleIndex == ParticleTools::g_PreviewParticleIndex.load(std::memory_order_acquire))
    {
        ParticleTools::g_PreviewParticleHandle.store(handle, std::memory_order_release);
    }
    return handle;
})

DECLARE_HOOK(ConstrainParticlePreviewRenderView, client.dll + 0x3723B0,
             [](auto& hook, CViewRender* pViewRender, const CViewRenderView* pRenderView, int clearFlags, int whatToDraw)
{
    const bool previewActive = ParticleTools::GetParticleToolSystem().ConstrainPreviewRenderView(pViewRender, pRenderView);
    const bool previousPreviewRenderViewActive = ParticleTools::g_PreviewRenderViewActive;
    const bool previousPreviewSceneCleared = ParticleTools::g_PreviewSceneCleared;
    ParticleTools::g_PreviewRenderViewActive = previewActive;
    ParticleTools::g_PreviewSceneCleared = false;
    hook.Original(pViewRender, pRenderView, clearFlags, previewActive ? whatToDraw & ~ParticleTools::RenderViewDrawHud : whatToDraw);
    ParticleTools::g_PreviewSceneCleared = previousPreviewSceneCleared;
    ParticleTools::g_PreviewRenderViewActive = previousPreviewRenderViewActive;
})

// Draw only the selected preview effect and clear depth in the preview viewport
// so map geometry provides context without occluding the authored particle.
DECLARE_HOOK(ConstrainParticlePreviewList, client.dll + 0x642780,
             [](auto& hook, void* pParticleSystemManager, const CViewRenderView* pRenderView, IMatRenderContext* pRenderContext,
                int renderMode) -> std::int64_t
{
    if (!ParticleTools::g_PreviewRenderViewActive)
        return hook.Original(pParticleSystemManager, pRenderView, pRenderContext, renderMode);

    if (!ParticleTools::g_PreviewSceneCleared)
    {
        pRenderContext->PushScissorRect(pRenderView->m_ViewportX, pRenderView->m_ViewportY, pRenderView->m_ViewportX + pRenderView->m_ViewportWidth,
                                        pRenderView->m_ViewportY + pRenderView->m_ViewportHeight);
        pRenderContext->ClearBuffers(false, true);
        pRenderContext->PopScissorRect();
        ParticleTools::g_PreviewSceneCleared = true;
    }

    void* previewParticle = ParticleTools::ResolvePreviewParticleEffect();
    if (!previewParticle)
        return hook.Original(pParticleSystemManager, pRenderView, pRenderContext, renderMode);

    auto* renderState = static_cast<std::byte*>(pParticleSystemManager) + static_cast<std::ptrdiff_t>(renderMode) * 0x20;
    auto** renderList = reinterpret_cast<void***>(renderState + 0x1C8);
    auto* renderCount = reinterpret_cast<std::int32_t*>(renderState + 0x1E0);
    void** previousList = *renderList;
    const std::int32_t previousCount = *renderCount;
    bool previewListed = false;
    for (std::int32_t index = 0; previousList && index < previousCount; ++index)
    {
        if (previousList[index] == previewParticle)
        {
            previewListed = true;
            break;
        }
    }

    void* previewList[] = {previewParticle};
    *renderList = previewList;
    *renderCount = previewListed ? 1 : 0;
    const std::int64_t result = hook.Original(pParticleSystemManager, pRenderView, pRenderContext, renderMode);
    *renderList = previousList;
    *renderCount = previousCount;
    return result;
})

// VClient018 delivers keys to VGUI before IN_KeyEvent. Swallowing only key-down
// here preserves editor text input and lets key-up clear any gameplay state that
// was active when the editor opened.
DECLARE_HOOK(BlockParticleToolsClientKeyEvent, client.dll + 0x190840,
             [](auto& hook, void* client, int eventCode, unsigned int keyCode, const char* binding) -> int
{
    if (eventCode != 0 && ParticleTools::GetParticleToolSystem().IsEditorInputEnabled())
        return 0;
    return hook.Original(client, eventCode, keyCode, binding);
})

// An inactive CreateMove resets accumulated mouse deltas without changing the
// view or movement command while the editor owns input.
DECLARE_HOOK(SuspendParticleToolsCreateMove, client.dll + 0x18EB50,
             [](auto& hook, void* client, int sequenceNumber, float inputSampleFrameTime, bool active)
{ hook.Original(client, sequenceNumber, inputSampleFrameTime, active && !ParticleTools::GetParticleToolSystem().IsEditorInputEnabled()); })

namespace ParticleTools
{
class CParticleToolDictionary final : public IToolDictionary
{
  public:
    bool Connect(CreateInterfaceFn factory) override
    {
        m_Factory = factory;
        return true;
    }

    void Disconnect() override
    {
        m_Factory = nullptr;
    }

    void* QueryInterface(const char* pInterfaceName) override
    {
        if (pInterfaceName && std::strcmp(pInterfaceName, VTOOLDICTIONARY_INTERFACE_VERSION) == 0)
            return this;
        return nullptr;
    }

    InitReturnVal_t Init() override
    {
        return INIT_OK;
    }
    void Shutdown() override
    {
    }
    const AppSystemInfo_t* GetDependencies() override
    {
        return nullptr;
    }
    void Reconnect(CreateInterfaceFn factory, const char* pInterfaceName) override
    {
        m_Factory = factory;
    }

    void CreateTools() override
    {
    }
    int GetToolCount() const override
    {
        return 1;
    }
    IToolSystem* GetTool(int index) override
    {
        return index == 0 ? &GetParticleToolSystem() : nullptr;
    }

  private:
    CreateInterfaceFn m_Factory = nullptr;
};

static CParticleToolSystem s_ParticleToolSystem;
static CParticleToolDictionary s_ParticleToolDictionary;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CParticleToolDictionary, IToolDictionary, VTOOLDICTIONARY_INTERFACE_VERSION, s_ParticleToolDictionary)

static std::string EscapeSquirrelString(std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }
    return escaped;
}

static std::string GetUtf8Path(const std::filesystem::path& path)
{
    const std::u8string utf8 = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

static bool ParseVectorValue(std::string_view value, Vector& result)
{
    float components[3]{};
    const char* current = value.data();
    const char* const end = current + value.size();
    for (float& component : components)
    {
        while (current != end && std::isspace(static_cast<unsigned char>(*current)))
            ++current;
        if (current == end)
            return false;

        const auto [next, error] = std::from_chars(current, end, component);
        if (error != std::errc())
            return false;
        current = next;
    }

    while (current != end && std::isspace(static_cast<unsigned char>(*current)))
        ++current;
    if (current != end)
        return false;

    Vector parsed(components[0], components[1], components[2]);
    if (!parsed.IsValid())
        return false;
    result = parsed;
    return true;
}

static void StabilizePreviewProjection(CViewRenderView& renderView)
{
    renderView.m_ProjectionMatrix.m_Elements[0][2] = 0.0f;
    renderView.m_ProjectionMatrix.m_Elements[1][2] = 0.0f;

    MatrixMultiply(renderView.m_ProjectionMatrix, renderView.m_ViewMatrix, renderView.m_ViewProjectionMatrix);
}

CParticleToolSystem::CParticleToolSystem() = default;

CParticleToolSystem::~CParticleToolSystem()
{
    CloseEditor();
}

const char* CParticleToolSystem::GetToolName()
{
    return "Northstar Particle Editor";
}

bool CParticleToolSystem::Init()
{
    OpenEditor();
    return m_pEditor && m_pEditor->IsOpen();
}

void CParticleToolSystem::Shutdown()
{
    RequestStopPreview();
    CloseEditor();
    m_pToolFramework = nullptr;
}

bool CParticleToolSystem::ServerInit(CreateInterfaceFn serverFactory)
{
    m_pServerTools = serverFactory ? static_cast<IServerTools*>(serverFactory(VSERVERTOOLS_INTERFACE_VERSION, nullptr)) : nullptr;
    AcquireRuntimeInterfaces();
    return true;
}

void CParticleToolSystem::AcquireInputInterfaces()
{
    if (!m_pEngineTool)
        m_pEngineTool = Sys_GetFactoryPtr("engine.dll", EngineToolInterfaceVersion).RCast<void*>();
    if (!m_pInputInternal)
        m_pInputInternal = Sys_GetFactoryPtr("vgui2.dll", VGuiInputInternalInterfaceVersion).RCast<vgui::IInput*>();
}

void CParticleToolSystem::AcquireRuntimeInterfaces()
{
    if (!m_pClientTools)
        m_pClientTools = Sys_GetFactoryPtr("client.dll", VCLIENTTOOLS_INTERFACE_VERSION).RCast<IClientTools*>();
    if (!m_pClientTools)
        m_pClientTools = g_pClientTools;
    if (!m_pServerTools)
        m_pServerTools = Sys_GetFactoryPtr("server.dll", VSERVERTOOLS_INTERFACE_VERSION).RCast<IServerTools*>();
    AcquireInputInterfaces();
    static constexpr std::array<const char*, 3> particleHookNames = {
        "CaptureParticlePreviewIndex",
        "CaptureParticlePreviewHandle",
        "ConstrainParticlePreviewList",
    };
    for (const char* hookName : particleHookNames)
    {
        const std::shared_ptr<HookSys::LambdaHookBase> particleHook = HookSys::FindHook(hookName);
        if (particleHook)
            particleHook->Dispatch();
    }
}

bool CParticleToolSystem::ClientInit(CreateInterfaceFn clientFactory)
{
    m_pClientTools = clientFactory ? static_cast<IClientTools*>(clientFactory(VCLIENTTOOLS_INTERFACE_VERSION, nullptr)) : nullptr;
    AcquireRuntimeInterfaces();
    return m_pClientTools != nullptr;
}

void CParticleToolSystem::ServerShutdown()
{
    m_MapCameraValid = false;
    m_MapCameraLookupAttempted = false;
    m_pServerTools = nullptr;
}

void CParticleToolSystem::ClientShutdown()
{
    RequestStopPreview();
    CloseEditor();
    SetClientLevelActive(false);
    m_pClientTools = nullptr;
    SetEditorInputEnabled(false);
    m_pInputInternal = nullptr;
    m_pEngineTool = nullptr;
}

bool CParticleToolSystem::CanQuit(const char* pExitMessage)
{
    NOTE_UNUSED(pExitMessage);
    return true;
}

void CParticleToolSystem::Think(bool finalTick)
{
    NOTE_UNUSED(finalTick);
    if (m_pEditor)
        m_pEditor->Think();
}

void CParticleToolSystem::ServerLevelInitPreEntity()
{
    m_MapCameraValid = false;
    m_MapCameraLookupAttempted = false;
}

void CParticleToolSystem::ServerLevelInitPostEntity()
{
    UpdateMapDefaultCamera();
}

void CParticleToolSystem::UpdateMapDefaultCamera()
{
    m_MapCameraLookupAttempted = true;
    m_MapCameraValid = FindMapDefaultCamera(m_MapCameraOrigin, m_MapCameraAngles, m_MapCameraFocus);
    if (m_MapCameraValid)
        m_PreviewCameraValid = false;
}

void CParticleToolSystem::ServerLevelShutdownPreEntity()
{
    m_MapCameraValid = false;
    m_MapCameraLookupAttempted = false;
}
void CParticleToolSystem::ServerLevelShutdownPostEntity()
{
}
void CParticleToolSystem::ServerFrameUpdatePreEntityThink()
{
}
void CParticleToolSystem::ServerFrameUpdatePostEntityThink()
{
}
void CParticleToolSystem::ServerPreClientUpdate()
{
}
void CParticleToolSystem::ServerPreSetupVisibility()
{
}
const char* CParticleToolSystem::GetEntityData(const char* pActualEntityData)
{
    return pActualEntityData;
}

bool CParticleToolSystem::FindMapDefaultCamera(Vector& origin, QAngle& angles, Vector& focus) const
{
    if (!m_pServerTools)
        return false;

    std::array<char, 64> className{};
    std::array<char, 128> originValue{};
    std::array<char, 128> anglesValue{};
    std::array<char, 128> targetValue{};
    for (CBaseEntity* entity = m_pServerTools->FirstEntity(); entity; entity = m_pServerTools->NextEntity(entity))
    {
        className.fill('\0');
        if (!m_pServerTools->GetKeyValue(entity, "classname", className.data(), static_cast<std::uint32_t>(className.size())) ||
            std::strcmp(className.data(), "info_intermission") != 0)
        {
            continue;
        }

        originValue.fill('\0');
        Vector parsedOrigin;
        if (!m_pServerTools->GetKeyValue(entity, "origin", originValue.data(), static_cast<std::uint32_t>(originValue.size())) ||
            !ParseVectorValue(originValue.data(), parsedOrigin))
        {
            continue;
        }

        Vector parsedAngles;
        anglesValue.fill('\0');
        if (m_pServerTools->GetKeyValue(entity, "angles", anglesValue.data(), static_cast<std::uint32_t>(anglesValue.size())))
        {
            ParseVectorValue(anglesValue.data(), parsedAngles);
        }

        origin = parsedOrigin;
        angles = QAngle(parsedAngles.x, parsedAngles.y, parsedAngles.z);
        const float pitch = DEG2RAD(angles.x);
        const float yaw = DEG2RAD(angles.y);
        focus = origin + Vector(std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), -std::sin(pitch)) * 256.0f;

        targetValue.fill('\0');
        if (!m_pServerTools->GetKeyValue(entity, "target", targetValue.data(), static_cast<std::uint32_t>(targetValue.size())) ||
            targetValue.front() == '\0')
        {
            return true;
        }

        std::array<char, 128> targetName{};
        std::array<char, 128> targetOriginValue{};
        for (CBaseEntity* targetEntity = m_pServerTools->FirstEntity(); targetEntity; targetEntity = m_pServerTools->NextEntity(targetEntity))
        {
            targetName.fill('\0');
            if (!m_pServerTools->GetKeyValue(targetEntity, "targetname", targetName.data(), static_cast<std::uint32_t>(targetName.size())) ||
                std::strcmp(targetName.data(), targetValue.data()) != 0)
            {
                continue;
            }

            targetOriginValue.fill('\0');
            Vector parsedFocus;
            if (!m_pServerTools->GetKeyValue(targetEntity, "origin", targetOriginValue.data(),
                                             static_cast<std::uint32_t>(targetOriginValue.size())) ||
                !ParseVectorValue(targetOriginValue.data(), parsedFocus))
            {
                continue;
            }

            focus = parsedFocus;
            const Vector direction = focus - origin;
            const float horizontalDistance = std::hypot(direction.x, direction.y);
            if (horizontalDistance > 0.001f || std::abs(direction.z) > 0.001f)
            {
                angles = QAngle(RAD2DEG(-std::atan2(direction.z, horizontalDistance)), RAD2DEG(std::atan2(direction.y, direction.x)), 0.0f);
            }
            return true;
        }
        return true;
    }
    return false;
}

bool CParticleToolSystem::IsClientLevelActive() const
{
    const SquirrelManager* pClientSquirrel = g_pSquirrel[ScriptContext::CLIENT];
    return pClientSquirrel && pClientSquirrel->m_pSQVM;
}

void CParticleToolSystem::SetClientLevelActive(bool active)
{
    if (active != m_ClientLevelActive)
    {
        m_ClientLevelActive = active;
        m_PreviewCameraValid = false;
    }
    if (!active)
    {
        m_PreviewCameraValid = false;
        m_MapCameraValid = false;
        m_MapCameraLookupAttempted = false;
    }
    if (m_pEditor)
        m_pEditor->SetPreviewEnabled(active);
}

void CParticleToolSystem::SynchronizeEditorWithClientLevel()
{
    const bool active = IsClientLevelActive();
    SetClientLevelActive(active);
    if (!active)
        return;

    AcquireRuntimeInterfaces();
    if (!m_MapCameraLookupAttempted)
        UpdateMapDefaultCamera();
}

void* CParticleToolSystem::QueryInterface(const char* pInterfaceName)
{
    NOTE_UNUSED(pInterfaceName);
    return nullptr;
}

void CParticleToolSystem::ClientLevelInitPreEntity()
{
    SetClientLevelActive(false);
}

void CParticleToolSystem::ClientLevelInitPostEntity()
{
    SetClientLevelActive(true);
}

void CParticleToolSystem::ClientLevelShutdownPreEntity()
{
    SetClientLevelActive(false);
    RequestStopPreview();
}

void CParticleToolSystem::ClientLevelShutdownPostEntity()
{
}

bool CParticleToolSystem::PreparePreviewView(const CViewSetup& sourceView, CViewRenderView* pRenderView, CViewSetup& previewView)
{
    if (!pRenderView || !m_pEditor)
        return false;

    constexpr std::size_t MainViewFromPlayerViewSetup = offsetof(CViewRender, m_MainView) - offsetof(CViewRender, m_PlayerViewSetup);
    const auto expectedPlayerViewSetup = reinterpret_cast<std::uintptr_t>(pRenderView) - MainViewFromPlayerViewSetup;
    if (reinterpret_cast<std::uintptr_t>(&sourceView) != expectedPlayerViewSetup)
        return false;

    SynchronizeEditorWithClientLevel();

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    if (!m_pEditor->GetPreviewViewport(x, y, width, height))
        return false;

    Vector previewOrigin;
    QAngle previewAngles;
    float previewFov = 0.0f;
    if (!SetupEngineView(previewOrigin, previewAngles, previewFov))
        return false;

    previewView = sourceView;
    previewView.m_X = 0;
    previewView.m_Y = 0;
    previewView.m_Width = width;
    previewView.m_Height = height;
    previewView.m_Origin = previewOrigin;
    previewView.m_Angles = previewAngles;
    previewView.m_UnreflectedOrigin = previewOrigin;
    previewView.m_UnreflectedAngles = previewAngles;
    previewView.m_ShutterOpenPosition = previewOrigin;
    previewView.m_ShutterOpenAngles = previewAngles;
    previewView.m_ShutterClosePosition = previewOrigin;
    previewView.m_ShutterCloseAngles = previewAngles;
    previewView.m_AspectRatio = static_cast<float>(height) / static_cast<float>(width);
    previewView.m_TanHalfFovX = std::tan(DEG2RAD(std::clamp(previewFov, 1.0f, 179.0f) * 0.5f));
    previewView.m_TanHalfFovY = previewView.m_TanHalfFovX * previewView.m_AspectRatio;
    return true;
}

void CParticleToolSystem::FinalizePreviewView(CViewRenderView* pRenderView)
{
    if (!pRenderView)
        return;


    int outputX = 0;
    int outputY = 0;
    int width = 0;
    int height = 0;
    if (!m_pEditor || !m_pEditor->GetPreviewViewport(outputX, outputY, width, height))
        return;

    // Render the scene at the origin of the full-frame buffers, then present it
    // into the editor's screen-space viewport. Retail assumes these origins are
    // identical; keeping the editor offset in the scene viewport shifts later
    // full-target post-processing and leaves a second, oversized scene copy.
    pRenderView->m_ViewportX = 0;
    pRenderView->m_ViewportY = 0;
    pRenderView->m_ViewportWidth = width;
    pRenderView->m_ViewportHeight = height;
    pRenderView->m_UnscaledViewportX = 0;
    pRenderView->m_UnscaledViewportY = 0;
    pRenderView->m_UnscaledViewportWidth = width;
    pRenderView->m_OutputViewportX = outputX;
    pRenderView->m_OutputViewportY = outputY;
    pRenderView->m_OutputViewportWidth = width;
    pRenderView->m_OutputViewportHeight = height;
    StabilizePreviewProjection(*pRenderView);
}

bool CParticleToolSystem::ConstrainPreviewRenderView(CViewRender* pViewRender, const CViewRenderView* pRenderView)
{
    if (!pViewRender || !pRenderView || pRenderView != &pViewRender->m_MainView || !m_pEditor || !vgui::g_pVGuiSurface)
        return false;

    SynchronizeEditorWithClientLevel();

    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    if (!m_pEditor->GetPreviewViewport(x, y, width, height))
        return false;

    int screenWidth = 0;
    int screenHeight = 0;
    vgui::g_pVGuiSurface->GetScreenSize(screenWidth, screenHeight);
    if (x < 0 || y < 0 || x + width > screenWidth || y + height > screenHeight)
        return false;

    CViewRenderView& renderView = *const_cast<CViewRenderView*>(pRenderView);
    // RenderView is invoked for m_MainView. Auxiliary records belong to other
    // retail passes, so leave them untouched and stabilize only the presented
    // scene view after retail's final matrix rebuild.
    FinalizePreviewView(&renderView);

    return true;
}

void CParticleToolSystem::ClientPreRender()
{
}

void CParticleToolSystem::ClientPostRender()
{
}

void CParticleToolSystem::AdjustEngineViewport(int& x, int& y, int& width, int& height)
{
    if (!m_pEditor || !m_pEditor->GetPreviewViewport(x, y, width, height))
        return;

}

void CParticleToolSystem::CenterPreviewCamera()
{
    Vector forward;
    Vector right;
    Vector up;
    AngleVectors(m_PreviewCameraAngles, &forward, &right, &up);

    float projectionCenterX = 0.0f;
    float projectionCenterY = 0.0f;
    float aspectRatio = 9.0f / 16.0f;
    int outputX = 0;
    int outputY = 0;
    int width = 0;
    int height = 0;
    if (m_pEditor && m_pEditor->GetPreviewViewport(outputX, outputY, width, height) && width > 0 && height > 0)
    {
        aspectRatio = static_cast<float>(height) / static_cast<float>(width);

        int screenWidth = 0;
        int screenHeight = 0;
        if (vgui::g_pVGuiSurface)
            vgui::g_pVGuiSurface->GetScreenSize(screenWidth, screenHeight);
        if (screenWidth > 0 && screenHeight > 0)
        {
            const float viewportCenterX = static_cast<float>(outputX) + static_cast<float>(width) * 0.5f;
            const float viewportCenterY = static_cast<float>(outputY) + static_cast<float>(height) * 0.5f;
            projectionCenterX = viewportCenterX * 2.0f / static_cast<float>(screenWidth) - 1.0f;
            projectionCenterY = 1.0f - viewportCenterY * 2.0f / static_cast<float>(screenHeight);
        }
    }

    const float tanHalfFovX = std::tan(DEG2RAD(std::clamp(m_PreviewCameraFov, 1.0f, 179.0f) * 0.5f));
    const float offsetRight = projectionCenterX * m_PreviewCameraDistance * tanHalfFovX;
    const float offsetUp = projectionCenterY * m_PreviewCameraDistance * tanHalfFovX * aspectRatio;
    m_PreviewCameraOrigin =
        m_PreviewFocus - forward * m_PreviewCameraDistance - right * offsetRight - up * offsetUp;

}

bool CParticleToolSystem::SetupEngineView(Vector& origin, QAngle& angles, float& fov)
{
    if (!IsEditorInputEnabled())
        return false;

    if (!m_PreviewCameraValid)
    {
        if (m_MapCameraValid)
        {
            m_PreviewCameraOrigin = m_MapCameraOrigin;
            m_PreviewCameraAngles = m_MapCameraAngles;
            m_PreviewFocus = m_MapCameraOrigin + (m_MapCameraFocus - m_MapCameraOrigin) * 0.75f;
            m_PreviewCameraDistance = (m_PreviewFocus - m_MapCameraOrigin).Length();
            m_PreviewCameraFov = 75.0f;
            m_PreviewCameraValid = true;
        }
        else
        {
            Vector playerEyeOrigin;
            QAngle playerEyeAngles;
            float playerFov = 0.0f;
            if (!m_pClientTools || !m_pClientTools->GetLocalPlayerEyePosition(playerEyeOrigin, playerEyeAngles, playerFov))
            {
                return false;
            }
            if (!playerEyeOrigin.IsValid() || std::abs(playerEyeOrigin.x) + std::abs(playerEyeOrigin.y) + std::abs(playerEyeOrigin.z) < 1.0f)
            {
                return false;
            }

            const float yaw = DEG2RAD(playerEyeAngles.y);
            m_PreviewFocus = playerEyeOrigin + Vector(std::cos(yaw) * 192.0f, std::sin(yaw) * 192.0f, -56.0f);
            m_PreviewCameraAngles = QAngle(18.0f, playerEyeAngles.y, 0.0f);
            m_PreviewCameraDistance = 256.0f;
            m_PreviewCameraFov = std::clamp(playerFov > 0.0f ? playerFov : 75.0f, 60.0f, 100.0f);
            m_PreviewCameraValid = true;
        }
    }
    CenterPreviewCamera();

    origin = m_PreviewCameraOrigin;
    angles = m_PreviewCameraAngles;
    fov = m_PreviewCameraFov;
    return true;
}

bool CParticleToolSystem::ShouldGameRenderView()
{
    return true;
}

bool CParticleToolSystem::IsThirdPersonCamera()
{
    return false;
}

bool CParticleToolSystem::ShouldGamePlaySounds()
{
    return true;
}

IMaterialProxy* CParticleToolSystem::LookupProxy(const char* pProxyName)
{
    NOTE_UNUSED(pProxyName);
    return nullptr;
}

void CParticleToolSystem::OnToolActivate()
{
    OpenEditor();
}

void CParticleToolSystem::OnToolDeactivate()
{
    RequestStopPreview();
    // Titanfall deactivates the selected tool while activating the frontend.
    // The tool dictionary remains loaded, so keep its workspace available.
}

bool CParticleToolSystem::GetSoundSpatialization(int userData, int guid, SpatializationInfo_t& info)
{
    NOTE_UNUSED(userData);
    NOTE_UNUSED(guid);
    NOTE_UNUSED(info);
    return false;
}
void CParticleToolSystem::RenderFrameBegin()
{
}
void CParticleToolSystem::RenderFrameEnd()
{
}

void CParticleToolSystem::HostRunFrameBegin()
{
}
void CParticleToolSystem::VGui_PreRender(int paintMode)
{
    NOTE_UNUSED(paintMode);
    if (m_pEditor)
        m_pEditor->PaintEngineUi();
}
void CParticleToolSystem::HostRunFrameEnd()
{
}

void CParticleToolSystem::VGui_PostRender(int paintMode)
{
    NOTE_UNUSED(paintMode);
}

void CParticleToolSystem::VGui_PreSimulate()
{
}
void CParticleToolSystem::VGui_PostSimulate()
{
}

vgui::VPANEL CParticleToolSystem::GetClientWorkspaceArea()
{
    return m_pEditor ? m_pEditor->GetVPanel() : 0;
}

bool CParticleToolSystem::ShouldAllowGameUI()
{
    return false;
}

void CParticleToolSystem::RequestPreview(const ParticleDocument& document, const std::filesystem::path& assetPath, const std::string& effectName,
                                         const std::vector<ParticlePreviewControlPoint>& controlPoints)
{
    NOTE_UNUSED(assetPath);
    if (effectName.empty())
    {
        if (m_pEditor)
            m_pEditor->SetStatus("Preview failed: the selected particle has no name");
        return;
    }

    const std::uint64_t generation = m_PreviewGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
    const std::string previewEffectName = effectName + "__northstar_preview_" + std::to_string(generation);
    ParticleDocument previewDocument = document;
    DmxElement* pPreviewSystem = nullptr;
    for (DmxElement& element : previewDocument.Elements())
    {
        if (element.m_Type == "DmeParticleSystemDefinition" && element.m_Name == effectName)
        {
            pPreviewSystem = &element;
            break;
        }
    }
    if (!pPreviewSystem)
    {
        if (m_pEditor)
            m_pEditor->SetStatus("Preview failed: the selected particle-system definition was not found");
        return;
    }
    pPreviewSystem->m_Name = previewEffectName;

    const std::filesystem::path previewPath = GetPreviewPath();
    std::error_code directoryError;
    std::filesystem::create_directories(previewPath.parent_path(), directoryError);
    if (directoryError)
    {
        if (m_pEditor)
            m_pEditor->SetStatus("Preview failed: " + directoryError.message());
        return;
    }

    std::string saveError;
    if (!previewDocument.Save(previewPath, saveError))
    {
        if (m_pEditor)
            m_pEditor->SetStatus("Preview failed: " + saveError);
        return;
    }

    std::ifstream input(previewPath, std::ios::binary | std::ios::ate);
    if (!input)
    {
        if (m_pEditor)
            m_pEditor->SetStatus("Preview failed: could not reopen native PCF buffer");
        return;
    }

    const std::streamoff size = input.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(std::numeric_limits<int>::max()))
    {
        if (m_pEditor)
            m_pEditor->SetStatus("Preview failed: native PCF buffer has an invalid size");
        return;
    }

    auto pData = std::make_shared<std::vector<std::byte>>(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    if (!input.read(reinterpret_cast<char*>(pData->data()), size))
    {
        if (m_pEditor)
            m_pEditor->SetStatus("Preview failed: could not read native PCF buffer");
        return;
    }

    RunInMainThread([this, pData = std::shared_ptr<const std::vector<std::byte>>(std::move(pData)), previewPath, effectName, previewEffectName,
                     controlPoints, generation]() mutable {
        RunPreview(std::move(pData), std::move(previewPath), std::move(effectName), std::move(previewEffectName), std::move(controlPoints),
                   generation);
    });
}

void CParticleToolSystem::RequestStopPreview()
{
    const std::uint64_t generation = m_PreviewGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
    RunInMainThread([this, generation] { RunStopPreview(generation); });
}
void CParticleToolSystem::SetEditorInputEnabled(bool enabled)
{
    if (enabled == m_ToolsInputEnabled.load(std::memory_order_acquire))
        return;

    AcquireInputInterfaces();
    if (enabled)
    {
        const vgui::VPANEL workspace = m_pEditor ? m_pEditor->GetVPanel() : 0;
        if (!workspace || !m_pEngineTool || !m_pInputInternal)
        {
            spdlog::error("[ParticleTools] Cannot enter tools input mode: workspace={}, "
                          "VENGINETOOL003={}, VGUI_InputInternal001={}",
                          workspace != 0, m_pEngineTool != nullptr, m_pInputInternal != nullptr);
            return;
        }

        m_pInputInternal->SetModalSubTree(workspace, 0, true);
        m_pInputInternal->SetModalSubTreeReceiveMessages(true);

        void** engineToolVTable = *reinterpret_cast<void***>(m_pEngineTool);
        using OnModeChangedFn = void(__fastcall*)(void*, bool);
        reinterpret_cast<OnModeChangedFn>(engineToolVTable[EngineToolOnModeChangedSlot])(m_pEngineTool, false);

        m_ToolsInputEnabled.store(true, std::memory_order_release);
        return;
    }

    if (m_pInputInternal)
    {
        m_pInputInternal->SetMouseCapture(0);
        m_pInputInternal->SetModalSubTreeReceiveMessages(false);
    }
    if (m_pEngineTool)
    {
        void** engineToolVTable = *reinterpret_cast<void***>(m_pEngineTool);
        using OnModeChangedFn = void(__fastcall*)(void*, bool);
        reinterpret_cast<OnModeChangedFn>(engineToolVTable[EngineToolOnModeChangedSlot])(m_pEngineTool, true);
    }
    if (m_pInputInternal)
        m_pInputInternal->ReleaseModalSubTree();

    m_ToolsInputEnabled.store(false, std::memory_order_release);
}

void CParticleToolSystem::SetEditorMouseCapture(bool enabled)
{
    AcquireInputInterfaces();
    if (!m_pInputInternal)
        return;

    const vgui::VPANEL workspace = enabled && m_pEditor ? m_pEditor->GetVPanel() : 0;
    m_pInputInternal->SetMouseCapture(workspace);
}
void CParticleToolSystem::AdjustPreviewCamera(float yawDelta, float pitchDelta, float zoomSteps)
{
    if (!m_PreviewCameraValid || !std::isfinite(yawDelta) || !std::isfinite(pitchDelta) || !std::isfinite(zoomSteps))
    {
        return;
    }

    m_PreviewCameraAngles.x = std::clamp(m_PreviewCameraAngles.x + pitchDelta, -85.0f, 85.0f);
    m_PreviewCameraAngles.y = std::remainder(m_PreviewCameraAngles.y + yawDelta, 360.0f);

    m_PreviewCameraDistance =
        std::clamp(m_PreviewCameraDistance * std::pow(0.85f, zoomSteps), 24.0f, 2048.0f);
    CenterPreviewCamera();
}

bool CParticleToolSystem::IsEditorInputEnabled() const
{
    return m_ToolsInputEnabled.load(std::memory_order_acquire);
}

void CParticleToolSystem::RunPreview(std::shared_ptr<const std::vector<std::byte>> pPcfData, std::filesystem::path sourcePath, std::string effectName,
                                     std::string previewEffectName, std::vector<ParticlePreviewControlPoint> controlPoints, std::uint64_t generation)
{
    if (generation != m_PreviewGeneration.load(std::memory_order_acquire))
        return;

    if (!m_pClientTools)
        m_pClientTools = g_pClientTools;
    if (!m_pClientTools)
    {
        if (m_pEditor)
            m_pEditor->SetStatus("Preview failed: VCLIENTTOOLS001 is unavailable");
        return;
    }

    const std::string sourceName = GetUtf8Path(sourcePath);
    m_pClientTools->EnableParticleSystems(true);
    m_pClientTools->ReloadParticleDefinitions(sourceName.c_str(), pPcfData->data(), static_cast<int>(pPcfData->size()));
    if (m_pServerTools)
        m_pServerTools->ReloadParticleDefinitions(sourceName.c_str(), pPcfData->data(), static_cast<int>(pPcfData->size()));

    SquirrelManager* pSquirrel = g_pSquirrel[ScriptContext::CLIENT];
    if (!pSquirrel || !pSquirrel->m_pSQVM)
    {
        if (m_pEditor)
            m_pEditor->SetStatus("Native PCF reloaded; enter a map before starting the visual preview");
        return;
    }

    Vector previewCameraOrigin;
    QAngle previewCameraAngles;
    float previewCameraFov = 0.0f;
    if (!SetupEngineView(previewCameraOrigin, previewCameraAngles, previewCameraFov))
    {
        if (m_pEditor)
            m_pEditor->SetStatus("Native PCF reloaded; a local player is required for visual preview");
        return;
    }
    const Vector previewOrigin = m_PreviewFocus;
    const QAngle previewAngles(0.0f, previewCameraAngles.y, 0.0f);

    std::ostringstream script;
    script.imbue(std::locale::classic());
    script << std::setprecision(9);
    const std::string escapedEffectName = EscapeSquirrelString(previewEffectName);
    script << "local nsParticleToolsRoot = getroottable()\n";
    script << "if (\"NSParticleToolsPreviewGeneration\" in nsParticleToolsRoot) "
              "nsParticleToolsRoot[\"NSParticleToolsPreviewGeneration\"] = "
           << generation << " else nsParticleToolsRoot[\"NSParticleToolsPreviewGeneration\"] <- " << generation << "\n";
    script << "if (!(\"NSParticleToolsPreviewHandle\" in nsParticleToolsRoot)) nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"] <- -1\n";
    script << "if (nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"] != -1 && "
              "EffectDoesExist(nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"])) "
              "EffectStop(nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"], true, false)\n";
    script << "nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"] = -1\n";
    script << "PrecacheParticleSystem($\"" << escapedEffectName << "\")\n";
    script << "void functionref() NSParticleToolsPreviewLoop" << generation << " = void function()\n";
    script << "{\n";
    script << " local nsParticleToolsRoot = getroottable()\n";
    script << " local nsParticleToolsIndex = GetParticleSystemIndex($\"" << escapedEffectName << "\")\n";
    script << " while (nsParticleToolsRoot[\"NSParticleToolsPreviewGeneration\"] == " << generation << ")\n";
    script << " {\n";
    script << "  if (nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"] == -1 || "
              "!EffectDoesExist(nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"]))\n";
    script << "  {\n";
    script << "   nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"] = "
              "StartParticleEffectInWorldWithHandle(nsParticleToolsIndex, < "
           << previewOrigin.x << "," << previewOrigin.y << "," << previewOrigin.z << ">, < " << previewAngles.x << "," << previewAngles.y << ","
           << previewAngles.z << ">)\n";

    for (const ParticlePreviewControlPoint& controlPoint : controlPoints)
    {
        const float x = previewOrigin.x + controlPoint.m_Position[0];
        const float y = previewOrigin.y + controlPoint.m_Position[1];
        const float z = previewOrigin.z + controlPoint.m_Position[2];
        script << "   EffectSetControlPointVector(nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"], " << controlPoint.m_Index << ", < " << x
               << "," << y << "," << z << ">)\n";
        script << "   EffectSetControlPointAngles(nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"], " << controlPoint.m_Index << ", < "
               << controlPoint.m_Angles[0] << "," << controlPoint.m_Angles[1] << "," << controlPoint.m_Angles[2] << ">)\n";
    }
    script << "  }\n";
    script << "  WaitFrame()\n";
    script << " }\n";
    script << "}\n";
    script << "thread NSParticleToolsPreviewLoop" << generation << "()\n";

    const SquirrelExecutionResult result = pSquirrel->ExecuteCode(script.str().c_str(), "Northstar Particle Editor live preview");
    if (!result.Succeeded())
    {
        if (m_pEditor)
            m_pEditor->SetStatus("Native PCF reloaded, but the client preview script failed");
        spdlog::error("[ParticleTools] Preview script failed for '{}'", effectName);
        return;
    }

    if (m_pEditor)
        m_pEditor->SetStatus("Live preview running: " + effectName);
}

void CParticleToolSystem::RunStopPreview(std::uint64_t generation)
{
    if (generation != m_PreviewGeneration.load(std::memory_order_acquire))
        return;
    g_PreviewParticleIndex.store(InvalidParticleHandle, std::memory_order_release);
    g_PreviewParticleHandle.store(InvalidParticleHandle, std::memory_order_release);

    SquirrelManager* pSquirrel = g_pSquirrel[ScriptContext::CLIENT];
    if (!pSquirrel || !pSquirrel->m_pSQVM)
        return;

    std::ostringstream script;
    script << "local nsParticleToolsRoot = getroottable()\n";
    script << "if (\"NSParticleToolsPreviewGeneration\" in nsParticleToolsRoot) "
              "nsParticleToolsRoot[\"NSParticleToolsPreviewGeneration\"] = "
           << generation << " else nsParticleToolsRoot[\"NSParticleToolsPreviewGeneration\"] <- " << generation << "\n";
    script << "if (\"NSParticleToolsPreviewHandle\" in nsParticleToolsRoot && "
              "nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"] != -1 && "
              "EffectDoesExist(nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"])) "
              "EffectStop(nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"], true, false)\n";
    script << "if (\"NSParticleToolsPreviewHandle\" in nsParticleToolsRoot) "
              "nsParticleToolsRoot[\"NSParticleToolsPreviewHandle\"] = -1\n";
    pSquirrel->ExecuteCode(script.str().c_str(), "Northstar Particle Editor stop preview");
    if (m_pEditor)
        m_pEditor->SetStatus("Live preview stopped");
}

std::filesystem::path CParticleToolSystem::GetPreviewPath() const
{
    return GetParticleEditorDirectory() / "preview.pcf";
}

bool CParticleToolSystem::LoadIntoEngineToolFramework(void* framework, bool explicitlyRequested)
{
    if (!framework)
        return false;
    if (m_pToolFramework == framework)
        return true;
    if (!explicitlyRequested && !std::strstr(GetCommandLineA(), "-particletools"))
        return false;

    std::array<char, 32768> modulePath{};
    if (!GetModuleFileNameA(g_NorthstarModule, modulePath.data(), static_cast<DWORD>(modulePath.size())))
    {
        spdlog::error("[ParticleTools] Could not resolve the loaded Northstar tool module path");
        return false;
    }

    const HMODULE engineModule = GetModuleHandleW(L"engine.dll");
    if (!engineModule)
        return false;
    using LoadToolModuleFn = void*(__fastcall*)(void*, const char*);
    const auto loadToolModule = reinterpret_cast<LoadToolModuleFn>(reinterpret_cast<std::uintptr_t>(engineModule) + 0x2438C0);
    if (!loadToolModule(framework, modulePath.data()))
    {
        spdlog::error("[ParticleTools] Engine tool framework rejected '{}'", modulePath.data());
        return false;
    }

    m_pToolFramework = framework;
    spdlog::info("[ParticleTools] VTOOLDICTIONARY003 loaded by the engine tool framework from '{}'", modulePath.data());
    return true;
}

bool CParticleToolSystem::ActivateEditorTool()
{
    if (!m_pToolFramework)
    {
        void* framework = Sys_GetFactoryPtr("engine.dll", ToolFrameworkInterfaceVersion).RCast<void*>();
        if (!framework)
        {
            spdlog::error("[ParticleTools] Cannot activate the editor: VTOOLFRAMEWORKVERSION002 is unavailable");
            return false;
        }
        if (!LoadIntoEngineToolFramework(framework, true))
            return false;
    }

    using SwitchToToolByNameFn = IToolSystem*(__fastcall*)(void*, const char*);
    const auto vtable = *reinterpret_cast<void***>(m_pToolFramework);
    const auto switchToTool = reinterpret_cast<SwitchToToolByNameFn>(vtable[ToolFrameworkSwitchToToolByNameSlot]);
    if (switchToTool(m_pToolFramework, GetToolName()) != this)
    {
        spdlog::error("[ParticleTools] Engine tool framework could not select '{}'", GetToolName());
        return false;
    }

    return true;
}

void CParticleToolSystem::OpenEditor()
{
    AcquireRuntimeInterfaces();
    if (!m_pEditor)
        m_pEditor = std::make_unique<CParticleEditorWorkspace>(*this);
    if (!m_pEditor->Open())
    {
        spdlog::error("[ParticleTools] Could not open the engine VGUI workspace");
        return;
    }
    SynchronizeEditorWithClientLevel();
}

bool CParticleToolSystem::OpenDocument(const std::filesystem::path& path)
{
    if (!ActivateEditorTool())
        return false;
    OpenEditor();
    return m_pEditor && m_pEditor->IsOpen() && m_pEditor->OpenDocument(path);
}

void CParticleToolSystem::PreviewDocument()
{
    if (m_pEditor)
        m_pEditor->PreviewDocument();
}
void CParticleToolSystem::CloseEditor()
{
    if (!m_pEditor)
        return;
    m_pEditor->Close();
    m_pEditor.reset();
}

CParticleToolSystem& GetParticleToolSystem()
{
    return s_ParticleToolSystem;
}

ON_DLL_LOAD_CLIENT("engine.dll", ParticleToolsFramework, [](CModule module)
{
    NOTE_UNUSED(module);
    DISPATCH_MODULE(ParticleToolsFrameworkHooks)
})

ON_DLL_LOAD_CLIENT("client.dll", ParticleToolsClientView, [](CModule module)
{
    NOTE_UNUSED(module);
    DISPATCH_HOOK(ParticleToolsFrameworkHooks, OverrideParticlePreviewCamera)
    DISPATCH_HOOK(ParticleToolsFrameworkHooks, ConstrainParticlePreviewRenderView)
    DISPATCH_HOOK(ParticleToolsFrameworkHooks, BlockParticleToolsClientKeyEvent)
    DISPATCH_HOOK(ParticleToolsFrameworkHooks, SuspendParticleToolsCreateMove)
})

static void ConCommand_ParticleToolsOpen(const CCommand& args)
{
    if (args.ArgC() > 1)
    {
        GetParticleToolSystem().OpenDocument(args.Arg(1));
        return;
    }
    if (GetParticleToolSystem().ActivateEditorTool())
        GetParticleToolSystem().OpenEditor();
}

static void ConCommand_ParticleToolsClose(const CCommand& args)
{
    NOTE_UNUSED(args);
    GetParticleToolSystem().CloseEditor();
}

static void ConCommand_ParticleToolsPreview(const CCommand& args)
{
    NOTE_UNUSED(args);
    GetParticleToolSystem().PreviewDocument();
}

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", ParticleToolsCommands, ConCommand, [](CModule module)
{
    NOTE_UNUSED(module);
    RegisterConCommand("ns_particle_tools_open", ConCommand_ParticleToolsOpen,
                       "Open the Northstar native particle editor, optionally loading a PCF path.", FCVAR_DONTRECORD);
    RegisterConCommand("ns_particle_tools_close", ConCommand_ParticleToolsClose, "Close the Northstar native particle editor.", FCVAR_DONTRECORD);
    RegisterConCommand("ns_particle_tools_preview", ConCommand_ParticleToolsPreview, "Preview the current particle document.", FCVAR_DONTRECORD);
})
} // namespace ParticleTools
