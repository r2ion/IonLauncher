#include "vscript/languages/squirrel_re/vsquirrel.h"

#include "tier0/callbacks.h"

#include <array>

using CSquirrelVMSetTime_t = void (*)(CSquirrelVM*, float);
using CSquirrelVMFindFunction_t = HSCRIPT (*)(CSquirrelVM*, const char*, const char*, HSCRIPT);
using CSquirrelVMReleaseFunction_t = void (*)(CSquirrelVM*, HSCRIPT);
using CSquirrelVMExecuteFunction_t = ScriptStatus_t (*)(CSquirrelVM*, HSCRIPT, ScriptVariant_t*, int, ScriptVariant_t*, HSCRIPT, bool);

struct CSquirrelVMNativeFunctions
{
    CSquirrelVMSetTime_t SetTime;
    CSquirrelVMFindFunction_t FindFunction;
    CSquirrelVMReleaseFunction_t ReleaseFunction;
    CSquirrelVMExecuteFunction_t ExecuteFunction;
};

std::array<CSquirrelVMNativeFunctions, 3> g_CSquirrelVMNativeFunctions;

CSquirrelVMNativeFunctions& GetNativeFunctions(const CSquirrelVM* vm)
{
    Assert(vm->vmContext >= static_cast<int>(ScriptContext::SERVER) && vm->vmContext <= static_cast<int>(ScriptContext::UI));
    return g_CSquirrelVMNativeFunctions[static_cast<size_t>(vm->vmContext)];
}

void CSquirrelVM::SetTime(const float time)
{
    GetNativeFunctions(this).SetTime(this, time);
}

HSCRIPT CSquirrelVM::FindFunction(const char* functionName, const char* requiredType, const HSCRIPT scope)
{
    return GetNativeFunctions(this).FindFunction(this, functionName, requiredType, scope);
}

void CSquirrelVM::ReleaseFunction(const HSCRIPT function)
{
    if (IsValid(function))
        GetNativeFunctions(this).ReleaseFunction(this, function);
}

ScriptStatus_t CSquirrelVM::ExecuteFunction(HSCRIPT function, ScriptVariant_t* arguments, const int argumentCount, ScriptVariant_t* returnValue,
                                            const HSCRIPT scope)
{
    return GetNativeFunctions(this).ExecuteFunction(this, function, arguments, argumentCount, returnValue, scope, true);
}

ON_DLL_LOAD_CLIENT("client.dll", ClientSquirrelVMMethods, [](CModule module)
{
    CSquirrelVMNativeFunctions& functions = g_CSquirrelVMNativeFunctions[static_cast<size_t>(ScriptContext::CLIENT)];
    functions.SetTime = module.Offset(0xF450).RCast<CSquirrelVMSetTime_t>();
    functions.FindFunction = module.Offset(0x100D0).RCast<CSquirrelVMFindFunction_t>();
    functions.ReleaseFunction = module.Offset(0x10150).RCast<CSquirrelVMReleaseFunction_t>();
    functions.ExecuteFunction = module.Offset(0x10500).RCast<CSquirrelVMExecuteFunction_t>();
    g_CSquirrelVMNativeFunctions[static_cast<size_t>(ScriptContext::UI)] = functions;
})

ON_DLL_LOAD("server.dll", ServerSquirrelVMMethods, [](CModule module)
{
    CSquirrelVMNativeFunctions& functions = g_CSquirrelVMNativeFunctions[static_cast<size_t>(ScriptContext::SERVER)];
    functions.SetTime = module.Offset(0x1C880).RCast<CSquirrelVMSetTime_t>();
    functions.FindFunction = module.Offset(0x1D500).RCast<CSquirrelVMFindFunction_t>();
    functions.ReleaseFunction = module.Offset(0x1D580).RCast<CSquirrelVMReleaseFunction_t>();
    functions.ExecuteFunction = module.Offset(0x1D930).RCast<CSquirrelVMExecuteFunction_t>();
})
