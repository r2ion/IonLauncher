//========= Copyright Valve Corporation, All rights reserved. ============//
#include "vscript/script_scope.h"
#include "core/tier1.h"
#include "vscript/languages/squirrel_re/squirrel.h"

SquirrelManager* s_ClientSquirrel;
HSCRIPT (*s_CreateScope)(CSquirrelVM*, const char*);
void (*s_ReleaseScope)(CSquirrelVM*, HSCRIPT);
HSCRIPT (*s_LookupFunction)(CSquirrelVM*, const char*, const char*, HSCRIPT);
void (*s_ReleaseFunction)(CSquirrelVM*, HSCRIPT);
ScriptStatus_t (*s_ExecuteFunction)(CSquirrelVM*, HSCRIPT, ScriptVariant_t*, int, ScriptVariant_t*, HSCRIPT, bool);
bool (*s_SetValue)(CSquirrelVM*, HSCRIPT, const char*, const ScriptVariant_t&);
bool (*s_ObjectToVariant)(const SQObject*, ScriptVariant_t*);
SQRESULT (*s_GetQuiet)(SQVM*);
void (*s_Pop)(SQVM*);
bool (*s_TableGet)(SQTable*, const SQObject&, SQObject&);
void (*s_TableRemove)(SQTable*, const SQObject&);

CSquirrelVM* CScriptScope::GetVM()
{
    return s_ClientSquirrel ? s_ClientSquirrel->m_pSQVM : nullptr;
}

bool CScriptScope::Init(const char* pszName)
{
    Term();
    m_hScope = s_CreateScope(GetVM(), pszName);
    return m_hScope != nullptr;
}

bool CScriptScope::Init(HSCRIPT hScope, bool bExternal)
{
    Term();
    m_hScope = hScope;
    m_flags = bExternal ? EXTERNAL : 0;
    return IsValid(hScope);
}

bool CScriptScope::InitGlobal()
{
    Term();
    m_hScope = nullptr;
    m_flags = EXTERNAL;
    return true;
}

void CScriptScope::Term()
{
    if (IsInitialized())
    {
        CSquirrelVM* vm = GetVM();
        if (vm)
        {
            for (int i = 0; i < m_FuncHandles.Count(); ++i)
            {
                if (IsValid(*m_FuncHandles[i]))
                    s_ReleaseFunction(vm, *m_FuncHandles[i]);
            }
            if (IsValid(m_hScope) && !(m_flags & EXTERNAL))
                s_ReleaseScope(vm, m_hScope);
        }
        m_FuncHandles.Purge();
        m_hScope = INVALID_HSCRIPT;
    }
    m_flags = 0;
}

void CScriptScope::InvalidateCachedValues()
{
    for (int i = 0; i < m_FuncHandles.Count(); ++i)
    {
        ReleaseFunction(*m_FuncHandles[i]);
        *m_FuncHandles[i] = INVALID_HSCRIPT;
    }
    m_FuncHandles.RemoveAll();
}

static bool ScriptScopePush(SQVM* vm, HSCRIPT scope)
{
    if (scope == INVALID_HSCRIPT)
        return false;
    if (scope)
    {
        SQObject* object = reinterpret_cast<SQObject*>(scope);
        if (object->_Type != OT_TABLE)
            return false;
        s_ClientSquirrel->pushobject(vm, object);
    }
    else
    {
        s_ClientSquirrel->pushroottable(vm);
    }
    return true;
}

bool CScriptScope::ValueExists(const char* pszKey) const
{
    SQVM* vm = GetVM()->sqvm;
    if (!ScriptScopePush(vm, m_hScope))
        return false;
    s_ClientSquirrel->pushstring(vm, pszKey);
    const bool found = s_GetQuiet(vm) == SQRESULT_NULL;
    if (found)
        s_Pop(vm);
    s_Pop(vm);
    return found;
}

bool CScriptScope::SetValue(const char* pszKey, const ScriptVariant_t& value)
{
    return s_SetValue(GetVM(), m_hScope, pszKey, value);
}

bool CScriptScope::GetValue(const char* pszKey, ScriptVariant_t* pValue) const
{
    SQVM* vm = GetVM()->sqvm;
    if (!ScriptScopePush(vm, m_hScope))
        return false;
    s_ClientSquirrel->pushstring(vm, pszKey);
    const bool found = s_GetQuiet(vm) == SQRESULT_NULL;
    if (found)
    {
        ReleaseValue(*pValue);
        s_ObjectToVariant(&vm->_stack[vm->_top - 1], pValue);
        s_Pop(vm);
    }
    s_Pop(vm);
    return found;
}

