//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef SCRIPT_SCOPE_H
#define SCRIPT_SCOPE_H
#ifdef _WIN32
#pragma once
#endif

#include "vscript/ivscript.h"
#include "vscript/languages/squirrel_re/vsquirrel.h"

#include <array>

template <ScriptContext context> class CScriptScopeT
{
  public:
    enum Flags_t
    {
        EXTERNAL = 0x01
    };

    CScriptScopeT() : m_hScope(INVALID_HSCRIPT), m_flags(0)
    {
    }
    ~CScriptScopeT()
    {
        Term();
    }
    CScriptScopeT(const CScriptScopeT&) = delete;
    CScriptScopeT& operator=(const CScriptScopeT&) = delete;

    static constexpr ScriptContext GetContext()
    {
        return context;
    }
    static CSquirrelVM* GetVM();
    bool IsInitialized() const
    {
        return m_hScope != INVALID_HSCRIPT;
    }
    bool Init(const char* pszName);
    bool Init(HSCRIPT hScope, bool bExternal = true);
    bool InitGlobal();
    void Term();
    void InvalidateCachedValues();
    operator HSCRIPT() const
    {
        return IsInitialized() ? m_hScope : nullptr;
    }

    bool ValueExists(const char* pszKey) const;
    bool SetValue(const char* pszKey, const ScriptVariant_t& value);
    bool GetValue(const char* pszKey, ScriptVariant_t* pValue) const;
    void ReleaseValue(ScriptVariant_t& value) const;
    bool ClearValue(const char* pszKey);

    ScriptStatus_t Run(HSCRIPT hScript);
    ScriptStatus_t Run(const char* pszScriptText, const char* pszScriptName = nullptr);
    ScriptStatus_t Run(const unsigned char* pszScriptText, const char* pszScriptName = nullptr)
    {
        return Run(reinterpret_cast<const char*>(pszScriptText), pszScriptName);
    }

    HSCRIPT LookupFunction(const char* pszFunction, const char* requiredType = nullptr) const;
    void ReleaseFunction(HSCRIPT hFunction) const;
    bool FunctionExists(const char* pszFunction) const;
    ScriptStatus_t ExecuteFunction(HSCRIPT hFunction, ScriptVariant_t* pArgs, int nArgs, ScriptVariant_t* pReturn = nullptr) const;

    template <typename... Args> ScriptStatus_t Call(HSCRIPT hFunction, ScriptVariant_t* pReturn = nullptr, const Args&... arguments) const
    {
        std::array<ScriptVariant_t, sizeof...(Args)> args{ScriptVariant_t(arguments)...};
        return ExecuteFunction(hFunction, args.data(), static_cast<int>(args.size()), pReturn);
    }

    template <typename... Args> ScriptStatus_t Call(const char* pszFunction, ScriptVariant_t* pReturn = nullptr, const Args&... arguments) const
    {
        HSCRIPT hFunction = LookupFunction(pszFunction);
        if (!hFunction)
            return SCRIPT_ERROR;
        const ScriptStatus_t result = Call(hFunction, pReturn, arguments...);
        ReleaseFunction(hFunction);
        return result;
    }

    HSCRIPT m_hScope;
    int m_flags;
    CUtlVectorConservative<HSCRIPT*> m_FuncHandles;
};

using CServerScriptScope = CScriptScopeT<ScriptContext::SERVER>;
using CClientScriptScope = CScriptScopeT<ScriptContext::CLIENT>;
using CUIScriptScope = CScriptScopeT<ScriptContext::UI>;

#define VScriptAddEnumToScope_(scope, enumVal, scriptName) (scope).SetValue(scriptName, static_cast<int>(enumVal))
#define VScriptAddEnumToScope(scope, enumVal) VScriptAddEnumToScope_(scope, enumVal, #enumVal)

static_assert(sizeof(CClientScriptScope) == 0x20);

#endif // SCRIPT_SCOPE_H
