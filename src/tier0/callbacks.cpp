#include "tier0/callbacks.h"
#include "tier0/hooks.h"

#include "dedicated/dedicated.h"
#include "plugins/pluginmanager.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <Psapi.h>
#include <winternl.h>

namespace fs = std::filesystem;

#define XINPUT1_3_DLL "XInput1_3.dll"

DECLARE_MODULE(CallbackManagerHooks)

#ifndef LDR_DLL_NOTIFICATION_REASON_LOADED
#define LDR_DLL_NOTIFICATION_REASON_LOADED 1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2
#endif

#ifndef LDR_DLL_NOTIFICATION_FUNCTION
typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA
{
	ULONG Flags;
	const UNICODE_STRING* FullDllName;
	const UNICODE_STRING* BaseDllName;
	PVOID DllBase;
	ULONG SizeOfImage;
} LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA
{
	ULONG Flags;
	const UNICODE_STRING* FullDllName;
	const UNICODE_STRING* BaseDllName;
	PVOID DllBase;
	ULONG SizeOfImage;
} LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

typedef union _LDR_DLL_NOTIFICATION_DATA
{
	LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
	LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
} LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

typedef const LDR_DLL_NOTIFICATION_DATA* PCLDR_DLL_NOTIFICATION_DATA;

typedef VOID(CALLBACK* PLDR_DLL_NOTIFICATION_FUNCTION)(ULONG NotificationReason,
	PCLDR_DLL_NOTIFICATION_DATA NotificationData,
	PVOID Context);
#endif

using LdrRegisterDllNotificationType = NTSTATUS(NTAPI*)(ULONG, PLDR_DLL_NOTIFICATION_FUNCTION, PVOID, PVOID*);
using LdrUnregisterDllNotificationType = NTSTATUS(NTAPI*)(PVOID);

struct DllLoadCallbackRecord
{
	std::string dll;
	DllLoadCallbackFuncType callback;
	std::string tag;
	std::vector<std::string> reliesOn;
	bool called;
};

static std::vector<DllLoadCallbackRecord>& GetDllLoadCallbacks()
{
	static std::vector<DllLoadCallbackRecord> vec = std::vector<DllLoadCallbackRecord>();
	return vec;
}

static std::vector<std::string>& GetCalledCallbackTags()
{
	static std::vector<std::string> tags = std::vector<std::string>();
	return tags;
}

static std::vector<HMODULE>& GetCalledModules()
{
	static std::vector<HMODULE> modules = std::vector<HMODULE>();
	return modules;
}

static std::vector<HMODULE>& GetPendingModules()
{
	static std::vector<HMODULE> modules = std::vector<HMODULE>();
	return modules;
}

static std::mutex& GetPendingModulesMutex()
{
	static std::mutex m;
	return m;
}

static PVOID g_DllNotificationCookie = nullptr;
static LdrUnregisterDllNotificationType g_LdrUnregisterDllNotification = nullptr;
static CallbackManager g_CallbackManager;
CallbackManager* g_pCallbackManager = &g_CallbackManager;

static VOID CALLBACK OnDllNotification(ULONG notificationReason, PCLDR_DLL_NOTIFICATION_DATA pNotificationData, PVOID context)
{
	auto* callbackManager = static_cast<CallbackManager*>(context);
	if (!callbackManager)
		callbackManager = g_pCallbackManager;

	if (notificationReason != LDR_DLL_NOTIFICATION_REASON_LOADED || !pNotificationData)
	{
		return;
	}

	callbackManager->QueueModuleForCallbacks(static_cast<HMODULE>(pNotificationData->Loaded.DllBase));
}

