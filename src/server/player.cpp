#include "player.h"

CPlayer*(__fastcall* UTIL_PlayerByIndex)(int playerIndex);

DECLARE_MODULE(PlayerHooks)

const char* (*GetWeaponName)(int index);
void* (*GetWeaponOwner)(uint64_t weapon_entity);

ConVar* Cvar_ns_enable_weapon_attack_callback;

static thread_local CPlayer* s_pObserverCycleInProgress;

DECLARE_HOOK(CPlayer::CycleObserverTarget, server.dll + 0x57BFD0, [](auto& hook, CPlayer* player, bool reverse) -> bool
{
	if (!player || s_pObserverCycleInProgress != player)
	{
		CPlayer* previousPlayer = s_pObserverCycleInProgress;
		s_pObserverCycleInProgress = player;
		const bool result = hook.Original(player, reverse);
		s_pObserverCycleInProgress = previousPlayer;
		return result;
	}

	spdlog::warn("Prevented recursive observer target cycling for player {}", player->m_nPlayerIndex);
	return true;
})

DECLARE_HOOK(PrimaryAttack, server.dll + 0x6A0220, [](auto& hook, void* weapon, int attackIndex) -> bool
{
	if(!Cvar_ns_enable_weapon_attack_callback->GetBool())
		return hook.Original(weapon, attackIndex);

	void* player = nullptr;
	std::string weaponName;
	int weapon_name_index = -1;

	if(!weapon)
	{
		spdlog::info("PrimaryAttack called with null weapon entity");
	}
	else
	{
		player = GetWeaponOwner(reinterpret_cast<uint64_t>(weapon));

		if(!player)
		{
			spdlog::info("PrimaryAttack called with weapon entity that has no owner");
		}
		else
		{
			auto weaponAddress = reinterpret_cast<uintptr_t>(weapon);
			weapon_name_index = *reinterpret_cast<int*>(weaponAddress + 0x12D8);

			if(weapon_name_index < 0)
			{
				spdlog::info("PrimaryAttack called with invalid weapon name index: {}", weapon_name_index);
			}
			else if (const char* weapon_name = GetWeaponName(weapon_name_index))
			{
				weaponName = weapon_name;
			}
		}
	}

	bool ret = hook.Original(weapon, attackIndex);
	if (!ret)
		return ret;

	auto* sq = g_pSquirrel[ScriptContext::SERVER];
	if (weaponName.empty() || !player || !weapon || sq->m_pSQVM == nullptr || sq->m_pSQVM->sqvm == nullptr)
		return ret;

	auto weapon_name = GetWeaponName(weapon_name_index);
	auto player_inst = g_pSquirrel[ScriptContext::SERVER]->__sq_createscriptinstance((void*)player);
	auto weapon_inst = g_pSquirrel[ScriptContext::SERVER]->__sq_createscriptinstance((void*)weapon);

	if(player_inst && weapon_inst && weapon_name)
		g_pSquirrel[ScriptContext::SERVER]->Call("CodeCallback_OnWeaponAttack", player_inst, weapon_inst ,weapon_name, 1);

	return ret;
})

ON_DLL_LOAD_RELIESON("server.dll", CPlayer, ConVar, [](CModule module)
{
	DISPATCH_MODULE(PlayerHooks);

	Cvar_ns_enable_weapon_attack_callback = new ConVar("ns_enable_weapon_attack_callback", "0", FCVAR_GAMEDLL, "Enables script weapon attack callback.");

	GetWeaponName = module.Offset(0x691300).RCast<const char* (*)(int)>();
	GetWeaponOwner = module.Offset(0xA6A20).RCast<void* (*)(uint64_t)>();
	UTIL_PlayerByIndex = module.Offset(0x26AA10).RCast<CPlayer*(__fastcall*)(int)>();
})

