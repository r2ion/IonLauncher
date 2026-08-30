#pragma once

#include "particleeditorwindow.h"
#include "toolframework/itoolsystem.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
class CViewRender;
struct CViewRenderView;
namespace vgui
{
class IInput;
}


namespace ParticleTools
{
class CParticleToolSystem final : public IToolSystem
{
  public:
    CParticleToolSystem();
    ~CParticleToolSystem();

    const char* GetToolName() override;
    bool Init() override;
    void Shutdown() override;
    bool ServerInit(CreateInterfaceFn serverFactory) override;
    bool ClientInit(CreateInterfaceFn clientFactory) override;
    void ServerShutdown() override;
    void ClientShutdown() override;
    bool CanQuit(const char* pExitMessage) override;
    void Think(bool finalTick) override;
    void ServerLevelInitPreEntity() override;
    void ServerLevelInitPostEntity() override;
    void ServerLevelShutdownPreEntity() override;
    void ServerLevelShutdownPostEntity() override;
    void ServerFrameUpdatePreEntityThink() override;
    void ServerFrameUpdatePostEntityThink() override;
    void ServerPreClientUpdate() override;
    void ServerPreSetupVisibility() override;
    const char* GetEntityData(const char* pActualEntityData) override;
    void* QueryInterface(const char* pInterfaceName) override;
    void ClientLevelInitPreEntity() override;
    void ClientLevelInitPostEntity() override;
    void ClientLevelShutdownPreEntity() override;
    void ClientLevelShutdownPostEntity() override;
    void ClientPreRender() override;
    void ClientPostRender() override;
    void AdjustEngineViewport(int& x, int& y, int& width, int& height) override;
    bool SetupEngineView(Vector& origin, QAngle& angles, float& fov) override;
    bool ShouldGameRenderView() override;
    bool IsThirdPersonCamera() override;
    bool ShouldGamePlaySounds() override;
    IMaterialProxy* LookupProxy(const char* pProxyName) override;
    void OnToolActivate() override;
    void OnToolDeactivate() override;
    bool GetSoundSpatialization(int userData, int guid, SpatializationInfo_t& info) override;
    void RenderFrameBegin() override;
    void RenderFrameEnd() override;
    void HostRunFrameBegin() override;
    void HostRunFrameEnd() override;
    void VGui_PreRender(int paintMode) override;
    void VGui_PostRender(int paintMode) override;
    void VGui_PreSimulate() override;
    void VGui_PostSimulate() override;
    vgui::VPANEL GetClientWorkspaceArea() override;
    bool ShouldAllowGameUI() override;

    void RequestPreview(const ParticleDocument& document, const std::filesystem::path& assetPath, const std::string& effectName,
                        const std::vector<ParticlePreviewControlPoint>& controlPoints);
    void RequestStopPreview();
    void SetEditorInputEnabled(bool enabled);
    void SetEditorMouseCapture(bool enabled);
    void AdjustPreviewCamera(float yawDelta, float pitchDelta, float zoomSteps);
    void PanPreviewCamera(float mouseDeltaX, float mouseDeltaY);
    bool IsEditorInputEnabled() const;

    void OpenEditor();
    bool OpenDocument(const std::filesystem::path& path);
    void CloseEditor();
    void PreviewDocument();
    bool LoadIntoEngineToolFramework(void* framework, bool explicitlyRequested);
    bool ActivateEditorTool();
    bool ConstrainPreviewRenderView(CViewRender* pViewRender, const CViewRenderView* pRenderView);
    bool PreparePreviewView(const CViewSetup& sourceView, CViewRenderView* pRenderView, CViewSetup& previewView);
    void FinalizePreviewView(CViewRenderView* pRenderView);

  private:

    void RunPreview(std::shared_ptr<const std::vector<std::byte>> pPcfData, std::filesystem::path sourcePath, std::string effectName,
                    std::string previewEffectName, std::vector<ParticlePreviewControlPoint> controlPoints, std::uint64_t generation);
    void RunStopPreview(std::uint64_t generation);
    std::filesystem::path GetPreviewPath() const;
    void AcquireInputInterfaces();
    void AcquireRuntimeInterfaces();
    bool IsClientLevelActive() const;
    void SetClientLevelActive(bool active);
    void SynchronizeEditorWithClientLevel();
    void UpdateMapDefaultCamera();
    bool FindMapDefaultCamera(Vector& origin, QAngle& angles, Vector& focus) const;
    void CenterPreviewCamera();

    std::unique_ptr<CParticleEditorWorkspace> m_pEditor;
    IClientTools* m_pClientTools = nullptr;
    IServerTools* m_pServerTools = nullptr;
    void* m_pEngineTool = nullptr;
    vgui::IInput* m_pInputInternal = nullptr;
    void* m_pToolFramework = nullptr;
    std::atomic<bool> m_ToolsInputEnabled = false;
    bool m_PreviewCameraValid = false;
    bool m_ClientLevelActive = false;
    bool m_MapCameraLookupAttempted = false;
    bool m_MapCameraValid = false;
    Vector m_MapCameraOrigin;
    QAngle m_MapCameraAngles;
    Vector m_MapCameraFocus;
    Vector m_PreviewCameraOrigin;
    QAngle m_PreviewCameraAngles;
    Vector m_PreviewFocus;
    float m_PreviewCameraFov = 75.0f;
    float m_PreviewCameraDistance = 0.0f;
    std::atomic<std::uint64_t> m_PreviewGeneration = 0;
};

CParticleToolSystem& GetParticleToolSystem();
} // namespace ParticleTools
