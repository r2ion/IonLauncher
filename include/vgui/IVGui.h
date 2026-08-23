//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#pragma once

#include "appframework/IAppSystem.h"
#include "vgui.h"

#include <cstdint>

class IVEngineClient;
class KeyValues;

namespace vgui
{
using HPanel = std::uint32_t;
using HContext = std::uint32_t;

inline constexpr HContext DEFAULT_VGUI_CONTEXT = ~HContext{0};

class IVGui : public IAppSystem
{
public:
	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual bool IsRunning() = 0;
	virtual void RunFrame() = 0;
	virtual void ShutdownMessage(unsigned int shutdownId) = 0;
	virtual VPANEL AllocPanel() = 0;
	virtual void FreePanel(VPANEL panel) = 0;
	virtual void DPrintf(const char* format, ...) = 0;
	virtual void DPrintf2(const char* format, ...) = 0;
	virtual void SpewAllActivePanelNames() = 0;
	virtual HPanel PanelToHandle(VPANEL panel) = 0;
	virtual VPANEL HandleToPanel(HPanel index) = 0;
	virtual void MarkPanelForDeletion(VPANEL panel) = 0;
	virtual void AddTickSignal(VPANEL panel, int intervalMilliseconds = 0) = 0;
	virtual void RemoveTickSignal(VPANEL panel) = 0;
	virtual void PostMessage(VPANEL target, KeyValues* params, VPANEL from, float delaySeconds = 0.0f) = 0;
	virtual HContext CreateContext() = 0;
	virtual void DestroyContext(HContext context) = 0;
	virtual void AssociatePanelWithContext(HContext context, VPANEL root) = 0;
	virtual void ActivateContext(HContext context) = 0;
	virtual void SetSleep(bool state) = 0;
	virtual bool GetShouldVGuiControlSleep() = 0;
	virtual void SetVRMode(bool enabled) = 0;
	virtual bool GetVRMode() = 0;
	virtual void AddTickSignalToHead(VPANEL panel, int intervalMilliseconds = 0) = 0;
	virtual IVEngineClient* GetVGUIEngine() = 0;
	virtual void InvalidateMdlCache() = 0;
	virtual std::uint32_t GetMdlCacheSerial() = 0;
};

inline constexpr char VGUI_IVGUI_INTERFACE_VERSION[] = "VGUI_ivgui008";
} // namespace vgui
