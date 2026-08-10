#include "crashhandler.h"
#include "config/profile.h"
#include "crash_sounds.h"
#include "dedicated/dedicated.h"
#include "logging.h"
#include "modsystem/modmanager.h"
#include "plugins/pluginmanager.h"
#include "plugins/plugins.h"
#include "rtech/pakfilesystem.h"
#include "rtech/pakstate.h"
#include "rtech/paktools.h"
#include "tier0/hooks.h"
#include "util/version.h"
#include "util/utils.h"

#include <DbgHelp.h>
#include <Mmsystem.h>
#include <cctype>
#include <codecvt>
#include <cstring>
#include <dxgi1_6.h>
#include <fstream>
#include <minidumpapiset.h>
#include <ns_version.h>
#include <string>
#include <vector>
#include <winternl.h>
#include <wrl/client.h>

typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "delayimp")

#define CRASHHANDLER_MAX_FRAMES 32
#define CRASHHANDLER_GETMODULEHANDLE_FAIL "<unknown module>"
#define CRASHHANDLER_NULL_INSTRUCTION_PTR "<null instruction pointer>"

#define ENGINE_ERROR_EXCEPTION_CODE 0xE0000001
#define ENGINE_ERROR_MESSAGE_CAPCITY 0x1000

DECLARE_MODULE(CrashHandlerHooks)

struct GPUInfo_s
{
	bool found = false;
	std::string name;
	uint64_t dedicatedVramBytes = 0;
	std::vector<std::pair<std::string, uint64_t>> allAdapters; // name, vram
};

DECLARE_HOOK_PROC_CC(Tier0Error, tier0.dll, Error, __cdecl, [](auto& hook, const char* pszFormat, ...) -> char
{
	char szMessage[ENGINE_ERROR_MESSAGE_CAPCITY] = {};
	if (pszFormat && hook.HasVarArgs())
	{
		va_list argList;
		va_copy(argList, *hook.VarArgs());
		vsnprintf_s(szMessage, sizeof(szMessage), _TRUNCATE, pszFormat, argList);
		va_end(argList);
	}
	else
	{
		strcpy_s(szMessage, pszFormat ? pszFormat : "<null Tier0 Error format>");
	}

	if (g_pCrashHandler)
		g_pCrashHandler->HandleTier0Error(szMessage);

	return hook.Original("%s", szMessage);
})

