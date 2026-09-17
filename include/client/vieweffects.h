#pragma once

#include "mathlib/vector.h"

#include <cstddef>
#include <cstdint>

enum class ShakeCommand_t : int
{
	Start = 0,
	Stop,
	Amplitude,
	Frequency,
	StartRumbleOnly,
	StartNoRumble,
	StartDirectional,
};

enum class ShakeMode_t : std::uint8_t
{
	Standard = 0,
	Randomized = 1,
	Directional = 2,
};

struct ScreenShake_t
{
	ShakeCommand_t m_Command;
	float m_Amplitude;
	float m_Frequency;
	float m_Duration;
	Vector3D m_Direction;
};

enum ScreenFadeFlags_t : std::int16_t
{
	ScreenFadeIn = 0x0001,
	ScreenFadeOut = 0x0002,
	ScreenFadeModulate = 0x0004,
	ScreenFadeStayOut = 0x0008,
	ScreenFadePurge = 0x0010,
};

inline constexpr int ScreenFadeFractionalBits = 6;

struct ScreenFade_t
{
	std::uint16_t m_Duration;
	std::uint16_t m_HoldTime;
	ScreenFadeFlags_t m_FadeFlags;
	std::uint8_t m_Red;
	std::uint8_t m_Green;
	std::uint8_t m_Blue;
	std::uint8_t m_Alpha;
};

struct ScreenTilt_t
{
	int m_Command;
	bool m_EaseInOut;
	std::uint8_t m_Pad0005[3];
	QAngle m_Angle;
	float m_Duration;
	float m_RampTime;
};

struct CViewEffectsFade_t
{
	float m_Speed;
	float m_EndTime;
	float m_ResetTime;
	std::uint8_t m_Red;
	std::uint8_t m_Green;
	std::uint8_t m_Blue;
	std::uint8_t m_Alpha;
	int m_Flags;
};

struct CViewEffectsShake_t
{
	float m_Amplitude;
	float m_Frequency;
	float m_Duration;
	float m_EndTime;
	float m_PreviousShakeTime;
	float m_NextShakeTime;
	Vector3D m_Offset;
	float m_Angle;
	ShakeCommand_t m_Command;
	Vector3D m_Direction;
	ShakeMode_t m_Mode;
	std::uint8_t m_Pad0039[3];
};

struct CViewEffectsTilt_t
{
	bool m_EaseInOut;
	std::uint8_t m_Pad0001[3];
	QAngle m_Angle;
	float m_StartTime;
	float m_EndTime;
	float m_Duration;
	float m_RampTime;
	std::uint8_t m_Pad0020[12];
	int m_Command;
};

template <typename T>
struct CViewEffectsPointerVector
{
	T** m_Entries;
	std::int64_t m_AllocationCount;
	std::int64_t m_GrowSize;
	int m_Count;
	std::uint32_t m_Pad001C;
};

class CViewEffects
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

	bool m_Initialized;
	std::uint8_t m_Pad0009[7];
	CViewEffectsPointerVector<CViewEffectsFade_t> m_Fades;
	CViewEffectsShake_t m_ShakeEntries[16];
	std::int64_t m_ShakeCount;
	Vector3D m_ShakeOffset;
	Vector2D m_ShakeAngleOffset;
	CViewEffectsPointerVector<CViewEffectsTilt_t> m_Tilts;
	QAngle m_TiltAngle;
	int m_FadeRed;
	int m_FadeGreen;
	int m_FadeBlue;
	int m_FadeAlpha;
	bool m_FadeBlend;
	std::uint8_t m_Pad044D[3];
};
