#include "server/weaponx.h"

#include "engine/shared/weapon_mods.h"
#include "vscript/languages/squirrel_re/squirrel.h"

static int Script_SetWeaponInfoFileKeyField(HSQUIRRELVM sqvm)
{
    constexpr ScriptContext context = ScriptContext::SERVER;
    CWeaponX* pWeapon = nullptr;
    if (!g_pSquirrel[context]->getthisentity(sqvm, &pWeapon))
        return SQRESULT_ERROR;

    const char* pFieldName = g_pSquirrel[context]->getstring(sqvm, 1);
    if (!pWeapon || !pFieldName || !*pFieldName)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "SetWeaponInfoFileKeyField requires a valid weapon and non-empty key");
        return SQRESULT_ERROR;
    }

    bool valueSupported = false;
    const bool updated =
        g_ServerWeaponMods.SetField(&pWeapon->GetWpnData(), &pWeapon->m_modVars, pFieldName, sqvm->_stackOfCurrentFunction[2], valueSupported);
    if (!valueSupported)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "SetWeaponInfoFileKeyField only accepts int, float, bool, or vector values");
        return SQRESULT_ERROR;
    }

    g_pSquirrel[context]->pushbool(sqvm, updated);
    return SQRESULT_NOTNULL;
}

ON_DLL_LOAD("server.dll", WeaponScriptMethods, [](CModule module)
{
    auto* pScriptDesc = module.Offset(0x16014E0).RCast<ScriptClassDesc_t*>();
    auto& binding = pScriptDesc->m_NativeFunctionBindings[pScriptDesc->m_NativeFunctionBindings.AddToTail()];
    binding.Init("SetWeaponInfoFileKeyField", "Script_SetWeaponInfoFileKeyField",
                 "Sets a scalar or vector field on this server WeaponX instance at runtime.", "bool", "string key, var value", false,
                 &Script_SetWeaponInfoFileKeyField);
})
