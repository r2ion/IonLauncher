#include "mcp_server.h"
#include "mcp_script_function_search.h"

#include "client/r2client.h"
#include "core/convar/concommand.h"
#include "core/convar/convar.h"
#include "core/convar/cvar.h"
#include "core/tier0.h"
#include "dedicated/dedicated.h"
#include "engine/r2engine.h"
#include "logging/logging.h"
#include "vscript/squirrel/squirrel.h"

#include <httplib.h>
#include <spdlog/sinks/base_sink.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <climits>
#include <cstdlib>
#include <optional>
#include <random>
#include <sstream>

namespace MCPServer
{
class Server::LogCaptureSink final : public spdlog::sinks::base_sink<std::mutex>
{
  protected:
    void sink_it_(const spdlog::details::log_msg& message) override
    {
        spdlog::memory_buf_t formatted;
        formatter_->format(message, formatted);
        std::string line = fmt::to_string(formatted);
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        Server::GetInstance().CaptureLogLine(line);
    }

    void flush_() override
    {
    }
};

std::string Server::ToLowerASCII(std::string_view value)
{
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return lowered;
}

std::optional<ScriptContext> Server::ParseContext(std::string_view value)
{
    const std::string context = ToLowerASCII(value);
    if (context == "server")
        return ScriptContext::SERVER;
    if (context == "client")
        return ScriptContext::CLIENT;
    if (context == "ui")
        return ScriptContext::UI;
    return std::nullopt;
}

std::string Server::GenerateGuid()
{
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<unsigned int> distribution(0, 15);
    constexpr std::string_view HEX = "0123456789abcdef";

    std::string guid;
    guid.reserve(36);
    for (int index = 0; index < 32; index++)
    {
        if (index == 8 || index == 12 || index == 16 || index == 20)
            guid.push_back('-');
        guid.push_back(HEX[distribution(generator)]);
    }
    return guid;
}

std::string Server::MakeEchoCommand(std::string_view marker)
{
    std::string command;
    command.reserve(marker.size() + 6);
    command.append("echo ").append(marker).push_back('\n');
    return command;
}

std::string Server::AppendNewline(std::string command)
{
    if (command.empty() || command.back() != '\n')
        command.push_back('\n');
    return command;
}

void Server::QueueConsoleText(const std::string& text)
{
    if (!Cbuf_AddText || !Cbuf_GetCurrentPlayer)
        throw std::runtime_error("engine command buffer is unavailable");
    Cbuf_AddText(Cbuf_GetCurrentPlayer(), text.c_str(), cmd_source_t::kCommandSrcCode);
}

int Server::BoundedInteger(const json& object, const char* name, int defaultValue, int minimum, int maximum)
{
    if (!object.contains(name) || !object[name].is_number_integer())
        return defaultValue;
    return std::clamp(object[name].get<int>(), minimum, maximum);
}

json Server::MakeToolResult(std::string text, bool isError)
{
    return {
        {"content", json::array({{{"type", "text"}, {"text", std::move(text)}}})},
        {"isError", isError},
    };
}

void Server::EngineTaskQueue::Resolve()
{
    std::deque<std::shared_ptr<Task>> tasks;
    {
        std::scoped_lock lock(m_Mutex);
        tasks.swap(m_Tasks);
    }

    for (const std::shared_ptr<Task>& task : tasks)
    {
        if (task->cancelled.load())
            continue;
        try
        {
            task->promise.set_value(task->function());
        }
        catch (...)
        {
            try
            {
                task->promise.set_exception(std::current_exception());
            }
            catch (...)
            {
            }
        }
    }
}

json Server::EngineTaskQueue::RunAndWait(std::function<json()> function, std::chrono::milliseconds timeout)
{
    if (ThreadInMainThread())
        return function();

    auto task = std::make_shared<Task>();
    task->function = std::move(function);
    std::future<json> result = task->promise.get_future();
    {
        std::scoped_lock lock(m_Mutex);
        if (m_Tasks.size() >= TASK_LIMIT)
            throw std::runtime_error("engine task queue is full");
        m_Tasks.push_back(task);
    }

    try
    {
        Server::QueueConsoleText("mcp_resolve_engine_tasks\n");
    }
    catch (...)
    {
        task->cancelled.store(true);
        throw;
    }
    if (result.wait_for(timeout) != std::future_status::ready)
    {
        task->cancelled.store(true);
        throw std::runtime_error("timed out waiting for the engine thread");
    }
    return result.get();
}

json Server::RunOnEngineThreadAndWait(std::function<json()> function, std::chrono::milliseconds timeout)
{
    return m_EngineTasks.RunAndWait(std::move(function), timeout);
}

void Server::ConsoleOutputCapture::Start(std::string guid)
{
    std::scoped_lock lock(m_Mutex);
    m_Guid = std::move(guid);
    m_Lines.clear();
    m_State = State::WaitingForStart;
    m_Truncated = false;
}

void Server::ConsoleOutputCapture::Stop()
{
    std::scoped_lock lock(m_Mutex);
    m_Guid.clear();
    m_State = State::Idle;
    m_Truncated = false;
    m_Condition.notify_all();
}

void Server::ConsoleOutputCapture::AddLine(std::string_view line)
{
    std::scoped_lock lock(m_Mutex);
    if (m_Guid.empty())
        return;

    const std::string startMarker = "[MCP-START:" + m_Guid + "]";
    if (line.find(startMarker) != std::string_view::npos)
    {
        m_State = State::Capturing;
        m_Condition.notify_all();
        return;
    }

    const std::string endMarker = "[MCP-END:" + m_Guid + "]";
    if (line.find(endMarker) != std::string_view::npos)
    {
        m_State = State::Completed;
        m_Condition.notify_all();
        return;
    }

    if (m_State != State::Capturing)
        return;
    if (m_Lines.size() < LINE_LIMIT)
    {
        m_Lines.emplace_back(line);
        return;
    }
    if (!m_Truncated)
    {
        m_Truncated = true;
        m_Lines.emplace_back("[MCP] Output truncated at 500 lines.");
        m_State = State::Completed;
        m_Condition.notify_all();
    }
}

std::vector<std::string> Server::ConsoleOutputCapture::TakeLines()
{
    std::scoped_lock lock(m_Mutex);
    std::vector<std::string> lines = std::move(m_Lines);
    m_Lines.clear();
    return lines;
}

bool Server::ConsoleOutputCapture::WaitForCompletion(std::string_view guid, std::chrono::milliseconds timeout)
{
    std::unique_lock lock(m_Mutex);
    const bool signalled = m_Condition.wait_for(lock, timeout, [&] { return m_Guid != guid || m_State == State::Completed; });
    return signalled && m_Guid == guid && m_State == State::Completed;
}

Server& Server::GetInstance()
{
    static Server* instance = new Server();
    return *instance;
}

json Server::CreateErrorResponse(int code, std::string message, const json& id) const
{
    json response = {
        {"jsonrpc", "2.0"},
        {"error", {{"code", code}, {"message", std::move(message)}}},
    };
    if (!id.is_null())
        response["id"] = id;
    return response;
}

json Server::CreateSuccessResponse(json result, const json& id) const
{
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

json Server::HandleMessage(const json& message)
{
    std::scoped_lock lock(m_RequestMutex);
    if (!message.is_object() || message.value("jsonrpc", "") != "2.0")
        return CreateErrorResponse(-32600, "Invalid Request: missing or invalid jsonrpc field");
    if (!message.contains("method") || !message["method"].is_string())
    {
        if (message.contains("id"))
            return CreateErrorResponse(-32600, "Invalid Request: server does not accept responses", message["id"]);
        return nullptr;
    }
    return message.contains("id") ? HandleRequest(message) : HandleNotification(message);
}

json Server::HandleRequest(const json& request)
{
    const json& id = request["id"];
    if (id.is_null())
        return CreateErrorResponse(-32600, "Invalid Request: id must not be null");

    try
    {
        const std::string method = request["method"].get<std::string>();
        const json params = request.value("params", json::object());
        if (method == "initialize")
            return CreateSuccessResponse(HandleInitialize(params), id);
        if (method == "ping")
            return CreateSuccessResponse(json::object(), id);
        if (!m_Initialized.load())
            return CreateErrorResponse(-32002, "Server not initialized", id);
        if (method == "tools/list")
            return CreateSuccessResponse(HandleToolsList(params), id);
        if (method == "tools/call")
            return CreateSuccessResponse(HandleToolsCall(params), id);
        return CreateErrorResponse(-32601, "Method not found: " + method, id);
    }
    catch (const std::exception& error)
    {
        return CreateErrorResponse(-32603, std::string("Internal error: ") + error.what(), id);
    }
}

json Server::HandleNotification(const json& notification)
{
    try
    {
        const std::string method = notification["method"].get<std::string>();
        if (method == "notifications/initialized" || method == "initialized")
            m_Initialized.store(true);
    }
    catch (const std::exception& error)
    {
        spdlog::warn("Failed handling MCP notification: {}", error.what());
    }
    return nullptr;
}

json Server::HandleInitialize(const json& params)
{
    NOTE_UNUSED(params);
    {
        std::scoped_lock lock(m_SessionMutex);
        m_SessionId = GenerateGuid();
    }
    m_Initialized.store(true);
    return {
        {"protocolVersion", PROTOCOL_VERSION},
        {"capabilities", {{"tools", {{"listChanged", false}}}}},
        {"serverInfo", {{"name", "northstar-mcp-server"}, {"version", "1.0.0"}}},
    };
}

std::vector<Server::Tool> Server::GetAvailableTools() const
{
    return {
        {
            "execute_squirrel",
            "Execute Squirrel Script",
            "Execute Squirrel code in the server, client, or UI VM.",
            {
                {"type", "object"},
                {"properties",
                 {
                     {"script", {{"type", "string"}, {"description", "Squirrel code to execute."}}},
                     {"context", {{"type", "string"}, {"enum", {"server", "client", "ui"}}, {"default", "server"}}},
                     {"capture_output", {{"type", "boolean"}, {"default", true}}},
                 }},
                {"required", {"script"}},
            },
        },
        {
            "execute_console_command",
            "Execute Console Command",
            "Execute a command through the game engine command buffer.",
            {
                {"type", "object"},
                {"properties",
                 {
                     {"command", {{"type", "string"}, {"description", "Console command to execute."}}},
                     {"capture_output", {{"type", "boolean"}, {"default", true}}},
                 }},
                {"required", {"command"}},
            },
        },
        {
            "get_game_state",
            "Get Game State",
            "Return connection, map, and dedicated-server state from the engine.",
            {{"type", "object"}, {"properties", json::object()}},
        },
        {
            "search_script_functions",
            "Search Script Functions",
            "Search captured native registrations and GameFS-backed .nut/.gnut source definitions.",
            {
                {"type", "object"},
                {"properties",
                 {
                     {"query", {{"type", "string"}, {"description", "Case-insensitive function name or description query."}}},
                     {"context", {{"type", "string"}, {"enum", {"server", "client", "ui"}}, {"default", "server"}}},
                     {"limit", {{"type", "integer"}, {"minimum", 1}, {"maximum", 500}, {"default", 50}}},
                     {"offset", {{"type", "integer"}, {"minimum", 0}, {"default", 0}}},
                     {"cursor", {{"type", "string"}}},
                     {"include_source_matches", {{"type", "boolean"}, {"default", false}}},
                     {"source_match_limit", {{"type", "integer"}, {"minimum", 1}, {"maximum", 200}, {"default", 25}}},
                     {"source_case_sensitive", {{"type", "boolean"}, {"default", false}}},
                 }},
                {"required", {"query"}},
            },
        },
        {
            "get_console_log",
            "Get Console Log",
            "Return the latest 50 lines from Northstar's existing spdlog ring buffer.",
            {{"type", "object"}, {"properties", json::object()}},
        },
    };
}

json Server::HandleToolsList(const json& params) const
{
    NOTE_UNUSED(params);
    json tools = json::array();
    for (const Tool& tool : GetAvailableTools())
    {
        tools.push_back({{"name", tool.name}, {"title", tool.title}, {"description", tool.description}, {"inputSchema", tool.inputSchema}});
    }
    return {{"tools", std::move(tools)}};
}

json Server::HandleToolsCall(const json& params)
{
    if (!params.contains("name") || !params["name"].is_string())
        throw std::runtime_error("Missing required parameter: name");
    const std::string name = params["name"].get<std::string>();
    const json arguments = params.value("arguments", json::object());
    if (name == "execute_squirrel")
        return ExecuteSquirrelScript(arguments);
    if (name == "execute_console_command")
        return ExecuteConsoleCommand(arguments);
    if (name == "get_game_state")
        return GetGameState(arguments);
    if (name == "search_script_functions")
        return SearchScriptFunctions(arguments);
    if (name == "get_console_log")
        return GetConsoleLog(arguments);
    throw std::runtime_error("Unknown tool: " + name);
}

json Server::ExecuteSquirrelScript(const json& arguments)
{
    if (!arguments.contains("script") || !arguments["script"].is_string())
        throw std::runtime_error("Missing required argument: script");

    const std::string script = arguments["script"].get<std::string>();
    const std::string contextName = arguments.value("context", "server");
    const std::optional<ScriptContext> context = ParseContext(contextName);
    if (!context)
        return MakeToolResult("Invalid context. Expected server, client, or ui.", true);
    const bool capture = arguments.value("capture_output", true);

    std::string guid;
    if (capture)
    {
        guid = GenerateGuid();
        m_OutputCapture.Start(guid);
        QueueConsoleText(MakeEchoCommand("[MCP-START:" + guid + "]"));
    }

    json execution;
    try
    {
        execution = RunOnEngineThreadAndWait([script, selectedContext = *context]
        {
            if (IsDedicatedServer() && selectedContext != ScriptContext::SERVER)
                return json{{"success", false}, {"error", "client and UI VMs do not exist on a dedicated server"}};

            SquirrelManager* manager = g_pSquirrel[selectedContext];
            if (!manager || !manager->m_pSQVM || !manager->m_pSQVM->sqvm)
                return json{{"success", false}, {"error", "requested Squirrel VM is unavailable"}};

            const SquirrelExecutionResult result = manager->ExecuteCode(script.c_str());
            json response = {
                {"success", result.Succeeded()},
                {"compile_result", static_cast<int>(result.compileResult)},
            };
            if (result.called)
                response["call_result"] = static_cast<int>(result.callResult);
            if (!result.Succeeded())
                response["error"] = result.compileResult == SQRESULT_ERROR ? "script compilation failed" : "script execution failed";
            return response;
        });
    }
    catch (...)
    {
        if (capture)
            m_OutputCapture.Stop();
        throw;
    }

    std::string output;
    if (capture)
    {
        QueueConsoleText(MakeEchoCommand("[MCP-END:" + guid + "]"));
        const bool complete = m_OutputCapture.WaitForCompletion(guid, CAPTURE_TIMEOUT);
        for (const std::string& line : m_OutputCapture.TakeLines())
            output.append(line).push_back('\n');
        m_OutputCapture.Stop();
        if (!complete)
            output.append("[MCP] Output capture timed out.\n");
    }
    else
        output = execution.value("success", false) ? "Script executed successfully." : execution.value("error", "Script execution failed.");

    return {
        {"content", json::array({{{"type", "text"}, {"text", std::move(output)}}})},
        {"structuredContent", execution},
        {"isError", !execution.value("success", false)},
    };
}

json Server::ExecuteConsoleCommand(const json& arguments)
{
    if (!arguments.contains("command") || !arguments["command"].is_string())
        throw std::runtime_error("Missing required argument: command");
    const std::string command = arguments["command"].get<std::string>();
    const bool capture = arguments.value("capture_output", true);

    std::string guid;
    if (capture)
    {
        guid = GenerateGuid();
        m_OutputCapture.Start(guid);
        QueueConsoleText(MakeEchoCommand("[MCP-START:" + guid + "]"));
    }

    try
    {
        QueueConsoleText(AppendNewline(command));
        RunOnEngineThreadAndWait([] { return json::object(); });
    }
    catch (...)
    {
        if (capture)
            m_OutputCapture.Stop();
        throw;
    }

    std::string output = "Command executed.";
    if (capture)
    {
        QueueConsoleText(MakeEchoCommand("[MCP-END:" + guid + "]"));
        const bool complete = m_OutputCapture.WaitForCompletion(guid, CAPTURE_TIMEOUT);
        output.clear();
        for (const std::string& line : m_OutputCapture.TakeLines())
            output.append(line).push_back('\n');
        m_OutputCapture.Stop();
        if (!complete)
            output.append("[MCP] Output capture timed out.\n");
    }

    return {
        {"content", json::array({{{"type", "text"}, {"text", std::move(output)}}})},
        {"isError", false},
    };
}

json Server::GetGameState(const json& arguments)
{
    NOTE_UNUSED(arguments);
    return RunOnEngineThreadAndWait([]
    {
        const bool dedicated = IsDedicatedServer();
        bool connected = false;
        bool inGame = false;
        int signonState = 0;
        std::string map;

        if (dedicated)
        {
            if (g_pCVar)
            {
                if (ConVar* hostMap = g_pCVar->FindVar("host_map"))
                    map = hostMap->GetString();
            }
            connected = !map.empty();
            inGame = connected;
        }
        else if (GetBaseLocalClient)
        {
            if (CClientState* client = GetBaseLocalClient())
            {
                signonState = static_cast<int>(client->m_nSignonState);
                connected = client->m_nSignonState >= eSignonState::CONNECTED;
                inGame = client->m_nSignonState == eSignonState::FULL;
                map = client->m_szLevelBaseName;
            }
        }

        json state = {
            {"in_game", inGame}, {"connected", connected}, {"is_dedicated", dedicated}, {"signon_state", signonState}, {"current_map", map},
        };
        std::ostringstream text;
        text << "Game State:\n"
             << "  In Game: " << (inGame ? "Yes" : "No") << '\n'
             << "  Connected: " << (connected ? "Yes" : "No") << '\n'
             << "  Dedicated Server: " << (dedicated ? "Yes" : "No") << '\n';
        if (!map.empty())
            text << "  Current Map: " << map << '\n';

        return json{
            {"content", json::array({{{"type", "text"}, {"text", text.str()}}})},
            {"structuredContent", std::move(state)},
            {"isError", false},
        };
    });
}

json Server::SearchScriptFunctions(const json& arguments)
{
    if (!arguments.contains("query") || !arguments["query"].is_string())
        throw std::runtime_error("Missing required argument: query");

    const std::optional<ScriptContext> context = ParseContext(arguments.value("context", "server"));
    if (!context)
        return MakeToolResult("Invalid context. Expected server, client, or ui.", true);

    int offset = BoundedInteger(arguments, "offset", 0, 0, INT_MAX);
    if (arguments.contains("cursor") && arguments["cursor"].is_string())
    {
        const std::string cursor = arguments["cursor"].get<std::string>();
        int parsed = 0;
        const auto [end, error] = std::from_chars(cursor.data(), cursor.data() + cursor.size(), parsed);
        if (error == std::errc() && end == cursor.data() + cursor.size() && parsed >= 0)
            offset = parsed;
    }

    MCPScriptFunctionSearch::Options options;
    options.query = arguments["query"].get<std::string>();
    options.context = *context;
    options.limit = BoundedInteger(arguments, "limit", 50, 1, 500);
    options.offset = offset;
    options.sourceLimit = BoundedInteger(arguments, "source_match_limit", 25, 1, 200);
    options.includeSourceMatches = arguments.value("include_source_matches", false);
    options.sourceCaseSensitive = arguments.value("source_case_sensitive", false);

    return RunOnEngineThreadAndWait([search = MCPScriptFunctionSearch(std::move(options))] { return search.Execute(); }, std::chrono::seconds(60));
}

json Server::GetConsoleLog(const json& arguments) const
{
    NOTE_UNUSED(arguments);
    const std::vector<std::string> lines = NS::log::GetRecentLogLines(50);
    std::ostringstream text;
    if (lines.empty())
        text << "Console log is empty.";
    else
    {
        text << "Console log (latest " << lines.size() << " lines):\n";
        for (const std::string& line : lines)
            text << line << '\n';
    }
    return {
        {"content", json::array({{{"type", "text"}, {"text", text.str()}}})},
        {"structuredContent", {{"requested_line_count", 50}, {"returned_line_count", lines.size()}, {"lines", lines}}},
        {"isError", false},
    };
}

void Server::CaptureLogLine(std::string_view line)
{
    m_OutputCapture.AddLine(line);
}

void Server::SetupHTTPRoutes()
{
    m_HTTPServer->Post("/", [this](const httplib::Request& request, httplib::Response& response)
    {
        if (request.has_header("Origin"))
        {
            const std::string origin = ToLowerASCII(request.get_header_value("Origin"));
            if (origin.find("localhost") == std::string::npos && origin.find("127.0.0.1") == std::string::npos &&
                origin.find("[::1]") == std::string::npos)
            {
                response.status = 403;
                response.set_content(R"({"error":"Forbidden origin"})", "application/json");
                return;
            }
        }
        if (request.has_header("MCP-Protocol-Version") && request.get_header_value("MCP-Protocol-Version") != PROTOCOL_VERSION)
        {
            response.status = 400;
            response.set_content(R"({"error":"Unsupported protocol version"})", "application/json");
            return;
        }

        try
        {
            const json message = json::parse(request.body);
            const bool initialize = message.is_object() && message.value("method", "") == "initialize";
            if (!initialize)
            {
                std::scoped_lock lock(m_SessionMutex);
                if (!m_SessionId.empty())
                {
                    if (!request.has_header("Mcp-Session-Id"))
                    {
                        response.status = 400;
                        response.set_content(R"({"error":"Missing session ID"})", "application/json");
                        return;
                    }
                    if (request.get_header_value("Mcp-Session-Id") != m_SessionId)
                    {
                        response.status = 404;
                        response.set_content(R"({"error":"Session not found"})", "application/json");
                        return;
                    }
                }
            }

            json result = HandleMessage(message);
            if (result.is_null())
            {
                response.status = 202;
                return;
            }
            response.set_content(result.dump(), "application/json");
            if (initialize)
            {
                std::scoped_lock lock(m_SessionMutex);
                response.set_header("Mcp-Session-Id", m_SessionId);
            }
        }
        catch (const json::parse_error&)
        {
            response.status = 400;
            response.set_content(CreateErrorResponse(-32700, "Parse error").dump(), "application/json");
        }
        catch (const std::exception& error)
        {
            response.status = 500;
            response.set_content(CreateErrorResponse(-32603, error.what()).dump(), "application/json");
        }
    });

    m_HTTPServer->Delete("/", [this](const httplib::Request&, httplib::Response& response)
    {
        m_Initialized.store(false);
        std::scoped_lock lock(m_SessionMutex);
        m_SessionId.clear();
        response.status = 204;
    });

    m_HTTPServer->Get("/health", [this](const httplib::Request&, httplib::Response& response)
    {
        json status = {
            {"status", "ok"},
            {"service", "Northstar MCP Server"},
            {"transport", "http"},
            {"port", m_Port},
        };
        response.set_content(status.dump(), "application/json");
    });
}

bool Server::StartHTTP(int port)
{
    if (port <= 0 || port > 65535)
        return false;

    std::scoped_lock lock(m_StateMutex);
    if (m_Running.load() || m_Stopping.load() || m_ServerThread.joinable())
        return false;

    m_HTTPServer = std::make_unique<httplib::Server>();
    m_HTTPServer->set_payload_max_length(4 * 1024 * 1024);
    m_Port = port;
    SetupHTTPRoutes();
    if (!m_HTTPServer->bind_to_port("127.0.0.1", port))
    {
        m_HTTPServer.reset();
        m_Port = 0;
        return false;
    }

    m_Running.store(true);
    m_ServerThread = std::thread([this]
    {
        spdlog::info("Northstar MCP server listening on http://127.0.0.1:{}", m_Port);
        if (!m_HTTPServer->listen_after_bind())
            spdlog::warn("Northstar MCP HTTP listener stopped with an error");
        m_Running.store(false);
    });
    return true;
}

void Server::Stop()
{
    std::scoped_lock stopLock(m_StopMutex);
    m_Stopping.store(true);

    std::thread serverThread;
    {
        std::scoped_lock stateLock(m_StateMutex);
        if (m_HTTPServer)
            m_HTTPServer->stop();
        if (m_ServerThread.joinable())
            serverThread = std::move(m_ServerThread);
    }
    if (serverThread.joinable())
        serverThread.join();

    {
        std::scoped_lock stateLock(m_StateMutex);
        m_Running.store(false);
        m_Initialized.store(false);
        m_HTTPServer.reset();
        m_Port = 0;
    }
    {
        std::scoped_lock sessionLock(m_SessionMutex);
        m_SessionId.clear();
    }
    m_Stopping.store(false);
    spdlog::info("Northstar MCP server stopped");
}

bool Server::IsRunning() const
{
    return m_Running.load();
}

int Server::GetPort() const
{
    std::scoped_lock lock(m_StateMutex);
    return m_Port;
}

void Server::StartHTTPCommand(const CCommand& command)
{
    int port = 8765;
    if (command.ArgC() > 1)
        port = std::atoi(command.Arg(1));
    if (!GetInstance().StartHTTP(port))
        spdlog::warn("Could not start Northstar MCP server on port {}", port);
}

void Server::StopCommand(const CCommand& command)
{
    NOTE_UNUSED(command);
    Server& server = GetInstance();
    std::thread(&Server::Stop, &server).detach();
}

void Server::StatusCommand(const CCommand& command)
{
    NOTE_UNUSED(command);
    const Server& server = GetInstance();
    if (server.IsRunning())
        spdlog::info("Northstar MCP server is running on http://127.0.0.1:{}", server.GetPort());
    else
        spdlog::info("Northstar MCP server is not running");
}

void Server::ResolveEngineTasksCommand(const CCommand& command)
{
    NOTE_UNUSED(command);
    GetInstance().m_EngineTasks.Resolve();
}

void Server::Initialize()
{
    static std::once_flag once;
    std::call_once(once, []
    {
        auto captureSink = std::make_shared<LogCaptureSink>();
        captureSink->set_pattern("[%n] %v");
        RegisterSink(std::move(captureSink));

        RegisterConCommand("mcp_start_http", StartHTTPCommand, "Start the loopback MCP HTTP server (optional: port).", FCVAR_NONE);
        RegisterConCommand("mcp_stop", StopCommand, "Stop the MCP HTTP server.", FCVAR_NONE);
        RegisterConCommand("mcp_status", StatusCommand, "Report MCP HTTP server status.", FCVAR_NONE);
        RegisterConCommand("mcp_resolve_engine_tasks", ResolveEngineTasksCommand, "Run queued MCP work on the engine thread.", FCVAR_HIDDEN);
    });
}

ON_DLL_LOAD_RELIESON("engine.dll", MCPServer, ConCommand, [](CModule module)
{
    NOTE_UNUSED(module);
    MCPServer::Server::GetInstance().Initialize();
})
} // namespace MCPServer
