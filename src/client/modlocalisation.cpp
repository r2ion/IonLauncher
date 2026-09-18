#include "core/tier1.h"
#include "engine/localize.h"
#include "localize/localize.h"
#include "modsystem/modmanager.h"

DECLARE_MODULE(ModLocalisationHooks)

ILocalize* g_pVguiLocalize = nullptr;

DECLARE_HOOK(CLocalise__AddFile, localize.dll + 0x6D80,
             [](auto& hook, CLocalize* pVguiLocalize, const char* path, const char* pathId, bool bIncludeFallbackSearchPaths) -> bool
{
    bool ret = hook.Original(pVguiLocalize, path, pathId, bIncludeFallbackSearchPaths);
    if (ret)
        spdlog::info("Loaded localisation file {} successfully", path);

    return true;
})

DECLARE_HOOK(CLocalize__ReloadLocalizationFiles, localize.dll + 0xB830, [](auto& hook, CLocalize* pVguiLocalize)
{
    // load all mod localization manually, so we keep track of all files, not just previously loaded ones
    for (const Mod& mod : g_pModManager->m_LoadedMods)
        if (mod.m_bEnabled)
            for (const std::string& localisationFile : mod.LocalisationFiles)
                g_pVguiLocalize->AddFile(localisationFile.c_str());

    spdlog::info("reloading localization...");
    hook.Original(pVguiLocalize);
})

// CEngineVGui::Init hook moved to engine/enginevguiconsole.cpp to consolidate with GameConsole setup

ON_DLL_LOAD_CLIENT("localize.dll", Localize, [](CModule module)
{
    g_pVguiLocalize = Sys_GetFactoryPtr("localize.dll", LOCALIZE_INTERFACE_VERSION).RCast<ILocalize*>();

    DISPATCH_MODULE(ModLocalisationHooks)
})
