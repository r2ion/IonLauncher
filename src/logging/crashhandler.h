#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <DbgHelp.h>

//-----------------------------------------------------------------------------
// Purpose: Exception handling
//-----------------------------------------------------------------------------
class CCrashHandler
{
public:
	CCrashHandler();
	~CCrashHandler();

	void Init();
	void Shutdown();

	void Lock() { m_Mutex.lock(); }

	void Unlock() { m_Mutex.unlock(); }

	void SetState(bool bState) { m_bState = bState; }

	bool GetState() const { return m_bState; }

	void SetAllFatal(bool bState) { m_bAllExceptionsFatal = bState; }

	bool GetAllFatal() const { return m_bAllExceptionsFatal; }

	//-----------------------------------------------------------------------------
	// Exception helpers
	//-----------------------------------------------------------------------------
	void SetExceptionInfos(EXCEPTION_POINTERS* pExceptionPointers);

	void SetCrashedModule();

	const CHAR* GetExceptionString() const;
	const CHAR* GetExceptionString(DWORD dwExceptionCode) const;

	bool IsExceptionFatal() const;
	bool IsExceptionFatal(DWORD dwExceptionCode) const;

	//-----------------------------------------------------------------------------
	// Formatting
	//-----------------------------------------------------------------------------
	void ShowPopUpMessage();

	void FormatException();
	void FormatCallstack();
	std::string FormatFlags(const CHAR* pszRegister, DWORD nValue);
	std::string FormatIntReg(const CHAR* pszRegister, DWORD64 nValue);
	std::string FormatFloatReg(const CHAR* pszRegister, M128A nValue);
	void FormatRegisters();
	void FormatLoadedMods();
	void FormatLoadedPlugins();
	void FormatModules();

	void PlayCrashSound(int resourceId);

	// Capture recent log lines before crash-report logging writes into the log.
	void CapturePreCrashLog(size_t maxLines = 200);

	bool TryCopyCString(const char* src, char* dst, size_t dstSize);
    bool TrySymFromAddrSafe(HANDLE process, DWORD64 address, DWORD64* displacement, PSYMBOL_INFO symbol);
    bool TrySymGetLineFromAddr64Safe(HANDLE process, DWORD64 address, DWORD* displacement, IMAGEHLP_LINE64* line);

	//-----------------------------------------------------------------------------
	// Minidump
	//-----------------------------------------------------------------------------
	void WriteMinidump();
	void WriteCrashComment();
	void OpenCrashComment(std::string filepath);

private:
	PVOID m_hExceptionFilter;
	EXCEPTION_POINTERS* m_pExceptionInfos;

	bool m_bHasSetConsolehandler;
	bool m_bAllExceptionsFatal;
	bool m_bHasShownCrashMsg;
	bool m_bState;

	std::string m_svCrashedModule;
	std::string m_svCrashedOffset;

	std::string m_svError;

	std::vector<std::string> m_PreCrashLogLines;

	std::mutex m_Mutex;
	bool m_bSymInit;
};

extern CCrashHandler* g_pCrashHandler;
