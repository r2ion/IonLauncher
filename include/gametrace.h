#pragma once

#include "mathlib/vector.h"

#include <cstddef>
#include <cstdint>

class IHandleEntity;

struct cplanetrace_t
{
	Vector3 m_Normal;
	float m_Distance;
};

struct csurface_t
{
	const char* m_pName;
	std::uint16_t m_SurfaceProperties;
	std::uint16_t m_Flags;
	std::uint32_t m_Padding0C;
};

struct GameTrace
{
	Vector3 m_StartPosition;
	float m_Reserved0C;
	Vector3 m_EndPosition;
	float m_Reserved1C;
	cplanetrace_t m_Plane;
	float m_Fraction;
	std::int32_t m_Contents;
	std::uint16_t m_DisplacementFlags;
	bool m_AllSolid;
	bool m_StartSolid;
	bool m_Hit;
	std::uint8_t m_Padding3D[3];
	float m_FractionLeftSolid;
	std::uint32_t m_Padding44;
	csurface_t m_Surface;
	std::int32_t m_HitGroup;
	std::int16_t m_PhysicsBone;
	std::int16_t m_WorldSurfaceIndex;
	IHandleEntity* m_pHitEntity;
	std::int32_t m_Hitbox;
	std::int32_t m_HitData;
};

using trace_t = GameTrace;


static_assert(sizeof(cplanetrace_t) == 0x10);
static_assert(sizeof(csurface_t) == 0x10);
static_assert(sizeof(GameTrace) == 0x70);
static_assert(offsetof(GameTrace, m_Plane) == 0x20);
static_assert(offsetof(GameTrace, m_Fraction) == 0x30);
static_assert(offsetof(GameTrace, m_AllSolid) == 0x3A);
static_assert(offsetof(GameTrace, m_FractionLeftSolid) == 0x40);
static_assert(offsetof(GameTrace, m_Surface) == 0x48);
static_assert(offsetof(GameTrace, m_pHitEntity) == 0x60);
static_assert(offsetof(GameTrace, m_Hitbox) == 0x68);
