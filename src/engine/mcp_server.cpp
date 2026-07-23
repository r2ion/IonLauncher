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
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
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
namespace
{
using JSONAllocator = rapidjson::Document::AllocatorType;

rapidjson::Value MakeJSONString(std::string_view value, JSONAllocator& allocator)
{
    return rapidjson::Value(value.data(), static_cast<rapidjson::SizeType>(value.size()), allocator);
}

void AddStringMember(rapidjson::Value& object, const char* name, std::string_view value, JSONAllocator& allocator)
{
    object.AddMember(rapidjson::Value(name, allocator), MakeJSONString(value, allocator), allocator);
}

void AddValueMember(rapidjson::Value& object, const char* name, const rapidjson::Value& value, JSONAllocator& allocator)
{
    rapidjson::Value copy;
    copy.CopyFrom(value, allocator);
    object.AddMember(rapidjson::Value(name, allocator), std::move(copy), allocator);
}

const rapidjson::Value* FindMember(const rapidjson::Value& object, const char* name)
{
    if (!object.IsObject())
        return nullptr;
    const auto member = object.FindMember(name);
    return member == object.MemberEnd() ? nullptr : &member->value;
}

std::string GetString(const rapidjson::Value& object, const char* name)
{
    const rapidjson::Value* value = FindMember(object, name);
    if (!value || !value->IsString())
        throw std::runtime_error(std::string("Missing required string: ") + name);
    return std::string(value->GetString(), value->GetStringLength());
}

std::string GetStringOr(const rapidjson::Value& object, const char* name, std::string_view defaultValue)
{
    const rapidjson::Value* value = FindMember(object, name);
    if (!value || !value->IsString())
        return std::string(defaultValue);
    return std::string(value->GetString(), value->GetStringLength());
}

bool GetBoolOr(const rapidjson::Value& object, const char* name, bool defaultValue)
{
    const rapidjson::Value* value = FindMember(object, name);
    return value && value->IsBool() ? value->GetBool() : defaultValue;
}

std::string SerializeJSON(const rapidjson::Value& value)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value.Accept(writer);
    return std::string(buffer.GetString(), buffer.GetSize());
}

JSONDocument ParseJSON(std::string_view text)
{
    JSONDocument document;
    document.Parse(text.data(), text.size());
    if (document.HasParseError())
    {
        throw std::runtime_error(std::string("JSON parse error at offset ") + std::to_string(document.GetErrorOffset()) + ": " +
                                 rapidjson::GetParseError_En(document.GetParseError()));
    }
    return document;
}
} // namespace

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

int Server::BoundedInteger(const JSONValue& object, const char* name, int defaultValue, int minimum, int maximum)
{
    const JSONValue* value = FindMember(object, name);
    if (!value || !value->IsInt())
        return defaultValue;
    return std::clamp(value->GetInt(), minimum, maximum);
}

