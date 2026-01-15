#include "logging.h"
#include "crashhandler.h"
#include <string>
#include <vector>
#include <fstream>
#include "config/profile.h"
#include "dedicated/dedicated.h"
#include "mods/modmanager.h"
#include "plugins/pluginmanager.h"
#include "plugins/plugins.h"
#include "util/version.h"
#include "crash_sounds.h"

#include <ns_version.h>
#include <DbgHelp.h>
#include <Mmsystem.h>
#include <minidumpapiset.h>

#pragma comment(lib, "dbghelp.lib")

#define CRASHHANDLER_MAX_FRAMES 32
#define CRASHHANDLER_GETMODULEHANDLE_FAIL "GetModuleHandleExA failed!"

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

	g_pCrashHandler->PlayCrashSound(CRASH_SOUND);

	// Don't run if a debbuger is attached
	if (IsDebuggerPresent())
	{
		g_pCrashHandler->Unlock();
		return EXCEPTION_CONTINUE_SEARCH;
	}

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
	g_pCrashHandler->FormatCallstack();
	g_pCrashHandler->FormatRegisters();
	g_pCrashHandler->FormatLoadedMods();
	g_pCrashHandler->FormatLoadedPlugins();
	g_pCrashHandler->FormatModules();

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
}
//-----------------------------------------------------------------------------
// Purpose: Sets the exception stirngs for message box
//-----------------------------------------------------------------------------
void CCrashHandler::SetCrashedModule()
{
	LPCSTR pCrashAddress = static_cast<LPCSTR>(m_pExceptionInfos->ExceptionRecord->ExceptionAddress);
	HMODULE hCrashedModule;
	if (!GetModuleHandleExA(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, pCrashAddress, &hCrashedModule))
	{
		m_svCrashedModule = CRASHHANDLER_GETMODULEHANDLE_FAIL;
		m_svCrashedOffset = "";

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
	if (!GetModuleFileNameExA(GetCurrentProcess(), hCrashedModule, szCrashedModulePath, sizeof(szCrashedModulePath)))
	{
		m_svCrashedModule = CRASHHANDLER_GETMODULEHANDLE_FAIL;
		m_svCrashedOffset = "";
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
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCrashHandler::FormatCallstack()
{
	spdlog::error("Callstack:");

	PVOID pFrames[CRASHHANDLER_MAX_FRAMES];

	int iFrames = RtlCaptureStackBackTrace(0, CRASHHANDLER_MAX_FRAMES, pFrames, NULL);

	bool bSkipExceptionHandlingFrames = true;
	if (m_svCrashedOffset.empty())
		bSkipExceptionHandlingFrames = false;

	for (int i = 0; i < iFrames; i++)
	{
		std::string svModuleFileName;

		uintptr_t addr = reinterpret_cast<uintptr_t>(pFrames[i]);
		LPCSTR pAddress = reinterpret_cast<LPCSTR>(addr);

		HMODULE hModule = nullptr;
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, pAddress, &hModule))
		{
			svModuleFileName = CRASHHANDLER_GETMODULEHANDLE_FAIL;
		}
		else
		{
			CHAR szModulePath[MAX_PATH] = {};
			if (GetModuleFileNameExA(GetCurrentProcess(), hModule, szModulePath, sizeof(szModulePath)))
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
		uintptr_t offset = moduleBase ? (addr - moduleBase) : addr;
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

					spdlog::error(
						"\t{}!{}+0x{:x} [{}:{}]",
						svModuleFileName,
						pSym->Name,
						static_cast<uint64_t>(displacement),
						svFileName,
						line.LineNumber);
				}
				else
				{
					spdlog::error("\t{}!{}+0x{:x}", svModuleFileName, pSym->Name, static_cast<uint64_t>(displacement));
				}

				printed = true;
			}
		}

		if (!printed)
			spdlog::error("\t{} + {:#x}", svModuleFileName, static_cast<uint64_t>(offset));
	}
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
void CCrashHandler::FormatLoadedMods()
{
	if (g_pModManager)
	{
		spdlog::error("Enabled mods:");
		for (const Mod& mod : g_pModManager->m_LoadedMods)
		{
			if (!mod.m_bEnabled)
				continue;

			spdlog::error("\t{}", mod.Name);
		}

		spdlog::error("Disabled mods:");
		for (const Mod& mod : g_pModManager->m_LoadedMods)
		{
			if (mod.m_bEnabled)
				continue;

			spdlog::error("\t{}", mod.Name);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCrashHandler::FormatLoadedPlugins()
{
	if (g_pPluginManager)
	{
		spdlog::error("Loaded Plugins:");
		for (const Plugin& plugin : g_pPluginManager->GetLoadedPlugins())
		{
			spdlog::error("\t{}", plugin.GetName());
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCrashHandler::FormatModules()
{
	spdlog::error("Loaded modules:");
	HMODULE hModules[1024];
	DWORD cbNeeded;

	if (EnumProcessModules(GetCurrentProcess(), hModules, sizeof(hModules), &cbNeeded))
	{
		for (DWORD i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			CHAR szModulePath[MAX_PATH];
			if (GetModuleFileNameExA(GetCurrentProcess(), hModules[i], szModulePath, sizeof(szModulePath)))
			{
				const CHAR* pszModuleFileName = strrchr(szModulePath, '\\') + 1;
				spdlog::error("\t{}", pszModuleFileName);
			}
		}
	}
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
	stream << std::put_time(&currentTime, (GetNorthstarPrefix() + "/logs/nscrash%Y-%m-%d %H-%M-%S.txt").c_str());

	std::ofstream commentFile(stream.str(), std::ios::out | std::ios::app);
	commentFile << "Unfortunately, Ion has crashed, please send this to a developer, you can reach us at:\n* GitHub: https://github.com/R2Ion/Ion\n* Discord (in #ion-tech-support): https://discord.gg/UhPwruvSFH\n\n";
	commentFile << "=== Crash Report ===\n";
	commentFile << fmt::format("Version: {}\n", version);
	commentFile << fmt::format("Patch: {}\n", ION_PATCH);
	if (!m_svError.empty())
	{
		commentFile << fmt::format("Encountered an error when gathering crash information!\n");
		commentFile << fmt::format("WinApi Error: {}\n", m_svError.c_str());
	}
	commentFile << fmt::format("Exception: {}\n", GetExceptionString());
	commentFile << fmt::format("At: {} + {}\n\n", m_svCrashedModule, m_svCrashedOffset);

	commentFile << "=== Callstack ===\n";
	PVOID pFrames[CRASHHANDLER_MAX_FRAMES];
	int iFrames = RtlCaptureStackBackTrace(0, CRASHHANDLER_MAX_FRAMES, pFrames, NULL);
	bool bSkipExceptionHandlingFrames = true;
	if (m_svCrashedOffset.empty())
		bSkipExceptionHandlingFrames = false;
	for (int i = 0; i < iFrames; i++)
	{
		std::string svModuleFileName;

		uintptr_t addr = reinterpret_cast<uintptr_t>(pFrames[i]);
		LPCSTR pAddress = reinterpret_cast<LPCSTR>(addr);

		HMODULE hModule = nullptr;
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, pAddress, &hModule))
		{
			svModuleFileName = CRASHHANDLER_GETMODULEHANDLE_FAIL;
		}
		else
		{
			CHAR szModulePath[MAX_PATH] = {};
			if (GetModuleFileNameExA(GetCurrentProcess(), hModule, szModulePath, sizeof(szModulePath)))
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
		uintptr_t offset = moduleBase ? (addr - moduleBase) : addr;
		std::string svCrashOffset = fmt::format("{:#x}", static_cast<uint64_t>(offset));

		if (bSkipExceptionHandlingFrames)
		{
			if (m_svCrashedModule == svModuleFileName && m_svCrashedOffset == svCrashOffset)
				bSkipExceptionHandlingFrames = false;
			else
				continue;
		}

		commentFile << fmt::format("{} + {:#x}\n", svModuleFileName, static_cast<uint64_t>(offset));
	}
	commentFile << "\n=== Registers ===\n";
	PCONTEXT pContext = m_pExceptionInfos->ContextRecord;

	spdlog::error("{}", FormatFlags("Flags:", pContext->ContextFlags));

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

	commentFile << "\n=== Loaded Mods ===\n";
	if (g_pModManager)
	{
		commentFile << "Enabled mods:\n";
		for (const Mod& mod : g_pModManager->m_LoadedMods)
		{
			if (!mod.m_bEnabled)
				continue;

			commentFile << fmt::format("\t{} v{}\n", mod.Name, mod.Version);
		}

		commentFile << "Disabled mods:\n";
		for (const Mod& mod : g_pModManager->m_LoadedMods)
		{
			if (mod.m_bEnabled)
				continue;

			commentFile << fmt::format("\t{} v{}\n", mod.Name, mod.Version);
		}
	}

	commentFile << "\n=== Recent Log (last 200 lines) ===\n";
	const std::vector<std::string>& lines = !m_PreCrashLogLines.empty() ? m_PreCrashLogLines : NS::log::GetRecentLogLines(200);
	for (const std::string& line : lines)
		commentFile << line << "\n";

	commentFile.close();

	OpenCrashComment(stream.str());
}

void CCrashHandler::OpenCrashComment(std::string filepath)
{
	char cmdLine[1024];
    sprintf_s(cmdLine, "notepad.exe \"%s\"", filepath.c_str());  // Build command line with the file path
    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = { 0 };

    if (CreateProcessA(
        NULL,        // Application name: NULL means the executable is the first token of the command line.
        cmdLine,     // Command line (must be mutable)
        NULL,        // Process handle not inheritable
        NULL,        // Thread handle not inheritable
        FALSE,       // Set handle inheritance to FALSE
        0,           // No creation flags
        NULL,        // Use parent's environment block
        NULL,        // Use parent's starting directory
        &si,         // Pointer to STARTUPINFO structure
        &pi          // Pointer to PROCESS_INFORMATION structure
    ))
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        Sleep(100);
    }
}

//-----------------------------------------------------------------------------
CCrashHandler* g_pCrashHandler = nullptr;
