#include "tier0.h"

#include <codecvt>
#include <locale>

DECLARE_MODULE(CoreTier0Hooks)

IMemAlloc* g_pMemAllocSingleton = nullptr;

CommandLineType CommandLine;
Plat_FloatTimeType Plat_FloatTime;
ThreadInServerFrameThreadType ThreadInServerFrameThread;

typedef IMemAlloc* (*CreateGlobalMemAllocType)();
CreateGlobalMemAllocType CreateGlobalMemAlloc;

// needs to be a seperate function, since memalloc.cpp calls it
void TryCreateGlobalMemAlloc()
{
	// init memalloc stuff
	CreateGlobalMemAlloc =
		reinterpret_cast<CreateGlobalMemAllocType>(GetProcAddress(GetModuleHandleA("tier0.dll"), "CreateGlobalMemAlloc"));
	g_pMemAllocSingleton = CreateGlobalMemAlloc(); // if it already exists, this returns the preexisting IMemAlloc instance
}

HRESULT WINAPI _SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription)
{
	// need to grab it dynamically as this function was only introduced at some point in Windows 10
	static decltype(&SetThreadDescription) _SetThreadDescription =
		CModule("KernelBase.dll").GetExportedFunction("SetThreadDescription").RCast<decltype(&SetThreadDescription)>();

	if (_SetThreadDescription)
		return _SetThreadDescription(hThread, lpThreadDescription);

	return ERROR_OLD_WIN_VERSION;
}

DECLARE_HOOK_PROC(ThreadSetDebugName, tier0.dll, ThreadSetDebugName, [](auto& hook, HANDLE threadHandle, const char* name)
{
	if (threadHandle == 0)
		threadHandle = GetCurrentThread();

	std::wstring wideName;
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	wideName = converter.from_bytes(name);
	_SetThreadDescription(threadHandle, wideName.c_str());

	hook.Original(threadHandle, name);
})

ON_DLL_LOAD("tier0.dll", Tier0GameFuncs, [](CModule module)
{
	// shouldn't be necessary, but do this just in case
	TryCreateGlobalMemAlloc();

	DISPATCH_MODULE(CoreTier0Hooks)

	// setup tier0 funcs
	CommandLine = module.GetExportedFunction("CommandLine").RCast<CommandLineType>();
	Plat_FloatTime = module.GetExportedFunction("Plat_FloatTime").RCast<Plat_FloatTimeType>();
	ThreadInServerFrameThread = module.GetExportedFunction("ThreadInServerFrameThread").RCast<ThreadInServerFrameThreadType>();
})