JSONDocument Server::MakeToolResult(std::string text, bool isError)
{
    JSONDocument result(rapidjson::kObjectType);
    JSONAllocator& allocator = result.GetAllocator();
    rapidjson::Value content(rapidjson::kArrayType);
    rapidjson::Value item(rapidjson::kObjectType);
    AddStringMember(item, "type", "text", allocator);
    AddStringMember(item, "text", text, allocator);
    content.PushBack(std::move(item), allocator);
    result.AddMember("content", std::move(content), allocator);
    result.AddMember("isError", isError, allocator);
    return result;
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

JSONDocument Server::EngineTaskQueue::RunAndWait(std::function<JSONDocument()> function, std::chrono::milliseconds timeout)
{
    if (ThreadInMainThread())
        return function();

    auto task = std::make_shared<Task>();
    task->function = std::move(function);
    std::future<JSONDocument> result = task->promise.get_future();
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

JSONDocument Server::RunOnEngineThreadAndWait(std::function<JSONDocument()> function, std::chrono::milliseconds timeout)
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

JSONDocument Server::CreateErrorResponse(int code, std::string message, const JSONValue* id) const
{
    JSONDocument response(rapidjson::kObjectType);
    JSONAllocator& allocator = response.GetAllocator();
    AddStringMember(response, "jsonrpc", "2.0", allocator);
    rapidjson::Value error(rapidjson::kObjectType);
    error.AddMember("code", code, allocator);
    AddStringMember(error, "message", message, allocator);
    response.AddMember("error", std::move(error), allocator);
    if (id && !id->IsNull())
        AddValueMember(response, "id", *id, allocator);
    return response;
}

JSONDocument Server::CreateSuccessResponse(JSONDocument result, const JSONValue& id) const
{
    JSONDocument response(rapidjson::kObjectType);
    JSONAllocator& allocator = response.GetAllocator();
    AddStringMember(response, "jsonrpc", "2.0", allocator);
    AddValueMember(response, "id", id, allocator);
    AddValueMember(response, "result", result, allocator);
    return response;
}

JSONDocument Server::HandleMessage(const JSONValue& message)
{
    std::scoped_lock lock(m_RequestMutex);
    if (!message.IsObject() || GetStringOr(message, "jsonrpc", "") != "2.0")
        return CreateErrorResponse(-32600, "Invalid Request: missing or invalid jsonrpc field");
    const JSONValue* method = FindMember(message, "method");
    if (!method || !method->IsString())
    {
        if (const JSONValue* id = FindMember(message, "id"))
            return CreateErrorResponse(-32600, "Invalid Request: server does not accept responses", id);
        return JSONDocument();
    }
    return FindMember(message, "id") ? HandleRequest(message) : HandleNotification(message);
}

JSONDocument Server::HandleRequest(const JSONValue& request)
{
    const JSONValue* id = FindMember(request, "id");
    if (!id || id->IsNull())
        return CreateErrorResponse(-32600, "Invalid Request: id must not be null", id);

    try
    {
        const std::string method = GetString(request, "method");
        JSONDocument emptyParams(rapidjson::kObjectType);
        const JSONValue* params = FindMember(request, "params");
        if (!params)
            params = &emptyParams;
        if (method == "initialize")
            return CreateSuccessResponse(HandleInitialize(*params), *id);
        if (method == "ping")
            return CreateSuccessResponse(JSONDocument(rapidjson::kObjectType), *id);
        if (!m_Initialized.load())
            return CreateErrorResponse(-32002, "Server not initialized", id);
        if (method == "tools/list")
            return CreateSuccessResponse(HandleToolsList(*params), *id);
        if (method == "tools/call")
            return CreateSuccessResponse(HandleToolsCall(*params), *id);
        return CreateErrorResponse(-32601, "Method not found: " + method, id);
    }
    catch (const std::exception& error)
    {
        return CreateErrorResponse(-32603, std::string("Internal error: ") + error.what(), id);
    }
}

JSONDocument Server::HandleNotification(const JSONValue& notification)
{
    try
    {
        const std::string method = GetString(notification, "method");
        if (method == "notifications/initialized" || method == "initialized")
            m_Initialized.store(true);
    }
    catch (const std::exception& error)
    {
        spdlog::warn("Failed handling MCP notification: {}", error.what());
    }
    return JSONDocument();
}

JSONDocument Server::HandleInitialize(const JSONValue& params)
{
    NOTE_UNUSED(params);
    {
        std::scoped_lock lock(m_SessionMutex);
        m_SessionId = GenerateGuid();
    }
    m_Initialized.store(true);
    JSONDocument result(rapidjson::kObjectType);
    JSONAllocator& allocator = result.GetAllocator();
    AddStringMember(result, "protocolVersion", PROTOCOL_VERSION, allocator);

    rapidjson::Value capabilities(rapidjson::kObjectType);
    rapidjson::Value tools(rapidjson::kObjectType);
    tools.AddMember("listChanged", false, allocator);
    capabilities.AddMember("tools", std::move(tools), allocator);
    result.AddMember("capabilities", std::move(capabilities), allocator);

    rapidjson::Value serverInfo(rapidjson::kObjectType);
    AddStringMember(serverInfo, "name", "northstar-mcp-server", allocator);
    AddStringMember(serverInfo, "version", "1.0.0", allocator);
    result.AddMember("serverInfo", std::move(serverInfo), allocator);
    return result;
}

std::vector<Server::Tool> Server::GetAvailableTools() const
{
    std::vector<Tool> tools;
    tools.push_back({
        "execute_squirrel",
        "Execute Squirrel Script",
        "Execute Squirrel code in the server, client, or UI VM.",
        ParseJSON(R"({
            "type":"object",
            "properties":{
                "script":{"type":"string","description":"Squirrel code to execute."},
                "context":{"type":"string","enum":["server","client","ui"],"default":"server"},
                "capture_output":{"type":"boolean","default":true}
            },
            "required":["script"]
        })"),
    });
    tools.push_back({
        "execute_console_command",
        "Execute Console Command",
        "Execute a command through the game engine command buffer.",
        ParseJSON(R"({
            "type":"object",
            "properties":{
                "command":{"type":"string","description":"Console command to execute."},
                "capture_output":{"type":"boolean","default":true}
            },
            "required":["command"]
        })"),
    });
    tools.push_back({
        "get_game_state",
        "Get Game State",
        "Return connection, map, and dedicated-server state from the engine.",
        ParseJSON(R"({"type":"object","properties":{}})"),
    });
    tools.push_back({
        "search_script_functions",
        "Search Script Functions",
        "Search captured native registrations and GameFS-backed .nut/.gnut source definitions.",
        ParseJSON(R"({
            "type":"object",
            "properties":{
                "query":{"type":"string","description":"Case-insensitive function name or description query."},
                "context":{"type":"string","enum":["server","client","ui"],"default":"server"},
                "limit":{"type":"integer","minimum":1,"maximum":500,"default":50},
                "offset":{"type":"integer","minimum":0,"default":0},
                "cursor":{"type":"string"},
                "include_source_matches":{"type":"boolean","default":false},
                "source_match_limit":{"type":"integer","minimum":1,"maximum":200,"default":25},
                "source_case_sensitive":{"type":"boolean","default":false}
            },
            "required":["query"]
        })"),
    });
    tools.push_back({
        "get_console_log",
        "Get Console Log",
        "Return the latest 50 lines from Northstar's existing spdlog ring buffer.",
        ParseJSON(R"({"type":"object","properties":{}})"),
    });
    return tools;
}

