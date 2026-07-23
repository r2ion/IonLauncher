#pragma once

#include <rapidjson/document.h>

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

enum class ScriptContext : int;

class MCPScriptFunctionSearch final
{
  public:
    struct Options
    {
        std::string query;
        ScriptContext context;
        int limit;
        int offset;
        int sourceLimit;
        bool includeSourceMatches;
        bool sourceCaseSensitive;
    };

    explicit MCPScriptFunctionSearch(Options options);

    rapidjson::Document Execute() const;

  private:
    struct GameScriptSource
    {
        std::string path;
        std::string contents;
    };

    static std::string ToLowerASCII(std::string_view value);
    static bool ContainsLowerASCII(std::string_view value, std::string_view loweredNeedle);
    static bool IsIdentifierStart(char character);
    static bool IsIdentifier(char character);
    static void SkipWhitespace(const std::string& text, size_t& position);
    static bool IsScriptPath(std::string_view path);
    static std::string JoinGamePath(std::string_view directory, std::string_view name);
    static void EnumerateGameScripts(std::string_view directory, std::unordered_set<std::string>& seen, std::vector<std::string>& paths,
                                     size_t depth);
    static std::vector<GameScriptSource> ReadGameScriptSources();
    static std::string ContextName(ScriptContext context);

    template <typename Callback> static void ForEachScriptFunction(const std::string& text, Callback&& callback);

    Options m_Options;
};
