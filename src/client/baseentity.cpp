#include "client/baseentity.h"
#include "tier0/callbacks.h"

using CBaseEntityValidateScriptScopeFn = bool (*)(C_BaseEntity*);
CBaseEntityValidateScriptScopeFn CBaseEntity__ValidateScriptScope;

HSCRIPT C_BaseEntity::GetScriptInstance()
{
    if (!CBaseEntity__ValidateScriptScope(this))
        return INVALID_HSCRIPT;

    return m_ScriptScope.m_hScope;
}

ON_DLL_LOAD_CLIENT("client.dll", ClientBaseEntityMethods,
                   [](CModule module) { CBaseEntity__ValidateScriptScope = module.Offset(0xC2FB0).RCast<CBaseEntityValidateScriptScopeFn>(); })