JSONDocument Server::HandleToolsList(const JSONValue& params) const
{
    NOTE_UNUSED(params);
    JSONDocument result(rapidjson::kObjectType);
    JSONAllocator& allocator = result.GetAllocator();
    rapidjson::Value tools(rapidjson::kArrayType);
    for (const Tool& tool : GetAvailableTools())
    {
        rapidjson::Value item(rapidjson::kObjectType);
        AddStringMember(item, "name", tool.name, allocator);
        AddStringMember(item, "title", tool.title, allocator);
        AddStringMember(item, "description", tool.description, allocator);
        AddValueMember(item, "inputSchema", tool.inputSchema, allocator);
        tools.PushBack(std::move(item), allocator);
    }
    result.AddMember("tools", std::move(tools), allocator);
    return result;
}

JSONDocument Server::HandleToolsCall(const JSONValue& params)
{
    const std::string name = GetString(params, "name");
    JSONDocument emptyArguments(rapidjson::kObjectType);
    const JSONValue* arguments = FindMember(params, "arguments");
    if (!arguments)
        arguments = &emptyArguments;
    if (name == "execute_squirrel")
        return ExecuteSquirrelScript(*arguments);
    if (name == "execute_console_command")
        return ExecuteConsoleCommand(*arguments);
    if (name == "get_game_state")
        return GetGameState(*arguments);
    if (name == "search_script_functions")
        return SearchScriptFunctions(*arguments);
    if (name == "get_console_log")
        return GetConsoleLog(*arguments);
    throw std::runtime_error("Unknown tool: " + name);
}