void CScriptScope::ReleaseValue(ScriptVariant_t& value) const
{
    if (value.m_type == FIELD_HSCRIPT && (value.m_flags & ScriptVariant_t::SV_RELEASE))
    {
        s_ReleaseFunction(GetVM(), value.m_hScript);
        value.m_dataPointer = nullptr;
        value.m_flags = 0;
    }
    value.FreeVariantMemory();
    value.m_flags = 0;
    value.m_type = FIELD_VOID;
}

bool CScriptScope::ClearValue(const char* pszKey)
{
    SQVM* vm = GetVM()->sqvm;
    if (!ScriptScopePush(vm, m_hScope))
        return false;
    s_ClientSquirrel->pushstring(vm, pszKey);
    SQTable* table = vm->_stack[vm->_top - 2]._VAL.asTable;
    const SQObject& key = vm->_stack[vm->_top - 1];
    SQObject oldValue{OT_NULL, 0, {}};
    const bool found = s_TableGet(table, key, oldValue);
    if (found)
    {
        s_TableRemove(table, key);
        if (ISREFCOUNTED(oldValue._Type) && --oldValue._VAL.asRefCounted->_uiRef == 0)
            oldValue._VAL.asRefCounted->Release();
    }
    s_Pop(vm);
    s_Pop(vm);
    return found;
}

HSCRIPT CScriptScope::LookupFunction(const char* pszFunction, const char* requiredType) const
{
    return s_LookupFunction(GetVM(), pszFunction, requiredType, m_hScope);
}

void CScriptScope::ReleaseFunction(HSCRIPT hFunction) const
{
    if (IsValid(hFunction))
        s_ReleaseFunction(GetVM(), hFunction);
}

bool CScriptScope::FunctionExists(const char* pszFunction) const
{
    HSCRIPT function = LookupFunction(pszFunction);
    ReleaseFunction(function);
    return IsValid(function);
}

ScriptStatus_t CScriptScope::ExecuteFunction(HSCRIPT hFunction, ScriptVariant_t* pArgs, int nArgs, ScriptVariant_t* pReturn) const
{
    return s_ExecuteFunction(GetVM(), hFunction, pArgs, nArgs, pReturn, m_hScope, true);
}

ScriptStatus_t CScriptScope::Run(HSCRIPT hScript)
{
    InvalidateCachedValues();
    return ExecuteFunction(hScript, nullptr, 0);
}

ScriptStatus_t CScriptScope::Run(const char* pszScriptText, const char* pszScriptName)
{
    InvalidateCachedValues();
    SQVM* vm = GetVM()->sqvm;
    const int top = vm->_top;
    SQBufferState buffer(pszScriptText);
    ScriptStatus_t status = SCRIPT_ERROR;
    if (s_ClientSquirrel->compilebuffer(&buffer, pszScriptName ? pszScriptName : "unnamedbuffer") != SQRESULT_ERROR)
    {
        SQObject script = vm->_stack[vm->_top - 1];
        status = ExecuteFunction(reinterpret_cast<HSCRIPT>(&script), nullptr, 0);
    }
    while (vm->_top > top)
        s_Pop(vm);
    return status;
}

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", ScriptScope, ClientSquirrel, [](CModule module)
{
    s_ClientSquirrel = g_pSquirrel[ScriptContext::CLIENT];
    s_CreateScope = module.Offset(0xFFD0).RCast<decltype(s_CreateScope)>();
    s_ReleaseScope = module.Offset(0x10090).RCast<decltype(s_ReleaseScope)>();
    s_LookupFunction = module.Offset(0x100D0).RCast<decltype(s_LookupFunction)>();
    s_ReleaseFunction = module.Offset(0x10150).RCast<decltype(s_ReleaseFunction)>();
    s_ExecuteFunction = module.Offset(0x10500).RCast<decltype(s_ExecuteFunction)>();
    s_SetValue = module.Offset(0x11800).RCast<decltype(s_SetValue)>();
    s_ObjectToVariant = module.Offset(0x14110).RCast<decltype(s_ObjectToVariant)>();
    s_GetQuiet = module.Offset(0x7D90).RCast<decltype(s_GetQuiet)>();
    s_Pop = module.Offset(0x35420).RCast<decltype(s_Pop)>();
    s_TableGet = module.Offset(0x6A670).RCast<decltype(s_TableGet)>();
    s_TableRemove = module.Offset(0x69F10).RCast<decltype(s_TableRemove)>();
})
