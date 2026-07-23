#include "mcp_script_function_search.h"

#include "core/filesystem/filesystem.h"
#include "vscript/squirrel/squirrel.h"
#include "vscript/squirrel/squirreldocumentation.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <utility>

MCPScriptFunctionSearch::MCPScriptFunctionSearch(Options options) : m_Options(std::move(options))
{
}

std::string MCPScriptFunctionSearch::ToLowerASCII(std::string_view value)
{
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return lowered;
}

bool MCPScriptFunctionSearch::ContainsLowerASCII(std::string_view value, std::string_view loweredNeedle)
{
    if (loweredNeedle.empty())
        return true;
    if (loweredNeedle.size() > value.size())
        return false;

    return std::search(value.begin(), value.end(), loweredNeedle.begin(), loweredNeedle.end(), [](char left, char right)
    { return static_cast<char>(std::tolower(static_cast<unsigned char>(left))) == right; }) != value.end();
}

bool MCPScriptFunctionSearch::IsIdentifierStart(char character)
{
    return character == '_' || std::isalpha(static_cast<unsigned char>(character)) != 0;
}

bool MCPScriptFunctionSearch::IsIdentifier(char character)
{
    return IsIdentifierStart(character) || std::isdigit(static_cast<unsigned char>(character)) != 0;
}

void MCPScriptFunctionSearch::SkipWhitespace(const std::string& text, size_t& position)
{
    while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])) != 0)
        position++;
}

template <typename Callback> void MCPScriptFunctionSearch::ForEachScriptFunction(const std::string& text, Callback&& callback)
{
    constexpr std::string_view KEYWORD = "function";
    size_t searchPosition = 0;
    size_t countedPosition = 0;
    size_t line = 1;

    while ((searchPosition = text.find(KEYWORD, searchPosition)) != std::string::npos)
    {
        const size_t afterKeyword = searchPosition + KEYWORD.size();
        if ((searchPosition > 0 && IsIdentifier(text[searchPosition - 1])) || (afterKeyword < text.size() && IsIdentifier(text[afterKeyword])))
        {
            searchPosition++;
            continue;
        }

        size_t position = afterKeyword;
        SkipWhitespace(text, position);
        if (position >= text.size() || !IsIdentifierStart(text[position]))
        {
            searchPosition++;
            continue;
        }

        const size_t nameStart = position;
        while (position < text.size())
        {
            if (IsIdentifier(text[position]) || text[position] == '.')
            {
                position++;
                continue;
            }
            if (position + 1 < text.size() && text[position] == ':' && text[position + 1] == ':')
            {
                position += 2;
                continue;
            }
            break;
        }
        const size_t nameEnd = position;

        SkipWhitespace(text, position);
        if (position >= text.size() || text[position] != '(')
        {
            searchPosition++;
            continue;
        }

        const size_t argumentsStart = ++position;
        int depth = 1;
        char quote = 0;
        bool escaped = false;
        while (position < text.size() && depth > 0)
        {
            const char character = text[position];
            if (quote)
            {
                if (escaped)
                    escaped = false;
                else if (character == '\\')
                    escaped = true;
                else if (character == quote)
                    quote = 0;
            }
            else if (character == '\'' || character == '"')
                quote = character;
            else if (character == '(')
                depth++;
            else if (character == ')')
                depth--;
            position++;
        }
        if (depth != 0)
            break;

        const size_t argumentsEnd = position - 1;
        SkipWhitespace(text, position);
        if (position >= text.size() || text[position] != '{')
        {
            searchPosition = position;
            continue;
        }

        line += static_cast<size_t>(std::count(text.begin() + countedPosition, text.begin() + nameStart, '\n'));
        countedPosition = nameStart;
        callback(std::string_view(text.data() + nameStart, nameEnd - nameStart),
                 std::string_view(text.data() + argumentsStart, argumentsEnd - argumentsStart), line);
        searchPosition = position + 1;
    }
}

