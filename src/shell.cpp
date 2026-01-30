#include "shell.h"

#include <string>
#include <windows.h>

bool Shell_RegisterProtocol(const std::filesystem::path& launcherPath)
{
	HKEY key = nullptr;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\r2ns", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) != ERROR_SUCCESS)
		return false;

	const wchar_t* protocolName = L"URL:R2Northstar Protocol";
	RegSetValueExW(key, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(protocolName),
		static_cast<DWORD>((wcslen(protocolName) + 1) * sizeof(wchar_t)));

	const wchar_t* emptyValue = L"";
	RegSetValueExW(key, L"URL Protocol", 0, REG_SZ, reinterpret_cast<const BYTE*>(emptyValue),
		static_cast<DWORD>((wcslen(emptyValue) + 1) * sizeof(wchar_t)));

	HKEY commandKey = nullptr;
	if (RegCreateKeyExW(key, L"shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &commandKey, NULL) != ERROR_SUCCESS)
	{
		RegCloseKey(key);
		return false;
	}

	std::wstring command = L"\"";
	command += launcherPath.wstring();
	command += L"\" \"%1\"";
	RegSetValueExW(commandKey, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
		static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));

	RegCloseKey(commandKey);
	RegCloseKey(key);
	return true;
}
