#pragma once

#include "inputsystem/ButtonCode.h"
#include "mathlib/vector.h"
#include "toolframework/itoolentity.h"
#include "vgui/vgui.h"


class AudioState_t;
class IMaterialProxy;
class KeyValues;
struct SpatializationInfo_t;

class IToolSystem
{
public:
	virtual const char* GetToolName() = 0; // 0
	virtual bool Init() = 0; // 1
	virtual void Shutdown() = 0; // 2
	virtual bool ServerInit(CreateInterfaceFn serverFactory) = 0; // 3
	virtual bool ClientInit(CreateInterfaceFn clientFactory) = 0; // 4
	virtual void ServerShutdown() = 0; // 5
	virtual void ClientShutdown() = 0; // 6
	virtual bool CanQuit(const char* pExitMessage) = 0; // 7
	virtual void Think(bool finalTick) = 0; // 8
	virtual void ServerLevelInitPreEntity() = 0; // 9
	virtual void ServerLevelInitPostEntity() = 0; // 10
	virtual void ServerLevelShutdownPreEntity() = 0; // 11
	virtual void ServerLevelShutdownPostEntity() = 0; // 12
	virtual void ServerFrameUpdatePreEntityThink() = 0; // 13
	virtual void ServerFrameUpdatePostEntityThink() = 0; // 14
	virtual void ServerPreClientUpdate() = 0; // 15
	virtual void ServerPreSetupVisibility() = 0; // 16
	virtual const char* GetEntityData(const char* pActualEntityData) = 0; // 17
	virtual void* QueryInterface(const char* pInterfaceName) = 0; // 18
	virtual void ClientLevelInitPreEntity() = 0; // 19
	virtual void ClientLevelInitPostEntity() = 0; // 20
	virtual void ClientLevelShutdownPreEntity() = 0; // 21
	virtual void ClientLevelShutdownPostEntity() = 0; // 22
	virtual void ClientPreRender() = 0; // 23
	virtual void ClientPostRender() = 0; // 24
	virtual void AdjustEngineViewport(int& x, int& y, int& width, int& height) = 0; // 25
	virtual bool SetupEngineView(Vector& origin, QAngle& angles, float& fov) = 0; // 26
	virtual bool ShouldGameRenderView() = 0; // 27
	virtual bool IsThirdPersonCamera() = 0; // 28
	virtual bool ShouldGamePlaySounds() = 0; // 29
	virtual IMaterialProxy* LookupProxy(const char* pProxyName) = 0; // 30
	virtual void OnToolActivate() = 0; // 31
	virtual void OnToolDeactivate() = 0; // 32
	virtual bool GetSoundSpatialization(int userData, int guid, SpatializationInfo_t& info) = 0; // 33
	virtual void RenderFrameBegin() = 0; // 34
	virtual void RenderFrameEnd() = 0; // 35
	virtual void HostRunFrameBegin() = 0; // 36
	virtual void HostRunFrameEnd() = 0; // 37
	virtual void VGui_PreRender(int paintMode) = 0; // 38
	virtual void VGui_PostRender(int paintMode) = 0; // 39
	virtual void VGui_PreSimulate() = 0; // 40
	virtual void VGui_PostSimulate() = 0; // 41
	virtual vgui::VPANEL GetClientWorkspaceArea() = 0; // 42
	virtual bool ShouldAllowGameUI() = 0; // 43
};

static_assert(sizeof(IToolSystem) == sizeof(void*));