bool MCPScriptFunctionSearch::IsScriptPath(std::string_view path)
{
    const size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos)
        return false;
    const std::string extension = ToLowerASCII(path.substr(dot));
    return extension == ".nut" || extension == ".gnut";
}

std::string MCPScriptFunctionSearch::JoinGamePath(std::string_view directory, std::string_view name)
{
    std::string path(directory);
    if (!path.empty() && path.back() != '\\' && path.back() != '/')
        path.push_back('\\');
    path.append(name);
    return path;
}

void MCPScriptFunctionSearch::EnumerateGameScripts(std::string_view directory, std::unordered_set<std::string>& seen, std::vector<std::string>& paths,
                                                   size_t depth)
{
    if (!g_pFilesystem || !g_pFilesystem->m_vtable || depth > 32)
        return;

    IFileSystem::VTable* vtable = g_pFilesystem->m_vtable;
    if (!vtable->FindFirstEx || !vtable->FindNext || !vtable->FindIsDirectory || !vtable->FindClose)
        return;

    const std::string wildcard = JoinGamePath(directory, "*");
    FileFindHandle_t handle = FILESYSTEM_INVALID_FIND_HANDLE;
    const char* found = vtable->FindFirstEx(g_pFilesystem, wildcard.c_str(), "GAME", &handle);
    std::vector<std::pair<std::string, bool>> entries;
    while (found)
    {
        std::string name(found);
        if (name != "." && name != "..")
            entries.emplace_back(std::move(name), vtable->FindIsDirectory(g_pFilesystem, handle));
        found = vtable->FindNext(g_pFilesystem, handle);
    }
    if (handle != FILESYSTEM_INVALID_FIND_HANDLE)
        vtable->FindClose(g_pFilesystem, handle);

    for (const auto& [name, isDirectory] : entries)
    {
        const std::string path = JoinGamePath(directory, name);
        if (isDirectory)
        {
            EnumerateGameScripts(path, seen, paths, depth + 1);
            continue;
        }
        if (!IsScriptPath(path))
            continue;

        const std::string key = ToLowerASCII(path);
        if (seen.insert(key).second)
            paths.push_back(path);
    }
}

std::vector<MCPScriptFunctionSearch::GameScriptSource> MCPScriptFunctionSearch::ReadGameScriptSources()
{
    std::unordered_set<std::string> seen;
    std::vector<std::string> paths;
    EnumerateGameScripts("scripts\\vscripts", seen, paths, 0);
    std::ranges::sort(paths);

    std::vector<GameScriptSource> sources;
    sources.reserve(paths.size());
    for (std::string& path : paths)
    {
        std::string contents = ReadVPKFile(path.c_str());
        if (contents.empty())
            continue;
        std::replace(path.begin(), path.end(), '\\', '/');
        sources.push_back({std::move(path), std::move(contents)});
    }
    return sources;
}

std::string MCPScriptFunctionSearch::ContextName(ScriptContext context)
{
    switch (context)
    {
    case ScriptContext::SERVER:
        return "server";
    case ScriptContext::CLIENT:
        return "client";
    case ScriptContext::UI:
        return "ui";
    default:
        return "invalid";
    }
}

