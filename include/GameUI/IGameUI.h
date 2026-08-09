#pragma once

#include "interface.h"

#include <cstdint>

namespace vgui
{
using VPANEL = std::uintptr_t;
}

inline constexpr char GAMEUI_INTERFACE_VERSION[] = "GameUI011";

class IGameUI
{
public:
	virtual ~IGameUI() = default;
	virtual void Initialize(CreateInterfaceFn appFactory) = 0;
	virtual void PostInit() = 0;
	virtual void Connect(CreateInterfaceFn gameFactory) = 0;
	virtual void Start() = 0;
	virtual void Shutdown() = 0;
	virtual void RunFrame() = 0;
	virtual void OnGameUIActivated() = 0;
	virtual void OnGameUIHidden() = 0;
	virtual void OnDisconnectFromServerLegacy(std::uint8_t loginFailure, const char* pUserName) = 0;
	virtual void OnLevelLoadingStarted(const char* pLevelName, bool showProgressDialog) = 0;
	virtual void OnLevelLoadingFinished(bool error, const char* pFailureReason, const char* pExtendedReason) = 0;
	virtual bool UpdateProgressBar(float progress, const char* pStatusText) = 0;
	virtual void SetProgressLevelName(const char* pLevelName, const char* pGameMode) = 0;
	virtual void RefreshLoadingProgress() = 0;
	virtual void SetLoadingProgressActive(bool value) = 0;
	virtual void SetLoadingProgressType(int progressType) = 0;
	virtual void SetPrimaryLoadingProgressText(const char* pFirst, const char* pSecond) = 0;
	virtual void ClearPrimaryLoadingProgressText() = 0;
	virtual void SetSecondaryLoadingProgressText(const char* pFirst, const char* pSecond, bool emphasized) = 0;
	virtual void ClearSecondaryLoadingProgressText() = 0;
	virtual void ResetLoadingProgress() = 0;
	virtual void LoadingProgressNoOp() = 0;
	virtual bool IsLevelLoading() = 0;
	virtual void SetLoadingBackgroundDialog(vgui::VPANEL panel) = 0;
	virtual void SetProgressOnStart() = 0;
	virtual void OnDisconnectFromServer(std::uint8_t loginFailure) = 0;
	virtual bool SetShowProgressText(bool show) = 0;
	virtual bool IsMainMenuVisible() = 0;
	virtual void SetMainMenuOverride(vgui::VPANEL panel) = 0;
	virtual void SendMainMenuCommand(const char* pCommand) = 0;
	virtual bool IsLoadingProgressActive() = 0;
	virtual bool HasGenericConfirmation() = 0;
};

static_assert(sizeof(IGameUI) == sizeof(void*));
