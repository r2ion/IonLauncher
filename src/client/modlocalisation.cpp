#include "modsystem/modmanager.h"

DECLARE_MODULE(ModLocalisationHooks)

// Exported for use in enginevguiconsole.cpp
void* g_pVguiLocalize;
bool(__fastcall* o_pCLocalise__AddFile)(
	void* pVguiLocalize, const char* path, const char* pathId, bool bIncludeFallbackSearchPaths) = nullptr;

DECLARE_HOOK(CLocalise__AddFile, localize.dll + 0x6D80, [](auto& hook, void* pVguiLocalize, const char* path, const char* pathId, bool bIncludeFallbackSearchPaths) -> bool
{
	// save this for later
	g_pVguiLocalize = pVguiLocalize;

	bool ret = hook.Original(pVguiLocalize, path, pathId, bIncludeFallbackSearchPaths);
	if (ret)
		spdlog::info("Loaded localisation file {} successfully", path);

	return true;
})

DECLARE_HOOK(CLocalize__ReloadLocalizationFiles, localize.dll + 0xB830, [](auto& hook, void* pVguiLocalize)
{
	// load all mod localization manually, so we keep track of all files, not just previously loaded ones
	for (Mod mod : g_pModManager->m_LoadedMods)
		if (mod.m_bEnabled)
			for (std::string& localisationFile : mod.LocalisationFiles)
				o_pCLocalise__AddFile(g_pVguiLocalize, localisationFile.c_str(), nullptr, false);

	spdlog::info("reloading localization...");
	hook.Original(pVguiLocalize);
})

// CEngineVGui::Init hook moved to engine/enginevguiconsole.cpp to consolidate with GameConsole setup

ON_DLL_LOAD_CLIENT("localize.dll", Localize, [](CModule module)
{
	DISPATCH_MODULE(ModLocalisationHooks)
	o_pCLocalise__AddFile = HookSys::GetOriginalFunction<decltype(o_pCLocalise__AddFile)>(HookSys::FindHook("CLocalise__AddFile"));
})
