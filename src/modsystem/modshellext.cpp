#include "modshellext.h"

#include "modsystem/modinstaller.h"
#include "modsystem/modmanager.h"
#include "modsystem/platform/modworkshop.h"

#include <Windows.h>
#include <spdlog/spdlog.h>

#include <thread>

std::optional<std::string> CModShellExtension::FindUriArgument(std::string_view commandLine, std::string_view schemePrefix)
{
	const size_t start = commandLine.find(schemePrefix);
	if (start == std::string_view::npos)
		return std::nullopt;

	size_t end = commandLine.find_first_of(" \"", start);
	if (end == std::string_view::npos)
		end = commandLine.size();

	std::string uri(commandLine.substr(start, end - start));
	if (!uri.empty() && uri.back() == '"')
		uri.pop_back();

	return uri;
}

void CModShellExtension::HandleUri(std::string_view uri)
{
	if (const std::optional<uint64_t> modId = CModWorkshopClient::TryParseInstallId(uri))
	{
		spdlog::info("Received ModWorkshop install URI. Mod ID: {}", *modId);
		if (!g_pModManager)
			StorePendingWorkshopInstall(*modId);
		else if (!CModInstallService::Get().Request(ModInstallAction::Replace, *modId))
			spdlog::warn("A ModWorkshop operation is already active; URI install {} was not queued", *modId);
		return;
	}

	spdlog::warn("Unhandled r2ns URI: {}", uri);
}

std::optional<std::string> CModShellExtension::GetCommandLineUri() const
{
	const char* commandLine = GetCommandLineA();
	return commandLine ? FindUriArgument(commandLine, "r2ns://") : std::nullopt;
}

bool CModShellExtension::ForwardToRunningInstance(std::string_view uri) const
{
	const HANDLE pipe = CreateFileA(PIPE_NAME.data(),
		GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr);

	if (pipe == INVALID_HANDLE_VALUE)
		return false;

	DWORD written = 0;
	const BOOL succeeded = WriteFile(pipe, uri.data(), static_cast<DWORD>(uri.size()), &written, nullptr);
	CloseHandle(pipe);

	return succeeded && written == uri.size();
}

void CModShellExtension::RunUriServer()
{
	for (;;)
	{
		const HANDLE pipe = CreateNamedPipeA(PIPE_NAME.data(),
			PIPE_ACCESS_INBOUND,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			1,
			4096,
			4096,
			0,
			nullptr);

		if (pipe == INVALID_HANDLE_VALUE)
			return;

		const BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : GetLastError() == ERROR_PIPE_CONNECTED;
		if (connected)
		{
			char buffer[4096] = {};
			DWORD bytesRead = 0;
			if (ReadFile(pipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
				HandleUri(std::string_view(buffer, bytesRead));
		}

		DisconnectNamedPipe(pipe);
		CloseHandle(pipe);
	}
}

void CModShellExtension::StartUriServer()
{
	std::thread(&CModShellExtension::RunUriServer, this).detach();
}

void CModShellExtension::StorePendingWorkshopInstall(uint64_t id)
{
	if (id == 0)
		return;
	std::scoped_lock lock(m_PendingWorkshopMutex);
	m_PendingWorkshopId = id;
}

std::optional<uint64_t> CModShellExtension::TakePendingWorkshopInstall()
{
	std::scoped_lock lock(m_PendingWorkshopMutex);

	std::optional<uint64_t> id = m_PendingWorkshopId;
	m_PendingWorkshopId.reset();
	return id;
}
