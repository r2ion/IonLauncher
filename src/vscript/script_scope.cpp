//========= Copyright Valve Corporation, All rights reserved. ============//
#include "vscript/script_scope.h"
#include "core/tier1.h"
#include "vscript/languages/squirrel_re/squirrel.h"

using ScriptScopeCreateScopeFn = HSCRIPT (*)(CSquirrelVM*, const char*);
using ScriptScopeReleaseScopeFn = void (*)(CSquirrelVM*, HSCRIPT);
using ScriptScopeSetValueFn = bool (*)(CSquirrelVM*, HSCRIPT, const char*, const ScriptVariant_t&);
using ScriptScopeObjectToVariantFn = bool (*)(const SQObject*, ScriptVariant_t*);
using ScriptScopeGetQuietFn = SQRESULT (*)(SQVM*);
using ScriptScopePopFn = void (*)(SQVM*);
using ScriptScopeTableGetFn = bool (*)(SQTable*, const SQObject&, SQObject&);
using ScriptScopeTableRemoveFn = void (*)(SQTable*, const SQObject&);

struct ScriptScopeNativeFunctions
{
    ScriptScopeCreateScopeFn CreateScope;
    ScriptScopeReleaseScopeFn ReleaseScope;
    ScriptScopeSetValueFn SetValue;
    ScriptScopeObjectToVariantFn ObjectToVariant;
    ScriptScopeGetQuietFn GetQuiet;
    ScriptScopePopFn Pop;
    ScriptScopeTableGetFn TableGet;
    ScriptScopeTableRemoveFn TableRemove;
};

std::array<ScriptScopeNativeFunctions, 3> s_ScriptScopeNativeFunctions;

ScriptScopeNativeFunctions& ScriptScopeFunctions(const ScriptContext context)
{
    return s_ScriptScopeNativeFunctions[static_cast<size_t>(context)];
}

template <ScriptContext context> CSquirrelVM* CScriptScopeT<context>::GetVM()
{
    SquirrelManager* squirrel = g_pSquirrel[context];
    return squirrel ? squirrel->m_pSQVM : nullptr;
}

template <ScriptContext context> bool CScriptScopeT<context>::Init(const char* pszName)
{
    Term();
    m_hScope = ScriptScopeFunctions(context).CreateScope(GetVM(), pszName);
    return m_hScope != nullptr;
}

template <ScriptContext context> bool CScriptScopeT<context>::Init(HSCRIPT hScope, bool bExternal)
{
    Term();
    m_hScope = hScope;
    m_flags = bExternal ? EXTERNAL : 0;
    return IsValid(hScope);
}

template <ScriptContext context> bool CScriptScopeT<context>::InitGlobal()
{
    Term();
    m_hScope = nullptr;
    m_flags = EXTERNAL;
    return true;
}

template <ScriptContext context> void CScriptScopeT<context>::Term()
{
    if (IsInitialized())
    {
        CSquirrelVM* vm = GetVM();
        if (vm)
        {
            for (int i = 0; i < m_FuncHandles.Count(); ++i)
            {
                if (IsValid(*m_FuncHandles[i]))
                    vm->ReleaseFunction(*m_FuncHandles[i]);
            }
            if (IsValid(m_hScope) && !(m_flags & EXTERNAL))
                ScriptScopeFunctions(context).ReleaseScope(vm, m_hScope);
        }
        m_FuncHandles.Purge();
        m_hScope = INVALID_HSCRIPT;
    }
    m_flags = 0;
}

template <ScriptContext context> void CScriptScopeT<context>::InvalidateCachedValues()
{
    for (int i = 0; i < m_FuncHandles.Count(); ++i)
    {
        ReleaseFunction(*m_FuncHandles[i]);
        *m_FuncHandles[i] = INVALID_HSCRIPT;
    }
    m_FuncHandles.RemoveAll();
}

