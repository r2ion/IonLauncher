#include "client/hud_scriptelements.h"
#include "server/remote_functions.h"
#include "vscript/languages/squirrel_re/squirrel.h"

#include <string>

template <ScriptContext context> SQRESULT CallOriginalRemoteScriptFunction(const char* functionName, HSQUIRRELVM sqvm)
{
    SquirrelManager* manager = g_pSquirrel[context];
    const auto original = manager->m_funcOriginals.find(functionName);
    if (original != manager->m_funcOriginals.end() && original->second)
        return original->second(sqvm);

    const std::string error = fmt::format("function '{}' is unavailable.", functionName);
    manager->raiseerror(sqvm, error.c_str());
    return SQRESULT_ERROR;
}

REPLACE_SQFUNC(Remote_BeginRegisteringFunctions, ScriptContext::CLIENT | ScriptContext::SERVER)
{
    if constexpr (context == ScriptContext::CLIENT)
        CHudScriptElements::BeginRemoteFunctionRegistration();
    else
        g_ServerScriptRemoteFunctions.BeginRegistration();

    return CallOriginalRemoteScriptFunction<context>("Remote_BeginRegisteringFunctions", sqvm);
}

REPLACE_SQFUNC(Remote_EndRegisteringFunctions, ScriptContext::CLIENT | ScriptContext::SERVER)
{
    const SQRESULT result = CallOriginalRemoteScriptFunction<context>("Remote_EndRegisteringFunctions", sqvm);

    if constexpr (context == ScriptContext::CLIENT)
        CHudScriptElements::FinishRemoteFunctionRegistration();
    else
        g_ServerScriptRemoteFunctions.FinishRegistration();

    return result;
}

template <ScriptContext context> static SQRESULT RegisterRemoteFunction(HSQUIRRELVM sqvm, const bool extended)
{
    bool callOriginal = false;
    SQRESULT result;
    if constexpr (context == ScriptContext::CLIENT)
        result = CHudScriptElements::RegisterRemoteFunction(sqvm, extended, callOriginal);
    else
        result = g_ServerScriptRemoteFunctions.RegisterFunction(sqvm, extended, callOriginal);

    if (SQ_FAILED(result) || !callOriginal)
        return result;

    return CallOriginalRemoteScriptFunction<context>("Remote_RegisterFunction", sqvm);
}

REPLACE_SQFUNC_WITH_ALIAS(Remote_RegisterFunction, "string name, bool extended = false", "NS_InternalRemote_RegisterFunction",
                          ScriptContext::CLIENT | ScriptContext::SERVER)
{
    SquirrelManager* manager = g_pSquirrel[context];
    const SQInteger parameterCount = sqvm->_top - sqvm->_stackbase - 1;
    if (parameterCount != 1)
    {
        const std::string error = fmt::format("Remote_RegisterFunction expected 1 argument, got {}.", parameterCount);
        manager->raiseerror(sqvm, error.c_str());
        return SQRESULT_ERROR;
    }

    return RegisterRemoteFunction<context>(sqvm, false);
}

ADD_NAMED_SQFUNC_WITH_DEFAULTS("var", "Remote_RegisterFunction", Remote_RegisterFunctionWithExtended, "string name, bool extended = false",
                               "Registers a remote function. Extended-only functions are sent by name and are unavailable to normal clients.", 1,
                               ".sb", ScriptContext::CLIENT | ScriptContext::SERVER)
{
    SquirrelManager* manager = g_pSquirrel[context];
    const SQInteger parameterCount = sqvm->_top - sqvm->_stackbase - 1;
    if (parameterCount < 1 || parameterCount > 2)
    {
        const std::string error = fmt::format("Remote_RegisterFunction expected 1 or 2 arguments, got {}.", parameterCount);
        manager->raiseerror(sqvm, error.c_str());
        return SQRESULT_ERROR;
    }

    const bool extended = parameterCount == 2 && manager->getbool(sqvm, 2);
    return RegisterRemoteFunction<context>(sqvm, extended);
}