JSONDocument Server::ExecuteSquirrelScript(const JSONValue& arguments)
{
    const std::string script = GetString(arguments, "script");
    const std::string contextName = GetStringOr(arguments, "context", "server");
    const std::optional<ScriptContext> context = ParseContext(contextName);
    if (!context)
        return MakeToolResult("Invalid context. Expected server, client, or ui.", true);
    const bool capture = GetBoolOr(arguments, "capture_output", true);

    std::string guid;
    if (capture)
    {
        guid = GenerateGuid();
        m_OutputCapture.Start(guid);
        QueueConsoleText(MakeEchoCommand("[MCP-START:" + guid + "]"));
    }

    JSONDocument execution;
    try
    {
        execution = RunOnEngineThreadAndWait([script, selectedContext = *context]
        {
            if (IsDedicatedServer() && selectedContext != ScriptContext::SERVER)
            {
                JSONDocument response(rapidjson::kObjectType);
                response.AddMember("success", false, response.GetAllocator());
                AddStringMember(response, "error", "client and UI VMs do not exist on a dedicated server", response.GetAllocator());
                return response;
            }

            SquirrelManager* manager = g_pSquirrel[selectedContext];
            if (!manager || !manager->m_pSQVM || !manager->m_pSQVM->sqvm)
            {
                JSONDocument response(rapidjson::kObjectType);
                response.AddMember("success", false, response.GetAllocator());
                AddStringMember(response, "error", "requested Squirrel VM is unavailable", response.GetAllocator());
                return response;
            }

            const SquirrelExecutionResult result = manager->ExecuteCode(script.c_str());
            JSONDocument response(rapidjson::kObjectType);
            JSONAllocator& allocator = response.GetAllocator();
            response.AddMember("success", result.Succeeded(), allocator);
            response.AddMember("compile_result", static_cast<int>(result.compileResult), allocator);
            if (result.called)
                response.AddMember("call_result", static_cast<int>(result.callResult), allocator);
            if (!result.Succeeded())
                AddStringMember(response, "error",
                                result.compileResult == SQRESULT_ERROR ? "script compilation failed" : "script execution failed", allocator);
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
        output = GetBoolOr(execution, "success", false) ? "Script executed successfully."
                                                       : GetStringOr(execution, "error", "Script execution failed.");

    const bool success = GetBoolOr(execution, "success", false);
    JSONDocument response(rapidjson::kObjectType);
    JSONAllocator& allocator = response.GetAllocator();
    rapidjson::Value content(rapidjson::kArrayType);
    rapidjson::Value item(rapidjson::kObjectType);
    AddStringMember(item, "type", "text", allocator);
    AddStringMember(item, "text", output, allocator);
    content.PushBack(std::move(item), allocator);
    response.AddMember("content", std::move(content), allocator);
    AddValueMember(response, "structuredContent", execution, allocator);
    response.AddMember("isError", !success, allocator);
    return response;
}

JSONDocument Server::ExecuteConsoleCommand(const JSONValue& arguments)
{
    const std::string command = GetString(arguments, "command");
    const bool capture = GetBoolOr(arguments, "capture_output", true);

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
        RunOnEngineThreadAndWait([] { return JSONDocument(rapidjson::kObjectType); });
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

    JSONDocument response(rapidjson::kObjectType);
    JSONAllocator& allocator = response.GetAllocator();
    rapidjson::Value content(rapidjson::kArrayType);
    rapidjson::Value item(rapidjson::kObjectType);
    AddStringMember(item, "type", "text", allocator);
    AddStringMember(item, "text", output, allocator);
    content.PushBack(std::move(item), allocator);
    response.AddMember("content", std::move(content), allocator);
    response.AddMember("isError", false, allocator);
    return response;
}

JSONDocument Server::GetGameState(const JSONValue& arguments)
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

        std::ostringstream text;
        text << "Game State:\n"
             << "  In Game: " << (inGame ? "Yes" : "No") << '\n'
             << "  Connected: " << (connected ? "Yes" : "No") << '\n'
             << "  Dedicated Server: " << (dedicated ? "Yes" : "No") << '\n';
        if (!map.empty())
            text << "  Current Map: " << map << '\n';

        JSONDocument response(rapidjson::kObjectType);
        JSONAllocator& allocator = response.GetAllocator();

        rapidjson::Value content(rapidjson::kArrayType);
        rapidjson::Value item(rapidjson::kObjectType);
        AddStringMember(item, "type", "text", allocator);
        AddStringMember(item, "text", text.str(), allocator);
        content.PushBack(std::move(item), allocator);
        response.AddMember("content", std::move(content), allocator);

        rapidjson::Value state(rapidjson::kObjectType);
        state.AddMember("in_game", inGame, allocator);
        state.AddMember("connected", connected, allocator);
        state.AddMember("is_dedicated", dedicated, allocator);
        state.AddMember("signon_state", signonState, allocator);
        AddStringMember(state, "current_map", map, allocator);
        response.AddMember("structuredContent", std::move(state), allocator);
        response.AddMember("isError", false, allocator);
        return response;
    });
}

