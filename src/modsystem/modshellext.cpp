#include "modshellext.h"

#include <string_view>
#include <windows.h>

#include <spdlog/spdlog.h>

#include "modsystem/platform/modworkshop.h"

static const char* kModShellPipeName = "\\\\.\\pipe\\NorthstarUriPipe";

std::optional<std::string> Mod_FindUriArgument(std::string_view commandLine, std::string_view schemePrefix)
{
	size_t start = commandLine.find(schemePrefix);
	if (start == std::string_view::npos)
		return std::nullopt;

	size_t end = commandLine.find_first_of(" \"", start);
	if (end == std::string_view::npos)
		end = commandLine.size();

	std::string uri = std::string(commandLine.substr(start, end - start));
	if (!uri.empty() && uri.back() == '"')
		uri.pop_back();

	return uri;
}

void HandleModShellExtensionUri(const std::string& uri)
{
	if (auto modId = ModWorkshop_TryParseInstallId(uri))
	{
		spdlog::info("Received ModWorkshop install URI. Mod ID: {}", *modId);
		// TODO: After local ModWorkshop installs are supported, write a .mws_id marker
		// into the installed package directory so the mod becomes managed.
		return;
	}

	spdlog::warn("Unhandled r2ns URI: {}", uri);
}

std::optional<std::string> Mod_TryGetUriFromCommandLine()
{
	const char* cmdLine = GetCommandLineA();
	if (!cmdLine)
		return std::nullopt;

	return Mod_FindUriArgument(std::string_view(cmdLine), "r2ns://");
}

bool Mod_ForwardUriToRunningInstance(const std::string& uri)
{
	HANDLE pipe = CreateFileA(
		kModShellPipeName,
		GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr);

	if (pipe == INVALID_HANDLE_VALUE)
		return false;

	DWORD written = 0;
	BOOL ok = WriteFile(pipe, uri.c_str(), static_cast<DWORD>(uri.size()), &written, nullptr);
	CloseHandle(pipe);

	return ok && written == uri.size();
}

static DWORD WINAPI Mod_PipeServerThread(LPVOID)
{
	for (;;)
	{
		HANDLE pipe = CreateNamedPipeA(
			kModShellPipeName,
			PIPE_ACCESS_INBOUND,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			1,
			4096,
			4096,
			0,
			nullptr);

		if (pipe == INVALID_HANDLE_VALUE)
			return 0;

		BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
		if (connected)
		{
			char buffer[4096] = {};
			DWORD read = 0;
			if (ReadFile(pipe, buffer, sizeof(buffer) - 1, &read, nullptr) && read > 0)
			{
				buffer[read] = '\0';
				HandleModShellExtensionUri(std::string(buffer));
			}
		}

		DisconnectNamedPipe(pipe);
		CloseHandle(pipe);
	}
}

void Mod_StartUriServer()
{
	DWORD threadId = 0;
	HANDLE thread = CreateThread(nullptr, 0, Mod_PipeServerThread, nullptr, 0, &threadId);
	if (thread)
		CloseHandle(thread);
}

void HandleModShellExtension()
{
	auto uri = Mod_TryGetUriFromCommandLine();
	if (!uri)
		return;

	HandleModShellExtensionUri(*uri);
}
