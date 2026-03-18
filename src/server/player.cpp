#include "player.h"

CPlayer*(__fastcall* UTIL_PlayerByIndex)(int playerIndex);

DECLARE_MODULE(PlayerHooks)

uint64_t* GlobalEntList = nullptr;

const char* (*GetWeaponName)(int index);
DECLARE_HOOK(PrimaryAttack, server.dll + 0x69F7C0, [](auto& hook, __int64 a1) -> void {
	int owner_index = *(_DWORD *)(a1 + 0xEB8);
	void* player = (void*)GlobalEntList[6 * (unsigned __int16)owner_index + 1];
	int weapon_index = *(_DWORD *)(a1 + 0x12D8);
	auto weapon_name = GetWeaponName(weapon_index);
	int shotsFired = 1;
	auto player_inst = g_pSquirrel[ScriptContext::SERVER]->__sq_createscriptinstance((void*)player);
	auto weapon_inst = g_pSquirrel[ScriptContext::SERVER]->__sq_createscriptinstance((void*)a1);
	auto ret = g_pSquirrel[ScriptContext::SERVER]->Call("CodeCallback_OnWeaponAttack", player_inst, weapon_inst ,weapon_name, shotsFired);
	return hook.Original(a1);
})

ON_DLL_LOAD("server.dll", CPlayer, [](CModule module)
{
	DISPATCH_MODULE(PlayerHooks);
	GetWeaponName = module.Offset(0x691300).RCast<const char* (*)(int)>();
	GlobalEntList = *module.Offset(0xB6AB58).RCast<uint64_t**>();
	UTIL_PlayerByIndex = module.Offset(0x26AA10).RCast<CPlayer*(__fastcall*)(int)>();
})

