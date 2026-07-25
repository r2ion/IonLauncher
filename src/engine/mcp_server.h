#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace httplib
{
class Server;
}

class CCommand;
enum class ScriptContext : int;

namespace MCPServer
{
using json = nlohmann::json;

class Server final
{
  public:
    static Server& GetInstance();

    void Initialize();
    bool StartHTTP(int port = 8765);
    void Stop();
    bool IsRunning() const;
    int GetPort() const;
    void CaptureLogLine(std::string_view line);

  private:
    class LogCaptureSink;
    struct Tool
    {
        std::string name;
        std::string title;
        std::string description;
        json inputSchema;
    };

    class EngineTaskQueue final
    {
      public:
        json RunAndWait(std::function<json()> function, std::chrono::milliseconds timeout = std::chrono::seconds(30));
        void Resolve();

      private:
        struct Task
        {
            std::function<json()> function;
            std::promise<json> promise;
            std::atomic_bool cancelled = false;
        };

        static constexpr size_t TASK_LIMIT = 64;

        std::mutex m_Mutex;
        std::deque<std::shared_ptr<Task>> m_Tasks;
    };

    class ConsoleOutputCapture final
    {
      public:
        void Start(std::string guid);
        void Stop();
        void AddLine(std::string_view line);
        std::vector<std::string> TakeLines();
        bool WaitForCompletion(std::string_view guid, std::chrono::milliseconds timeout);

      private:
        enum class State
        {
            Idle,
            WaitingForStart,
            Capturing,
            Completed,
        };

        static constexpr size_t LINE_LIMIT = 500;

        std::mutex m_Mutex;
        std::condition_variable m_Condition;
        std::string m_Guid;
        std::vector<std::string> m_Lines;
        State m_State = State::Idle;
        bool m_Truncated = false;
    };

    static constexpr const char* PROTOCOL_VERSION = "2025-06-18";
    static constexpr auto CAPTURE_TIMEOUT = std::chrono::seconds(10);

    Server() = default;

    static void StartHTTPCommand(const CCommand& command);
    static void StopCommand(const CCommand& command);
    static void StatusCommand(const CCommand& command);
    static void ResolveEngineTasksCommand(const CCommand& command);

    static std::string ToLowerASCII(std::string_view value);
    static std::optional<ScriptContext> ParseContext(std::string_view value);
    static std::string GenerateGuid();
	static std::string MakeDeferredSquirrelScript(std::string_view script, std::string_view guid);
	static std::string MakeEchoCommand(std::string_view marker);
    static std::string AppendNewline(std::string command);
    static void QueueConsoleText(const std::string& text);
    static int BoundedInteger(const json& object, const char* name, int defaultValue, int minimum, int maximum);
    static json MakeToolResult(std::string text, bool isError);

    json RunOnEngineThreadAndWait(std::function<json()> function, std::chrono::milliseconds timeout = std::chrono::seconds(30));
    json HandleMessage(const json& message);
    json HandleRequest(const json& request);
    json HandleNotification(const json& notification);
    json HandleInitialize(const json& params);
    json HandleToolsList(const json& params) const;
    json HandleToolsCall(const json& params);

    json ExecuteSquirrelScript(const json& arguments);
    json ExecuteConsoleCommand(const json& arguments);
    json GetGameState(const json& arguments);
    json SearchScriptFunctions(const json& arguments);
    json GetConsoleLog(const json& arguments) const;

    std::vector<Tool> GetAvailableTools() const;
    json CreateErrorResponse(int code, std::string message, const json& id = nullptr) const;
    json CreateSuccessResponse(json result, const json& id) const;
	void SetupHTTPRoutes();

	std::once_flag m_InitializeOnce;
	std::mutex m_SquirrelExecutionMutex;
	EngineTaskQueue m_EngineTasks;
	ConsoleOutputCapture m_OutputCapture;
    std::atomic_bool m_Running = false;
    std::atomic_bool m_Initialized = false;
    std::atomic_bool m_Stopping = false;
    int m_Port = 0;
    std::string m_SessionId;
    std::unique_ptr<httplib::Server> m_HTTPServer;
    std::thread m_ServerThread;
    mutable std::mutex m_StateMutex;
    std::mutex m_StopMutex;
    std::mutex m_SessionMutex;
    std::mutex m_RequestMutex;
};
} // namespace MCPServer
