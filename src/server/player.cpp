#include "player.h"

CPlayer*(__fastcall* UTIL_PlayerByIndex)(int playerIndex);

DECLARE_MODULE(PlayerHooks)

const char* (*GetWeaponName)(int index);
void* (*GetWeaponOwner)(uint64_t weapon_entity);
DECLARE_HOOK(PrimaryAttack, server.dll + 0x6A0220, [](auto& hook, __int64 a1, int a2) -> bool {
	if(!a1) {
		spdlog::info("PrimaryAttack called with null weapon entity");
		return hook.Original(a1,a2);
	}
	void* player = GetWeaponOwner(a1);
	if(!player)
		return hook.Original(a1,a2);
	int weapon_name_index = *(_DWORD *)(a1 + 0x12D8);
	auto weapon_name = GetWeaponName(weapon_name_index);
	int shotsFired = 1;
	auto player_inst = g_pSquirrel[ScriptContext::SERVER]->__sq_createscriptinstance((void*)player);
	auto weapon_inst = g_pSquirrel[ScriptContext::SERVER]->__sq_createscriptinstance((void*)a1);
	g_pSquirrel[ScriptContext::SERVER]->AsyncCall("CodeCallback_OnWeaponAttack", player_inst, weapon_inst ,weapon_name, shotsFired);
	return hook.Original(a1,a2);
})

ON_DLL_LOAD("server.dll", CPlayer, [](CModule module)
{
	DISPATCH_MODULE(PlayerHooks);
	GetWeaponName = module.Offset(0x691300).RCast<const char* (*)(int)>();
	GetWeaponOwner = module.Offset(0xA6A20).RCast<void* (*)(uint64_t)>();
	UTIL_PlayerByIndex = module.Offset(0x26AA10).RCast<CPlayer*(__fastcall*)(int)>();
})

