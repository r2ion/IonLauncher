#include "surface.h"
#include "core/tier1.h"
#include "vgui/elements/panel.h"

vgui::ISurface* g_pVGuiSurface = nullptr;

void (*oPaintTraverse)(uintptr_t thisptr, vgui::Panel* panel, bool forceRepaint, bool allowForce);
void PaintTraverse(uintptr_t thisptr, vgui::Panel* paintPanel, bool forceRepaint, bool allowForce) {
	static vgui::Panel* inGameRenderPanel = nullptr;
	static vgui::Panel* menuRenderPanel = nullptr;

	oPaintTraverse(thisptr, paintPanel, forceRepaint, allowForce);

	if (!inGameRenderPanel) {
		if (!strcmp(paintPanel->GetName(), "MatSystemTopPanel")) {
            inGameRenderPanel = paintPanel;
		}
	}
    if (!menuRenderPanel) {
        if (!strcmp(paintPanel->GetName(), "CBaseModPanel")) {
            menuRenderPanel = paintPanel;
        }
    }

    // if (paintPanel == inGameRenderPanel)
    // {
    //     DrawWatermark();
    //     DrawDamageNumbers();
    // }

    // if (paintPanel == inGameRenderPanel || paintPanel == menuRenderPanel)
	// 	DrawScriptErrors();
}

ON_DLL_LOAD_CLIENT("client.dll", VGuiSurface, (CModule module))
{
	g_pVGuiSurface = Sys_GetFactoryPtr("vguimatsurface.dll", "VGUI_Surface031").RCast<vgui::ISurface*>();
}
