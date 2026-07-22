#pragma once

#include "rtech/rui/rui_core_types.h"

#include <cstddef>
#include <cstdint>

struct RuiFloat2Value
{
	float x;
	float y;
};
static_assert(sizeof(RuiFloat2Value) == 0x8);

// Compiled instance-data prefix used by the gamestate_info callback. The
// callback owns substantially more data; hooks only consume this input prefix.
struct RuiGamestateInfoInputPrefix
{
	float position[3];
	float leftTextPosition;
	float rightTextPosition;
	float statusYPosition;
	float timerSize;
	float shadowAmount;
	float textFillAmount;
	float reserved24;
	float leftColor[3];
	float rightColor[3];
	float textSize;
	uint32_t reserved44;
	uint64_t reserved48;
	float endTime;
	uint32_t reserved54;
	const char* statusText;
	float leftTeamScore;
	float rightTeamScore;
	int32_t maxTeamScore;
	int32_t maxTeamPlayers;
};
static_assert(sizeof(RuiGamestateInfoInputPrefix) == 0x70);
static_assert(offsetof(RuiGamestateInfoInputPrefix, leftColor) == 0x28);
static_assert(offsetof(RuiGamestateInfoInputPrefix, rightColor) == 0x34);

struct RuiGamestateInfoFfaInstanceData
{
	uint8_t reserved00[0x44];
	float endTime;
	const char* statusText;
	float leftTeamScore;
	float rightTeamScore;
	int32_t maxTeamScore;
	uint32_t reserved5C;
	const char* factionImage;
	const char* friendlyPlayerCardImage;
	const char* enemyPlayerCardImage;
	uint8_t reserved78[0x40];
	RuiImageHandle whiteImage;
	RuiImageHandle selectedPlayerCardImage;
	float selectedTopColor[4];
	float selectedBottomColor[4];
	float topColor[4];
	float bottomColor[4];
	const char* formattedTime;
	const char* localizedGameMode;
	RuiImageHandle otherPlayerCardImage;
	RuiImageHandle leftFillImage;
	RuiImageHandle rightFillImage;
	float selectedScoreFraction;
	float otherScoreFraction;
	RuiImageHandle factionImageAsset;
	const char* selectedScoreText;
	const char* otherScoreText;
};
static_assert(sizeof(RuiGamestateInfoFfaInstanceData) == 0x138);
static_assert(offsetof(RuiGamestateInfoFfaInstanceData, endTime) == 0x44);
static_assert(offsetof(RuiGamestateInfoFfaInstanceData, formattedTime) == 0x100);
static_assert(offsetof(RuiGamestateInfoFfaInstanceData, selectedScoreText) == 0x128);

struct RuiGamestateInfoPsInstanceData
{
	float position[3];
	float leftTextPosition;
	float rightTextPosition;
	float timerScale;
	float shadowAmount;
	float textFillAmount;
	float reserved20;
	float leftColor[3];
	float rightColor[3];
	float textSize;
	uint64_t reserved40;
	uint64_t reserved48;
	float endTime;
	float leftTeamScore;
	float rightTeamScore;
	int32_t maxTeamScore;
	int32_t maxTeamPlayers;
	uint32_t reserved64;
	const char* factionImage;
	uint8_t reserved70[0x3C];
	RuiImageHandle factionImageAsset;
	const char* formattedTime;
	RuiImageHandle leftScoreBarImage;
	RuiImageHandle leftFillImage;
	float leftScoreFraction;
	RuiImageHandle rightScoreBarImage;
	RuiImageHandle rightFillImage;
	float rightScoreFraction;
	const char* leftScoreText;
	const char* rightScoreText;
};
static_assert(sizeof(RuiGamestateInfoPsInstanceData) == 0xE0);
static_assert(offsetof(RuiGamestateInfoPsInstanceData, leftColor) == 0x24);
static_assert(offsetof(RuiGamestateInfoPsInstanceData, endTime) == 0x50);
static_assert(offsetof(RuiGamestateInfoPsInstanceData, formattedTime) == 0xB0);

struct RuiCrosshairPlusInstanceData
{
	uint8_t reserved00[0xC];
	float adjustedSpread;
	float adsFraction;
	int32_t isSprinting;
	int32_t isReloading;
	int32_t isGrappleInRange;
	float teamColor[3];
	float movementX;
	float movementY;
	int32_t isAmped;
	RuiFloat2Value topOuterMin;
	RuiFloat2Value topOuterMax;
	RuiFloat2Value topInnerMin;
	RuiFloat2Value topInnerMax;
	RuiFloat2Value bottomOuterMin;
	RuiFloat2Value bottomOuterMax;
	RuiFloat2Value bottomInnerMin;
	RuiFloat2Value bottomInnerMax;
	RuiFloat2Value leftOuterMin;
	RuiFloat2Value leftOuterMax;
	RuiFloat2Value leftInnerMin;
	RuiFloat2Value leftInnerMax;
	RuiFloat2Value rightOuterMin;
	RuiFloat2Value rightOuterMax;
	RuiFloat2Value rightInnerMin;
	RuiFloat2Value rightInnerMax;
	RuiImageHandle whiteImage;
	uint32_t reservedBC;
	float color[4];
};
static_assert(sizeof(RuiCrosshairPlusInstanceData) == 0xD0);
static_assert(offsetof(RuiCrosshairPlusInstanceData, adjustedSpread) == 0xC);
static_assert(offsetof(RuiCrosshairPlusInstanceData, topOuterMin) == 0x38);
static_assert(offsetof(RuiCrosshairPlusInstanceData, whiteImage) == 0xB8);
static_assert(offsetof(RuiCrosshairPlusInstanceData, color) == 0xC0);

