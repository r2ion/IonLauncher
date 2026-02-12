#include "core/tier0.h"
#include "tier0/module.h"
#include "logging/logging.h"
#include "logging/sourceconsole.h"
#include "modsystem/modmanager.h"

DECLARE_MODULE(EngineVGuiConsoleHooks)

static CGameConsole** g_pEngineGameConsole = nullptr;

// External declarations for localization hooks
extern void* g_pVguiLocalize;
extern bool(__fastcall* o_pCLocalise__AddFile)(void* pVguiLocalize, const char* path, const char* pathId, bool bIncludeFallbackSearchPaths);

// CEngineVGui::Init hook at engine.dll + 0x0247E10
// Consolidated hook that handles both GameConsole setup AND mod localization loading
DECLARE_HOOK(CEngineVGui__Init, engine.dll + 0x247E10, [](auto& hook, void* thisptr)
{
    // Get GameConsole004 interface from client.dll BEFORE calling Init
    CModule clientModule("client.dll");

    typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);
    CreateInterfaceFn clientCreateInterface = clientModule.GetExportedFunction("CreateInterface").RCast<CreateInterfaceFn>();

    if (clientCreateInterface)
    {
        int returnCode = 0;
        CGameConsole* gameConsole = reinterpret_cast<CGameConsole*>(clientCreateInterface("GameConsole004", &returnCode));

        if (gameConsole && returnCode == 0)
        {
            // Set the global pointer BEFORE calling Init
            *g_pEngineGameConsole = gameConsole;
            spdlog::info("GameConsole004 interface set at engine.dll + 0x14055B88: {:p}", (void*)gameConsole);
        }
        else
        {

            spdlog::warn("Failed to get GameConsole004 interface from client.dll (return code: {})", returnCode);
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
    if (g_pVguiLocalize && o_pCLocalise__AddFile)
    {
        for (Mod mod : g_pModManager->m_LoadedMods)
        {
            if (mod.m_bEnabled)
            {
                for (std::string& localisationFile : mod.LocalisationFiles)
                {
                    o_pCLocalise__AddFile(g_pVguiLocalize, localisationFile.c_str(), nullptr, false);
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
