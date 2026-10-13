#include "tier0/threadtools.h"
#include "tier0/threadtools_internal.h"

#include <codecvt>
#include <locale>

CThreadMutex::CThreadMutex()
{
	InitializeCriticalSectionAndSpinCount(&m_CriticalSection, 4000);
#ifdef THREAD_MUTEX_TRACING_SUPPORTED
	m_currentOwnerID = 0;
	m_lockCount = 0;
	m_bTrace = false;
#endif
}

CThreadMutex::~CThreadMutex()
{
	DeleteCriticalSection(&m_CriticalSection);
}

DECLARE_MODULE(ThreadToolsHooks)

using ThreadCheck_t = bool (*)();
static ThreadCheck_t g_pfnThreadInMainThread = nullptr;
static ThreadCheck_t g_pfnThreadInServerFrameThread = nullptr;

bool ThreadInMainThread()
{
	return g_pfnThreadInMainThread && g_pfnThreadInMainThread();
}

bool ThreadInServerFrameThread()
{
	return g_pfnThreadInServerFrameThread && g_pfnThreadInServerFrameThread();
}

HRESULT WINAPI _SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription)
{
	// This export is unavailable on older Windows versions.
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

void InitialiseThreadDebugNameHook()
{
	DISPATCH_MODULE(ThreadToolsHooks)
}

void InitialiseThreadCheckBindings(CModule module)
{
	g_pfnThreadInMainThread = module.GetExportedFunction("ThreadInMainThread").RCast<ThreadCheck_t>();
	g_pfnThreadInServerFrameThread = module.GetExportedFunction("ThreadInServerFrameThread").RCast<ThreadCheck_t>();
}