static bool ScriptScopePush(SquirrelManager* squirrel, SQVM* vm, HSCRIPT scope)
{
    if (scope == INVALID_HSCRIPT)
        return false;
    if (scope)
    {
        SQObject* object = reinterpret_cast<SQObject*>(scope);
        if (object->_Type != OT_TABLE)
            return false;
        squirrel->pushobject(vm, object);
    }
    else
    {
        squirrel->pushroottable(vm);
    }
    return true;
}

template <ScriptContext context> bool CScriptScopeT<context>::ValueExists(const char* pszKey) const
{
    SquirrelManager* squirrel = g_pSquirrel[context];
    SQVM* vm = GetVM()->sqvm;
    if (!ScriptScopePush(squirrel, vm, m_hScope))
        return false;
    squirrel->pushstring(vm, pszKey);
    const bool found = ScriptScopeFunctions(context).GetQuiet(vm) == SQRESULT_NULL;
    if (found)
        ScriptScopeFunctions(context).Pop(vm);
    ScriptScopeFunctions(context).Pop(vm);
    return found;
}

template <ScriptContext context> bool CScriptScopeT<context>::SetValue(const char* pszKey, const ScriptVariant_t& value)
{
    return ScriptScopeFunctions(context).SetValue(GetVM(), m_hScope, pszKey, value);
}

template <ScriptContext context> bool CScriptScopeT<context>::GetValue(const char* pszKey, ScriptVariant_t* pValue) const
{
    SquirrelManager* squirrel = g_pSquirrel[context];
    SQVM* vm = GetVM()->sqvm;
    if (!ScriptScopePush(squirrel, vm, m_hScope))
        return false;
    squirrel->pushstring(vm, pszKey);
    const bool found = ScriptScopeFunctions(context).GetQuiet(vm) == SQRESULT_NULL;
    if (found)
    {
        ReleaseValue(*pValue);
        ScriptScopeFunctions(context).ObjectToVariant(&vm->_stack[vm->_top - 1], pValue);
        ScriptScopeFunctions(context).Pop(vm);
    }
    ScriptScopeFunctions(context).Pop(vm);
    return found;
}

template <ScriptContext context> void CScriptScopeT<context>::ReleaseValue(ScriptVariant_t& value) const
{
    if (value.m_type == FIELD_HSCRIPT && (value.m_flags & ScriptVariant_t::SV_RELEASE))
    {
        GetVM()->ReleaseFunction(value.m_hScript);
        value.m_dataPointer = nullptr;
        value.m_flags = 0;
    }
    value.FreeVariantMemory();
    value.m_flags = 0;
    value.m_type = FIELD_VOID;
}

template <ScriptContext context> bool CScriptScopeT<context>::ClearValue(const char* pszKey)
{
    SquirrelManager* squirrel = g_pSquirrel[context];
    SQVM* vm = GetVM()->sqvm;
    if (!ScriptScopePush(squirrel, vm, m_hScope))
        return false;
    squirrel->pushstring(vm, pszKey);
    SQTable* table = vm->_stack[vm->_top - 2]._VAL.asTable;
    const SQObject& key = vm->_stack[vm->_top - 1];
    SQObject oldValue{OT_NULL, 0, {}};
    const bool found = ScriptScopeFunctions(context).TableGet(table, key, oldValue);
    if (found)
    {
        ScriptScopeFunctions(context).TableRemove(table, key);
        if (ISREFCOUNTED(oldValue._Type) && --oldValue._VAL.asRefCounted->_uiRef == 0)
            oldValue._VAL.asRefCounted->Release();
    }
    ScriptScopeFunctions(context).Pop(vm);
    ScriptScopeFunctions(context).Pop(vm);
    return found;
}

template <ScriptContext context> HSCRIPT CScriptScopeT<context>::LookupFunction(const char* pszFunction, const char* requiredType) const
{
    return GetVM()->FindFunction(pszFunction, requiredType, m_hScope);
}

template <ScriptContext context> void CScriptScopeT<context>::ReleaseFunction(HSCRIPT hFunction) const
{
    GetVM()->ReleaseFunction(hFunction);
}

