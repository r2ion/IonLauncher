#include "surface.h"
#include "core/tier1.h"
#include "core/convar/concommand.h"
#include "core/convar/convar.h"
#include "vgui/elements/panel.h"

vgui::ISurface* g_pVGuiSurface = nullptr;

using TextureStreamMgr_GetStreamOverlayFn = void(__fastcall*)(char* output, size_t capacity, char* scratchBuffer);

TextureStreamMgr_GetStreamOverlayFn GetStreamOverlayText;



void ConCommand_dump(const CCommand& args)
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

ON_DLL_LOAD_CLIENT("client.dll", VGuiSurface, [](CModule module)
{
	g_pVGuiSurface = Sys_GetFactoryPtr("vguimatsurface.dll", "VGUI_Surface031").RCast<vgui::ISurface*>();
})



ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", StreamOverlayCommand, ConCommand, [](CModule module)
{
    RegisterConCommand("dump", ConCommand_dump, "Dumps the texture stream overlay to log.", FCVAR_CLIENTDLL | FCVAR_DONTRECORD);
})

ON_DLL_LOAD_CLIENT("materialsystem_dx11.dll", StreamOverlay, [](CModule module)
{
	GetStreamOverlayText = module.Offset(0x974D0).RCast<TextureStreamMgr_GetStreamOverlayFn>();
})
