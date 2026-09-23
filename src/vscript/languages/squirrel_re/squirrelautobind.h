#pragma once
#include <vector>

typedef void (*SqAutoBindFunc)();

class SquirrelAutoBindContainer
{
  public:
    std::vector<std::function<void()>> clientSqAutoBindFuncs;
    std::vector<std::function<void()>> serverSqAutoBindFuncs;
};

extern SquirrelAutoBindContainer* g_pSqAutoBindContainer;

class __squirrelautobind;

#define ADD_NAMED_SQFUNC_WITH_DEFAULTS(returnType, squirrelName, funcName, argTypes, helpText, defaultParameterCount, typeMask, runOnContext)        \
    template <ScriptContext context> SQRESULT CONCAT2(Script_, funcName)(HSQUIRRELVM sqvm);                                                          \
    namespace                                                                                                                                        \
    {                                                                                                                                                \
    __squirrelautobind CONCAT2(__squirrelautobind, __LINE__)(                                                                                        \
        []()                                                                                                                                         \
    {                                                                                                                                                \
        if constexpr ((runOnContext) & ScriptContext::UI)                                                                                            \
            g_pSquirrel[ScriptContext::UI]->AddFuncRegistration(returnType, squirrelName, argTypes, helpText,                                        \
                                                                CONCAT2(Script_, funcName) < ScriptContext::UI >, defaultParameterCount, typeMask);  \
        if constexpr ((runOnContext) & ScriptContext::CLIENT)                                                                                        \
            g_pSquirrel[ScriptContext::CLIENT]->AddFuncRegistration(returnType, squirrelName, argTypes, helpText,                                    \
                                                                    CONCAT2(Script_, funcName) < ScriptContext::CLIENT >, defaultParameterCount,     \
                                                                    typeMask);                                                                       \
    }, []()                                                                                                                                          \
    {                                                                                                                                                \
        if constexpr ((runOnContext) & ScriptContext::SERVER)                                                                                        \
            g_pSquirrel[ScriptContext::SERVER]->AddFuncRegistration(returnType, squirrelName, argTypes, helpText,                                    \
                                                                    CONCAT2(Script_, funcName) < ScriptContext::SERVER >, defaultParameterCount,     \
                                                                    typeMask);                                                                       \
    });                                                                                                                                              \
    }                                                                                                                                                \
    template <ScriptContext context> SQRESULT CONCAT2(Script_, funcName)(HSQUIRRELVM sqvm)

#define ADD_SQFUNC(returnType, funcName, argTypes, helpText, runOnContext)                                                                           \
    ADD_NAMED_SQFUNC_WITH_DEFAULTS(returnType, __STR(funcName), funcName, argTypes, helpText, 0, nullptr, runOnContext)

#define REPLACE_SQFUNC_WITH_ALIAS(funcName, argTypes, nativeName, runOnContext)                                                                      \
    template <ScriptContext context> SQRESULT CONCAT2(Script_, funcName)(HSQUIRRELVM sqvm);                                                          \
    namespace                                                                                                                                        \
    {                                                                                                                                                \
    __squirrelautobind CONCAT2(__squirrelautobind, __LINE__)(                                                                                        \
        []()                                                                                                                                         \
    {                                                                                                                                                \
        if constexpr ((runOnContext) & ScriptContext::UI)                                                                                            \
            g_pSquirrel[ScriptContext::UI]->AddFuncOverride(__STR(funcName), CONCAT2(Script_, funcName) < ScriptContext::UI >, argTypes,             \
                                                            nativeName);                                                                             \
        if constexpr ((runOnContext) & ScriptContext::CLIENT)                                                                                        \
            g_pSquirrel[ScriptContext::CLIENT]->AddFuncOverride(__STR(funcName), CONCAT2(Script_, funcName) < ScriptContext::CLIENT >, argTypes,     \
                                                                nativeName);                                                                         \
    }, []()                                                                                                                                          \
    {                                                                                                                                                \
        if constexpr ((runOnContext) & ScriptContext::SERVER)                                                                                        \
            g_pSquirrel[ScriptContext::SERVER]->AddFuncOverride(__STR(funcName), CONCAT2(Script_, funcName) < ScriptContext::SERVER >, argTypes,     \
                                                                nativeName);                                                                         \
    });                                                                                                                                              \
    }                                                                                                                                                \
    template <ScriptContext context> SQRESULT CONCAT2(Script_, funcName)(HSQUIRRELVM sqvm)

#define REPLACE_SQFUNC(funcName, runOnContext) REPLACE_SQFUNC_WITH_ALIAS(funcName, nullptr, nullptr, runOnContext)

class __squirrelautobind
{
  public:
    __squirrelautobind() = delete;

    __squirrelautobind(std::function<void()> clientAutoBindFunc, std::function<void()> serverAutoBindFunc)
    {
        // Bit hacky but we can't initialise this normally since this gets run automatically on load
        if (g_pSqAutoBindContainer == nullptr)
            g_pSqAutoBindContainer = new SquirrelAutoBindContainer();

        g_pSqAutoBindContainer->clientSqAutoBindFuncs.push_back(clientAutoBindFunc);
        g_pSqAutoBindContainer->serverSqAutoBindFuncs.push_back(serverAutoBindFunc);
    }
};
