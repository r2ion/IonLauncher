#include "vgui/vguiscreens.h"

#include <cstdint>

DECLARE_MODULE(VGuiScreensHooks)

namespace
{
constexpr const char* VGUI_SCREENS_FILE = "scripts/vgui_screens.txt";

// CPanelMetaClassMgrImp singleton accessor (client.dll + 0x2745D0).
using GetPanelMetaClassMgr_t = void* (*)();
// vtable slot 0: parses a metaclass definition file and registers its screens.
// Re-loading a file already registered unregisters its previous screens first.
// Return type is not recovered from the binary (the only caller discards it), so rax is passed through untouched.
using LoadMetaClassDefinitionFile_t = std::uintptr_t (*)(void* thisptr, const char* fileName);

GetPanelMetaClassMgr_t GetPanelMetaClassMgr = nullptr;
bool g_bVGuiScreensLoaded = false;
} // namespace

// CPanelMetaClassMgrImp::LoadMetaClassDefinitionFile
DECLARE_HOOK(PanelMetaClassMgr_LoadDefinitionFile, client.dll + 0x273AE0, [](auto& hook, void* thisptr, const char* fileName) -> std::uintptr_t
{
	const std::uintptr_t result = hook.Original(thisptr, fileName);

	// the game's own boot-time load; from now on mods can re-parse the file
	if (fileName && !_stricmp(fileName, VGUI_SCREENS_FILE))
		g_bVGuiScreensLoaded = true;
	return result;
})

void ReloadVGuiScreens()
{
	// client.dll not loaded (dedicated), or the game hasn't done its initial load yet,
	// in which case it will pick up the mod patches on its own
	if (!GetPanelMetaClassMgr || !g_bVGuiScreensLoaded)
		return;

	void* manager = GetPanelMetaClassMgr();
	if (!manager)
		return;

	spdlog::info("Reloading {}...", VGUI_SCREENS_FILE);
	(*reinterpret_cast<LoadMetaClassDefinitionFile_t**>(manager))[0](manager, VGUI_SCREENS_FILE);
}

ON_DLL_LOAD_CLIENT("client.dll", VGuiScreens, [](CModule module)
{
	GetPanelMetaClassMgr = module.Offset(0x2745D0).RCast<GetPanelMetaClassMgr_t>();
	g_bVGuiScreensLoaded = false;
	DISPATCH_MODULE(VGuiScreensHooks)
})
