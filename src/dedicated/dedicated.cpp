#include "dedicated.h"
#include "dedicatedlogtoclient.h"
#include "core/tier0.h"
#include "shared/playlist.h"
#include "engine/r2engine.h"
#include "engine/hoststate.h"
#include "server/auth/serverauthentication.h"
#include "masterserver/masterserver.h"
#include "util/printcommands.h"
#include "util/utils.h"
#include "tier1/cvar.h"

DECLARE_MODULE(DedicatedHooks)

bool IsDedicatedServer()
{
	static bool result = strstr(GetCommandLineA(), "-dedicated") || GetCurrentProcessExeName() == L"r2ds.exe";
	return result;
}

bool CDedicatedExports::Connect(CreateInterfaceFn)
{
	return true;
}

void CDedicatedExports::Disconnect()
{
}

void* CDedicatedExports::QueryInterface(const char*)
{
	return nullptr;
}

InitReturnVal_t CDedicatedExports::Init()
{
	return INIT_OK;
}

void CDedicatedExports::Shutdown()
{
}

const AppSystemInfo_t* CDedicatedExports::GetDependencies()
{
	return nullptr;
}

AppSystemTier_t CDedicatedExports::GetTier()
{
	return APP_SYSTEM_TIER_OTHER;
}

void CDedicatedExports::Reconnect(CreateInterfaceFn, const char*)
{
}

void CDedicatedExports::Sys_Printf(const char* msg)
{
	spdlog::info("[DEDICATED SERVER] {}", msg);
}

void CDedicatedExports::RunServer()
{
	spdlog::info("CDedicatedExports::RunServer(): starting");
	spdlog::info(CommandLine()->GetCmdLine());

	// initialise engine
	g_pEngine->Frame();

	// add +map if no map loading command is present
	// don't manually execute this from cbuf as users may have it in their startup args anyway, easier just to run from stuffcmds if present
	if (!CommandLine()->CheckParm("+map") && !CommandLine()->CheckParm("+launchplaylist"))
		CommandLine()->AppendParm("+map", g_pCVar->FindVar("match_defaultMap")->GetString());

	// re-run commandline
	Cbuf_AddText(Cbuf_GetCurrentPlayer(), "stuffcmds", cmd_source_t::kCommandSrcCode);
	Cbuf_Execute();

	// main loop
	double frameTitle = 0;
	while (g_pEngine->m_nQuitting == EngineQuitState::QUIT_NOTQUITTING)
	{
		double frameStart = g_PlatFloatTime();
		g_pEngine->Frame();

		std::this_thread::sleep_for(
			std::chrono::duration<double, std::ratio<1>>(g_pGlobals->m_flTickInterval - fmin(g_PlatFloatTime() - frameStart, 0.25)));
	}
}

// use server presence to update window title
class DedicatedConsoleServerPresence : public ServerPresenceReporter
{
	void ReportPresence(const ServerPresence* pServerPresence) override
	{
		SetConsoleTitleA(
			fmt::format(
				"{} - {} {}/{} players ({})",
				pServerPresence->m_sServerName,
				pServerPresence->m_MapName,
				pServerPresence->m_iPlayerCount,
				pServerPresence->m_iMaxPlayers,
				pServerPresence->m_PlaylistName)
				.c_str());
	}
};

HANDLE consoleInputThreadHandle = NULL;
DWORD WINAPI ConsoleInputThread(PVOID pThreadParameter)
{
	NOTE_UNUSED(pThreadParameter);

	while (!g_pEngine || !g_pHostState || g_pHostState->m_iCurrentState != HostState_t::HS_RUN)
		Sleep(1000);

	// Bind stdin to receive console input.
	FILE* fp = nullptr;
	freopen_s(&fp, "CONIN$", "r", stdin);

	spdlog::info("Ready to receive console commands.");

	{
		// Process console input
		std::string input;
		while (g_pEngine && g_pEngine->m_nQuitting == EngineQuitState::QUIT_NOTQUITTING && std::getline(std::cin, input))
		{
			input += "\n";
			Cbuf_AddText(Cbuf_GetCurrentPlayer(), input.c_str(), cmd_source_t::kCommandSrcCode);
			TryPrintCvarHelpForCommand(input.c_str()); // this needs to be done on main thread, unstable in this one
		}
	}

	return 0;
}

