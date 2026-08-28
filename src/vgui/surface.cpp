#include "surface.h"
#include "core/convar/concommand.h"
#include "core/tier0.h"
#include "core/tier1.h"
#include "engine/localize.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"
#include "tier1/strtools.h"
#include "vgui/IPanel.h"
#include "vscript/languages/squirrel_re/squirrel.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>

vgui::ISurface* vgui::g_pVGuiSurface = nullptr;

using PaintTraverseFn = void(*)(vgui::IPanel*, vgui::VPANEL, bool, bool);
using TextureStreamMgr_GetStreamOverlayFn = void(*)(char* output, size_t capacity, char* scratchBuffer);




constexpr std::size_t s_PaintTraverseVTableIndex = 46;
constexpr double s_NotificationLifetime = 10.0;
constexpr double s_NotificationFadeTime = 1.0;
constexpr double s_NotificationFlashTime = 0.35;
constexpr int s_NotificationX = 32;
constexpr int s_NotificationY = 32;
constexpr int s_NotificationHeight = 36;
constexpr int s_NotificationSpacing = 44;

vgui::IPanel* s_pPanel;
PaintTraverseFn s_PaintTraverse;
ConVar* s_pScriptErrorNotifications;
vgui::HFont s_ScriptErrorFont;
TextureStreamMgr_GetStreamOverlayFn GetStreamOverlayText;

bool EnsureScriptErrorFont()
{
	if (s_ScriptErrorFont)
		return true;

	s_ScriptErrorFont = vgui::g_pVGuiSurface->CreateFont();
	if (!s_ScriptErrorFont ||
		!vgui::g_pVGuiSurface->SetFontGlyphSet(
			s_ScriptErrorFont, "Tahoma", 14, 600, 0, 0, vgui::FONTFLAG_ANTIALIAS))
	{
		spdlog::error("Failed to create the script error notification font");
		return false;
	}

	return true;
}
bool DrawScriptErrorNotification(ScriptContext context, int y, double now)
{
	const char* contextName;
	switch (context)
	{
	case ScriptContext::SERVER:
		contextName = "SERVER";
		break;
	case ScriptContext::CLIENT:
		contextName = "CLIENT";
		break;
	case ScriptContext::UI:
		contextName = "UI";
		break;
	default:
		assert(false);
		return false;
	}

	const double errorTime = g_LastSQErrorTimes[static_cast<int>(context)];
	const double age = now - errorTime;
	if (errorTime <= 0.0 || age < 0.0 || age >= s_NotificationLifetime)
		return false;

	if (!EnsureScriptErrorFont())
		return false;

	const std::string localizedMessage =
		Localize("#SCRIPT_ERROR_NOTIFICATION", contextName, static_cast<const char*>(nullptr));
	wchar_t message[256];
	const int messageLength = V_UTF8ToUnicode(localizedMessage.c_str(), message, sizeof(message));
	if (messageLength <= 1)
		return false;

	const double remaining = s_NotificationLifetime - age;
	const double fade = std::clamp(remaining / s_NotificationFadeTime, 0.0, 1.0);
	const int opacity = static_cast<int>(std::lround(255.0 * fade));
	const auto ScaleAlpha = [opacity](int alpha) { return alpha * opacity / 255; };

	int textWidth = 0;
	int textHeight = 0;
	vgui::g_pVGuiSurface->GetTextSize(s_ScriptErrorFont, message, textWidth, textHeight);

	const int width = textWidth + 56;
	const int right = s_NotificationX + width;
	const int bottom = y + s_NotificationHeight;

	vgui::g_pVGuiSurface->DrawSetColor(0, 0, 0, ScaleAlpha(150));
	vgui::g_pVGuiSurface->DrawFilledRect(s_NotificationX + 2, y + 2, right + 2, bottom + 2);

	vgui::g_pVGuiSurface->DrawSetColor(32, 34, 38, ScaleAlpha(242));
	vgui::g_pVGuiSurface->DrawFilledRect(s_NotificationX, y, right, bottom);

	if (age < s_NotificationFlashTime)
	{
		const double flash = 1.0 - age / s_NotificationFlashTime;
		vgui::g_pVGuiSurface->DrawSetColor(
			255, 188, 32, ScaleAlpha(static_cast<int>(std::lround(95.0 * flash))));
		vgui::g_pVGuiSurface->DrawFilledRect(s_NotificationX, y, right, bottom);
	}

	vgui::g_pVGuiSurface->DrawSetColor(238, 172, 28, ScaleAlpha(255));
	vgui::g_pVGuiSurface->DrawFilledRect(s_NotificationX, y, s_NotificationX + 4, bottom);

	vgui::g_pVGuiSurface->DrawSetColor(82, 84, 90, ScaleAlpha(255));
	vgui::g_pVGuiSurface->DrawOutlinedRect(s_NotificationX, y, right, bottom);

	vgui::g_pVGuiSurface->DrawSetTextFont(s_ScriptErrorFont);
	vgui::g_pVGuiSurface->DrawSetTextColor(255, 194, 38, ScaleAlpha(255));
	vgui::g_pVGuiSurface->DrawSetTextPos(s_NotificationX + 17, y + (s_NotificationHeight - textHeight) / 2);
	vgui::g_pVGuiSurface->DrawPrintText(L"!", 1);

	vgui::g_pVGuiSurface->DrawSetTextColor(235, 236, 238, ScaleAlpha(255));
	vgui::g_pVGuiSurface->DrawSetTextPos(s_NotificationX + 42, y + (s_NotificationHeight - textHeight) / 2);
	vgui::g_pVGuiSurface->DrawPrintText(message, messageLength - 1);

	return true;
}

