#include "config/profile.h"
#include "common/netmessages.h"
#include "logging/crashhandler.h"
#include "logging/logging.h"
#include "plugins/pluginmanager.h"
#include "plugins/plugins.h"
#include "server/serverpresence.h"
#include "vscript/squirrel/squirrel.h"
#include "modsystem/modshellext.h"
#include "shell.h"
#include "util/version.h"
#include "util/wininfo.h"
#include "tier0/callbacks.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <filesystem>
#include <string>
#include <string.h>
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	NOTE_UNUSED(hModule);
	NOTE_UNUSED(lpReserved);
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		g_NorthstarModule = hModule;
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		g_pCallbackManager->Shutdown();
		break;
	}

	return TRUE;
}

bool InitialiseNorthstar()
{
	static bool bInitialised = false;
	if (bInitialised)
		return false;

	bInitialised = true;

	InitialiseNorthstarPrefix();
	if (curl_global_init_mem(CURL_GLOBAL_DEFAULT, _malloc_base, _free_base, _realloc_base, _strdup_base, _calloc_base) != CURLE_OK)
	{
		return false;
	}

	wchar_t modulePath[MAX_PATH] = {};
	if (GetModuleFileNameW(g_NorthstarModule, modulePath, MAX_PATH) > 0)
	{
		std::filesystem::path moduleDir = std::filesystem::path(modulePath).parent_path();
		std::filesystem::path launcherPath = moduleDir / L"NorthstarLauncher.exe";
		if (std::filesystem::exists(launcherPath))
			Shell_RegisterProtocol(launcherPath);
	}

	static HANDLE s_uriMutex = CreateMutexA(nullptr, FALSE, "Local\\NorthstarUriMutex");
	const bool uriMutexExists = (GetLastError() == ERROR_ALREADY_EXISTS);

	if (auto uri = CModShellExtension::Get().GetCommandLineUri())
	{
		if (uriMutexExists && CModShellExtension::Get().ForwardToRunningInstance(*uri))
			ExitProcess(0);
		CModShellExtension::Get().HandleUri(*uri);
	}

	if (!uriMutexExists)
		CModShellExtension::Get().StartUriServer();

	// Init minhook
	HookSys_Init();
	if (strstr(GetCommandLineA(), "-allocconsole") != NULL)
		LogSys_InitialiseConsole();

	// initialise logging before most other things so that they can use spdlog and it have the proper formatting
	LogSys_InitialiseLogging();
	InitialiseVersion();
	LogSys_CreateLogFiles();
	g_pCrashHandler = new CCrashHandler();
	bool bAllFatal = strstr(GetCommandLineA(), "-crash_handle_all") != NULL;
	g_pCrashHandler->SetAllFatal(bAllFatal);

	// determine if we are in vanilla-compatibility mode
	g_pVanillaCompatibility = new VanillaCompatibility();
	// Write launcher version to log
	LogSys_StartupLog();


	g_pCallbackManager->Initialize();

	g_pServerPresence = new ServerPresenceManager();

	g_pPluginManager = new PluginManager();
	g_pPluginManager->LoadPlugins();

	g_pSquirrel.InitialiseSquirrelManagers();

	SetEnvironmentVariableA("OPENSSL_ia32cap", "~0x200000200000000");

	g_pCallbackManager->ProcessLoadedModules();

	return true;
}