//-----------------------------------------------------------------------------
// Purpose: Vectored exception callback
//-----------------------------------------------------------------------------
LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS* pExceptionInfo)
{
	g_pCrashHandler->Lock();

	g_pCrashHandler->SetExceptionInfos(pExceptionInfo);

	// Check if we should handle this
	// NOTE [Fifty]: This gets called before even a try{} catch() {} can handle an exception
	//               we don't handle these unless "-crash_handle_all" is passed as a launch arg
	if (!g_pCrashHandler->IsExceptionFatal() && !g_pCrashHandler->GetAllFatal())
	{
		g_pCrashHandler->Unlock();
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// Don't run if a debugger is attached
	if (IsDebuggerPresent())
	{
		g_pCrashHandler->Unlock();
		return EXCEPTION_CONTINUE_SEARCH;
	}

	g_pCrashHandler->PlayCrashSound(CRASH_SOUND);

	// Prevent recursive calls
	if (g_pCrashHandler->GetState())
	{
		g_pCrashHandler->Unlock();
		ExitProcess(1);
	}

	g_pCrashHandler->SetState(true);

	// Snapshot recent logs before we write crash details into spdlog.
	g_pCrashHandler->CapturePreCrashLog(200);

	// Needs to be called first as we use the members this sets later on
	g_pCrashHandler->SetCrashedModule();

	// Format
	g_pCrashHandler->FormatException();
	spdlog::error("Callstack:");
	for (const std::string& line : g_pCrashHandler->FormatCallstack())
		spdlog::error("\t{}", line);
	g_pCrashHandler->FormatRegisters();
	spdlog::error("Loaded Mods:");
	for (const std::string& line : g_pCrashHandler->FormatLoadedMods())
		spdlog::error("\t{}", line);
	spdlog::error("Loaded Plugins:");
	for (const std::string& line : g_pCrashHandler->FormatLoadedPlugins())
		spdlog::error("\t{}", line);
	spdlog::error("Loaded Modules:");
	for (const std::string& line : g_pCrashHandler->FormatModules())
		spdlog::error("\t{}", line);

	// Flush
	NS::log::FlushLoggers();

	// Write minidump
	g_pCrashHandler->WriteMinidump();
	g_pCrashHandler->WriteCrashComment();

	g_pCrashHandler->Unlock();

	g_pCrashHandler->PlayCrashSound(CRASH_SOUND2);

	if (!g_pCrashHandler->IsExceptionFatal())
		ExitProcess(1);

	return EXCEPTION_EXECUTE_HANDLER;
}

void CCrashHandler::CapturePreCrashLog(size_t maxLines)
{
	m_PreCrashLogLines = NS::log::GetRecentLogLines(maxLines);
}

void CCrashHandler::HandleTier0Error(const char* pszMessage)
{
	m_svCrashReason = pszMessage ? pszMessage : "<null Tier0 Error message>";
	RaiseException(ENGINE_ERROR_EXCEPTION_CODE, EXCEPTION_NONCONTINUABLE, 0, nullptr);
	ExitProcess(1);
}

//-----------------------------------------------------------------------------
// Purpose: console control signal handler
//-----------------------------------------------------------------------------
BOOL WINAPI ConsoleCtrlRoutine(DWORD dwCtrlType)
{
	// NOTE [Fifty]: When closing the process by closing the console we don't want
	//               to trigger the crash handler so we remove it
	switch (dwCtrlType)
	{
	case CTRL_CLOSE_EVENT:
		spdlog::info("Exiting due to console close...");
		delete g_pCrashHandler;
		g_pCrashHandler = nullptr;
		std::exit(EXIT_SUCCESS);
		return TRUE;
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCrashHandler::CCrashHandler()
	: m_hExceptionFilter(nullptr)
	, m_pExceptionInfos(nullptr)
	, m_bHasSetConsolehandler(false)
	, m_bAllExceptionsFatal(false)
	, m_bHasShownCrashMsg(false)
	, m_bState(false)
	, m_pszActiveHookName(nullptr)
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCrashHandler::~CCrashHandler()
{
	Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: Initilazes crash handler
//-----------------------------------------------------------------------------
void CCrashHandler::Init()
{
	m_hExceptionFilter = AddVectoredExceptionHandler(TRUE, ExceptionFilter);
	DISPATCH_MODULE(CrashHandlerHooks)
	m_bHasSetConsolehandler = SetConsoleCtrlHandler(ConsoleCtrlRoutine, TRUE);

	SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
	m_bSymInit = SymInitialize(GetCurrentProcess(), nullptr, TRUE) == TRUE;
	spdlog::info("Initialized symbol handler for crash reporting: {}", m_bSymInit ? "Success" : "Failed");
}

void CCrashHandler::PlayCrashSound(int resourceId)
{
	// IMPORTANT: resources are in Northstar.dll, not necessarily in the module that crashed.
	HMODULE hModule = nullptr;
	if (!GetModuleHandleExA(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCSTR>(&ExceptionFilter), // address inside THIS module
			&hModule))
	{
		return;
	}

	HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(resourceId), "WAVE");
	if (!hRes)
	{
		spdlog::error("PlayCrashSound: FindResource failed for id {} (err={})", resourceId, GetLastError());
		return;
	}

	HGLOBAL hData = LoadResource(hModule, hRes);
	if (!hData)
	{
		spdlog::error("PlayCrashSound: LoadResource failed for id {} (err={})", resourceId, GetLastError());
		return;
	}

	const void* pData = LockResource(hData);
	const DWORD size = SizeofResource(hModule, hRes);
	if (!pData || size == 0)
	{
		spdlog::error("PlayCrashSound: resource empty for id {}", resourceId);
		return;
	}

	// Synchronous so the process doesn't exit before playback starts.
	PlaySoundA(reinterpret_cast<LPCSTR>(pData), NULL, SND_MEMORY | SND_SYNC | SND_NODEFAULT);
}

//-----------------------------------------------------------------------------
// Purpose: Shutdowns crash handler
//-----------------------------------------------------------------------------
void CCrashHandler::Shutdown()
{
	if (m_hExceptionFilter)
	{
		RemoveVectoredExceptionHandler(m_hExceptionFilter);
		m_hExceptionFilter = nullptr;
	}

	if (m_bHasSetConsolehandler)
	{
		SetConsoleCtrlHandler(ConsoleCtrlRoutine, FALSE);
	}
}

int CCrashHandler::SafeCaptureStackBackTrace(PVOID* frames, ULONG maxFrames)
{
	if (!frames || maxFrames == 0)
		return 0;
	typedef USHORT(WINAPI* CaptureStackBackTraceFn)(ULONG, ULONG, PVOID*, PULONG);
	static CaptureStackBackTraceFn pCaptureStackBackTrace = nullptr;
	if (!pCaptureStackBackTrace)
	{
		HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
		if (!hKernel32)
			return 0;
		pCaptureStackBackTrace = reinterpret_cast<CaptureStackBackTraceFn>(GetProcAddress(hKernel32, "RtlCaptureStackBackTrace"));
	}

	if (!pCaptureStackBackTrace)
		return 0;

	__try
	{
		return static_cast<int>(pCaptureStackBackTrace(0, maxFrames, frames, NULL));
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return 0;
	}
}

bool CCrashHandler::SafeStackWalk64(DWORD machineType, LPSTACKFRAME64 stackFrame, PCONTEXT context)
{
	if (!stackFrame || !context)
		return false;

	__try
	{
		return ::StackWalk64(
				machineType,
				GetCurrentProcess(),
				GetCurrentThread(),
				stackFrame,
				context,
				nullptr,
				SymFunctionTableAccess64,
				SymGetModuleBase64,
				nullptr) == TRUE;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

bool CCrashHandler::SafeGetModuleHandleFromAddr(LPCVOID address, HMODULE* outModule)
{
	if (!outModule)
		return false;
	*outModule = nullptr;
	if (!address)
		return false;
	__try
	{
		if (GetModuleHandleExA(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCSTR>(address),
				outModule) != 0)
		{
			return true;
		}

		MEMORY_BASIC_INFORMATION mbi = {};
		if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0)
			return false;

		if (!mbi.AllocationBase)
			return false;

		*outModule = reinterpret_cast<HMODULE>(mbi.AllocationBase);
		return *outModule != nullptr;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		*outModule = nullptr;
		return false;
	}
}

bool CCrashHandler::SafeGetModuleFileNameExA(HANDLE process, HMODULE module, CHAR* buffer, DWORD bufferSize)
{
	if (!buffer || bufferSize == 0)
		return false;
	buffer[0] = '\0';
	__try
	{
		return GetModuleFileNameExA(process, module, buffer, bufferSize) != 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		buffer[0] = '\0';
		return false;
	}
}

bool CCrashHandler::TryCopyCString(const char* src, char* dst, size_t dstSize)
{
	if (!dst || dstSize == 0)
		return false;

	dst[0] = '\0';
	if (!src)
		return false;

	__try
	{
		for (size_t i = 0; i < dstSize - 1; i++)
		{
			const char c = src[i];
			dst[i] = c;
			if (c == '\0')
				return true;
		}

		dst[dstSize - 1] = '\0';
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		dst[0] = '\0';
		return false;
	}
}

bool CCrashHandler::TrySymFromAddrSafe(HANDLE process, DWORD64 address, DWORD64* displacement, PSYMBOL_INFO symbol)
{
	if (!process || !displacement || !symbol)
		return false;

	__try
	{
		return SymFromAddr(process, address, displacement, symbol) == TRUE;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

bool CCrashHandler::TrySymGetLineFromAddr64Safe(HANDLE process, DWORD64 address, DWORD* displacement, IMAGEHLP_LINE64* line)
{
	if (!process || !displacement || !line)
		return false;

	__try
	{
		return SymGetLineFromAddr64(process, address, displacement, line) == TRUE;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the exception info
//-----------------------------------------------------------------------------
void CCrashHandler::SetExceptionInfos(EXCEPTION_POINTERS* pExceptionPointers)
{
	m_pExceptionInfos = pExceptionPointers;
	m_pszActiveHookName = HookSys::GetActiveHookName();
}
//-----------------------------------------------------------------------------
// Purpose: Sets the exception stirngs for message box
//-----------------------------------------------------------------------------
void CCrashHandler::SetCrashedModule()
{
	const LPCVOID pCrashAddress = m_pExceptionInfos->ExceptionRecord->ExceptionAddress;
	if (!pCrashAddress)
	{
		m_svCrashedModule = CRASHHANDLER_NULL_INSTRUCTION_PTR;
		m_svCrashedOffset = "0x0";
		m_svError.clear();
		return;
	}

	HMODULE hCrashedModule = nullptr;
	if (!CCrashHandler::SafeGetModuleHandleFromAddr(pCrashAddress, &hCrashedModule))
	{
		m_svCrashedModule = CRASHHANDLER_GETMODULEHANDLE_FAIL;
		m_svCrashedOffset = fmt::format("{:#x}", static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pCrashAddress)));

		DWORD dwErrorID = GetLastError();
		if (dwErrorID != 0)
		{
			LPSTR pszBuffer;
			DWORD dwSize = FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				dwErrorID,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPSTR)&pszBuffer,
				0,
				NULL);

			if (dwSize > 0)
			{
				m_svError = pszBuffer;
				LocalFree(pszBuffer);
			}
		}

		return;
	}

	// Get module filename
	CHAR szCrashedModulePath[MAX_PATH] = {};
	if (!CCrashHandler::SafeGetModuleFileNameExA(GetCurrentProcess(), hCrashedModule, szCrashedModulePath, sizeof(szCrashedModulePath)))
	{
		m_svCrashedModule = CRASHHANDLER_GETMODULEHANDLE_FAIL;
		m_svCrashedOffset = fmt::format("{:#x}", static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pCrashAddress)));
		return;
	}

	const CHAR* pSlash = strrchr(szCrashedModulePath, '\\');
	const CHAR* pszCrashedModuleFileName = pSlash ? (pSlash + 1) : szCrashedModulePath;

	// Get relative address (offset from module base)
	const uintptr_t addr = reinterpret_cast<uintptr_t>(m_pExceptionInfos->ExceptionRecord->ExceptionAddress);
	const uintptr_t moduleBase = reinterpret_cast<uintptr_t>(hCrashedModule);
	const uintptr_t offset = (moduleBase != 0 && addr >= moduleBase) ? (addr - moduleBase) : addr;

	m_svCrashedModule = pszCrashedModuleFileName;
	m_svCrashedOffset = fmt::format("{:#x}", static_cast<uint64_t>(offset));
}

//-----------------------------------------------------------------------------
// Purpose: Gets the exception null terminated stirng
//-----------------------------------------------------------------------------

const CHAR* CCrashHandler::GetExceptionString() const
{
	return GetExceptionString(m_pExceptionInfos->ExceptionRecord->ExceptionCode);
}

//-----------------------------------------------------------------------------
// Purpose: Gets the exception null terminated stirng
//-----------------------------------------------------------------------------
const CHAR* CCrashHandler::GetExceptionString(DWORD dwExceptionCode) const
{
	// clang-format off
	switch (dwExceptionCode)
	{
	case ENGINE_ERROR_EXCEPTION_CODE:          return "TIER0_ERROR";
	case EXCEPTION_ACCESS_VIOLATION:         return "EXCEPTION_ACCESS_VIOLATION";
	case EXCEPTION_DATATYPE_MISALIGNMENT:    return "EXCEPTION_DATATYPE_MISALIGNMENT";
	case EXCEPTION_BREAKPOINT:               return "EXCEPTION_BREAKPOINT";
	case EXCEPTION_SINGLE_STEP:              return "EXCEPTION_SINGLE_STEP";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
	case EXCEPTION_FLT_DENORMAL_OPERAND:     return "EXCEPTION_FLT_DENORMAL_OPERAND";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
	case EXCEPTION_FLT_INEXACT_RESULT:       return "EXCEPTION_FLT_INEXACT_RESULT";
	case EXCEPTION_FLT_INVALID_OPERATION:    return "EXCEPTION_FLT_INVALID_OPERATION";
	case EXCEPTION_FLT_OVERFLOW:             return "EXCEPTION_FLT_OVERFLOW";
	case EXCEPTION_FLT_STACK_CHECK:          return "EXCEPTION_FLT_STACK_CHECK";
	case EXCEPTION_FLT_UNDERFLOW:            return "EXCEPTION_FLT_UNDERFLOW";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "EXCEPTION_INT_DIVIDE_BY_ZERO";
	case EXCEPTION_INT_OVERFLOW:             return "EXCEPTION_INT_OVERFLOW";
	case EXCEPTION_PRIV_INSTRUCTION:         return "EXCEPTION_PRIV_INSTRUCTION";
	case EXCEPTION_IN_PAGE_ERROR:            return "EXCEPTION_IN_PAGE_ERROR";
	case EXCEPTION_ILLEGAL_INSTRUCTION:      return "EXCEPTION_ILLEGAL_INSTRUCTION";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
	case EXCEPTION_STACK_OVERFLOW:           return "EXCEPTION_STACK_OVERFLOW";
	case EXCEPTION_INVALID_DISPOSITION:      return "EXCEPTION_INVALID_DISPOSITION";
	case EXCEPTION_GUARD_PAGE:               return "EXCEPTION_GUARD_PAGE";
	case EXCEPTION_INVALID_HANDLE:           return "EXCEPTION_INVALID_HANDLE";
	case 3765269347:                         return "RUNTIME_EXCEPTION";
	}
	// clang-format on
	return "UNKNOWN_EXCEPTION";
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if exception is known
//-----------------------------------------------------------------------------
bool CCrashHandler::IsExceptionFatal() const
{
	return IsExceptionFatal(m_pExceptionInfos->ExceptionRecord->ExceptionCode);
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if exception is known
//-----------------------------------------------------------------------------
bool CCrashHandler::IsExceptionFatal(DWORD dwExceptionCode) const
{
	// clang-format off
	switch (dwExceptionCode)
	{
	case ENGINE_ERROR_EXCEPTION_CODE:
	case EXCEPTION_ACCESS_VIOLATION:
	case EXCEPTION_DATATYPE_MISALIGNMENT:
	case EXCEPTION_BREAKPOINT:
	case EXCEPTION_SINGLE_STEP:
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
	case EXCEPTION_FLT_DENORMAL_OPERAND:
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	case EXCEPTION_FLT_INEXACT_RESULT:
	case EXCEPTION_FLT_INVALID_OPERATION:
	case EXCEPTION_FLT_OVERFLOW:
	case EXCEPTION_FLT_STACK_CHECK:
	case EXCEPTION_FLT_UNDERFLOW:
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
	case EXCEPTION_INT_OVERFLOW:
	case EXCEPTION_PRIV_INSTRUCTION:
	case EXCEPTION_IN_PAGE_ERROR:
	case EXCEPTION_ILLEGAL_INSTRUCTION:
	case EXCEPTION_NONCONTINUABLE_EXCEPTION:
	case EXCEPTION_STACK_OVERFLOW:
	case EXCEPTION_INVALID_DISPOSITION:
	case EXCEPTION_GUARD_PAGE:
	case EXCEPTION_INVALID_HANDLE:
		return true;
	}
	// clang-format on
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Shows a message box
//-----------------------------------------------------------------------------
void CCrashHandler::ShowPopUpMessage()
{
	if (m_bHasShownCrashMsg)
		return;

	m_bHasShownCrashMsg = true;

	if (!IsDedicatedServer())
	{
		std::string svMessage = fmt::format(
			"Northstar has crashed! Crash info can be found at {}/logs!\n\n{}\n{} + {}",
			GetNorthstarPrefix(),
			GetExceptionString(),
			m_svCrashedModule,
			m_svCrashedOffset);

		MessageBoxA(GetForegroundWindow(), svMessage.c_str(), "Northstar has crashed!", MB_SETFOREGROUND | MB_OK);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCrashHandler::FormatException()
{
	spdlog::error("-------------------------------------------");
	spdlog::error("Northstar has crashed!");
	spdlog::error("\tVersion: {}", version);
	if (!m_svCrashReason.empty())
		spdlog::error("\tCrash reason: {}", m_svCrashReason);
	if (!m_svError.empty())
	{
		spdlog::info("\tEncountered an error when gathering crash information!");
		spdlog::info("\tWinApi Error: {}", m_svError.c_str());
	}
	spdlog::error("\t{}", GetExceptionString());

	DWORD dwExceptionCode = m_pExceptionInfos->ExceptionRecord->ExceptionCode;
	if (dwExceptionCode == EXCEPTION_ACCESS_VIOLATION || dwExceptionCode == EXCEPTION_IN_PAGE_ERROR)
	{
		ULONG_PTR uExceptionInfo0 = m_pExceptionInfos->ExceptionRecord->ExceptionInformation[0];
		ULONG_PTR uExceptionInfo1 = m_pExceptionInfos->ExceptionRecord->ExceptionInformation[1];

		if (!uExceptionInfo0)
			spdlog::error("\tAttempted to read from: {:#x}", uExceptionInfo1);
		else if (uExceptionInfo0 == 1)
			spdlog::error("\tAttempted to write to: {:#x}", uExceptionInfo1);
		else if (uExceptionInfo0 == 8)
			spdlog::error("\tData Execution Prevention (DEP) at: {:#x}", uExceptionInfo1);
		else
			spdlog::error("\tUnknown access violation at: {:#x}", uExceptionInfo1);
	}

	spdlog::error("\tAt: {} + {}", m_svCrashedModule, m_svCrashedOffset);
	if (m_pszActiveHookName && *m_pszActiveHookName)
		spdlog::error("\tActive hook: {}", m_pszActiveHookName);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::vector<std::string> CCrashHandler::FormatCallstack()
{
	std::vector<std::string> lines;

	if (m_pExceptionInfos && m_pExceptionInfos->ExceptionRecord
		&& m_pExceptionInfos->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
	{
		lines.emplace_back("<callstack unavailable: stack overflow>");
		return lines;
	}

	std::vector<uintptr_t> frameAddrs;
	frameAddrs.reserve(CRASHHANDLER_MAX_FRAMES);

	bool bUsedExceptionContext = false;
	bool bNullInstructionPointer = false;

	if (m_pExceptionInfos && m_pExceptionInfos->ContextRecord)
	{
		CONTEXT context = *m_pExceptionInfos->ContextRecord;
		bUsedExceptionContext = true;

#ifdef _WIN64
		DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
		uintptr_t ip = static_cast<uintptr_t>(context.Rip);
		uintptr_t sp = static_cast<uintptr_t>(context.Rsp);
		uintptr_t fp = static_cast<uintptr_t>(context.Rbp);
#else
		DWORD machineType = IMAGE_FILE_MACHINE_I386;
		uintptr_t ip = static_cast<uintptr_t>(context.Eip);
		uintptr_t sp = static_cast<uintptr_t>(context.Esp);
		uintptr_t fp = static_cast<uintptr_t>(context.Ebp);
#endif

		if (ip == 0)
		{
			bNullInstructionPointer = true;
			uintptr_t guessedReturn = 0;
			if (sp != 0 && TryReadMemory(reinterpret_cast<const void*>(sp), &guessedReturn, sizeof(guessedReturn)) && guessedReturn != 0)
				frameAddrs.push_back(guessedReturn);
		}
		else
		{
			frameAddrs.push_back(ip);
		}

		STACKFRAME64 stackFrame {};
		stackFrame.AddrPC.Offset = (ip != 0) ? static_cast<DWORD64>(ip)
			: (frameAddrs.empty() ? 0 : static_cast<DWORD64>(frameAddrs.front()));
		stackFrame.AddrPC.Mode = AddrModeFlat;
		stackFrame.AddrStack.Offset = static_cast<DWORD64>(sp);
		stackFrame.AddrStack.Mode = AddrModeFlat;
		stackFrame.AddrFrame.Offset = static_cast<DWORD64>(fp);
		stackFrame.AddrFrame.Mode = AddrModeFlat;

		if (stackFrame.AddrPC.Offset != 0)
		{
			for (int i = 0; i < CRASHHANDLER_MAX_FRAMES; ++i)
			{
				const bool walkOk = CCrashHandler::SafeStackWalk64(machineType, &stackFrame, &context);

				if (!walkOk || stackFrame.AddrPC.Offset == 0)
					break;

				uintptr_t walkedAddr = static_cast<uintptr_t>(stackFrame.AddrPC.Offset);
				if (frameAddrs.empty() || frameAddrs.back() != walkedAddr)
					frameAddrs.push_back(walkedAddr);
			}
		}
	}

	if (frameAddrs.empty())
	{
		PVOID pFrames[CRASHHANDLER_MAX_FRAMES] = {};
		int iFrames = CCrashHandler::SafeCaptureStackBackTrace(pFrames, CRASHHANDLER_MAX_FRAMES);
		if (iFrames <= 0)
		{
			lines.emplace_back("<callstack unavailable>");
			return lines;
		}

		for (int i = 0; i < iFrames; ++i)
			frameAddrs.push_back(reinterpret_cast<uintptr_t>(pFrames[i]));
	}

	lines.reserve(frameAddrs.size() + 2);
	if (bNullInstructionPointer)
		lines.emplace_back("<faulting instruction pointer is null; possible null function pointer call>");

	bool bSkipExceptionHandlingFrames = true;
	if (m_svCrashedOffset.empty())
		bSkipExceptionHandlingFrames = false;
	if (m_svCrashedModule == CRASHHANDLER_GETMODULEHANDLE_FAIL || m_svCrashedModule == CRASHHANDLER_NULL_INSTRUCTION_PTR)
		bSkipExceptionHandlingFrames = false;
	if (bUsedExceptionContext)
		bSkipExceptionHandlingFrames = false;

	HMODULE hMainModule = GetModuleHandleA(nullptr);
	uintptr_t mainModuleBase = 0;
	uintptr_t mainModuleEnd = 0;
	uintptr_t mainModuleSize = 0;
	std::string svMainModuleFileName;
	if (hMainModule)
	{
		MODULEINFO moduleInfo = {};
		if (GetModuleInformation(GetCurrentProcess(), hMainModule, &moduleInfo, sizeof(moduleInfo)))
		{
			mainModuleBase = reinterpret_cast<uintptr_t>(moduleInfo.lpBaseOfDll);
			mainModuleSize = static_cast<uintptr_t>(moduleInfo.SizeOfImage);
			mainModuleEnd = mainModuleBase + mainModuleSize;
		}

		CHAR szMainModulePath[MAX_PATH] = {};
		if (CCrashHandler::SafeGetModuleFileNameExA(GetCurrentProcess(), hMainModule, szMainModulePath, sizeof(szMainModulePath)))
		{
			const CHAR* pSlash = strrchr(szMainModulePath, '\\');
			svMainModuleFileName = pSlash ? (pSlash + 1) : szMainModulePath;
		}
	}

	for (size_t i = 0; i < frameAddrs.size(); i++)
	{
		std::string svModuleFileName;
		bool bAddressIsMainModuleRva = false;

		uintptr_t addr = frameAddrs[i];
		if (addr == 0)
		{
			lines.emplace_back("<null frame>");
			continue;
		}
		LPCVOID pAddress = reinterpret_cast<LPCVOID>(addr);

		HMODULE hModule = nullptr;
		if (!CCrashHandler::SafeGetModuleHandleFromAddr(pAddress, &hModule))
		{
			if (mainModuleBase != 0 && addr >= mainModuleBase && addr < mainModuleEnd)
			{
				hModule = hMainModule;
				svModuleFileName = svMainModuleFileName.empty() ? "Titanfall2.exe" : svMainModuleFileName;
			}
			else
			{
				constexpr uintptr_t kLikelyMainModuleRvaUpperBound = 0x10000000; // 256 MB
				const bool bLooksLikeMainModuleRva =
					hMainModule && addr >= 0x1000
					&& ((mainModuleSize != 0 && addr < mainModuleSize) || (addr < kLikelyMainModuleRvaUpperBound));

				if (bLooksLikeMainModuleRva)
				{
					hModule = hMainModule;
					bAddressIsMainModuleRva = true;
					svModuleFileName = svMainModuleFileName.empty() ? "Titanfall2.exe" : svMainModuleFileName;
				}
				else
				{
					svModuleFileName = CRASHHANDLER_GETMODULEHANDLE_FAIL;
				}
			}
		}
		else
		{
			CHAR szModulePath[MAX_PATH] = {};
			if (CCrashHandler::SafeGetModuleFileNameExA(GetCurrentProcess(), hModule, szModulePath, sizeof(szModulePath)))
			{
				const CHAR* pSlash = strrchr(szModulePath, '\\');
				svModuleFileName = pSlash ? (pSlash + 1) : szModulePath;
			}
			else
			{
				svModuleFileName = CRASHHANDLER_GETMODULEHANDLE_FAIL;
			}
		}

		uintptr_t moduleBase = reinterpret_cast<uintptr_t>(hModule);
		uintptr_t offset = bAddressIsMainModuleRva ? addr : (moduleBase ? (addr - moduleBase) : addr);
		if (svModuleFileName == CRASHHANDLER_GETMODULEHANDLE_FAIL && hMainModule)
		{
			constexpr uintptr_t kLikelyMainModuleRvaUpperBound = 0x10000000; // 256 MB
			auto isLikelyMainRva = [&](uintptr_t value)
			{
				if (value < 0x1000)
					return false;
				if (mainModuleSize != 0 && value < mainModuleSize)
					return true;
				return value < kLikelyMainModuleRvaUpperBound;
			};

			uintptr_t inferredRva = 0;
			if (isLikelyMainRva(addr))
				inferredRva = addr;
			else if (isLikelyMainRva(offset))
				inferredRva = offset;

			if (inferredRva != 0)
			{
				hModule = hMainModule;
				svModuleFileName = svMainModuleFileName.empty() ? "Titanfall2.exe" : svMainModuleFileName;
				bAddressIsMainModuleRva = true;
				offset = inferredRva;
			}
		}
		std::string svCrashOffset = fmt::format("{:#x}", static_cast<uint64_t>(offset));

		if (bSkipExceptionHandlingFrames)
		{
			if (m_svCrashedModule == svModuleFileName && m_svCrashedOffset == svCrashOffset)
				bSkipExceptionHandlingFrames = false;
			else
				continue;
		}

		bool printed = false;
		if (m_bSymInit)
		{
			DWORD64 symAddr = static_cast<DWORD64>(addr);
			if (bAddressIsMainModuleRva && mainModuleBase != 0)
				symAddr = static_cast<DWORD64>(mainModuleBase + offset);

			alignas(SYMBOL_INFO) char symBuffer[sizeof(SYMBOL_INFO) + 256];
			PSYMBOL_INFO pSym = reinterpret_cast<PSYMBOL_INFO>(symBuffer);
			pSym->SizeOfStruct = sizeof(SYMBOL_INFO);
			pSym->MaxNameLen = 255;

			DWORD64 displacement = 0;
			if (TrySymFromAddrSafe(GetCurrentProcess(), symAddr, &displacement, pSym))
			{
				IMAGEHLP_LINE64 line;
				memset(&line, 0, sizeof(line));
				line.SizeOfStruct = sizeof(line);
				DWORD lineDisp = 0;

				const bool hasLine = TrySymGetLineFromAddr64Safe(GetCurrentProcess(), symAddr, &lineDisp, &line) && line.FileName;
				if (hasLine)
				{
					char fileNameBuf[1024] = {};
					std::string svFileName;
					if (TryCopyCString(line.FileName, fileNameBuf, sizeof(fileNameBuf)))
						svFileName = fileNameBuf;
					else
						svFileName = "<unknown>";

					const char* marker1 = "NorthstarLauncher\\";
					const char* marker2 = "NorthstarLauncher/";
					size_t pos = svFileName.find(marker1);
					if (pos == std::string::npos)
						pos = svFileName.find(marker2);
					if (pos != std::string::npos)
						svFileName = svFileName.substr(pos);

					lines.emplace_back(
						fmt::format(
							"{}!{}+0x{:x} [{}:{}]",
							svModuleFileName,
							pSym->Name,
							static_cast<uint64_t>(displacement),
							svFileName,
							line.LineNumber));
				}
				else
				{
					lines.emplace_back(fmt::format("{}!{}+0x{:x}", svModuleFileName, pSym->Name, static_cast<uint64_t>(displacement)));
				}

				printed = true;
			}
		}

		if (!printed)
			lines.emplace_back(fmt::format("{} + {:#x}", svModuleFileName, static_cast<uint64_t>(offset)));
	}

	if (lines.empty())
		lines.emplace_back("<callstack unavailable>");

	return lines;
}
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::string CCrashHandler::FormatFlags(const CHAR* pszRegister, DWORD nValue)
{
	return fmt::format("{}: {:#b}", pszRegister, nValue);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::string CCrashHandler::FormatIntReg(const CHAR* pszRegister, DWORD64 nValue)
{
	return fmt::format("{}: {:#x}", pszRegister, nValue);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::string CCrashHandler::FormatFloatReg(const CHAR* pszRegister, M128A nValue)
{
	DWORD nVec[4] = {
		static_cast<DWORD>(nValue.Low & UINT_MAX),
		static_cast<DWORD>(nValue.Low >> 32),
		static_cast<DWORD>(nValue.High & UINT_MAX),
		static_cast<DWORD>(nValue.High >> 32)};

	return fmt::format(
		"{}: [ {:G}, {:G}, {:G}, {:G} ]; [ {:#x}, {:#x}, {:#x}, {:#x} ]",
		pszRegister,
		static_cast<float>(nVec[0]),
		static_cast<float>(nVec[1]),
		static_cast<float>(nVec[2]),
		static_cast<float>(nVec[3]),
		nVec[0],
		nVec[1],
		nVec[2],
		nVec[3]);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCrashHandler::FormatRegisters()
{
	spdlog::error("Registers:");

	PCONTEXT pContext = m_pExceptionInfos->ContextRecord;

	spdlog::error("\t{}", FormatFlags("Flags:", pContext->ContextFlags));

	spdlog::error("\t{}", FormatIntReg("Rax", pContext->Rax));
	spdlog::error("\t{}", FormatIntReg("Rcx", pContext->Rcx));
	spdlog::error("\t{}", FormatIntReg("Rdx", pContext->Rdx));
	spdlog::error("\t{}", FormatIntReg("Rbx", pContext->Rbx));
	spdlog::error("\t{}", FormatIntReg("Rsp", pContext->Rsp));
	spdlog::error("\t{}", FormatIntReg("Rbp", pContext->Rbp));
	spdlog::error("\t{}", FormatIntReg("Rsi", pContext->Rsi));
	spdlog::error("\t{}", FormatIntReg("Rdi", pContext->Rdi));
	spdlog::error("\t{}", FormatIntReg("R8 ", pContext->R8));
	spdlog::error("\t{}", FormatIntReg("R9 ", pContext->R9));
	spdlog::error("\t{}", FormatIntReg("R10", pContext->R10));
	spdlog::error("\t{}", FormatIntReg("R11", pContext->R11));
	spdlog::error("\t{}", FormatIntReg("R12", pContext->R12));
	spdlog::error("\t{}", FormatIntReg("R13", pContext->R13));
	spdlog::error("\t{}", FormatIntReg("R14", pContext->R14));
	spdlog::error("\t{}", FormatIntReg("R15", pContext->R15));
	spdlog::error("\t{}", FormatIntReg("Rip", pContext->Rip));

	spdlog::error("\t{}", FormatFloatReg("Xmm0 ", pContext->Xmm0));
	spdlog::error("\t{}", FormatFloatReg("Xmm1 ", pContext->Xmm1));
	spdlog::error("\t{}", FormatFloatReg("Xmm2 ", pContext->Xmm2));
	spdlog::error("\t{}", FormatFloatReg("Xmm3 ", pContext->Xmm3));
	spdlog::error("\t{}", FormatFloatReg("Xmm4 ", pContext->Xmm4));
	spdlog::error("\t{}", FormatFloatReg("Xmm5 ", pContext->Xmm5));
	spdlog::error("\t{}", FormatFloatReg("Xmm6 ", pContext->Xmm6));
	spdlog::error("\t{}", FormatFloatReg("Xmm7 ", pContext->Xmm7));
	spdlog::error("\t{}", FormatFloatReg("Xmm8 ", pContext->Xmm8));
	spdlog::error("\t{}", FormatFloatReg("Xmm9 ", pContext->Xmm9));
	spdlog::error("\t{}", FormatFloatReg("Xmm10", pContext->Xmm10));
	spdlog::error("\t{}", FormatFloatReg("Xmm11", pContext->Xmm11));
	spdlog::error("\t{}", FormatFloatReg("Xmm12", pContext->Xmm12));
	spdlog::error("\t{}", FormatFloatReg("Xmm13", pContext->Xmm13));
	spdlog::error("\t{}", FormatFloatReg("Xmm14", pContext->Xmm14));
	spdlog::error("\t{}", FormatFloatReg("Xmm15", pContext->Xmm15));
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::vector<std::string> CCrashHandler::FormatLoadedMods()
{
	std::vector<std::string> lines;
	if (!g_pModManager)
	{
		lines.emplace_back("<mod manager unavailable>");
		return lines;
	}

	lines.emplace_back("Enabled mods:");
	bool anyEnabled = false;
	if (g_pModManager)
	{
		for (const Mod& mod : g_pModManager->m_LoadedMods)
		{
			if (!mod.m_bEnabled)
				continue;
			anyEnabled = true;
			lines.emplace_back(fmt::format("{} v{}", mod.Name, mod.Version));
		}
	}
	if (!anyEnabled)
		lines.emplace_back("<none>");

	lines.emplace_back("Disabled mods:");
	bool anyDisabled = false;
	if (g_pModManager)
	{
		for (const Mod& mod : g_pModManager->m_LoadedMods)
		{
			if (mod.m_bEnabled)
				continue;
			anyDisabled = true;
			lines.emplace_back(fmt::format("{} v{}", mod.Name, mod.Version));
		}
	}
	if (!anyDisabled)
		lines.emplace_back("<none>");

	return lines;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::vector<std::string> CCrashHandler::FormatLoadedPlugins()
{
	std::vector<std::string> lines;
	if (!g_pPluginManager)
	{
		lines.emplace_back("<plugin manager unavailable>");
		return lines;
	}

	const auto plugins = g_pPluginManager->GetLoadedPlugins();
	if (plugins.empty())
	{
		lines.emplace_back("<none>");
		return lines;
	}

	for (const Plugin& plugin : plugins)
		lines.emplace_back(plugin.GetName());

	return lines;
}

std::vector<std::string> CCrashHandler::FormatLoadedPaks()
{
	PakGlobalState_s* pakState = Pak_GetGlobals();
	std::vector<std::string> lines;
	if (!pakState)
	{
		lines.emplace_back("<pak state unavailable>");
		return lines;
	}

	std::vector<PakHandle_t> handles = g_pPakLoadManager->GetPakHandles();
	if (handles.empty())
	{
		lines.emplace_back("<none>");
		return lines;
	}

	for (size_t i = 0; i < handles.size(); i++)
	{
		if (handles[i] == PAK_INVALID_HANDLE)
			continue;

		PakLoadedInfo_s& pakInfo = pakState->loadedPaks[handles[i] & PAK_MAX_LOADED_PAKS_MASK];

		std::string formattedLine =
			fmt::format("({}) {} [{}]", static_cast<int>(pakInfo.handle), pakInfo.filename, Pak_StatusToString(pakInfo.status));
		lines.emplace_back(formattedLine);
	}

	return lines;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::vector<std::string> CCrashHandler::FormatModules()
{
	std::vector<std::string> lines;
	HMODULE hModules[1024];
	DWORD cbNeeded;

	if (EnumProcessModules(GetCurrentProcess(), hModules, sizeof(hModules), &cbNeeded))
	{
		for (DWORD i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			CHAR szModulePath[MAX_PATH];
			MODULEINFO moduleInfo;

			if (GetModuleInformation(GetCurrentProcess(), hModules[i], &moduleInfo, sizeof(moduleInfo)))
			{
				GetModuleFileNameExA(GetCurrentProcess(), hModules[i], szModulePath, sizeof(szModulePath));
				const CHAR* pSlash = strrchr(szModulePath, '\\');
				const CHAR* pszModuleFileName = pSlash ? (pSlash + 1) : szModulePath;
				char details[256];
				sprintf_s(
					details,
					"%s, Size: 0x%lX, Region: 0x%p - 0x%p",
					pszModuleFileName,
					static_cast<unsigned long>(moduleInfo.SizeOfImage),
					static_cast<const void*>(hModules[i]),
					static_cast<const void*>(reinterpret_cast<const uint8_t*>(hModules[i]) + moduleInfo.SizeOfImage));
				lines.emplace_back(details);
			}
		}
	}
	else
	{
		lines.emplace_back("<EnumProcessModules failed>");
	}

	return lines;
}

//-----------------------------------------------------------------------------
// Purpose: Writes minidump to disk
//-----------------------------------------------------------------------------
void CCrashHandler::WriteMinidump()
{
	time_t time = std::time(nullptr);
	tm currentTime = *std::localtime(&time);
	std::stringstream stream;
	stream << std::put_time(&currentTime, (GetNorthstarPrefix() + "/logs/nsdump%Y-%m-%d %H-%M-%S.dmp").c_str());

	HANDLE hMinidumpFile = CreateFileA(stream.str().c_str(), GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hMinidumpFile)
	{
		MINIDUMP_EXCEPTION_INFORMATION dumpExceptionInfo;
		dumpExceptionInfo.ThreadId = GetCurrentThreadId();
		dumpExceptionInfo.ExceptionPointers = m_pExceptionInfos;
		dumpExceptionInfo.ClientPointers = false;

		MiniDumpWriteDump(
			GetCurrentProcess(),
			GetCurrentProcessId(),
			hMinidumpFile,
			MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
			&dumpExceptionInfo,
			nullptr,
			nullptr);
		CloseHandle(hMinidumpFile);
	}
	else
		spdlog::error("Failed to write minidump file {}!", stream.str());
}

void CCrashHandler::WriteCrashComment()
{
	time_t time = std::time(nullptr);
	tm currentTime = *std::localtime(&time);
	std::stringstream stream;
	stream << std::put_time(&currentTime, (GetNorthstarPrefix() + "/logs/nscrash%Y-%m-%d %H-%M-%S.log").c_str());

	std::ofstream commentFile(stream.str(), std::ios::out | std::ios::app);
	if (!commentFile.is_open())
	{
		spdlog::error("Failed to open crash comment file {}", stream.str());
		return;
	}

	commentFile << "Unfortunately Ion has crashed, please send this to a developer - you can reach us at:\n* GitHub: "
				   "https://github.com/R2Ion/Ion\n* Discord (in #ion-tech-support): https://discord.gg/UhPwruvSFH\n\n";
	commentFile << "=== Crash Report ===\n";
	commentFile << "Timestamp: " << std::put_time(&currentTime, "%Y-%m-%d %H-%M-%S") << "\n";
	commentFile << fmt::format("Version: {}\n", version);
	commentFile << fmt::format("Patch: {}\n", ION_PATCH);
	if (!m_svCrashReason.empty())
		commentFile << fmt::format("Crash reason: {}\n", m_svCrashReason);
	if (!m_svError.empty())
	{
		commentFile << fmt::format("Encountered an error when gathering crash information!\n");
		commentFile << fmt::format("WinApi Error: {}\n", m_svError.c_str());
	}
	commentFile << fmt::format("Exception: {}\n", GetExceptionString());
	commentFile << fmt::format("At: {} + {}\n", m_svCrashedModule, m_svCrashedOffset);
	if (m_pszActiveHookName && *m_pszActiveHookName)
		commentFile << fmt::format("Active hook: {}\n", m_pszActiveHookName);
	commentFile << "\n";

	commentFile << "=== Callstack ===\n";
	for (const std::string& line : FormatCallstack())
		commentFile << line << "\n";
	commentFile << "\n=== Registers ===\n";
	PCONTEXT pContext = (m_pExceptionInfos != nullptr) ? m_pExceptionInfos->ContextRecord : nullptr;
	if (!pContext)
	{
		commentFile << "<register context unavailable>\n";
		commentFile << "\n=== Loaded Mods ===\n";
		for (const std::string& line : FormatLoadedMods())
			commentFile << line << "\n";

		commentFile << "\n=== Loaded Plugins ===\n";
		for (const std::string& line : FormatLoadedPlugins())
			commentFile << line << "\n";

		commentFile << "\n=== Custom Paks ===\n";
		for (const std::string& line : FormatLoadedPaks())
			commentFile << line << "\n";

		commentFile << "\n=== Loaded Modules ===\n";
		for (const std::string& line : FormatModules())
			commentFile << line << "\n";

		commentFile << "\n=== Recent Log (last 200 lines) ===\n";
		const std::vector<std::string>& lines = !m_PreCrashLogLines.empty() ? m_PreCrashLogLines : NS::log::GetRecentLogLines(200);
		for (const std::string& line : lines)
			commentFile << line << "\n";

		commentFile.close();
		OpenCrashComment(stream.str());
		return;
	}

	commentFile << fmt::format("{}\n", FormatFlags("Flags:", pContext->ContextFlags));

	commentFile << fmt::format("{}\n", FormatIntReg("Rax", pContext->Rax));
	commentFile << fmt::format("{}\n", FormatIntReg("Rcx", pContext->Rcx));
	commentFile << fmt::format("{}\n", FormatIntReg("Rdx", pContext->Rdx));
	commentFile << fmt::format("{}\n", FormatIntReg("Rbx", pContext->Rbx));
	commentFile << fmt::format("{}\n", FormatIntReg("Rsp", pContext->Rsp));
	commentFile << fmt::format("{}\n", FormatIntReg("Rbp", pContext->Rbp));
	commentFile << fmt::format("{}\n", FormatIntReg("Rsi", pContext->Rsi));
	commentFile << fmt::format("{}\n", FormatIntReg("Rdi", pContext->Rdi));
	commentFile << fmt::format("{}\n", FormatIntReg("R8 ", pContext->R8));
	commentFile << fmt::format("{}\n", FormatIntReg("R9 ", pContext->R9));
	commentFile << fmt::format("{}\n", FormatIntReg("R10", pContext->R10));
	commentFile << fmt::format("{}\n", FormatIntReg("R11", pContext->R11));
	commentFile << fmt::format("{}\n", FormatIntReg("R12", pContext->R12));
	commentFile << fmt::format("{}\n", FormatIntReg("R13", pContext->R13));
	commentFile << fmt::format("{}\n", FormatIntReg("R14", pContext->R14));
	commentFile << fmt::format("{}\n", FormatIntReg("R15", pContext->R15));
	commentFile << fmt::format("{}\n", FormatIntReg("Rip", pContext->Rip));

	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm0 ", pContext->Xmm0));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm1 ", pContext->Xmm1));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm2 ", pContext->Xmm2));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm3 ", pContext->Xmm3));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm4 ", pContext->Xmm4));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm5 ", pContext->Xmm5));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm6 ", pContext->Xmm6));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm7 ", pContext->Xmm7));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm8 ", pContext->Xmm8));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm9 ", pContext->Xmm9));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm10", pContext->Xmm10));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm11", pContext->Xmm11));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm12", pContext->Xmm12));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm13", pContext->Xmm13));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm14", pContext->Xmm14));
	commentFile << fmt::format("{}\n", FormatFloatReg("Xmm15", pContext->Xmm15));

	commentFile << "\n=== Stack Dump (from SP, 512 bytes) ===\n";
	for (const std::string& line : FormatStackMemoryDump(pContext, 512))
		commentFile << line << "\n";

	commentFile << "\n=== Loaded Mods ===\n";
	for (const std::string& line : FormatLoadedMods())
		commentFile << line << "\n";

	commentFile << "\n=== Loaded Plugins ===\n";
	for (const std::string& line : FormatLoadedPlugins())
		commentFile << line << "\n";

	commentFile << "\n=== Custom Paks ===\n";
	for (const std::string& line : FormatLoadedPaks())
		commentFile << line << "\n";

	commentFile << "\n=== Loaded Modules ===\n";
	for (const std::string& line : FormatModules())
		commentFile << line << "\n";

	commentFile << "\n=== Recent Log (last 200 lines) ===\n";
	const std::vector<std::string>& lines = !m_PreCrashLogLines.empty() ? m_PreCrashLogLines : NS::log::GetRecentLogLines(200);
	for (const std::string& line : lines)
		commentFile << line << "\n";

	commentFile << "\n=== System Information ===\n";
	commentFile << fmt::format("Operating System: {}\n", GetWindowsVersionFormatted());

	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	switch (sysInfo.wProcessorArchitecture)
	{
	case PROCESSOR_ARCHITECTURE_AMD64:
		commentFile << "Processor Architecture: x64 (AMD or Intel)\n";
		break;
	case PROCESSOR_ARCHITECTURE_ARM:
		commentFile << "Processor Architecture: ARM\n";
		break;
	case PROCESSOR_ARCHITECTURE_ARM64:
		commentFile << "Processor Architecture: ARM64\n";
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		commentFile << "Processor Architecture: x86\n";
		break;
	default:
		commentFile << "Processor Architecture: Unknown\n";
		break;
	}

	commentFile << fmt::format("Number of Processors: {}\n", sysInfo.dwNumberOfProcessors);

	std::string processorName;

	HKEY hKey;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		processorName = "Unknown Processor";

	char buffer[256];
	DWORD bufSize = sizeof(buffer);
	if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)buffer, &bufSize) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		processorName = "Unknown Processor";
	}

	processorName = buffer;
	commentFile << fmt::format("CPU: {}\n", processorName);

	DWORD mhz;
	bufSize = sizeof(mhz);
	if (RegQueryValueExA(hKey, "~MHz", NULL, NULL, (LPBYTE)&mhz, &bufSize) == ERROR_SUCCESS)
		commentFile << fmt::format("CPU Speed: {} MHz\n", mhz);

	RegCloseKey(hKey);

	MEMORYSTATUSEX memStatus;
	memStatus.dwLength = sizeof(memStatus);
	if (GlobalMemoryStatusEx(&memStatus))
	{
		commentFile << fmt::format("Total Physical Memory: {} MB\n", memStatus.ullTotalPhys / (1024 * 1024));
		commentFile << fmt::format("Available Physical Memory: {} MB\n", memStatus.ullAvailPhys / (1024 * 1024));
		commentFile << fmt::format("Memory Load: {}%\n", memStatus.dwMemoryLoad);
	}

	char exePath[MAX_PATH];
	GetModuleFileNameA(NULL, exePath, MAX_PATH);

	std::string driveLetter = std::string(exePath).substr(0, 3);

	ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
	if (GetDiskFreeSpaceExA(driveLetter.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes))
	{
		commentFile << fmt::format("Total Disk Space ({}): {} GB\n", driveLetter, totalBytes.QuadPart / (1024 * 1024 * 1024));
		commentFile << fmt::format("Free Disk Space ({}): {} GB\n", driveLetter, totalFreeBytes.QuadPart / (1024 * 1024 * 1024));
	}

	const GPUInfo_s gpu = GetBestGpuInfoDxgi();
	if (gpu.found)
	{
		commentFile << "GPU Model: " << gpu.name << "\n";
		commentFile << "VRAM: " << (gpu.dedicatedVramBytes / (1024ull * 1024ull)) << " MB\n";

		if (!gpu.allAdapters.empty())
		{
			commentFile << "Detected Graphics Adapters:\n";
			for (const auto& [name, vram] : gpu.allAdapters)
				commentFile << "  - " << name << " (" << (vram / (1024ull * 1024ull)) << " MB)\n";
		}
	}
	else
	{
		commentFile << "GPU Model: [Unable to retrieve GPU information via DXGI]\n";
	}

	commentFile.close();

	OpenCrashComment(stream.str());
}

