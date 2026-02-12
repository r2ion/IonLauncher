#include "modsystem/modmanager.h"

DECLARE_MODULE(ClientVideoOverridesHooks)

DECLARE_HOOK_PROC(BinkOpen, bink2w64.dll, BinkOpen, [](auto& hook, const char* path, uint32_t flags) -> void*
{
	std::string filename(fs::path(path).filename().string());
	spdlog::info("BinkOpen {}", filename);

	// figure out which mod is handling the bink
	Mod* fileOwner = nullptr;
	for (Mod& mod : g_pModManager->m_LoadedMods)
	{
		if (!mod.m_bEnabled)
			continue;

		if (std::find(mod.BinkVideos.begin(), mod.BinkVideos.end(), filename) != mod.BinkVideos.end())
			fileOwner = &mod;
	}

	if (fileOwner)
	{
		// create new path
		fs::path binkPath(fileOwner->m_ModDirectory / "media" / filename);
		return hook.Original(binkPath.string().c_str(), flags);
	}

	return hook.Original(path, flags);
})

ON_DLL_LOAD_CLIENT("bink2w64.dll", BinkRead, [](CModule module)
{
	DISPATCH_MODULE(ClientVideoOverridesHooks)
	module.Offset(0x035BD7).NoOP(5);
})

ON_DLL_LOAD_CLIENT("engine.dll", BinkVideo, [](CModule module)
{
	// remove engine check for whether the bik we're trying to load exists in r2/media, as this will fail for biks in mods
	// note: the check in engine is actually unnecessary, so it's just useless in practice and we lose nothing by removing it
	module.Offset(0x459AD).NoOP(6);
})
