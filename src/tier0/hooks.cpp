#include "tier0/hooks.h"

#include "util/utils.h"

#include <iostream>
#include <wchar.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>

DECLARE_MODULE(HookSysInternalHooks)

ManualHook::ManualHook(const char* funcName, LPVOID func)
	: svFuncName(funcName)
	, pHookFunc(func)
	, ppOrigFunc(nullptr)
{
}

ManualHook::ManualHook(const char* funcName, LPVOID* orig, LPVOID func)
	: svFuncName(funcName)
	, pHookFunc(func)
	, ppOrigFunc(orig)
{
}

bool ManualHook::Dispatch(LPVOID addr, LPVOID* orig)
{
	if (orig)
		ppOrigFunc = orig;

	if (!addr)
		spdlog::error("Address for hook {} is invalid", svFuncName);
	else if (MH_CreateHook(addr, pHookFunc, ppOrigFunc) == MH_OK)
	{
		if (MH_EnableHook(addr) == MH_OK)
		{
			spdlog::info("Enabling hook {}", svFuncName);
			return true;
		}
		else
			spdlog::error("MH_EnableHook failed for function {}", svFuncName);
	}
	else
		spdlog::error("MH_CreateHook failed for function {}", svFuncName);

	return false;
}

uintptr_t ParseDLLOffsetString(const char* pAddrString)
{
	// in the format server.dll + 0xDEADBEEF
	size_t iDllNameEnd = 0;
	do
		++iDllNameEnd;
	while (!isspace(pAddrString[iDllNameEnd]) && pAddrString[iDllNameEnd] != '+');

	const std::string svModuleName(pAddrString, iDllNameEnd);

	// get the module address
	const HMODULE pModuleAddr = GetModuleHandleA(svModuleName.c_str());

	if (!pModuleAddr)
		return 0;

	// get the offset string
	// seek until we hit the start of the number offset
	size_t iOffsetBegin = iDllNameEnd;
	do
		++iOffsetBegin;
	while (!isdigit(pAddrString[iOffsetBegin]) && pAddrString[iOffsetBegin]);

	uintptr_t iOffset = 0;
	const bool bIsHex = pAddrString[iOffsetBegin] == '0' && (pAddrString[iOffsetBegin + 1] == 'X' || pAddrString[iOffsetBegin + 1] == 'x');
	if (bIsHex)
		iOffset = std::stoi(pAddrString + iOffsetBegin + 2, 0, 16);
	else
		iOffset = std::stoi(pAddrString + iOffsetBegin);

	return ((uintptr_t)pModuleAddr + iOffset);
}

void MakeHook(LPVOID pTarget, LPVOID pDetour, void* ppOriginal, const char* pFuncName)
{
	char* pStrippedFuncName = (char*)pFuncName;
	// strip & char from funcname
	if (*pStrippedFuncName == '&')
		pStrippedFuncName++;

	if (MH_CreateHook(pTarget, pDetour, (LPVOID*)ppOriginal) == MH_OK)
	{
		if (MH_EnableHook(pTarget) == MH_OK)
			spdlog::info("Enabling hook {}", pStrippedFuncName);
		else
			spdlog::error("MH_EnableHook failed for function {}", pStrippedFuncName);
	}
	else
		spdlog::error("MH_CreateHook failed for function {}", pStrippedFuncName);
}

DECLARE_HOOK_PROC(HookSysGetCommandLineA, KERNEL32.DLL, GetCommandLineA, [](auto& hook) -> LPSTR
{
	static char* cmdlineModified;
	static char* cmdlineOrg;

	if (cmdlineOrg == nullptr || cmdlineModified == nullptr)
	{
		cmdlineOrg = hook.Original();
		bool isDedi = strstr(cmdlineOrg, "-dedicated"); // well, this one has to be a real argument
		bool ignoreStartupArgs = strstr(cmdlineOrg, "-nostartupargs");

		if(GetCurrentProcessExeName() == L"r2ds.exe")
			isDedi = true;

		std::string args;
		std::ifstream cmdlineArgFile;

		// it looks like CommandLine() prioritizes parameters apprearing first, so we want the real commandline to take priority
		// not to mention that cmdlineOrg starts with the EXE path
		args.append(cmdlineOrg);
		args.append(" ");

		// append those from the file

		if (!ignoreStartupArgs)
		{

			cmdlineArgFile = std::ifstream(!isDedi ? "ns_startup_args.txt" : "ns_startup_args_dedi.txt");

			if (cmdlineArgFile)
			{
				std::stringstream argBuffer;
				argBuffer << cmdlineArgFile.rdbuf();
				cmdlineArgFile.close();

				// if some other command line option includes "-northstar" in the future then you have to refactor this check to check with
				// both either space after or ending with
				if (!isDedi && argBuffer.str().find("-northstar") != std::string::npos)
					MessageBoxA(
						NULL,
						"The \"-northstar\" command line option is NOT supposed to go into ns_startup_args.txt file!\n\nThis option is "
						"supposed to go into Origin/Steam game launch options, and then you are supposed to launch the original "
						"Titanfall2.exe "
						"rather than NorthstarLauncher.exe to make use of it.",
						"Northstar Warning",
						MB_ICONWARNING);

				args.append(argBuffer.str());
			}
		}

		auto len = args.length();
		cmdlineModified = new char[len + 1];
		if (!cmdlineModified)
		{
			spdlog::error("malloc failed for command line");
			return cmdlineOrg;
		}
		memcpy(cmdlineModified, args.c_str(), len + 1);
	}

	return cmdlineModified;
})


void HookSys_Init()
{
	if (MH_Initialize() != MH_OK)
	{
		spdlog::error("MH_Initialize (minhook initialization) failed");
	}

	DISPATCH_MODULE(HookSysInternalHooks)
}
