#pragma once

#include <cstdint>

struct ScreenShake_t;
struct ScreenFade_t;

class IViewEffects
{
public:
	virtual void Init(bool enabled) = 0;
	virtual void LevelInit() = 0;
	virtual void GetFadeParams(std::uint8_t* pRed, std::uint8_t* pGreen, std::uint8_t* pBlue,
		std::uint8_t* pAlpha, bool* pBlend) = 0;
	virtual void Shake(const ScreenShake_t& shake) = 0;
	virtual void Fade(const ScreenFade_t& fade) = 0;
	virtual void ClearPermanentFades() = 0;
	virtual void ClearAllFades() = 0;
	virtual void CalcShake() = 0;
	virtual void ApplyShake(float* pOrigin, float* pAngles, float coordinateFactor,
		float angularFactor) = 0;
	virtual void CalcTilt() = 0;
	virtual void ApplyTilt(float* pAngles) = 0;
	virtual void Save(void* pSave) = 0;
	virtual void Restore(void* pRestore) = 0;
	virtual void ClearAllShakes() = 0;
};
