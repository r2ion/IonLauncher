#include "core/tier0.h"
#include "engine/localize.h"
#include "logging/logging.h"
#include "logging/sourceconsole.h"
#include "modsystem/modmanager.h"
#include "tier0/module.h"

DECLARE_MODULE(EngineVGuiConsoleHooks)

static CGameConsole** g_pEngineGameConsole = nullptr;

// CEngineVGui::Init hook at engine.dll + 0x0247E10
// Consolidated hook that handles both GameConsole setup AND mod localization loading
DECLARE_HOOK(CEngineVGui__Init, engine.dll + 0x247E10, [](auto& hook, void* thisptr)
{
    // Get the game console interface from client.dll BEFORE calling Init
    CModule clientModule("client.dll");

    typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);
    CreateInterfaceFn clientCreateInterface = clientModule.GetExportedFunction("CreateInterface").RCast<CreateInterfaceFn>();

    if (clientCreateInterface)
    {
        int returnCode = 0;
        CGameConsole* gameConsole = static_cast<CGameConsole*>(clientCreateInterface(GAMECONSOLE_INTERFACE_VERSION, &returnCode));

        if (gameConsole && returnCode == 0)
        {
            // Set the global pointer BEFORE calling Init
            *g_pEngineGameConsole = gameConsole;
            spdlog::info("{} interface set at engine.dll + 0x14055B88: {:p}", GAMECONSOLE_INTERFACE_VERSION, static_cast<void*>(gameConsole));
        }
        else
        {

            spdlog::warn("Failed to get {} interface from client.dll (return code: {})", GAMECONSOLE_INTERFACE_VERSION, returnCode);
        }
    }
    else
    {
        spdlog::error("Failed to get CreateInterface from client.dll");
    }

    // Call the original Init function
    // This loads r1_english, valve_english, dev_english
    hook.Original(thisptr);

    // AFTER Init: Load mod localization files
    // Previously this was in modlocalisation.cpp h_CEngineVGui__Init
    if (g_pVguiLocalize)
    {
        for (const Mod& mod : g_pModManager->m_LoadedMods)
        {
            if (mod.m_bEnabled)
            {
                for (const std::string& localisationFile : mod.LocalisationFiles)
                {
                    g_pVguiLocalize->AddFile(localisationFile.c_str());
                }
            }
        }
    }
})

ON_DLL_LOAD("engine.dll", EngineVGuiConsole, [](CModule module)
{
    g_pEngineGameConsole = module.Offset(0x14055B88).RCast<CGameConsole**>();
    DISPATCH_MODULE(EngineVGuiConsoleHooks)
})