void CCrashHandler::OpenCrashComment(std::string filepath)
{
	char cmdLine[1024];
	sprintf_s(cmdLine, "notepad.exe \"%s\"", filepath.c_str()); // Build command line with the file path
	STARTUPINFOA si = {0};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi = {0};

	if (CreateProcessA(
			NULL, // Application name: NULL means the executable is the first token of the command line.
			cmdLine, // Command line (must be mutable)
			NULL, // Process handle not inheritable
			NULL, // Thread handle not inheritable
			FALSE, // Set handle inheritance to FALSE
			0, // No creation flags
			NULL, // Use parent's environment block
			NULL, // Use parent's starting directory
			&si, // Pointer to STARTUPINFO structure
			&pi // Pointer to PROCESS_INFORMATION structure
			))
	{
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		Sleep(100);
	}
}

GPUInfo_s CCrashHandler::GetBestGpuInfoDxgi()
{
	GPUInfo_s info;
	auto cmd = GetCommandLineA();
	bool isDedi = strstr(cmd, "-dedicated"); // well, this one has to be a real argument

	if(GetCurrentProcessExeName() == L"r2ds.exe")
		isDedi = true;

	if(isDedi)
		return info; // Don't attempt to get GPU info on dedicated servers, as they may not have a GPU or the necessary drivers installed

	Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) || !factory)
		return info;

	uint64_t bestVram = 0;
	std::string bestName;

	for (UINT i = 0;; i++)
	{
		Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
		if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND)
			break;
		if (!adapter)
			continue;

		DXGI_ADAPTER_DESC1 desc {};
		if (FAILED(adapter->GetDesc1(&desc)))
			continue;

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		const std::wstring widestr = desc.Description;
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

		const std::string name = converter.to_bytes(widestr);
		const uint64_t vram = static_cast<uint64_t>(desc.DedicatedVideoMemory);

		info.allAdapters.emplace_back(name, vram);

		if (vram >= bestVram)
		{
			bestVram = vram;
			bestName = name;
		}
	}

	if (!bestName.empty())
	{
		info.found = true;
		info.name = std::move(bestName);
		info.dedicatedVramBytes = bestVram;
	}

	return info;
}

