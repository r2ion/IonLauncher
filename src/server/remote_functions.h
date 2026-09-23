#pragma once

#include "vscript/ivscript.h"
#include "vscript/languages/squirrel_re/include/squirrel.h"

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

class CPlayer;

class CServerScriptRemoteFunctions
{
  public:
    void BeginRegistration();
    void FinishRegistration();
    SQRESULT RegisterFunction(HSQUIRRELVM sqvm, bool extended, bool& callOriginal);
    std::optional<int> Call(HSQUIRRELVM sqvm, bool replay, bool callFromUI) const;

  private:
    enum class State
    {
        Init,
        Registering,
        Done,
        Count,
    };

    struct FunctionNameHash
    {
        using is_transparent = void;

        std::size_t operator()(std::string_view name) const noexcept;
    };

    struct RegisteredFunction
    {
        bool m_ExtendedOnly;
    };

    using RegisteredFunctions = std::unordered_map<std::string, RegisteredFunction, FunctionNameHash, std::equal_to<>>;

    void SendNamedRemoteFunctionCall(const CPlayer* player, std::string_view functionName, bool replay, bool callFromUI,
                                     std::span<const ScriptVariant_t> parameters) const;

    RegisteredFunctions m_Functions;
    State m_State = State::Init;
};

extern CServerScriptRemoteFunctions g_ServerScriptRemoteFunctions;
