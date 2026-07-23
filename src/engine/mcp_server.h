#pragma once

#include <rapidjson/document.h>

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
using JSONDocument = rapidjson::Document;
using JSONValue = rapidjson::Value;

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
        JSONDocument inputSchema;
    };

    class EngineTaskQueue final
    {
      public:
        JSONDocument RunAndWait(std::function<JSONDocument()> function, std::chrono::milliseconds timeout = std::chrono::seconds(30));
        void Resolve();

      private:
        struct Task
        {
            std::function<JSONDocument()> function;
            std::promise<JSONDocument> promise;
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
    static std::string MakeEchoCommand(std::string_view marker);
    static std::string AppendNewline(std::string command);
    static void QueueConsoleText(const std::string& text);
    static int BoundedInteger(const JSONValue& object, const char* name, int defaultValue, int minimum, int maximum);
    static JSONDocument MakeToolResult(std::string text, bool isError);

    JSONDocument RunOnEngineThreadAndWait(std::function<JSONDocument()> function,
                                          std::chrono::milliseconds timeout = std::chrono::seconds(30));
    JSONDocument HandleMessage(const JSONValue& message);
    JSONDocument HandleRequest(const JSONValue& request);
    JSONDocument HandleNotification(const JSONValue& notification);
    JSONDocument HandleInitialize(const JSONValue& params);
    JSONDocument HandleToolsList(const JSONValue& params) const;
    JSONDocument HandleToolsCall(const JSONValue& params);

    JSONDocument ExecuteSquirrelScript(const JSONValue& arguments);
    JSONDocument ExecuteConsoleCommand(const JSONValue& arguments);
    JSONDocument GetGameState(const JSONValue& arguments);
    JSONDocument SearchScriptFunctions(const JSONValue& arguments);
    JSONDocument GetConsoleLog(const JSONValue& arguments) const;

    std::vector<Tool> GetAvailableTools() const;
    JSONDocument CreateErrorResponse(int code, std::string message, const JSONValue* id = nullptr) const;
    JSONDocument CreateSuccessResponse(JSONDocument result, const JSONValue& id) const;
    void SetupHTTPRoutes();

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