std::string CCrashHandler::GetWindowsVersionFormatted()
{
	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	if (!ntdll)
		return "Unknown Windows version";

	typedef const char*(CDECL* wine_get_host_version_type)(const char**, const char**);
	wine_get_host_version_type wine_get_host_version;

	typedef const char*(CDECL* wine_get_build_id_type)(void);
	wine_get_build_id_type wine_get_build_id;


	wine_get_host_version = (wine_get_host_version_type)GetProcAddress(ntdll, "wine_get_host_version");
	if (wine_get_host_version)
	{
		const char* sysname;
		wine_get_host_version(&sysname, NULL);

		wine_get_build_id = (wine_get_build_id_type)GetProcAddress(ntdll, "wine_get_build_id");
		return fmt::format("Wine {} (build {})", sysname, wine_get_build_id ? wine_get_build_id() : "unknown");
	}

	RtlGetVersionPtr rtlGetVersion = (RtlGetVersionPtr)GetProcAddress(ntdll, "RtlGetVersion");
	if (!rtlGetVersion)
		return "Unknown Windows version";

	RTL_OSVERSIONINFOW osvi = {0};
	osvi.dwOSVersionInfoSize = sizeof(osvi);

	NTSTATUS status = rtlGetVersion(&osvi);
	if (status != 0)
		return "Unknown Windows version";

	std::stringstream result;

	std::string productName = "Windows";
	std::string displayVersion = "";
	std::string ubr = "";

	HKEY hKey;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		char buffer[256];
		DWORD bufSize = sizeof(buffer);

		if (RegQueryValueExA(hKey, "ProductName", NULL, NULL, (LPBYTE)buffer, &bufSize) == ERROR_SUCCESS)
		{
			productName = buffer;
		}

		bufSize = sizeof(buffer);
		if (RegQueryValueExA(hKey, "DisplayVersion", NULL, NULL, (LPBYTE)buffer, &bufSize) == ERROR_SUCCESS)
		{
			displayVersion = buffer;
		}

		DWORD ubrValue = 0;
		bufSize = sizeof(ubrValue);
		if (RegQueryValueExA(hKey, "UBR", NULL, NULL, (LPBYTE)&ubrValue, &bufSize) == ERROR_SUCCESS)
		{
			ubr = std::to_string(ubrValue);
		}

		RegCloseKey(hKey);
	}

	result << productName;

	if (!displayVersion.empty())
	{
		result << " (Version " << displayVersion;
		if (!ubr.empty())
		{
			result << ", Build " << osvi.dwBuildNumber << "." << ubr;
		}
		else
		{
			result << ", Build " << osvi.dwBuildNumber;
		}
		result << ")";
	}
	else
	{
		result << " (Version " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion;
		result << ", Build " << osvi.dwBuildNumber << ")";
	}

