#include "rtech/rui/rui_callback_types.h"

#include <algorithm>
#include <cmath>
#include <cstring>

DECLARE_MODULE(RuiHooks)

namespace
{
constexpr ptrdiff_t PILOT_SPEEDOMETER_VISIBILITY_BRANCH = 0x6AB7F;
constexpr ptrdiff_t GAUNTLET_MAX_SPEED_MPH = 0xD1AD8;
constexpr ptrdiff_t GAUNTLET_MAX_SPEED_KPH = 0xD1B1C;

ConVar* Cvar_ion_use_custom_crosshair;
ConVar* Cvar_ion_crosshair_gap_v;
ConVar* Cvar_ion_crosshair_length_v;
ConVar* Cvar_ion_crosshair_inset_v;
ConVar* Cvar_ion_crosshair_thickness_l;
ConVar* Cvar_ion_crosshair_thickness_r;
ConVar* Cvar_ion_crosshair_gap_h;
ConVar* Cvar_ion_crosshair_length_h;
ConVar* Cvar_ion_chroma_gameinfo;
ConVar* Cvar_ion_speedometer_always_show;
ConVar* Cvar_gauntlet_timer_max_speed_metric;
ConVar* Cvar_gauntlet_timer_max_speed_imperial;

float* s_GauntletMaxSpeedMph;
float* s_GauntletMaxSpeedKph;
bool s_RuiUiModuleReady;
bool s_RuiConVarsReady;
bool s_RuiUiHooksDispatched;

__m128 MakeColor(float red, float green, float blue, float alpha = 1.0f)
{
	return _mm_setr_ps(red, green, blue, alpha);
}

__m128 FriendlyTeamColor()
{
	return MakeColor(0.095f, 0.309f, 0.708f);
}

__m128 EnemyTeamColor()
{
	return MakeColor(1.0f, 0.188f, 0.014f);
}

__m128 RainbowColor(float time, float speed, float hueOffset)
{
	float hue = std::fmod(time * speed + hueOffset, 1.0f);
	if (hue < 0.0f)
		hue += 1.0f;

	const float sector = hue * 6.0f;
	const int sectorIndex = static_cast<int>(sector);
	const float rising = sector - static_cast<float>(sectorIndex);
	const float falling = 1.0f - rising;

	switch (sectorIndex % 6)
	{
	case 0: return MakeColor(1.0f, rising, 0.0f);
	case 1: return MakeColor(falling, 1.0f, 0.0f);
	case 2: return MakeColor(0.0f, 1.0f, rising);
	case 3: return MakeColor(0.0f, falling, 1.0f);
	case 4: return MakeColor(rising, 0.0f, 1.0f);
	default: return MakeColor(1.0f, 0.0f, falling);
	}
}

void StoreColor(float (&destination)[4], __m128 color)
{
	_mm_storeu_ps(destination, color);
}

void StoreRgb(float (&destination)[3], __m128 color)
{
	destination[0] = color.m128_f32[0];
	destination[1] = color.m128_f32[1];
	destination[2] = color.m128_f32[2];
}

float FractionalPart(float value)
{
	return value - std::floor(value);
}

float LowAmmoPulse(float currentTime)
{
	const float phase = FractionalPart(currentTime * 2.0f);
	const float risingEdge = std::max(0.0f, 1.0f - (1.0f - phase) * 4.0f);
	return 1.0f - std::min(2.0f - risingEdge * 2.0f, 1.0f);
}

float NoiseEnvelope(float currentTime)
{
	const float phase = FractionalPart(currentTime * 200.0f);
	return std::max(0.0f, 1.0f - (1.0f - phase) * 0.2f);
}

struct AmmoUrgency
{
	float scale;
	float pulse;
	__m128 color;
};

AmmoUrgency ComputeAmmoUrgency(float ammoFraction, float currentTime)
{
	if (ammoFraction <= 0.33f)
		return {4.0f, LowAmmoPulse(currentTime), MakeColor(1.0f, 0.065f, 0.051f)};

	if (ammoFraction < 0.60f)
		return {3.0f, 0.0f, MakeColor(1.0f, 0.216f, 0.051f)};

	return {1.0f, 0.0f, MakeColor(0.242f, 0.831f, 1.0f)};
}

void UpdateGamestateInfoColors(RuiGlobalState* globals, float (&left)[3], float (&right)[3])
{
	if (Cvar_ion_chroma_gameinfo->GetBool())
	{
		StoreRgb(left, RainbowColor(globals->currentTime, 0.5f, 0.0f));
		StoreRgb(right, RainbowColor(globals->currentTime, 0.5f, 0.5f));
		return;
	}

	StoreRgb(left, FriendlyTeamColor());
	StoreRgb(right, EnemyTeamColor());
}

void UpdateGamestateInfoFfa(
	const RuiFunctionTable_t* api,
	RuiGlobalState* globals,
	RuiInstance* rui,
	RuiGamestateInfoFfaInstanceData* data)
{
	const float timeRemaining = data->endTime - globals->currentTime;
	if (timeRemaining < 0.0f || data->endTime == -1.0e30f)
		data->formattedTime = "--:--";
	else if (timeRemaining > 30.0f)
		data->formattedTime = api->format(
			rui, "%i:%02i", static_cast<unsigned int>(timeRemaining) / 60,
			static_cast<unsigned int>(timeRemaining) % 60);
	else
		data->formattedTime = api->format(rui, "%05.2f", timeRemaining);

	data->leftFillImage = api->findImageAsset(rui, "rui/hud/gamestate/score_fill_right");
	data->rightFillImage = api->findImageAsset(rui, "rui/hud/gamestate/score_fill_right");

	const float maxTeamScore = static_cast<float>(data->maxTeamScore);
	if (maxTeamScore == 0.0f)
	{
		api->setErrorWithReason(
			rui,
			data->leftTeamScore < data->rightTeamScore
				? "content\\r2\\ui\\hud\\gamemode_ffa.rui (96,47): divide by zero.\n"
				: "content\\r2\\ui\\hud\\gamemode_ffa.rui (83,46): divide by zero.\n");
		return;
	}

	__m128 topColor;
	__m128 bottomColor;
	float selectedScore;
	float otherScore;
	if (data->leftTeamScore < data->rightTeamScore)
	{
		data->selectedPlayerCardImage = api->findImageAsset(rui, data->enemyPlayerCardImage);
		data->otherPlayerCardImage = api->findImageAsset(rui, data->friendlyPlayerCardImage);
		data->selectedScoreFraction = data->rightTeamScore / maxTeamScore;
		data->otherScoreFraction = data->leftTeamScore / maxTeamScore;
		topColor = EnemyTeamColor();
		bottomColor = FriendlyTeamColor();
		selectedScore = data->rightTeamScore;
		otherScore = data->leftTeamScore;
	}
	else
	{
		data->selectedPlayerCardImage = api->findImageAsset(rui, data->friendlyPlayerCardImage);
		data->otherPlayerCardImage = api->findImageAsset(rui, data->enemyPlayerCardImage);
		data->selectedScoreFraction = data->leftTeamScore / maxTeamScore;
		data->otherScoreFraction = data->rightTeamScore / maxTeamScore;
		topColor = FriendlyTeamColor();
		bottomColor = EnemyTeamColor();
		selectedScore = data->leftTeamScore;
		otherScore = data->rightTeamScore;
	}

	if (Cvar_ion_chroma_gameinfo->GetBool())
	{
		topColor = RainbowColor(globals->currentTime, 0.5f, 0.0f);
		bottomColor = RainbowColor(globals->currentTime, 0.5f, 0.5f);
	}

	StoreColor(data->selectedTopColor, topColor);
	StoreColor(data->selectedBottomColor, bottomColor);
	StoreColor(data->topColor, topColor);
	StoreColor(data->bottomColor, bottomColor);
	data->selectedScoreText = api->format(rui, "%.8g", selectedScore);
	data->otherScoreText = api->format(rui, "%.8g", otherScore);
	data->whiteImage = api->findImageAsset(rui, "white");

	const char* localizationKey = data->statusText && data->statusText[0]
		? data->statusText
		: "#PL_ffa";
	data->localizedGameMode = api->localize(rui, localizationKey, 0, 0, 0, 0, 0);
	data->factionImageAsset = api->findImageAsset(rui, data->factionImage);

	__m128* transformSizes = api->getTransformSizes(rui);
	transformSizes[3] = _mm_setr_ps(200.0f, 200.0f, 40.0f, 40.0f);
	api->executeTransform(rui, 1);
	transformSizes[4] = _mm_setr_ps(200.0f, 200.0f, 40.0f, 40.0f);
	api->executeTransform(rui, 2);
	transformSizes[5] = _mm_setr_ps(1920.0f, 1920.0f, 1080.0f, 1080.0f);
	transformSizes[6] = _mm_setr_ps(2.0f, 2.0f, 84.0f, 84.0f);
	transformSizes[7] = _mm_setr_ps(1920.0f, 1920.0f, 1080.0f, 1080.0f);
	transformSizes[8] = _mm_setr_ps(128.0f, 128.0f, 40.0f, 40.0f);
	transformSizes[9] = _mm_setr_ps(25.866f, 25.866f, 40.0f, 40.0f);
	transformSizes[10] = _mm_setr_ps(25.866f, 25.866f, 40.0f, 40.0f);
	transformSizes[11] = api->normalizeTransformRange(rui, 3, 4);
	transformSizes[12] = api->normalizeTransformRange(rui, 4, 5);
	transformSizes[13] = api->measureTextJob(rui, 42);
	transformSizes[14] = api->measureTextJob(rui, 60);
	transformSizes[15] = api->measureTextJob(rui, 78);
	transformSizes[16] = api->measureTextJob(rui, 348);
	transformSizes[17] = api->measureTextJob(rui, 366);
	transformSizes[18] = _mm_set1_ps(48.0f);
	api->executeTransform(rui, 158);
}

RuiFloat2Value AddMovement(float movementX, float movementY, float x, float y)
{
	return {movementX + x, movementY + y};
}

void UpdateCrosshairPlus(
	const RuiFunctionTable_t* api,
	RuiGlobalState* globals,
	RuiInstance* rui,
	RuiCrosshairPlusInstanceData* data)
{
	if (globals->isMenuOpen)
	{
		api->setNoRender(rui);
		return;
	}

	const float spreadScaled = data->adjustedSpread * 540.0f;
	const float spreadY = spreadScaled / 1080.0f;
	const float spreadX = spreadScaled / 1920.0f;
	const float actionFade = data->isSprinting || data->isReloading ? 0.55000001f : 0.0f;
	const float alpha = (0.75f - data->adsFraction) - actionFade;

	__m128 color;
	if (data->isGrappleInRange)
		color = MakeColor(0.305f, 0.956f, 0.578f);
	else if (data->isAmped)
		color = MakeColor(0.965f, 0.525f, 0.157f, alpha);
	else
		color = MakeColor(data->teamColor[0], data->teamColor[1], data->teamColor[2], alpha);
	StoreColor(data->color, color);
	data->whiteImage = api->findImageAsset(rui, "white");

    __m128 movX = _mm_set_ss(data->movementX);
    __m128 movY = _mm_set_ss(data->movementY);
    __m128 movXY = _mm_unpacklo_ps(movX, movY);

    // Packs (movX + offX, movY + offY) into a uint64 for qword fields
    // offX and offY are passed as __m128 scalars (raw hex bits preserved)
    auto PackXY = [&](uint32_t rawOffX, uint32_t rawOffY) -> uint64_t
    {
        __m128 offX, offY;
        offX.m128_u32[0] = rawOffX;
        offY.m128_u32[0] = rawOffY;
        return _mm_add_ps(movXY, _mm_unpacklo_ps(offX, offY)).m128_u64[0];
    };

    // Encode spread-adjusted Y offsets as raw bits for PackXY
    auto FloatBits = [](float f) -> uint32_t
    {
        uint32_t bits;
        memcpy(&bits, &f, sizeof(bits));
        return bits;
    };

    float kGapV = Cvar_ion_crosshair_gap_v->GetFloat();
    float kLengthV = Cvar_ion_crosshair_length_v->GetFloat();
    float kInsetV = Cvar_ion_crosshair_inset_v->GetFloat();
    float kThicknessVL = Cvar_ion_crosshair_thickness_l->GetFloat();
    float kThicknessVR = Cvar_ion_crosshair_thickness_r->GetFloat();
    float kGapH = Cvar_ion_crosshair_gap_h->GetFloat();
    float kLengthH = Cvar_ion_crosshair_length_h->GetFloat();
    float kInsetH = kLengthH * 0.15f;

    const float Vx_left = 0.5f - kThicknessVL;
    const float Vx_right = 0.5f + kThicknessVR;
    const float Vx_left2 = 0.5f - (kThicknessVL * 0.5f);
    const float Vx_right2 = 0.5f + (kThicknessVR * 0.5f);

    const float Vtop_near = 0.5f - kGapV;
    const float Vtop_far = Vtop_near - kLengthV;
    const float Vbot_near = 0.5f + kGapV;
    const float Vbot_far = Vbot_near + kLengthV;

    const float Hleft_far = 0.5f - kGapH - kLengthH;
    const float Hleft_near = 0.5f - kGapH;
    const float Hright_near = 0.5f + kGapH;
    const float Hright_far = 0.5f + kGapH + kLengthH;

    // Vertical top
    data->qword38 = PackXY(0x3EFEEEEFu, FloatBits(Vtop_far - spreadY));
    data->qword40 = PackXY(0x3F008889u, FloatBits(Vtop_near - spreadY));
    data->qword48 = PackXY(0x3EFF7777u, FloatBits(Vtop_far + kInsetV - spreadY));
    data->qword50 = PackXY(0x3F004444u, FloatBits(Vtop_near - kInsetV - spreadY));

    // Vertical bottom
    data->qword58 = PackXY(0x3EFEEEEFu, FloatBits(Vbot_near + spreadY));
    data->qword60 = PackXY(0x3F008889u, FloatBits(Vbot_far + spreadY));
    data->qword68 = PackXY(0x3EFF7777u, FloatBits(Vbot_near + kInsetV + spreadY));
    data->qword70 = PackXY(0x3F004444u, FloatBits(Vbot_far - kInsetV + spreadY));

    // Horizontal left
    data->qword78 = PackXY(FloatBits(Hleft_far - spreadX), 0x3EFE1A8Cu);
    data->qword80 = PackXY(FloatBits(Hleft_near - spreadX), 0x3F00F2BAu);
    data->qword88 = PackXY(FloatBits(Hleft_far + kInsetH - spreadX), 0x3EFF0D46u);
    data->qword90 = PackXY(FloatBits(Hleft_near - kInsetH - spreadX), 0x3F00795Du);

    // Horizontal right
    data->qword98 = PackXY(FloatBits(Hright_near + spreadX), 0x3EFE1A8Cu);
    data->qwordA0 = PackXY(FloatBits(Hright_far + spreadX), 0x3F00F2BAu);
    data->qwordA8 = PackXY(FloatBits(Hright_near + kInsetH + spreadX), 0x3EFF0D46u);
    data->qwordB0 = PackXY(FloatBits(Hright_far - kInsetH + spreadX), 0x3F00795Du);

	api->executeTransform(rui, 162);
}

void UpdateKraberAmmoCounter(
	const RuiFunctionTable_t* api,
	RuiGlobalState* globals,
	RuiInstance* rui,
	RuiKraberAmmoCounterInstanceData* data)
{
	data->ammoFraction = static_cast<float>(data->ammo) / std::max(static_cast<float>(data->clipSize), 1.0f);
	const AmmoUrgency urgency = ComputeAmmoUrgency(data->ammoFraction, globals->currentTime);
	data->urgencyScale = urgency.scale;
	StoreColor(data->urgencyColor, urgency.color);
	data->clipEnd = static_cast<float>(data->clipSize) - 0.015625f;
	data->scrollPosition = globals->currentTime * 10.0f;
	data->nextScrollPosition = data->scrollPosition + 1.0f;
	data->emptyBulletImage = api->findImageAsset(rui, "models/weapons/attachments/50cal_bullet_C_Black");
	data->loadedBulletImage = api->findImageAsset(rui, "models/weapons/attachments/50cal_bullet_C");
	data->urgencyAlpha = data->urgencyScale * 0.25f;
	data->bleedImage = api->findImageAsset(rui, "models/weapons/attachments/hemlok_panel_bleed");

	const __m128 pulsedColor = _mm_mul_ps(
		_mm_mul_ps(_mm_set1_ps(NoiseEnvelope(globals->currentTime)), urgency.color),
		_mm_set1_ps(api->randomFloat(rui) * 0.15000001f));
	StoreColor(data->pulsedColor, pulsedColor);
	data->whiteMultiplyImage = api->findImageAsset(rui, "models/weapons/attachments/whiteMult");
	data->pulsedAlpha = urgency.pulse * 0.5f * data->urgencyScale * data->pulseAmplitude;
	api->executeTransform(rui, 82);
}

void UpdateMastiffAmmoCounter(
	const RuiFunctionTable_t* api,
	RuiGlobalState* globals,
	RuiInstance* rui,
	RuiMastiffAmmoCounterInstanceData* data)
{
	data->ammoFraction = static_cast<float>(data->ammo) / std::max(static_cast<float>(data->clipSize), 1.0f);
	data->randomValue = api->randomFloat(rui);
	const AmmoUrgency urgency = ComputeAmmoUrgency(data->ammoFraction, globals->currentTime);
	data->urgencyScale = urgency.scale;
	StoreColor(data->urgencyColor, urgency.color);
	data->clipEnd = static_cast<float>(data->clipSize) - 0.015625f;
	data->scrollPosition = globals->currentTime * 10.0f;
	data->nextScrollPosition = data->scrollPosition + 1.0f;
	data->shellImage = api->findImageAsset(rui, "models/weapons/attachments/Shotgun_Shell_C");
	data->noiseImage = api->findImageAsset(rui, "rui/noise_uniform");
	data->urgencyAlpha = data->urgencyScale * 0.25f;
	data->bleedImage = api->findImageAsset(rui, "models/weapons/attachments/hemlok_panel_bleed");

	const __m128 pulsedColor = _mm_mul_ps(
		_mm_mul_ps(_mm_set1_ps(NoiseEnvelope(globals->currentTime)), urgency.color),
		_mm_set1_ps(api->randomFloat(rui) * 0.15000001f));
	StoreColor(data->pulsedColor, pulsedColor);
	data->whiteMultiplyImage = api->findImageAsset(rui, "models/weapons/attachments/whiteMult");
	data->pulsedAlpha = urgency.pulse * 0.5f * data->urgencyScale * data->pulseAmplitude;
	api->executeTransform(rui, 82);
}

template <typename T>
bool WriteProtectedValue(T* address, const T& value)
{
	DWORD oldProtection;
	if (!VirtualProtect(address, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtection))
		return false;

	std::memcpy(address, &value, sizeof(T));
	VirtualProtect(address, sizeof(T), oldProtection, &oldProtection);
	return true;
}

void UpdateGauntletSpeedLimits()
{
	static float appliedMaxSpeedMph = *s_GauntletMaxSpeedMph;
	static float appliedMaxSpeedKph = *s_GauntletMaxSpeedKph;

	const float requestedMaxSpeedMph = Cvar_gauntlet_timer_max_speed_imperial->GetFloat();
	if (requestedMaxSpeedMph != appliedMaxSpeedMph
		&& WriteProtectedValue(s_GauntletMaxSpeedMph, requestedMaxSpeedMph))
	{
		appliedMaxSpeedMph = requestedMaxSpeedMph;
	}

	const float requestedMaxSpeedKph = Cvar_gauntlet_timer_max_speed_metric->GetFloat();
	if (requestedMaxSpeedKph != appliedMaxSpeedKph
		&& WriteProtectedValue(s_GauntletMaxSpeedKph, requestedMaxSpeedKph))
	{
		appliedMaxSpeedKph = requestedMaxSpeedKph;
	}
}
}