__dllLoadCallback::__dllLoadCallback(
	eDllLoadCallbackSide side,
	const std::string dllName,
	DllLoadCallbackFuncType callback,
	std::string uniqueStr,
	std::string reliesOn)
{
	std::vector<std::string> reliesOnArray;

	if (reliesOn.length() && reliesOn[0] != '(')
	{
		reliesOnArray.push_back(reliesOn);
	}
	else
	{
		std::string currentTag;
		for (size_t i = 1; i < reliesOn.length(); i++)
		{
			if (!isspace(static_cast<unsigned char>(reliesOn[i])))
			{
				if (reliesOn[i] == ',' || reliesOn[i] == ')')
				{
					reliesOnArray.push_back(currentTag);
					currentTag.clear();
				}
				else
					currentTag += reliesOn[i];
			}
		}
	}

	switch (side)
	{
	case eDllLoadCallbackSide::UNSIDED:
	{
		AddDllLoadCallback(dllName, callback, uniqueStr, reliesOnArray);
		break;
	}

	case eDllLoadCallbackSide::CLIENT:
	{
		AddDllLoadCallbackForClient(dllName, callback, uniqueStr, reliesOnArray);
		break;
	}

	case eDllLoadCallbackSide::DEDICATED_SERVER:
	{
		AddDllLoadCallbackForDedicatedServer(dllName, callback, uniqueStr, reliesOnArray);
		break;
	}
	}
}

void AddDllLoadCallback(std::string dll, DllLoadCallbackFuncType callback, std::string tag, std::vector<std::string> reliesOn)
{
	if (!g_pCallbackManager)
		return;

	g_pCallbackManager->AddDllCallback(dll, callback, tag, reliesOn);
}

void AddDllLoadCallbackForDedicatedServer(
	std::string dll,
	DllLoadCallbackFuncType callback,
	std::string tag,
	std::vector<std::string> reliesOn)
{
	if (!IsDedicatedServer())
		return;

	if (!g_pCallbackManager)
		return;

	g_pCallbackManager->AddDllCallback(dll, callback, tag, reliesOn);
}

void AddDllLoadCallbackForClient(
	std::string dll,
	DllLoadCallbackFuncType callback,
	std::string tag,
	std::vector<std::string> reliesOn)
{
	if (IsDedicatedServer())
		return;

	if (!g_pCallbackManager)
		return;

	g_pCallbackManager->AddDllCallback(dll, callback, tag, reliesOn);
}

void CallbackManager::AddDllCallback(std::string dll, DllLoadCallbackFuncType callback, std::string tag, std::vector<std::string> reliesOn)
{
	DllLoadCallbackRecord& callbackStruct = GetDllLoadCallbacks().emplace_back();

	callbackStruct.dll = std::move(dll);
	callbackStruct.callback = callback;
	callbackStruct.tag = std::move(tag);
	callbackStruct.reliesOn = std::move(reliesOn);
	callbackStruct.called = false;
}

void CallbackManager::CallDllLoadCallbacks(const char* moduleName, HMODULE moduleHandle)
{
	auto& calledTags = GetCalledCallbackTags();
	CModule cModule(moduleHandle);
	NOTE_UNUSED(cModule);

	while (true)
	{
		bool doneCalling = true;

		for (auto& callbackStruct : GetDllLoadCallbacks())
		{
			if (!callbackStruct.called && fs::path(moduleName).filename() == fs::path(callbackStruct.dll).filename())
			{
				bool shouldContinue = false;

				if (!callbackStruct.reliesOn.empty())
				{
					for (std::string tag : callbackStruct.reliesOn)
					{
						if (std::find(calledTags.begin(), calledTags.end(), tag) == calledTags.end())
						{
							doneCalling = false;
							shouldContinue = true;
							break;
						}
					}
				}

				if (shouldContinue)
					continue;

				callbackStruct.callback(moduleHandle);
				calledTags.push_back(callbackStruct.tag);
				callbackStruct.called = true;
			}
		}

		if (doneCalling)
			break;
	}
}

void CallbackManager::RunModuleCallbacks(HMODULE hModule)
{
	auto& calledModules = GetCalledModules();

	if (!hModule)
	{
		return;
	}

	if (std::find(calledModules.begin(), calledModules.end(), hModule) != calledModules.end())
	{
		return;
	}
	calledModules.push_back(hModule);

	CHAR moduleName[MAX_PATH];
	GetModuleBaseNameA(GetCurrentProcess(), hModule, moduleName, MAX_PATH);

	CModule cModule(hModule);
	for (const std::string& importName : cModule.GetImportedModules())
		RunModuleCallbacks(GetModuleHandleA(importName.c_str()));

	CallDllLoadCallbacks(moduleName, hModule);
	if (g_pPluginManager)
		g_pPluginManager->InformDllLoad(hModule, fs::path(moduleName));
}

