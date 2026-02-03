#include "config/profile.h"
#include "engine/netmessages.h"
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

#include "windows/libsys.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <filesystem>
#include <string>
#include <string.h>
#include <windows.h>

static bool StartGameWithUri(const std::string& uri)
{
	if (!g_NorthstarModule)
		return false;

	wchar_t modulePath[MAX_PATH] = {};
	if (GetModuleFileNameW(g_NorthstarModule, modulePath, MAX_PATH) == 0)
		return false;

	std::filesystem::path moduleDir = std::filesystem::path(modulePath).parent_path();
	std::filesystem::path launcherExePath = moduleDir / L"NorthstarLauncher.exe";
	if (!std::filesystem::exists(launcherExePath))
		return false;

	int wideLen = MultiByteToWideChar(CP_UTF8, 0, uri.c_str(), -1, nullptr, 0);
	if (wideLen <= 0)
		return false;

	std::wstring uriWide;
	uriWide.resize(static_cast<size_t>(wideLen - 1));
	MultiByteToWideChar(CP_UTF8, 0, uri.c_str(), -1, uriWide.data(), wideLen);

	std::wstring command = L"\"" + launcherExePath.wstring() + L"\" \"" + uriWide + L"\"";
	std::vector<wchar_t> commandLine(command.begin(), command.end());
	commandLine.push_back(L'\0');

	STARTUPINFOW si = {};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi = {};

	BOOL ok = CreateProcessW(
		launcherExePath.c_str(),
		commandLine.data(),
		nullptr,
		nullptr,
		FALSE,
		0,
		nullptr,
		moduleDir.c_str(),
		&si,
		&pi);

	if (ok)
	{
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return true;
	}

	return false;
}


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
	case DLL_PROCESS_DETACH:
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

	if (auto uri = Mod_TryGetUriFromCommandLine())
	{
		if (uriMutexExists && Mod_ForwardUriToRunningInstance(*uri))
			ExitProcess(0);
		HandleModShellExtensionUri(*uri);
	}

	if (!uriMutexExists)
		Mod_StartUriServer();

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

	// Init minhook
	HookSys_Init();
	if (strstr(GetCommandLineA(), "-allocconsole") != NULL)
		LogSys_InitialiseConsole();
	// Init loadlibrary callbacks
	LibSys_Init();

	g_pServerPresence = new ServerPresenceManager();

	g_pPluginManager = new PluginManager();
	g_pPluginManager->LoadPlugins();

	g_pSquirrel.InitialiseSquirrelManagers();

	// Fix some users' failure to connect to respawn datacenters
	SetEnvironmentVariableA("OPENSSL_ia32cap", "~0x200000200000000");

	curl_global_init_mem(CURL_GLOBAL_DEFAULT, _malloc_base, _free_base, _realloc_base, _strdup_base, _calloc_base);

	// run callbacks for any libraries that are already loaded by now
	CallAllPendingDLLLoadCallbacks();

	return true;
}