DECLARE_HOOK(gamestate_info_ffa, ui(11).dll + 0x3E8E0, [](auto& hook,
	const RuiFunctionTable_t* api, RuiGlobalState* globals, RuiInstance* rui,
	RuiGamestateInfoFfaInstanceData* data)
{
	UpdateGamestateInfoFfa(api, globals, rui, data);
})

DECLARE_HOOK(gamestate_info, ui(11).dll + 0x33AE0, [](auto& hook,
	const RuiFunctionTable_t* api, RuiGlobalState* globals, RuiInstance* rui,
	RuiGamestateInfoInputPrefix* data)
{
	UpdateGamestateInfoColors(globals, data->leftColor, data->rightColor);
	hook.Original(api, globals, rui, data);
})

DECLARE_HOOK(gamestate_info_ps, ui(11).dll + 0x46280, [](auto& hook,
	const RuiFunctionTable_t* api, RuiGlobalState* globals, RuiInstance* rui,
	RuiGamestateInfoPsInstanceData* data)
{
	UpdateGamestateInfoColors(globals, data->leftColor, data->rightColor);
	hook.Original(api, globals, rui, data);
})

DECLARE_HOOK(crosshair_plus, ui(11).dll + 0x1D560, [](auto& hook,
	const RuiFunctionTable_t* api, RuiGlobalState* globals, RuiInstance* rui,
	RuiCrosshairPlusInstanceData* data)
{
	if (Cvar_ion_use_custom_crosshair->GetBool())
	{
		UpdateCrosshairPlus(api, globals, rui, data);
		return;
	}
	hook.Original(api, globals, rui, data);
})