void CallbackManager::QueueModuleForCallbacks(HMODULE hModule)
{
	if (!hModule)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(GetPendingModulesMutex());
	auto& pendingModules = GetPendingModules();
	if (std::find(pendingModules.begin(), pendingModules.end(), hModule) == pendingModules.end())
	{
		pendingModules.push_back(hModule);
	}
}

void CallbackManager::DrainPendingModuleCallbacks()
{
	std::vector<HMODULE> pendingModules;
	{
		std::lock_guard<std::mutex> lock(GetPendingModulesMutex());
		pendingModules.swap(GetPendingModules());
	}

	for (HMODULE hModule : pendingModules)
	{
		RunModuleCallbacks(hModule);
	}
}

DECLARE_HOOK_PROC(CallbackManagerLoadLibraryExA, KERNEL32.DLL, LoadLibraryExA, [](auto& hook, LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) -> HMODULE
{
	HMODULE hModule = nullptr;

	if (lpLibFileName)
	{
		LPCSTR lpLibFileNameEnd = lpLibFileName + strlen(lpLibFileName);
		LPCSTR lpLibName = lpLibFileNameEnd - strlen(XINPUT1_3_DLL);

		if (lpLibFileName <= lpLibName && !strncmp(lpLibName, XINPUT1_3_DLL, strlen(XINPUT1_3_DLL) + 1))
		{
			const char* replacementDll = "XInput1_4.dll";
			hModule = hook.Original(replacementDll, hFile, dwFlags);

			if (!hModule)
			{
				replacementDll = "XInput9_1_0.dll";
				spdlog::warn("Couldn't load XInput1_4.dll. Will try XInput9_1_0.dll. If on Windows 7 this is expected");
				hModule = hook.Original(replacementDll, hFile, dwFlags);
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

			spdlog::info("Successfully loaded {} as a replacement for XInput1_3.dll", replacementDll);
		}
	}

	if (!hModule)
		hModule = hook.Original(lpLibFileName, hFile, dwFlags);

	if (g_pCallbackManager)
		g_pCallbackManager->DrainPendingModuleCallbacks();

	return hModule;
})

void CallbackManager::Initialize()
{
	if (!g_DllNotificationCookie)
	{
		const HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
		if (!hNtdll)
		{
			spdlog::error("Failed to register DLL notifications: couldn't get ntdll.dll handle");
		}
		else
		{
			auto pLdrRegisterDllNotification = reinterpret_cast<LdrRegisterDllNotificationType>(
				GetProcAddress(hNtdll, "LdrRegisterDllNotification"));
			g_LdrUnregisterDllNotification = reinterpret_cast<LdrUnregisterDllNotificationType>(
				GetProcAddress(hNtdll, "LdrUnregisterDllNotification"));

			if (!pLdrRegisterDllNotification || !g_LdrUnregisterDllNotification)
			{
				spdlog::error("Failed to register DLL notifications: missing LdrRegisterDllNotification exports");
			}
			else
			{
				const NTSTATUS status = pLdrRegisterDllNotification(0, OnDllNotification, g_pCallbackManager, &g_DllNotificationCookie);
				if (status < 0 || !g_DllNotificationCookie)
				{
					g_DllNotificationCookie = nullptr;
					spdlog::error("LdrRegisterDllNotification failed with status {:#x}", static_cast<unsigned long>(status));
				}
			}
		}
	}

	DISPATCH_MODULE(CallbackManagerHooks)
	DrainPendingModuleCallbacks();
}

void CallbackManager::Shutdown()
{
	if (!g_DllNotificationCookie || !g_LdrUnregisterDllNotification)
	{
		return;
	}

	const NTSTATUS status = g_LdrUnregisterDllNotification(g_DllNotificationCookie);
	if (status < 0)
	{
		spdlog::warn("LdrUnregisterDllNotification failed with status {:#x}", static_cast<unsigned long>(status));
	}

	g_DllNotificationCookie = nullptr;
	g_LdrUnregisterDllNotification = nullptr;
}

void CallbackManager::ProcessLoadedModules()
{
	HMODULE hMods[1024];
	HANDLE hProcess = GetCurrentProcess();
	DWORD cbNeeded;

	if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
	{
		for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			wchar_t szModName[MAX_PATH];
			if (GetModuleFileNameExW(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR)))
			{
				RunModuleCallbacks(hMods[i]);
			}
		}
	}

	DrainPendingModuleCallbacks();
}
