#include "game/server/basecombatcharacter.h"

CWeaponX* (*CBaseCombatCharacter__GetActiveWeapon)(CBaseCombatCharacter*);
bool (*CBaseCombatCharacter__IsWeaponHolstered)(CBaseCombatCharacter*);
void (*CBaseCombatCharacter__HolsterWeapon)(CBaseCombatCharacter*);

CWeaponX* CBaseCombatCharacter::GetActiveWeapon()
{
    return CBaseCombatCharacter__GetActiveWeapon(this);
}

bool CBaseCombatCharacter::IsWeaponHolstered()
{
    return CBaseCombatCharacter__IsWeaponHolstered(this);
}

void CBaseCombatCharacter::HolsterWeapon()
{
    CBaseCombatCharacter__HolsterWeapon(this);
}

ON_DLL_LOAD("server.dll", CBaseCombatCharacter, [](CModule module)
{
    CBaseCombatCharacter__GetActiveWeapon = module.Offset(0xE38D0).RCast<decltype(CBaseCombatCharacter__GetActiveWeapon)>();
    CBaseCombatCharacter__IsWeaponHolstered = module.Offset(0xEA760).RCast<decltype(CBaseCombatCharacter__IsWeaponHolstered)>();
    CBaseCombatCharacter__HolsterWeapon = module.Offset(0xE9AD0).RCast<decltype(CBaseCombatCharacter__HolsterWeapon)>();
})
