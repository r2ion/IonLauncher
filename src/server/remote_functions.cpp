#include "server/remote_functions.h"

#include "common/remote_functions.h"
#include "engine/client/client.h"
#include "engine/r2engine.h"
#include "server/player.h"
#include "server/recipientfilter.h"
#include "server/usermessages.h"
#include "tier0/hooks.h"
#include "vscript/ivscript.h"
#include "vscript/languages/squirrel_re/squirrel.h"

#include <array>

CServerScriptRemoteFunctions g_ServerScriptRemoteFunctions;

std::size_t CServerScriptRemoteFunctions::FunctionNameHash::operator()(const std::string_view name) const noexcept
{
    return std::hash<std::string_view>{}(name);
}

void CServerScriptRemoteFunctions::BeginRegistration()
{
    m_Functions.clear();
    m_State = State::Registering;
}

void CServerScriptRemoteFunctions::FinishRegistration()
{
    m_State = State::Done;
}

SQRESULT CServerScriptRemoteFunctions::RegisterFunction(HSQUIRRELVM sqvm, const bool extended, bool& callOriginal)
{
    SquirrelManager* manager = g_pSquirrel[ScriptContext::SERVER];
    const char* functionName = manager->getstring(sqvm, 1);

    callOriginal = false;
    if (m_State != State::Registering)
    {
        const std::string error =
            fmt::format("Can't register '{}' - {}.", functionName,
                        m_State == State::Done ? "Already called EndRegisteringFunctions()" : "Haven't called BeginRegisteringFunctions()");
        manager->raiseerror(sqvm, error.c_str());
        return SQRESULT_ERROR;
    }

    const bool inserted = m_Functions.try_emplace(std::string(functionName), RegisteredFunction{extended}).second;
    if (!inserted)
    {
        const std::string error = fmt::format("Remote function '{}' is already registered.", functionName);
        manager->raiseerror(sqvm, error.c_str());
        return SQRESULT_ERROR;
    }

    callOriginal = !extended;
    return SQRESULT_NULL;
}

void CServerScriptRemoteFunctions::SendNamedRemoteFunctionCall(const CPlayer* player, const std::string_view functionName, const bool replay,
                                                               const bool callFromUI, const std::span<const ScriptVariant_t> parameters) const
{
    CSingleUserRecipientFilter filter(player);
    filter.MakeReliable();

    UserMessageBegin(filter, "RemoteFunctionCall", replay ? IN_REPLAY : NOT_IN_REPLAY);
    MessageWriteString(functionName.data());
    MessageWriteBool(callFromUI);
    MessageWriteLong(g_pGlobals->m_nTickCount);
    MessageWriteUBitLong(static_cast<std::uint32_t>(parameters.size()), 4);

    for (const ScriptVariant_t& parameter : parameters)
    {
        switch (parameter.m_type)
        {
        case FIELD_VOID:
            MessageWriteUBitLong(static_cast<std::uint32_t>(ScriptRemoteFunctionWireType::Null), 2);
            break;
        case FIELD_BOOLEAN:
            MessageWriteUBitLong(static_cast<std::uint32_t>(ScriptRemoteFunctionWireType::Boolean), 2);
            MessageWriteBool(parameter.m_bool);
            break;
        case FIELD_INTEGER:
            MessageWriteUBitLong(static_cast<std::uint32_t>(ScriptRemoteFunctionWireType::Integer), 2);
            MessageWriteLong(parameter.m_int);
            break;
        case FIELD_FLOAT:
            MessageWriteUBitLong(static_cast<std::uint32_t>(ScriptRemoteFunctionWireType::Float), 2);
            MessageWriteFloat(parameter.m_float);
            break;
        default:
            break;
        }
    }

    MessageWriteBool(replay);
    MessageEnd();
}

