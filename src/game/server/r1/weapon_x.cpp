#include "game/server/r1/weapon_x.h"
#include "game/shared/weapon_mod_data.h"
#include "vscript/languages/squirrel_re/squirrel.h"
#include "tier1/convar.h"

const char* (*CWeaponX__GetWeaponNameSlow)(unsigned int);
ConVar* Cvar_ns_enable_weapon_attack_callback;

const char* CWeaponX::GetWeaponName() const
{
    return m_weaponNameIndex >= 0 ? CWeaponX__GetWeaponNameSlow(static_cast<unsigned int>(m_weaponNameIndex)) : nullptr;
}

CWeaponXExtended CWeaponX::sm_WeaponsExtended[NUM_ENT_ENTRIES];

CWeaponXExtended::CWeaponXExtended()
{
    m_WeaponPrediction.Clear();
}

CWeaponXExtended* CWeaponX::GetWeaponExtended()
{
    const CBaseHandle handle = GetRefEHandle();
    const int index = handle.GetEntryIndex();
    if (index < 0 || index >= NUM_ENT_ENTRIES)
        return nullptr;
    CWeaponXExtended& extended = sm_WeaponsExtended[index];
    if (extended.m_WeaponPrediction.weaponHandle != 0xFFFFFFFFu && extended.m_WeaponPrediction.weaponHandle != handle.ToInt())
        extended.Forget();
    return &extended;
}

CWeaponXExtended* CWeaponX::FindWeaponExtended(const std::uint32_t handle)
{
    const unsigned int index = handle & 0xFFFFu;
    if (index >= NUM_ENT_ENTRIES)
        return nullptr;
    CWeaponXExtended& extended = sm_WeaponsExtended[index];
    return extended.m_WeaponPrediction.weaponHandle == handle ? &extended : nullptr;
}
DECLARE_MODULE(ServerWeaponHooks)
DECLARE_HOOK(CWeaponX::PrimaryAttack, server.dll + 0x6A0220, [](auto& hook, CWeaponX* weapon, int attackIndex) -> bool
{
    if (!Cvar_ns_enable_weapon_attack_callback->GetBool() || !weapon)
        return hook.Original(weapon, attackIndex);

    CBaseCombatCharacter* player = weapon->GetWeaponOwner();
    const char* weaponName = player ? weapon->GetWeaponName() : nullptr;
    if (!weaponName || !*weaponName)
        return hook.Original(weapon, attackIndex);

    bool ret = hook.Original(weapon, attackIndex);
    if (!ret)
        return ret;

    auto* sq = g_pSquirrel[ScriptContext::SERVER];
    if (!sq->m_pSQVM || !sq->m_pSQVM->sqvm)
        return ret;

    weaponName = weapon->GetWeaponName();
    auto playerInstance = sq->createscriptinstance(player);
    auto weaponInstance = sq->createscriptinstance(weapon);

    if (playerInstance && weaponInstance && weaponName)
        sq->Call("CodeCallback_OnWeaponAttack", playerInstance, weaponInstance, weaponName, 1);

    return ret;
})
DECLARE_HOOK(CWeaponX_Destructor, server.dll + 0x6813E0, [](auto& hook, CWeaponX* pWeapon) -> std::uintptr_t
{
    if (CWeaponXExtended* const extended = pWeapon->GetWeaponExtended())
        extended->Forget();
    g_ServerWeaponMods.ClearFieldOverrides(&pWeapon->m_modVars);
    return hook.Original(pWeapon);
});

ON_DLL_LOAD_RELIESON("server.dll", ServerWeaponModHooks, (WeaponMods_Server, ConVar), [](CModule)
{
    Cvar_ns_enable_weapon_attack_callback =
        new ConVar("ns_enable_weapon_attack_callback", "0", FCVAR_GAMEDLL, "Enables script weapon attack callback.");
    DISPATCH_MODULE(ServerWeaponHooks)
});

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
    CWeaponX__GetWeaponNameSlow = module.Offset(0x691300).RCast<decltype(CWeaponX__GetWeaponNameSlow)>();
    auto* pScriptDesc = module.Offset(0x16014E0).RCast<ScriptClassDesc_t*>();
    auto& binding = pScriptDesc->m_NativeFunctionBindings[pScriptDesc->m_NativeFunctionBindings.AddToTail()];
    binding.Init("SetWeaponInfoFileKeyField", "Script_SetWeaponInfoFileKeyField",
                 "Sets a scalar or vector field on this server WeaponX instance at runtime.", "bool", "string key, var value", false,
                 &Script_SetWeaponInfoFileKeyField);
    RegisterWeaponPredictionScriptMethods(ScriptContext::SERVER, pScriptDesc);
})