DECLARE_HOOK(IsGameActiveWindow, engine.dll + 0x1CDC80, [](auto& hook) -> bool
{
	NOTE_UNUSED(hook);
	return true;
})

ON_DLL_LOAD_DEDI_RELIESON("engine.dll", DedicatedServer, ServerPresence, [](CModule module)
{
	spdlog::info("InitialiseDedicated");

	DedicatedHooks.DispatchForModule("engine.dll");

	// Host_Init
	// prevent a particle init that relies on client dll
	module.Offset(0x156799).NoOP(5);

	// Host_Init
	// don't call Key_Init to avoid loading some extra rsons from rpak (will be necessary to boot if we ever wanna disable rpaks entirely on
	// dedi)
	module.Offset(0x1565B0).NoOP(5);

	{
		// CModAppSystemGroup::Create
		// force the engine into dedicated mode by changing the first comparison to IsServerOnly to an assignment
		CMemory base = module.Offset(0x1C4EBD);

		// cmp => mov
		base.Offset(1).Patch("C6 87");

		// 00 => 01
		base.Offset(7).Patch("01");
	}

	// Some init that i'm not sure of that crashes
	// nop the call to it
	module.Offset(0x156A63).NoOP(5);

	if (CommandLine()->CheckParm("-noshaderapi"))
    {
        // Likely CL_ClearState: clears the client map/model state.
        // Preserve all software-side cleanup, but skip the render-context reset and release because the null MaterialSystem has no context.
        module.Offset(0x72B9A).NoOP(9);
        module.Offset(0x72BCE).NoOP(6);
        module.Offset(0x72BDC).Patch("C3 90 90 90");

        // Called by Mod_LoadCubemapSamples to select the map cubemap or engine/defaultcubemap.
        // Keep the texture reference and BSP model metadata, but skip BindLocalCubemap and the context release because the context is null.
        module.Offset(0xC8404).NoOP(9);
        module.Offset(0xC8414).NoOP(6);
        module.Offset(0xC842E).Patch("C3 90 90 90");
    }

    if (CommandLine()->CheckParm("-nowindow"))
    {
        // CVideoMode_Common::CreateGameWindow: skip creating and binding the Win32 window and skip IMaterialSystem::SetMode.
        // Resume at the software-side datatable and LCD screen effect RPak type registrations, then return success.
        module.Offset(0x1CD0ED).Patch("E9 70 00 00 00");
    }

	// runframeserver
	// nop some access violations
	module.Offset(0x159819).NoOP(17);

	module.Offset(0x156B4C).NoOP(7);

	// previously patched these, took me a couple weeks to figure out they were the issue
	// removing these will mess up register state when this function is over, so we'll write HS_RUN to the wrong address
	// so uhh, don't do that
	// NSMem::NOP(ea + 0x156B4C + 7, 8);
	module.Offset(0x156B4C).Offset(15).NoOP(9);

	// HostState_State_NewGame
	// nop an access violation
	module.Offset(0xB934C).NoOP(9);

	// CEngineAPI::Connect
	// remove call to Shader_Connect
	module.Offset(0x1C4D7D).NoOP(5);
	// Host_Init
	// remove call to ui loading stuff
	module.Offset(0x156595).NoOP(5);

	// some function that gets called from RunFrameServer
	// nop a function that makes requests to stryder, this will eventually access violation if left alone and isn't necessary anyway
	module.Offset(0x15A0BB).NoOP(5);

	// RunFrameServer
	// nop a function that access violations
	module.Offset(0x159BF3).NoOP(5);

	// func that checks if origin is inited
	// always return 1
	module.Offset(0x183B70).Patch("B0 01 C3"); // mov al,01 ret

	// HostState_State_ChangeLevel
	// nop clientinterface call
	module.Offset(0x1552ED).NoOP(16);

	// HostState_State_ChangeLevel
	// nop clientinterface call
	module.Offset(0x155363).NoOP(16);

	// IVideoMode::CreateGameWindow
	// nop call to ShowWindow
	module.Offset(0x1CD146).NoOP(5);

    // CEngineVGUI::ActivateGameUI
    // Do not show game UI on disconnect cmd.
    module.Offset(0x249D50).Patch("C3");

	static CDedicatedExports dedicatedExports;
	*module.Offset(0x13F0B668).RCast<CDedicatedExports**>() = &dedicatedExports;

	// extra potential patches:
	// nop engine.dll+1c67d1 and +1c67d8 to skip videomode creategamewindow
	// also look into launcher.dll+d381, seems to cause renderthread to get made
	// this crashes HARD if no window which makes sense tbh
	// also look into materialsystem + 5B344 since it seems to be the base of all the renderthread stuff

	// big note: datatable gets registered in window creation
	// make sure it still gets registered

	// add cmdline args that are good for dedi
	CommandLine()->AppendParm("-nomenuvid", 0);
	CommandLine()->AppendParm("-nosound", 0);
	CommandLine()->AppendParm("-windowed", 0);
	CommandLine()->AppendParm("-nomessagebox", 0);
	CommandLine()->AppendParm("+host_preload_shaders", "0");
	CommandLine()->AppendParm("+net_usesocketsforloopback", "1");
	CommandLine()->AppendParm("+community_frame_run", "0");

	// use presence reporter for console title
	DedicatedConsoleServerPresence* presenceReporter = new DedicatedConsoleServerPresence;
	g_pServerPresence->AddPresenceReporter(presenceReporter);

	// setup dedicated printing to client
	RegisterSink(std::make_shared<DedicatedServerLogToClientSink>());

	// Disable Quick Edit mode to reduce chance of user unintentionally hanging their server by selecting something.
	if (!CommandLine()->CheckParm("-bringbackquickedit"))
	{
		HANDLE stdIn = GetStdHandle(STD_INPUT_HANDLE);
		DWORD mode = 0;

		if (GetConsoleMode(stdIn, &mode))
		{
			if (mode & ENABLE_QUICK_EDIT_MODE)
			{
				mode &= ~ENABLE_QUICK_EDIT_MODE;
				mode &= ~ENABLE_MOUSE_INPUT;

				mode |= ENABLE_PROCESSED_INPUT;

				SetConsoleMode(stdIn, mode);
			}
		}
	}
	else
		spdlog::info("Quick Edit enabled by user request");

	// create console input thread
	if (!CommandLine()->CheckParm("-noconsoleinput"))
		consoleInputThreadHandle = CreateThread(0, 0, ConsoleInputThread, 0, 0, NULL);
	else
		spdlog::info("Console input disabled by user request");
})

