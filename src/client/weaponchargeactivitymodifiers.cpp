#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>

struct C_WeaponX;

namespace
{
constexpr int ACTIVITY_MODIFIER_CAPACITY = 32;
constexpr std::uint16_t INVALID_ACTIVITY_MODIFIER = 0xFFFF;
constexpr int PARTIAL_CHARGE_LEVEL_COUNT = 4;

enum class ChargeActivityModifier : int
{
	Uncharged,
	Level1,
	Level2,
	Level3,
	Level4,
	FullyCharged,
};

using InternActivityModifierFn = std::uint16_t*(__fastcall*)(std::uint16_t* output, const char* name);
using GetWeaponChargeLevelFn = int(__fastcall*)(C_WeaponX* weapon);
using GetWeaponChargeFractionFn = float(__fastcall*)(C_WeaponX* weapon);

InternActivityModifierFn s_InternActivityModifier;
GetWeaponChargeLevelFn s_GetWeaponChargeLevel;
GetWeaponChargeFractionFn s_GetWeaponChargeFraction;

std::once_flag s_ChargeModifierInit;
std::array<std::uint16_t, 6> s_ChargeModifierSymbols = {
	INVALID_ACTIVITY_MODIFIER,
	INVALID_ACTIVITY_MODIFIER,
	INVALID_ACTIVITY_MODIFIER,
	INVALID_ACTIVITY_MODIFIER,
	INVALID_ACTIVITY_MODIFIER,
	INVALID_ACTIVITY_MODIFIER,
};

thread_local C_WeaponX* s_ChargedAttackWeapon;
thread_local ChargeActivityModifier s_ChargedAttackModifier;

void InitializeChargeModifierSymbols()
{
	std::call_once(s_ChargeModifierInit, [] {
		s_InternActivityModifier(&s_ChargeModifierSymbols[1], "charged_lv1");
		s_InternActivityModifier(&s_ChargeModifierSymbols[2], "charged_lv2");
		s_InternActivityModifier(&s_ChargeModifierSymbols[3], "charged_lv3");
		s_InternActivityModifier(&s_ChargeModifierSymbols[4], "charged_lv4");
		s_InternActivityModifier(&s_ChargeModifierSymbols[5], "fully_charged");
	});
}

ChargeActivityModifier GetAttackChargeModifier(C_WeaponX* weapon)
{
	const float chargeFraction = std::clamp(s_GetWeaponChargeFraction(weapon), 0.0f, 1.0f);
	if (chargeFraction <= 0.0f)
		return ChargeActivityModifier::Uncharged;

	if (chargeFraction >= 1.0f)
		return ChargeActivityModifier::FullyCharged;

	// The native getter numbers the first partial band as zero. Shift it by
	// one so four native bands become charged_lv1 through charged_lv4.
	const int partialLevel = std::clamp(s_GetWeaponChargeLevel(weapon) + 1, 1, PARTIAL_CHARGE_LEVEL_COUNT);
	return static_cast<ChargeActivityModifier>(partialLevel);
}
}

DECLARE_MODULE(WeaponChargeActivityModifierHooks)


DECLARE_HOOK(C_WeaponX_PrimaryAttack_CaptureChargeLevel, client.dll + 0x5B48C0, [](auto& hook, C_WeaponX* weapon) -> char
{
	C_WeaponX* previousWeapon = s_ChargedAttackWeapon;
	const ChargeActivityModifier previousModifier = s_ChargedAttackModifier;

	s_ChargedAttackWeapon = weapon;
	s_ChargedAttackModifier = GetAttackChargeModifier(weapon);

	const char result = hook.Original(weapon);

	s_ChargedAttackWeapon = previousWeapon;
	s_ChargedAttackModifier = previousModifier;
	return result;
})


DECLARE_HOOK(C_WeaponX_BuildActivityModifiers_ChargeLevel, client.dll + 0xBAE00,
	[](auto& hook, C_WeaponX* weapon, std::uint16_t* modifiers) -> int
{
	int modifierCount = hook.Original(weapon, modifiers);

	if (weapon != s_ChargedAttackWeapon || s_ChargedAttackModifier == ChargeActivityModifier::Uncharged
		|| modifierCount >= ACTIVITY_MODIFIER_CAPACITY)
		return modifierCount;

	InitializeChargeModifierSymbols();

	const std::uint16_t modifier = s_ChargeModifierSymbols[static_cast<int>(s_ChargedAttackModifier)];
	if (modifier != INVALID_ACTIVITY_MODIFIER)
		modifiers[modifierCount++] = modifier;

	return modifierCount;
})

ON_DLL_LOAD_CLIENT("client.dll", WeaponChargeActivityModifierSetup, [](CModule module)
{
	s_InternActivityModifier = module.Offset(0x32FBF0).RCast<InternActivityModifierFn>();
	s_GetWeaponChargeLevel = module.Offset(0x5A9540).RCast<GetWeaponChargeLevelFn>();
	s_GetWeaponChargeFraction = module.Offset(0x5A6D60).RCast<GetWeaponChargeFractionFn>();

	DISPATCH_MODULE(WeaponChargeActivityModifierHooks)
})
