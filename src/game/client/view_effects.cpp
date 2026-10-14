#include "mathlib/vector.h"
#include "game/client/ivieweffects.h"
#include "shake.h"

#include <cstddef>
#include <cstdint>


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

struct ScreenShake_Internal
{
	enum ShakeType_t
	{
		SHAKE_TYPE_LEGACY = 0,
		SHAKE_TYPE_DEFAULT,
		SHAKE_TYPE_DIRECTIONAL,
	};

	float amplitude;
	float frequency;
	float duration;
	float endtime;
	float shakeSpikeBegin;
	float shakeSpikeEnd;
	Vector3D offset;
	float shakeAngle;
	int command;
	Vector3D direction;
	std::uint8_t nShakeType;
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

class CViewEffects : public IViewEffects
{
public:
	bool m_Initialized;
	std::uint8_t m_Pad0009[7];
	CViewEffectsPointerVector<CViewEffectsFade_t> m_Fades;
	ScreenShake_Internal m_ShakeEntries[16];
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