ON_DLL_LOAD_DEDI("tier0.dll", DedicatedServerOrigin, [](CModule module)
{
	// disable origin on dedicated
	// for any big ea lawyers, this can't be used to play the game without origin, game will throw a fit if you try to do anything without
	// an origin id as a client for dedi it's fine though, game doesn't care if origin is disabled as long as there's only a server
	module.GetExportedFunction("Tier0_InitOrigin").Patch("C3");
})

DECLARE_HOOK(PrintSquirrelError, server.dll + 0x794D0, [](auto& hook, void* sqvm)
{
	hook.Original(sqvm);

	// close dedicated server if a fatal error is hit
	// atm, this will crash if not aborted, so this just closes more gracefully
	static ConVar* Cvar_fatal_script_errors = g_pCVar->FindVar("fatal_script_errors");
	if (Cvar_fatal_script_errors->GetBool())
	{
		NS::log::FlushLoggers();
		abort();
	}
})

ON_DLL_LOAD_DEDI("server.dll", DedicatedServerGameDLL, [](CModule module)
{
	DedicatedHooks.DispatchForModule("server.dll");

	if (CommandLine()->CheckParm("-nopakdedi"))
	{
		module.Offset(0x6BA350).Patch("C3"); // dont load skins.rson from rpak if we don't have rpaks, as loading it will cause a crash
		module.Offset(0x6BA300).Patch(
			"B8 C8 00 00 00 C3"); // return 200 as the number of skins from server.dll + 6BA300, this is the normal value read from
								  // skins.rson and should be updated when we need it more modular
	}
})
