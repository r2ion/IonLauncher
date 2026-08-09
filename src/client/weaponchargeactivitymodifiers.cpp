#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>

struct C_WeaponX;

namespace WeaponChargeActivityModifiers
{
constexpr int ACTIVITY_MODIFIER_CAPACITY = 32;
constexpr std::uint16_t INVALID_ACTIVITY_MODIFIER = 0xFFFF;
constexpr int PARTIAL_CHARGE_LEVEL_COUNT = 4;
constexpr bool LOG_CHARGE_ACTIVITY_MODIFIERS = false;

enum class ChargeActivityModifier : int
{
	Uncharged,
	Level1,
	Level2,
	Level3,
	Level4,
	FullyCharged,
};

struct LatchedChargeActivity
{
	ChargeActivityModifier modifier = ChargeActivityModifier::Uncharged;
	float fraction = 0.0f;
	int nativeLevel = 0;
};

constexpr std::array<const char*, 6> CHARGE_MODIFIER_NAMES = {
	"",
	"charged_lv1",
	"charged_lv2",
	"charged_lv3",
	"charged_lv4",
	"fully_charged",
};

using InternActivityModifierFn = std::uint16_t*(__fastcall*)(std::uint16_t* output, const char* name);
using GetWeaponChargeLevelFn = int(__fastcall*)(C_WeaponX* weapon);
using GetWeaponChargeFractionFn = float(__fastcall*)(C_WeaponX* weapon);

static InternActivityModifierFn s_InternActivityModifier;
static GetWeaponChargeLevelFn s_GetWeaponChargeLevel;
static GetWeaponChargeFractionFn s_GetWeaponChargeFraction;

static std::once_flag s_ChargeModifierInit;
static std::array<std::uint16_t, 6> s_ChargeModifierSymbols = {
	INVALID_ACTIVITY_MODIFIER,
	INVALID_ACTIVITY_MODIFIER,
	INVALID_ACTIVITY_MODIFIER,
	INVALID_ACTIVITY_MODIFIER,
	INVALID_ACTIVITY_MODIFIER,
	INVALID_ACTIVITY_MODIFIER,
};

static thread_local C_WeaponX* s_ChargedAttackWeapon;
static thread_local LatchedChargeActivity s_ChargedAttack;

static void InitializeChargeModifierSymbols()
{
	std::call_once(s_ChargeModifierInit, [] {
		for (int i = 1; i < static_cast<int>(CHARGE_MODIFIER_NAMES.size()); ++i)
			s_InternActivityModifier(&s_ChargeModifierSymbols[i], CHARGE_MODIFIER_NAMES[i]);
	});
}

static LatchedChargeActivity GetAttackChargeActivity(C_WeaponX* weapon)
{
	const float chargeFraction = std::clamp(s_GetWeaponChargeFraction(weapon), 0.0f, 1.0f);
	const int nativeLevel = s_GetWeaponChargeLevel(weapon);
	if (chargeFraction <= 0.0f)
		return { ChargeActivityModifier::Uncharged, chargeFraction, nativeLevel };

	if (chargeFraction >= 1.0f)
		return { ChargeActivityModifier::FullyCharged, chargeFraction, nativeLevel };

	// The native getter numbers the first partial band as zero. Shift it by
	// one so four native bands become charged_lv1 through charged_lv4.
	const int partialLevel = std::clamp(nativeLevel + 1, 1, PARTIAL_CHARGE_LEVEL_COUNT);
	return { static_cast<ChargeActivityModifier>(partialLevel), chargeFraction, nativeLevel };
}
}

DECLARE_MODULE(WeaponChargeActivityModifierHooks)


DECLARE_HOOK(C_WeaponX_PrimaryAttack_CaptureChargeLevel, client.dll + 0x5B48C0, [](auto& hook, C_WeaponX* weapon) -> char
{
	C_WeaponX* previousWeapon = WeaponChargeActivityModifiers::s_ChargedAttackWeapon;
	const WeaponChargeActivityModifiers::LatchedChargeActivity previousCharge =
		WeaponChargeActivityModifiers::s_ChargedAttack;

	WeaponChargeActivityModifiers::s_ChargedAttackWeapon = weapon;
	WeaponChargeActivityModifiers::s_ChargedAttack =
		WeaponChargeActivityModifiers::GetAttackChargeActivity(weapon);

	const char result = hook.Original(weapon);

	WeaponChargeActivityModifiers::s_ChargedAttackWeapon = previousWeapon;
	WeaponChargeActivityModifiers::s_ChargedAttack = previousCharge;
	return result;
})


DECLARE_HOOK(C_WeaponX_BuildActivityModifiers_ChargeLevel, client.dll + 0xBAE00,
	[](auto& hook, C_WeaponX* weapon, std::uint16_t* modifiers) -> int
{
	int modifierCount = hook.Original(weapon, modifiers);

	if (weapon != WeaponChargeActivityModifiers::s_ChargedAttackWeapon
		|| WeaponChargeActivityModifiers::s_ChargedAttack.modifier
			== WeaponChargeActivityModifiers::ChargeActivityModifier::Uncharged
		|| modifierCount >= WeaponChargeActivityModifiers::ACTIVITY_MODIFIER_CAPACITY)
		return modifierCount;

	WeaponChargeActivityModifiers::InitializeChargeModifierSymbols();

	const int chargeModifierIndex =
		static_cast<int>(WeaponChargeActivityModifiers::s_ChargedAttack.modifier);
	const std::uint16_t modifier =
		WeaponChargeActivityModifiers::s_ChargeModifierSymbols[chargeModifierIndex];
	if (modifier != WeaponChargeActivityModifiers::INVALID_ACTIVITY_MODIFIER)
	{
		modifiers[modifierCount++] = modifier;

		if constexpr (WeaponChargeActivityModifiers::LOG_CHARGE_ACTIVITY_MODIFIERS)
		{
			spdlog::info("[weapon activity] injected {} (fraction={:.3f}, nativeLevel={}, modifierCount={})",
				WeaponChargeActivityModifiers::CHARGE_MODIFIER_NAMES[chargeModifierIndex],
				WeaponChargeActivityModifiers::s_ChargedAttack.fraction,
				WeaponChargeActivityModifiers::s_ChargedAttack.nativeLevel,
				modifierCount);
		}
	}

	return modifierCount;
})

ON_DLL_LOAD_CLIENT("client.dll", WeaponChargeActivityModifierSetup, [](CModule module)
{
	WeaponChargeActivityModifiers::s_InternActivityModifier =
		module.Offset(0x32FBF0).RCast<WeaponChargeActivityModifiers::InternActivityModifierFn>();
	WeaponChargeActivityModifiers::s_GetWeaponChargeLevel =
		module.Offset(0x5A9540).RCast<WeaponChargeActivityModifiers::GetWeaponChargeLevelFn>();
	WeaponChargeActivityModifiers::s_GetWeaponChargeFraction =
		module.Offset(0x5A6D60).RCast<WeaponChargeActivityModifiers::GetWeaponChargeFractionFn>();

	DISPATCH_MODULE(WeaponChargeActivityModifierHooks)
})
