#include "server/weaponx.h"

#include "engine/shared/weapon_mods.h"
#include "vscript/languages/squirrel_re/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqstring.h"

static bool ScriptObjectToWeaponRuntimeFieldValue(const SQObject& object, ScriptVariant_t& value)
{
    switch (object._Type)
    {
    case OT_INTEGER:
        value = static_cast<int>(_integer(object));
        return true;
    case OT_FLOAT:
        value = static_cast<float>(_float(object));
        return true;
    case OT_BOOL:
        value = static_cast<bool>(_bool(object));
        return true;
    case OT_VECTOR:
    {
        const SQFloat* vector = _vector(object);
        value = Vector3D{vector[0], vector[1], vector[2]};
        return true;
    }
    default:
        return false;
    }
}

ADD_NAMED_SQFUNC_WITH_DEFAULTS("bool", "SetWeaponInfoFileKeyField", NSSetServerWeaponInfoFileKeyField, "entity weapon, string key, var value",
                               "Sets a scalar or vector field on one server WeaponX instance at runtime.", 0, nullptr, ScriptContext::SERVER)
{
    CWeaponX* pWeapon = g_pSquirrel[context]->template getentity<CWeaponX>(sqvm, 1);
    const char* pFieldName = g_pSquirrel[context]->getstring(sqvm, 2);
    if (!pWeapon || !pFieldName || !*pFieldName)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "SetWeaponInfoFileKeyField requires a valid weapon and non-empty key");
        return SQRESULT_ERROR;
    }

    ScriptVariant_t value;
    if (!ScriptObjectToWeaponRuntimeFieldValue(sqvm->_stackOfCurrentFunction[3], value))
    {
        g_pSquirrel[context]->raiseerror(sqvm, "SetWeaponInfoFileKeyField only accepts int, float, bool, or vector values");
        return SQRESULT_ERROR;
    }

    const bool updated = g_ServerWeaponMods.SetRuntimeField(pWeapon, &pWeapon->m_modVars, pFieldName, value);
    g_pSquirrel[context]->pushbool(sqvm, updated);
    return SQRESULT_NOTNULL;
}
