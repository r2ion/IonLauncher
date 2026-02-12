#include "libsys.h"
#include "plugins/pluginmanager.h"

#define XINPUT1_3_DLL "XInput1_3.dll"

DECLARE_MODULE(LibSysHooks)

//-----------------------------------------------------------------------------
// Purpose: Run detour callbacks for given HMODULE
//-----------------------------------------------------------------------------
void LibSys_RunModuleCallbacks(HMODULE hModule)
{
	// Modules that we have already ran callbacks for.
	// Note: If we ever hook unloading modules, then this will need updating to handle removal etc.
	static std::vector<HMODULE> vCalledModules;

	if (!hModule)
	{
		return;
	}

	// If we have already ran callbacks for this module, don't run them again.
	if (std::find(vCalledModules.begin(), vCalledModules.end(), hModule) != vCalledModules.end())
	{
		return;
	}
	vCalledModules.push_back(hModule);

	// Get module base name in ASCII as noone wants to deal with unicode
	CHAR szModuleName[MAX_PATH];
	GetModuleBaseNameA(GetCurrentProcess(), hModule, szModuleName, MAX_PATH);

	// Run calllbacks for all imported modules
	CModule cModule(hModule);
	for (const std::string& svImport : cModule.GetImportedModules())
		LibSys_RunModuleCallbacks(GetModuleHandleA(svImport.c_str()));

	// DevMsg(eLog::NONE, "%s\n", szModuleName);

	// Call callbacks
	CallLoadLibraryACallbacks(szModuleName, hModule);
	g_pPluginManager->InformDllLoad(hModule, fs::path(szModuleName));
}

//-----------------------------------------------------------------------------
// Load library callbacks
DECLARE_HOOK_PROC(LibSysLoadLibraryA, KERNEL32.DLL, LoadLibraryA, [](auto& hook, LPCSTR lpLibFileName) -> HMODULE
{
	HMODULE hModule = hook.Original(lpLibFileName);
	LibSys_RunModuleCallbacks(hModule);
	return hModule;
})

DECLARE_HOOK_PROC(LibSysLoadLibraryExA, KERNEL32.DLL, LoadLibraryExA, [](auto& hook, LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) -> HMODULE
{
	HMODULE hModule = nullptr;

	if (lpLibFileName)
	{
		LPCSTR lpLibFileNameEnd = lpLibFileName + strlen(lpLibFileName);
		LPCSTR lpLibName = lpLibFileNameEnd - strlen(XINPUT1_3_DLL);

		// replace xinput dll with one that has ASLR
		if (lpLibFileName <= lpLibName && !strncmp(lpLibName, XINPUT1_3_DLL, strlen(XINPUT1_3_DLL) + 1))
		{
			const char* pszReplacementDll = "XInput1_4.dll";
			hModule = hook.Original(pszReplacementDll, hFile, dwFlags);

			if (!hModule)
			{
				pszReplacementDll = "XInput9_1_0.dll";
				spdlog::warn("Couldn't load XInput1_4.dll. Will try XInput9_1_0.dll. If on Windows 7 this is expected");
				hModule = hook.Original(pszReplacementDll, hFile, dwFlags);
			}

			if (!hModule)
			{
				spdlog::error("Couldn't load XInput9_1_0.dll");
				MessageBoxA(
					0,
					"Could not load a replacement for XInput1_3.dll\nTried: XInput1_4.dll and XInput9_1_0.dll",
					"Northstar",
					MB_ICONERROR);
				exit(EXIT_FAILURE);
				return nullptr;
			}

			spdlog::info("Successfully loaded {} as a replacement for XInput1_3.dll", pszReplacementDll);
		}
	}

	if (!hModule)
		hModule = hook.Original(lpLibFileName, hFile, dwFlags);

	bool bShouldRunCallbacks =
		!(dwFlags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));
	if (bShouldRunCallbacks)
		LibSys_RunModuleCallbacks(hModule);

	return hModule;
})

DECLARE_HOOK_PROC(LibSysLoadLibraryW, KERNEL32.DLL, LoadLibraryW, [](auto& hook, LPCWSTR lpLibFileName) -> HMODULE
{
	HMODULE hModule = hook.Original(lpLibFileName);
	LibSys_RunModuleCallbacks(hModule);
	return hModule;
})

DECLARE_HOOK_PROC(LibSysLoadLibraryExW, KERNEL32.DLL, LoadLibraryExW, [](auto& hook, LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) -> HMODULE
{
	HMODULE hModule = hook.Original(lpLibFileName, hFile, dwFlags);

	bool bShouldRunCallbacks =
		!(dwFlags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));
	if (bShouldRunCallbacks)
		LibSys_RunModuleCallbacks(hModule);

	return hModule;
})

//-----------------------------------------------------------------------------
// Purpose: Initilase dll load callbacks
//-----------------------------------------------------------------------------
void LibSys_Init()
{
	DISPATCH_MODULE(LibSysHooks)
}
