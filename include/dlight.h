#pragma once

#include "mathlib/vector.h"

#include <cstddef>
#include <cstdint>

enum DLightFlags_t : std::uint32_t
{
	DLIGHT_NO_WORLD_ILLUMINATION = 1 << 0,
	DLIGHT_NO_MODEL_ILLUMINATION = 1 << 1,
	DLIGHT_ADD_DISPLACEMENT_ALPHA = 1 << 2,
	DLIGHT_SUBTRACT_DISPLACEMENT_ALPHA = 1 << 3,
	DLIGHT_DISPLACEMENT_MASK = DLIGHT_ADD_DISPLACEMENT_ALPHA | DLIGHT_SUBTRACT_DISPLACEMENT_ALPHA,
};

struct dlight_t
{
	DLightFlags_t m_Flags;
	Vector3 m_Origin;
	float m_Radius;
	float m_HalfDistance;
	Vector3 m_Color;
	float m_Die;
	float m_Decay;
	int m_Key;
	int m_Style;
	Vector3 m_Direction;
	float m_InnerAngle;
	float m_OuterAngle;
	std::uint32_t m_Unknown48;
	std::uint32_t m_Unknown4C;
};

static_assert(sizeof(dlight_t) == 0x50);
static_assert(offsetof(dlight_t, m_Flags) == 0x0);
static_assert(offsetof(dlight_t, m_Origin) == 0x4);
static_assert(offsetof(dlight_t, m_Radius) == 0x10);
static_assert(offsetof(dlight_t, m_HalfDistance) == 0x14);
static_assert(offsetof(dlight_t, m_Color) == 0x18);
static_assert(offsetof(dlight_t, m_Die) == 0x24);
static_assert(offsetof(dlight_t, m_Decay) == 0x28);
static_assert(offsetof(dlight_t, m_Key) == 0x2C);
static_assert(offsetof(dlight_t, m_Style) == 0x30);
static_assert(offsetof(dlight_t, m_Direction) == 0x34);
static_assert(offsetof(dlight_t, m_InnerAngle) == 0x40);
static_assert(offsetof(dlight_t, m_OuterAngle) == 0x44);
static_assert(offsetof(dlight_t, m_Unknown48) == 0x48);
static_assert(offsetof(dlight_t, m_Unknown4C) == 0x4C);
