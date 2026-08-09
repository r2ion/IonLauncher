#pragma once

#include "engine/createinterface.h"
#include "inputsystem/InputEnums.h"

#include <cstdint>

inline constexpr char VENGINE_VGUI_INTERFACE_VERSION[] = "VEngineVGui001";
enum VGuiPanel_t
{
	PANEL_ROOT = 0,
	PANEL_GAMEUIDLL,
	PANEL_CLIENTDLL,
	PANEL_TOOLS,
	PANEL_INGAMESCREENS,
	PANEL_GAMEDLL,
	PANEL_CLIENTDLL_TOOLS,
	PANEL_GAMEUIBACKGROUND,
	PANEL_TRANSITIONEFFECT,
	PANEL_STEAMOVERLAY
};

enum PaintMode_t : std::uint32_t
{
	PAINT_UIPANELS = 1U << 0,
	PAINT_INGAMEPANELS = 1U << 1
};

enum class LevelLoadingProgress_t : int;

class IEngineVGui
{
public:
	virtual ~IEngineVGui() = default; // 0
	virtual std::uintptr_t GetPanel(VGuiPanel_t panelType) = 0; // 1
	virtual bool IsGameUIVisible() = 0; // 2
	virtual bool IsConsoleVisible() = 0; // 3
	virtual bool IsGameUILoadingProgressActive() = 0; // 4
	virtual void ActivateGameUI() = 0; // 5
	virtual bool HideGameUI() = 0; // 6
	virtual void Simulate() = 0; // 7
	virtual bool IsNotAllowedToHideGameUI() = 0; // 8
	virtual void SetRuiFuncs(void* pRuiFuncs) = 0; // 9
	virtual void UpdateLevelLoadingProgress(LevelLoadingProgress_t progress) = 0; // 10
	virtual void BeginProgressBarUpdateBlock() = 0; // 11
	virtual void EndProgressBarUpdateBlock() = 0; // 12
	virtual int Init() = 0; // 13
	virtual void Connect() = 0; // 14
	virtual void Shutdown() = 0; // 15
	virtual bool SetVGUIDirectories() = 0; // 16
	virtual bool IsInitialized() const = 0; // 17
	virtual InterfaceFactoryFn GetGameUIFactory() = 0; // 18
	virtual bool KeyEvent(const InputEvent_t& event) = 0; // 19
	virtual bool StartFixedEvent(const InputEvent_t& event) = 0; // 20
	virtual bool StartMappedEvent(const InputEvent_t& event) = 0; // 21
	virtual void PostInit() = 0; // 22
	virtual void Paint(PaintMode_t mode) = 0; // 23
	virtual void ShowConsole() = 0; // 24
	virtual void HideConsole() = 0; // 25
	virtual void ClearConsole() = 0; // 26
	virtual void OnLevelLoadingStarted(const char* pLevelName, bool localServer) = 0; // 27
	virtual void OnLevelLoadingFinished() = 0; // 28
	virtual void EnableProgressBarForNextLoad() = 0; // 29
	virtual void UpdateProgressBar(LevelLoadingProgress_t progress, const char* pStatusTextToken) = 0; // 30
	virtual void StartCustomProgress() = 0; // 31
	virtual void FinishCustomProgress() = 0; // 32
	virtual void ShowErrorMessage() = 0; // 33
	virtual void HideLoadingPlaque() = 0; // 34
	virtual bool ShouldPause() = 0; // 35
	virtual void SetGameDLLPanelsVisible(bool visible) = 0; // 36
	virtual bool SetShowProgressText(bool show) = 0; // 37
	virtual void ShowNewGameDialog(int chapter) = 0; // 38
	virtual void SessionNotification(int notification, int parameter) = 0; // 39
	virtual void SystemNotification(int notification) = 0; // 40
	virtual void ShowMessageDialog(std::uint32_t type, void* pOwnerPanel) = 0; // 41
	virtual void UpdatePlayerInfo(std::uint64_t playerId, const char* pName, int team,
		std::uint8_t voiceState, int playersNeeded, bool host) = 0; // 42
	virtual void SessionSearchResult(int searchIndex, void* pHostData, void* pResult, int ping) = 0; // 43
	virtual void OnCreditsFinished() = 0; // 44
	virtual void BonusMapUnlock(const char* pFileName, const char* pMapName) = 0; // 45
	virtual void SetLoadingBackgroundDialog(std::uintptr_t panel) = 0; // 46
	virtual void SetNotAllowedToHideGameUI(bool notAllowed) = 0; // 47
	virtual void SetNotAllowedToShowGameUI(bool notAllowed) = 0; // 48
	virtual void SetEngineToolsPanelVisible(bool visible) = 0; // 49
	virtual std::uintptr_t GetInputContext() = 0; // 50
	virtual int BonusMapNumAdvancedCompleted() = 0; // 51
	virtual bool BonusMapNumMedals(int* pNumMedals) = 0; // 52
	virtual bool OnConnectToServer2(const char* pGame, int ip, int connectionPort,
		int queryPort) = 0; // 53
	virtual bool ValidateStorageDevice(int* pStorageDeviceValidated) = 0; // 54
	virtual bool SetProgressOnStart() = 0; // 55
	virtual void NullSub56() = 0; // 56
};

static_assert(sizeof(IEngineVGui) == sizeof(void*));