std::optional<int> CServerScriptRemoteFunctions::Call(HSQUIRRELVM sqvm, const bool replay, const bool callFromUI) const
{
    SquirrelManager* manager = g_pSquirrel[ScriptContext::SERVER];
    CPlayer* player = manager->getentity<CPlayer>(sqvm, 1);
    const std::uint32_t playerIndex = player ? player->m_nPlayerIndex : 0;
    if (playerIndex == 0 || playerIndex > MAX_PLAYERS)
        return std::nullopt;

    const char* functionName = manager->getstring(sqvm, 2);
    const auto registration = m_Functions.find(std::string_view(functionName));
    CClient* client = &g_pClientArray[playerIndex - 1];
    CClientExtended* extendedClient = client->GetClientExtended();
    if (!extendedClient || !extendedClient->IsExtendedClient())
    {
        if (registration != m_Functions.end() && registration->second.m_ExtendedOnly)
        {
            const std::string error = fmt::format("Remote function '{}' is only available to extended clients.", functionName);
            manager->raiseerror(sqvm, error.c_str());
            return 0;
        }

        return std::nullopt;
    }

    if (!player->m_bIsFullyConnected)
        return std::nullopt;

    if (registration == m_Functions.end())
    {
        const std::string error = fmt::format("Unregistered remote function name: '{}'", functionName);
        manager->raiseerror(sqvm, error.c_str());
        return 0;
    }

    if (registration->first.size() >= MAX_NAMED_REMOTE_FUNCTION_NAME)
    {
        const std::string error =
            fmt::format("Remote function name '{}' exceeds the {} byte extended-protocol limit.", functionName, MAX_NAMED_REMOTE_FUNCTION_NAME - 1);
        manager->raiseerror(sqvm, error.c_str());
        return 0;
    }

    const int parameterCount = sqvm->_top - sqvm->_stackbase - 3;
    if (parameterCount < 0 || parameterCount > MAX_SCRIPT_REMOTE_FUNCTION_PARAMETERS)
    {
        const std::string error = fmt::format("Too many extra params: {}/{}", parameterCount, MAX_SCRIPT_REMOTE_FUNCTION_PARAMETERS);
        manager->raiseerror(sqvm, error.c_str());
        return 0;
    }

    std::array<ScriptVariant_t, MAX_SCRIPT_REMOTE_FUNCTION_PARAMETERS> parameters;
    for (int i = 0; i < parameterCount; ++i)
    {
        SQObject object{};
        manager->__sq_getobject(sqvm, static_cast<SQInteger>(i + 3), &object);
        ScriptVariant_t& parameter = parameters[i];

        switch (object._Type)
        {
        case OT_NULL:
            parameter.m_type = FIELD_VOID;
            break;
        case OT_BOOL:
            parameter.m_bool = object._VAL.asInteger != 0;
            parameter.m_type = FIELD_BOOLEAN;
            break;
        case OT_INTEGER:
            parameter.m_int = object._VAL.asInteger;
            parameter.m_type = FIELD_INTEGER;
            break;
        case OT_FLOAT:
            parameter.m_float = object._VAL.asFloat;
            parameter.m_type = FIELD_FLOAT;
            break;
        default:
        {
            const std::string error = fmt::format("param {} is not of type null, bool, integer, or float", i + 3);
            manager->raiseerror(sqvm, error.c_str());
            return 0;
        }
        }
    }

    SendNamedRemoteFunctionCall(player, registration->first, replay, callFromUI,
                                std::span<const ScriptVariant_t>(parameters.data(), static_cast<std::size_t>(parameterCount)));
    return 0;
}

DECLARE_MODULE(ServerRemoteFunctionsHooks)

DECLARE_HOOK(CServerScriptRemoteFunctions_Call, server.dll + 0x27E680,
             ([](auto& hook, std::uintptr_t registry, HSQUIRRELVM sqvm, bool replay, bool callFromUI) -> int
{
    const std::optional<int> result = g_ServerScriptRemoteFunctions.Call(sqvm, replay, callFromUI);
    if (result)
        return *result;

    return hook.Original(registry, sqvm, replay, callFromUI);
}))

ON_DLL_LOAD("server.dll", ServerRemoteFunctionsLoad, [](CModule module) { DISPATCH_MODULE(ServerRemoteFunctionsHooks) })