JSONDocument Server::SearchScriptFunctions(const JSONValue& arguments)
{
    const std::string query = GetString(arguments, "query");
    const std::optional<ScriptContext> context = ParseContext(GetStringOr(arguments, "context", "server"));
    if (!context)
        return MakeToolResult("Invalid context. Expected server, client, or ui.", true);

    int offset = BoundedInteger(arguments, "offset", 0, 0, INT_MAX);
    if (const JSONValue* cursorValue = FindMember(arguments, "cursor"); cursorValue && cursorValue->IsString())
    {
        const std::string cursor(cursorValue->GetString(), cursorValue->GetStringLength());
        int parsed = 0;
        const auto [end, error] = std::from_chars(cursor.data(), cursor.data() + cursor.size(), parsed);
        if (error == std::errc() && end == cursor.data() + cursor.size() && parsed >= 0)
            offset = parsed;
    }

    MCPScriptFunctionSearch::Options options;
    options.query = query;
    options.context = *context;
    options.limit = BoundedInteger(arguments, "limit", 50, 1, 500);
    options.offset = offset;
    options.sourceLimit = BoundedInteger(arguments, "source_match_limit", 25, 1, 200);
    options.includeSourceMatches = GetBoolOr(arguments, "include_source_matches", false);
    options.sourceCaseSensitive = GetBoolOr(arguments, "source_case_sensitive", false);

    return RunOnEngineThreadAndWait([search = MCPScriptFunctionSearch(std::move(options))] { return search.Execute(); }, std::chrono::seconds(60));
}

JSONDocument Server::GetConsoleLog(const JSONValue& arguments) const
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
    JSONDocument response(rapidjson::kObjectType);
    JSONAllocator& allocator = response.GetAllocator();

    rapidjson::Value content(rapidjson::kArrayType);
    rapidjson::Value item(rapidjson::kObjectType);
    AddStringMember(item, "type", "text", allocator);
    AddStringMember(item, "text", text.str(), allocator);
    content.PushBack(std::move(item), allocator);
    response.AddMember("content", std::move(content), allocator);

    rapidjson::Value structured(rapidjson::kObjectType);
    structured.AddMember("requested_line_count", 50, allocator);
    structured.AddMember("returned_line_count", static_cast<uint64_t>(lines.size()), allocator);
    rapidjson::Value jsonLines(rapidjson::kArrayType);
    for (const std::string& line : lines)
        jsonLines.PushBack(MakeJSONString(line, allocator), allocator);
    structured.AddMember("lines", std::move(jsonLines), allocator);
    response.AddMember("structuredContent", std::move(structured), allocator);
    response.AddMember("isError", false, allocator);
    return response;
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

        JSONDocument message;
        message.Parse(request.body.data(), request.body.size());
        if (message.HasParseError())
        {
            response.status = 400;
            response.set_content(SerializeJSON(CreateErrorResponse(-32700, "Parse error")), "application/json");
            return;
        }

        try
        {
            const bool initialize = message.IsObject() && GetStringOr(message, "method", "") == "initialize";
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

            JSONDocument result = HandleMessage(message);
            if (result.IsNull())
            {
                response.status = 202;
                return;
            }
            response.set_content(SerializeJSON(result), "application/json");
            if (initialize)
            {
                std::scoped_lock lock(m_SessionMutex);
                response.set_header("Mcp-Session-Id", m_SessionId);
            }
        }
        catch (const std::exception& error)
        {
            response.status = 500;
            response.set_content(SerializeJSON(CreateErrorResponse(-32603, error.what())), "application/json");
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
        JSONDocument status(rapidjson::kObjectType);
        JSONAllocator& allocator = status.GetAllocator();
        AddStringMember(status, "status", "ok", allocator);
        AddStringMember(status, "service", "Northstar MCP Server", allocator);
        AddStringMember(status, "transport", "http", allocator);
        status.AddMember("port", m_Port, allocator);
        response.set_content(SerializeJSON(status), "application/json");
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