void DrawScriptErrorNotifications()
{
	if (!s_pScriptErrorNotifications || !s_pScriptErrorNotifications->GetBool() || !g_PlatFloatTime)
		return;

	const double now = g_PlatFloatTime();
	int y = s_NotificationY;

	if (DrawScriptErrorNotification(ScriptContext::SERVER, y, now))
		y += s_NotificationSpacing;
	if (DrawScriptErrorNotification(ScriptContext::CLIENT, y, now))
		y += s_NotificationSpacing;
	DrawScriptErrorNotification(ScriptContext::UI, y, now);
}

bool IsNotificationPanel(const char* pName)
{
	return pName &&
		(std::strcmp(pName, "MatSystemTopPanel") == 0 || std::strcmp(pName, "CBaseModPanel") == 0);
}

void PaintTraverse(
	vgui::IPanel* pPanel, vgui::VPANEL paintPanel, bool forceRepaint, bool allowForce)
{
	const bool drawNotifications = IsNotificationPanel(pPanel->GetName(paintPanel));
	s_PaintTraverse(pPanel, paintPanel, forceRepaint, allowForce);

	if (drawNotifications)
		DrawScriptErrorNotifications();
}

static void ConCommand_dump(const CCommand& args)
{
	if (!GetStreamOverlayText)
	{
		spdlog::warn("TextureStreamMgr_GetStreamOverlay is not available");
		return;
	}

	char text[65000]{};
    char scratch[32768]{};
    text[0] = '\0';
    text[std::size(text) - 1] = '\0';
    scratch[0] = '\0';
    GetStreamOverlayText(text, std::size(text), scratch);

	spdlog::info("{}", text);
}

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", VGuiSurface, ConVar, [](CModule module)
{
	NOTE_UNUSED(module);

	vgui::g_pVGuiSurface =
		Sys_GetFactoryPtr("vguimatsurface.dll", "VGUI_Surface031").RCast<vgui::ISurface*>();
	s_pPanel = Sys_GetFactoryPtr("vgui2.dll", vgui::VGUI_PANEL_INTERFACE_VERSION).RCast<vgui::IPanel*>();
	if (!vgui::g_pVGuiSurface || !s_pPanel)
	{
		spdlog::error("VGUI_Surface031 or VGUI_Panel009 is unavailable");
		return;
	}

	s_pScriptErrorNotifications = new ConVar(
		"ns_script_error_notifications",
		"1",
		FCVAR_CLIENTDLL | FCVAR_ARCHIVE_PLAYERPROFILE,
		"Show a notification when a Squirrel script error occurs.");

	void** const pPanelVTable = *reinterpret_cast<void***>(s_pPanel);
	assert(pPanelVTable && pPanelVTable[s_PaintTraverseVTableIndex]);
	if (!pPanelVTable || !pPanelVTable[s_PaintTraverseVTableIndex])
	{
		spdlog::error("VGUI_Panel009 PaintTraverse is unavailable");
		return;
	}

	MAKEHOOK(pPanelVTable[s_PaintTraverseVTableIndex], &PaintTraverse, &s_PaintTraverse);
})

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", StreamOverlayCommand, ConCommand, [](CModule module)
{
    RegisterConCommand("dump", ConCommand_dump, "Dumps the texture stream overlay to log.", FCVAR_CLIENTDLL | FCVAR_DONTRECORD);
})

ON_DLL_LOAD_CLIENT("materialsystem_dx11.dll", StreamOverlay, [](CModule module)
{
	GetStreamOverlayText = module.Offset(0x974D0).RCast<TextureStreamMgr_GetStreamOverlayFn>();
})