struct RuiKraberAmmoCounterInstanceData
{
	uint8_t reserved00[0x28];
	int32_t ammo;
	int32_t clipSize;
	uint32_t reserved30;
	float pulseAmplitude;
	float urgencyAlpha;
	float urgencyScale;
	float urgencyColor[4];
	float pulsedColor[4];
	float pulsedAlpha;
	RuiImageHandle emptyBulletImage;
	float clipEnd;
	RuiImageHandle loadedBulletImage;
	float ammoFraction;
	RuiImageHandle bleedImage;
	float scrollPosition;
	float nextScrollPosition;
	RuiImageHandle whiteMultiplyImage;
};
static_assert(sizeof(RuiKraberAmmoCounterInstanceData) == 0x84);
static_assert(offsetof(RuiKraberAmmoCounterInstanceData, ammo) == 0x28);
static_assert(offsetof(RuiKraberAmmoCounterInstanceData, urgencyColor) == 0x40);
static_assert(offsetof(RuiKraberAmmoCounterInstanceData, ammoFraction) == 0x70);

struct alignas(16) RuiMastiffAmmoCounterInstanceData
{
	uint8_t reserved00[0x38];
	int32_t ammo;
	int32_t clipSize;
	uint32_t reserved40;
	float pulseAmplitude;
	float urgencyAlpha;
	float urgencyScale;
	float urgencyColor[4];
	float pulsedColor[4];
	float pulsedAlpha;
	RuiImageHandle shellImage;
	float clipEnd;
	RuiImageHandle noiseImage;
	float ammoFraction;
	float randomValue;
	RuiImageHandle bleedImage;
	float scrollPosition;
	float nextScrollPosition;
	RuiImageHandle whiteMultiplyImage;
};
static_assert(sizeof(RuiMastiffAmmoCounterInstanceData) == 0xA0);
static_assert(offsetof(RuiMastiffAmmoCounterInstanceData, ammo) == 0x38);
static_assert(offsetof(RuiMastiffAmmoCounterInstanceData, urgencyColor) == 0x50);
static_assert(offsetof(RuiMastiffAmmoCounterInstanceData, ammoFraction) == 0x80);

struct RuiHcogLowerInstanceData
{
	uint8_t reserved00[0x4C];
	float visibility;
	int32_t ammo;
	int32_t clipSize;
	float ammoColor[3];
	RuiImageHandle frontImage;
	const char* formattedAmmo;
	float frontScale[4];
	float frontTranslucentScale[4];
	float rearScale[4];
	float meterScale[4];
	float urgencyColor[4];
	RuiImageHandle noiseImage;
	float randomValue;
	RuiImageHandle frontTranslucentImage;
	RuiImageHandle rearImage;
	RuiImageHandle meterImage;
	float ammoFraction;
};
static_assert(sizeof(RuiHcogLowerInstanceData) == 0xD8);
static_assert(offsetof(RuiHcogLowerInstanceData, visibility) == 0x4C);
static_assert(offsetof(RuiHcogLowerInstanceData, formattedAmmo) == 0x68);
static_assert(offsetof(RuiHcogLowerInstanceData, ammoFraction) == 0xD4);

struct RuiP2011GreenSightsInstanceData
{
	float inputColor[4];
	float visibility;
	RuiImageHandle reticleImage;
	uint8_t reserved18[8];
	float primaryScale[4];
	float secondaryScale[4];
};
static_assert(sizeof(RuiP2011GreenSightsInstanceData) == 0x40);

struct RuiPilotSpeedometerInstanceData
{
	uint8_t reserved00[0x30];
	int32_t useMetric;
	float currentOrigin[3];
	float previousSpeed;
	float previousOrigin[3];
	float previousSampleTime;
	float lowSpeedStartTime;
	float speedColor[3];
	float visibility;
	const char* formattedSpeed;
	const char* localizedUnit;
};
static_assert(sizeof(RuiPilotSpeedometerInstanceData) == 0x78);
static_assert(offsetof(RuiPilotSpeedometerInstanceData, useMetric) == 0x30);
static_assert(offsetof(RuiPilotSpeedometerInstanceData, visibility) == 0x64);

struct RuiGauntletHudInstanceData
{
	uint8_t reserved00[0x84];
	float startTime;
	int32_t isFinished;
	float finalTime;
	float bestTime;
	uint8_t reserved94[0xC];
	int32_t useMetric;
	float currentOrigin[3];
	float smoothedSpeed;
	float previousOrigin[3];
	float previousSampleTime;
	float speedColor[3];
	float speedGaugePosition;
	RuiImageHandle borderImage;
	RuiImageHandle whiteImage;
	uint32_t reservedDC;
	const char* speedText;
	const char* unitText;
	const char* timeTitle;
	const char* timeText;
	const char* bestTimeText;
	const char* bestTimeTitle;
};
static_assert(sizeof(RuiGauntletHudInstanceData) == 0x110);
static_assert(offsetof(RuiGauntletHudInstanceData, startTime) == 0x84);
static_assert(offsetof(RuiGauntletHudInstanceData, useMetric) == 0xA0);
static_assert(offsetof(RuiGauntletHudInstanceData, speedText) == 0xE0);

struct RuiProScreenPanelInstanceData
{
	uint8_t reserved00[0x38];
	uint32_t value;
	int32_t isOwnedByPlayer;
	float red;
	float green;
	float blue;
	float alpha;
	RuiImageHandle whiteImage;
	uint32_t reserved54;
	const char* formattedValue;
};
static_assert(sizeof(RuiProScreenPanelInstanceData) == 0x60);
static_assert(offsetof(RuiProScreenPanelInstanceData, value) == 0x38);
static_assert(offsetof(RuiProScreenPanelInstanceData, formattedValue) == 0x58);