#ifdef _WIN64
	result << " 64-bit";
#else
	BOOL isWow64 = FALSE;
	if (IsWow64Process(GetCurrentProcess(), &isWow64) && isWow64)
	{
		result << " 32-bit on 64-bit";
	}
	else
	{
		result << " 32-bit";
	}
#endif

	return result.str();
}

bool CCrashHandler::TryReadMemory(const void* src, void* dst, size_t size)
{
	if (!src || !dst || size == 0)
		return false;

	__try
	{
		std::memcpy(dst, src, size);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

std::vector<std::string> CCrashHandler::MakeHexDumpLines(uintptr_t baseAddress, const uint8_t* data, size_t size)
{
	std::vector<std::string> out;
	if (!data || size == 0)
		return out;

	constexpr size_t kBytesPerLine = 16;
	out.reserve((size + kBytesPerLine - 1) / kBytesPerLine);

	for (size_t i = 0; i < size; i += kBytesPerLine)
	{
		const size_t lineSize = std::min(kBytesPerLine, size - i);
		const uint64_t addr = static_cast<uint64_t>(baseAddress + i);

		std::string bytes;
		bytes.reserve(kBytesPerLine * 3);
		for (size_t j = 0; j < kBytesPerLine; j++)
		{
			if (j < lineSize)
				bytes += fmt::format("{:02X} ", data[i + j]);
			else
				bytes += "   ";
		}

		std::string ascii;
		ascii.reserve(kBytesPerLine);
		for (size_t j = 0; j < lineSize; j++)
		{
			const unsigned char c = static_cast<unsigned char>(data[i + j]);
			ascii.push_back((c >= 32 && c <= 126) ? static_cast<char>(c) : '.');
		}

		out.emplace_back(fmt::format("{:#018x}: {}|{}|", addr, bytes, ascii));
	}

	return out;
}

std::vector<std::string> CCrashHandler::FormatStackMemoryDump(PCONTEXT context, size_t bytesToDump)
{
	std::vector<std::string> out;
	if (!context || bytesToDump == 0)
		return out;

#ifdef _WIN64
	uintptr_t sp = static_cast<uintptr_t>(context->Rsp);
#else
	uintptr_t sp = static_cast<uintptr_t>(context->Esp);
#endif
	if (sp == 0)
		return out;

	sp &= ~static_cast<uintptr_t>(0xF);

	std::vector<uint8_t> buffer(bytesToDump);
	if (!TryReadMemory(reinterpret_cast<const void*>(sp), buffer.data(), buffer.size()))
	{
		out.emplace_back("<unable to read stack memory>");
		return out;
	}

	return MakeHexDumpLines(sp, buffer.data(), buffer.size());
}

//-----------------------------------------------------------------------------
CCrashHandler* g_pCrashHandler = nullptr;