DECLARE_HOOK(kraber_ammo_counter, ui(11).dll + 0x58BC0, [](auto& hook,
	const RuiFunctionTable_t* api, RuiGlobalState* globals, RuiInstance* rui,
	RuiKraberAmmoCounterInstanceData* data)
{
	UpdateKraberAmmoCounter(api, globals, rui, data);
})

DECLARE_HOOK(mastiff_ammo_counter, ui(11).dll + 0x5D710, [](auto& hook,
	const RuiFunctionTable_t* api, RuiGlobalState* globals, RuiInstance* rui,
	RuiMastiffAmmoCounterInstanceData* data)
{
	UpdateMastiffAmmoCounter(api, globals, rui, data);
})

DECLARE_HOOK(pilot_speedometer, ui(11).dll + 0x6AA50, [](auto& hook,
	const RuiFunctionTable_t* api, RuiGlobalState* globals, RuiInstance* rui,
	RuiPilotSpeedometerInstanceData* data)
{
	static CMemory module = GetModuleHandleA("ui(11).dll");
	static bool isAlwaysVisiblePatched = false;
	const bool shouldAlwaysShow = Cvar_ion_speedometer_always_show->GetBool();
	if (shouldAlwaysShow != isAlwaysVisiblePatched)
	{
		if (shouldAlwaysShow)
			module.Offset(PILOT_SPEEDOMETER_VISIBILITY_BRANCH).Patch({0x90, 0x90});
		else
			module.Offset(PILOT_SPEEDOMETER_VISIBILITY_BRANCH).Patch({0x72, 0x2C});
		isAlwaysVisiblePatched = shouldAlwaysShow;
	}

	hook.Original(api, globals, rui, data);
})

