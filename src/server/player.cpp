#include "player.h"

CPlayer*(__fastcall* UTIL_PlayerByIndex)(int playerIndex);

DECLARE_MODULE(PlayerHooks)

const char* (*GetWeaponName)(int index);
void* (*GetWeaponOwner)(uint64_t weapon_entity);
DECLARE_HOOK(PrimaryAttack, server.dll + 0x6A0220, [](auto& hook, __int64 a1, int a2) -> bool {
	bool ret =  hook.Original(a1,a2);

	if (g_pSquirrel[ScriptContext::SERVER]->m_pSQVM == nullptr || g_pSquirrel[ScriptContext::SERVER]->m_pSQVM->sqvm == nullptr)
		return ret;

	if(!a1)
	{
		spdlog::info("PrimaryAttack called with null weapon entity");
		return ret;
	}

	void* player = GetWeaponOwner(a1);

	if(!player)
	{
		spdlog::info("PrimaryAttack called with weapon entity that has no owner");
		return ret;
	}

	int weapon_name_index = *(_DWORD *)(a1 + 0x12D8);

	if(weapon_name_index < 0)
	{
		spdlog::info("PrimaryAttack called with invalid weapon name index: {}", weapon_name_index);
		return ret;
	}

	auto weapon_name = GetWeaponName(weapon_name_index);
	auto player_inst = g_pSquirrel[ScriptContext::SERVER]->__sq_createscriptinstance((void*)player);
	auto weapon_inst = g_pSquirrel[ScriptContext::SERVER]->__sq_createscriptinstance((void*)a1);

	if(player_inst && weapon_inst && weapon_name)
		g_pSquirrel[ScriptContext::SERVER]->AsyncCall("CodeCallback_OnWeaponAttack", player_inst, weapon_inst ,weapon_name, 1);

	return ret;
})

ON_DLL_LOAD("server.dll", CPlayer, [](CModule module)
{
	DISPATCH_MODULE(PlayerHooks);
	GetWeaponName = module.Offset(0x691300).RCast<const char* (*)(int)>();
	GetWeaponOwner = module.Offset(0xA6A20).RCast<void* (*)(uint64_t)>();
	UTIL_PlayerByIndex = module.Offset(0x26AA10).RCast<CPlayer*(__fastcall*)(int)>();
})