template <ScriptContext context> bool CScriptScopeT<context>::FunctionExists(const char* pszFunction) const
{
    HSCRIPT function = LookupFunction(pszFunction);
    ReleaseFunction(function);
    return IsValid(function);
}

template <ScriptContext context>
ScriptStatus_t CScriptScopeT<context>::ExecuteFunction(HSCRIPT hFunction, ScriptVariant_t* pArgs, int nArgs, ScriptVariant_t* pReturn) const
{
    return GetVM()->ExecuteFunction(hFunction, pArgs, nArgs, pReturn, m_hScope);
}

template <ScriptContext context> ScriptStatus_t CScriptScopeT<context>::Run(HSCRIPT hScript)
{
    InvalidateCachedValues();
    return ExecuteFunction(hScript, nullptr, 0);
}

template <ScriptContext context> ScriptStatus_t CScriptScopeT<context>::Run(const char* pszScriptText, const char* pszScriptName)
{
    InvalidateCachedValues();
    SQVM* vm = GetVM()->sqvm;
    const int top = vm->_top;
    SQBufferState buffer(pszScriptText);
    ScriptStatus_t status = SCRIPT_ERROR;
    if (g_pSquirrel[context]->compilebuffer(&buffer, pszScriptName ? pszScriptName : "unnamedbuffer") != SQRESULT_ERROR)
    {
        SQObject script = vm->_stack[vm->_top - 1];
        status = ExecuteFunction(reinterpret_cast<HSCRIPT>(&script), nullptr, 0);
    }
    while (vm->_top > top)
        ScriptScopeFunctions(context).Pop(vm);
    return status;
}

template class CScriptScopeT<ScriptContext::SERVER>;
template class CScriptScopeT<ScriptContext::CLIENT>;
template class CScriptScopeT<ScriptContext::UI>;

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", ClientScriptScope, ClientSquirrel, [](CModule module)
{
    ScriptScopeNativeFunctions& functions = ScriptScopeFunctions(ScriptContext::CLIENT);
    functions.CreateScope = module.Offset(0xFFD0).RCast<ScriptScopeCreateScopeFn>();
    functions.ReleaseScope = module.Offset(0x10090).RCast<ScriptScopeReleaseScopeFn>();
    functions.SetValue = module.Offset(0x11800).RCast<ScriptScopeSetValueFn>();
    functions.ObjectToVariant = module.Offset(0x14110).RCast<ScriptScopeObjectToVariantFn>();
    functions.GetQuiet = module.Offset(0x7D90).RCast<ScriptScopeGetQuietFn>();
    functions.Pop = module.Offset(0x35420).RCast<ScriptScopePopFn>();
    functions.TableGet = module.Offset(0x6A670).RCast<ScriptScopeTableGetFn>();
    functions.TableRemove = module.Offset(0x69F10).RCast<ScriptScopeTableRemoveFn>();
    ScriptScopeFunctions(ScriptContext::UI) = functions;
})

ON_DLL_LOAD_RELIESON("server.dll", ServerScriptScope, ServerSquirrel, [](CModule module)
{
    ScriptScopeNativeFunctions& functions = ScriptScopeFunctions(ScriptContext::SERVER);
    functions.CreateScope = module.Offset(0x1D400).RCast<ScriptScopeCreateScopeFn>();
    functions.ReleaseScope = module.Offset(0x1D4C0).RCast<ScriptScopeReleaseScopeFn>();
    functions.SetValue = module.Offset(0x1EC30).RCast<ScriptScopeSetValueFn>();
    functions.ObjectToVariant = module.Offset(0x21540).RCast<ScriptScopeObjectToVariantFn>();
    functions.GetQuiet = module.Offset(0x7D60).RCast<ScriptScopeGetQuietFn>();
    functions.Pop = module.Offset(0x353D0).RCast<ScriptScopePopFn>();
    functions.TableGet = module.Offset(0x6A600).RCast<ScriptScopeTableGetFn>();
    functions.TableRemove = module.Offset(0x69EA0).RCast<ScriptScopeTableRemoveFn>();
})