DECLARE_HOOK(gauntlet_hud, ui(11).dll + 0x4E030, [](auto& hook,
	const RuiFunctionTable_t* api, RuiGlobalState* globals, RuiInstance* rui,
	RuiGauntletHudInstanceData* data)
{
	UpdateGauntletSpeedLimits();
	hook.Original(api, globals, rui, data);
})

DECLARE_HOOK(RuiInstance_SetErrorWithReason, engine.dll + 0xF80D0, [](auto& hook, RuiInstance* rui, const char* reason)
{
	NS::log::RUI->info("{}", reason);
	hook.Original(rui, reason);
})

ON_DLL_LOAD("ui(11).dll", RuiUiCallbacks, [](CModule module)
{
	s_GauntletMaxSpeedMph = module.Offset(GAUNTLET_MAX_SPEED_MPH).RCast<float*>();
	s_GauntletMaxSpeedKph = module.Offset(GAUNTLET_MAX_SPEED_KPH).RCast<float*>();
	s_RuiUiModuleReady = true;
	if (s_RuiConVarsReady && !s_RuiUiHooksDispatched)
	{
		RuiHooks.DispatchForModule("ui(11).dll");
		s_RuiUiHooksDispatched = true;
	}
})

ON_DLL_LOAD_CLIENT("engine.dll", RuiEngineHooks, [](CModule module)
{
	(void)module;
	RuiHooks.DispatchForModule("engine.dll");
})

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", RuiConVars, ConVar, [](CModule module)
{
	(void)module;
	Cvar_ion_use_custom_crosshair = new ConVar(
		"ion_use_custom_crosshair", "0", FCVAR_ARCHIVE_PLAYERPROFILE,
		"Use custom crosshairs. 1 = enabled, 0 = disabled.");
	Cvar_ion_crosshair_gap_v = new ConVar(
		"ion_crosshair_gap_v", "0.0074074", FCVAR_ARCHIVE_PLAYERPROFILE,
		"Vertical arm gap from center.");
	Cvar_ion_crosshair_length_v = new ConVar(
		"ion_crosshair_length_v", "0.01200", FCVAR_ARCHIVE_PLAYERPROFILE,
		"Vertical arm segment length.");
	Cvar_ion_crosshair_inset_v = new ConVar(
		"ion_crosshair_inset_v", "0.00100", FCVAR_ARCHIVE_PLAYERPROFILE,
		"Vertical arm inner shadow inset.");
	Cvar_ion_crosshair_thickness_l = new ConVar(
		"ion_crosshair_thickness_l", "0.00185", FCVAR_ARCHIVE_PLAYERPROFILE,
		"Vertical arm thickness left of center.");
	Cvar_ion_crosshair_thickness_r = new ConVar(
		"ion_crosshair_thickness_r", "0.00833", FCVAR_ARCHIVE_PLAYERPROFILE,
		"Vertical arm thickness right of center.");
	Cvar_ion_crosshair_gap_h = new ConVar(
		"ion_crosshair_gap_h", "0.00417", FCVAR_ARCHIVE_PLAYERPROFILE,
		"Horizontal arm gap from center.");
	Cvar_ion_crosshair_length_h = new ConVar(
		"ion_crosshair_length_h", "0.00700", FCVAR_ARCHIVE_PLAYERPROFILE,
		"Horizontal arm segment length.");
	Cvar_ion_chroma_gameinfo = new ConVar(
		"ion_chroma_gameinfo", "0", FCVAR_ARCHIVE_PLAYERPROFILE,
		"Rainbow colors for game info. 1 = enabled, 0 = disabled.");
	Cvar_ion_speedometer_always_show = new ConVar(
		"ion_speedometer_always_show", "0", FCVAR_ARCHIVE_PLAYERPROFILE,
		"Always show speedometer. 1 = enabled, 0 = disabled.");
	Cvar_gauntlet_timer_max_speed_metric = new ConVar(
		"gauntlet_timer_max_speed_metric", "50.05047", FCVAR_ARCHIVE_PLAYERPROFILE,
		"Max speed in gauntlet timer (metric).");
	Cvar_gauntlet_timer_max_speed_imperial = new ConVar(
		"gauntlet_timer_max_speed_imperial", "31.1", FCVAR_ARCHIVE_PLAYERPROFILE,
		"Max speed in gauntlet timer (imperial).");

	s_RuiConVarsReady = true;
	if (s_RuiUiModuleReady && !s_RuiUiHooksDispatched)
	{
		RuiHooks.DispatchForModule("ui(11).dll");
		s_RuiUiHooksDispatched = true;
	}
})
