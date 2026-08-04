#include "surface.h"
#include "core/tier1.h"
#include "core/convar/concommand.h"
#include "core/convar/convar.h"
#include "vgui/elements/panel.h"

vgui::ISurface* g_pVGuiSurface = nullptr;
vgui::Panel* g_pVGuiPanel = nullptr;
namespace
{
constexpr size_t STREAM_OVERLAY_TEXT_CAPACITY = 4096 * 16;
constexpr int STREAM_OVERLAY_DRAW_CHUNK_LENGTH = 2048;

using TextureStreamMgr_GetStreamOverlayFn = void(__fastcall*)(char* output, size_t capacity, char* scratchBuffer);

TextureStreamMgr_GetStreamOverlayFn s_TextureStreamMgr_GetStreamOverlay;
char s_StreamOverlayText[STREAM_OVERLAY_TEXT_CAPACITY]{};
char s_StreamOverlayScratch[4096*8]{};

bool UpdateStreamOverlayText()
{
	s_StreamOverlayText[0] = '\0';
	s_StreamOverlayText[std::size(s_StreamOverlayText) - 1] = '\0';
	s_StreamOverlayScratch[0] = '\0';
	s_TextureStreamMgr_GetStreamOverlay(s_StreamOverlayText, std::size(s_StreamOverlayText),
		s_StreamOverlayScratch);

	// Do not trust the engine function to terminate output that fills the buffer.
	s_StreamOverlayText[std::size(s_StreamOverlayText) - 1] = '\0';
	return s_StreamOverlayText[0] != '\0';
}





void ConCommand_dump(const CCommand& args)
{
	if (!s_TextureStreamMgr_GetStreamOverlay)
	{
		spdlog::warn("TextureStreamMgr_GetStreamOverlay is not available");
		return;
	}

	if (!UpdateStreamOverlayText())
	{
		spdlog::info("Texture stream overlay returned no data");
		return;
	}

	spdlog::info("{}", s_StreamOverlayText);
}
}

ON_DLL_LOAD_CLIENT("client.dll", VGuiSurface, [](CModule module)
{
	g_pVGuiSurface = Sys_GetFactoryPtr("vguimatsurface.dll", "VGUI_Surface031").RCast<vgui::ISurface*>();
})



ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", StreamOverlayCommand, ConCommand, [](CModule module)
{
    RegisterConCommand("dump", ConCommand_dump, "Dumps the texture stream overlay to the log.", FCVAR_CLIENTDLL | FCVAR_DONTRECORD);
})

ON_DLL_LOAD_CLIENT("materialsystem_dx11.dll", StreamOverlay, [](CModule module)
{
	s_TextureStreamMgr_GetStreamOverlay = module.Offset(0x974D0).RCast<TextureStreamMgr_GetStreamOverlayFn>();
})
