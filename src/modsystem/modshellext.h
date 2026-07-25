#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

class CModShellExtension final
{
public:
	static CModShellExtension& Get()
	{
		static CModShellExtension* s_pInstance = new CModShellExtension;
		return *s_pInstance;
	}

	void HandleUri(std::string_view uri);
	std::optional<std::string> GetCommandLineUri() const;
	bool ForwardToRunningInstance(std::string_view uri) const;
	void StartUriServer();
	std::optional<uint64_t> TakePendingWorkshopInstall();

	CModShellExtension(const CModShellExtension&) = delete;
	CModShellExtension& operator=(const CModShellExtension&) = delete;

private:
	static constexpr std::string_view PIPE_NAME = R"(\\.\pipe\NorthstarUriPipe)";

	CModShellExtension() = default;
	~CModShellExtension() = delete;

	static std::optional<std::string> FindUriArgument(std::string_view commandLine, std::string_view schemePrefix);
	void StorePendingWorkshopInstall(uint64_t id);
	void RunUriServer();

	std::mutex m_PendingWorkshopMutex;
	std::optional<uint64_t> m_PendingWorkshopId;
};