nlohmann::json MCPScriptFunctionSearch::Execute() const
{
    using json = nlohmann::json;

    const std::string loweredQuery = ToLowerASCII(m_Options.query);
    std::unordered_map<std::string, json> matched;
    for (const SquirrelDocumentation::Function& function : SquirrelDocumentation::GetInstance().GetFunctions(m_Options.context))
    {
        if (!ContainsLowerASCII(function.name, loweredQuery) && !ContainsLowerASCII(function.description, loweredQuery))
            continue;
        matched.insert_or_assign(ToLowerASCII(function.name), json{
                                                                  {"name", function.name},
                                                                  {"signature", function.signature},
                                                                  {"description", function.description},
                                                                  {"native", true},
                                                              });
    }

    const std::vector<GameScriptSource> sources = ReadGameScriptSources();
    json sourceMatches = json::array();
    bool sourceMatchesTruncated = false;
    for (const GameScriptSource& source : sources)
    {
        ForEachScriptFunction(source.contents, [&](std::string_view name, std::string_view functionArguments, size_t line)
        {
            if (!ContainsLowerASCII(name, loweredQuery))
                return;
            const std::string key = ToLowerASCII(name);
            if (matched.contains(key))
                return;

            std::string signature;
            signature.reserve(name.size() + functionArguments.size() + 11);
            signature.append("function ").append(name).push_back('(');
            signature.append(functionArguments).push_back(')');
            matched.emplace(key, json{
                                     {"name", std::string(name)},
                                     {"signature", std::move(signature)},
                                     {"description", "<script function defined at " + source.path + " line " + std::to_string(line) + ">"},
                                     {"native", false},
                                     {"definition", {{"path", source.path}, {"line", line}}},
                                 });
        });

        if (!m_Options.includeSourceMatches || sourceMatchesTruncated)
            continue;
        size_t lineNumber = 1;
        size_t lineStart = 0;
        while (lineStart <= source.contents.size())
        {
            const size_t lineEnd = source.contents.find('\n', lineStart);
            const size_t length = (lineEnd == std::string::npos ? source.contents.size() : lineEnd) - lineStart;
            std::string_view line(source.contents.data() + lineStart, length);
            const bool matches =
                m_Options.sourceCaseSensitive ? line.find(m_Options.query) != std::string_view::npos : ContainsLowerASCII(line, loweredQuery);
            if (matches)
            {
                if (sourceMatches.size() >= static_cast<size_t>(m_Options.sourceLimit))
                {
                    sourceMatchesTruncated = true;
                    break;
                }
                std::string snippet(line.substr(0, 240));
                if (line.size() > 240)
                    snippet.append("...");
                sourceMatches.push_back({{"path", source.path}, {"line", lineNumber}, {"snippet", std::move(snippet)}});
            }
            if (lineEnd == std::string::npos)
                break;
            lineStart = lineEnd + 1;
            lineNumber++;
        }
    }

    std::vector<json> results;
    results.reserve(matched.size());
    for (auto& entry : matched)
        results.push_back(std::move(entry.second));
    std::ranges::sort(results, [](const json& left, const json& right)
    { return left["name"].get_ref<const std::string&>() < right["name"].get_ref<const std::string&>(); });

    const int total = static_cast<int>(results.size());
    const int start = std::min(m_Options.offset, total);
    const int end = std::min(start + m_Options.limit, total);
    json pageResults = json::array();
    for (int index = start; index < end; index++)
        pageResults.push_back(std::move(results[index]));
    const bool hasMore = end < total;

    json structured = {
        {"results", std::move(pageResults)},
        {"count", end - start},
        {"total", total},
        {"context", ContextName(m_Options.context)},
        {"query", m_Options.query},
        {"gamefs_file_count", sources.size()},
        {"page", {{"offset", start}, {"limit", m_Options.limit}, {"nextCursor", hasMore ? std::to_string(end) : ""}, {"hasMore", hasMore}}},
    };
    if (m_Options.includeSourceMatches)
    {
        structured["sourceSearch"] = {
            {"matches", std::move(sourceMatches)},
            {"limit", m_Options.sourceLimit},
            {"hasMore", sourceMatchesTruncated},
            {"caseSensitive", m_Options.sourceCaseSensitive},
        };
    }

    json content = json::array({{{"type", "text"}, {"text", structured.dump()}}});
    content.push_back({{"type", "text"}, {"text", "Found " + std::to_string(total) + " functions; returning " + std::to_string(end - start) + "."}});
    return {
        {"content", std::move(content)},
        {"structuredContent", std::move(structured)},
        {"isError", false},
    };
}
