#pragma once

#include <cstddef>
#include <cstdint>

struct matrix3x4_t
{
	float m_Rows[3][4];
};

struct alignas(16) RayVector_t
{
	float x;
	float y;
	float z;
	float w;
};

struct Ray_t
{
	RayVector_t m_Start;
	RayVector_t m_Delta;
	RayVector_t m_StartOffset;
	RayVector_t m_Extents;
	const matrix3x4_t* m_pWorldAxisTransform;
	std::uint32_t m_TraceFlags;
	bool m_IsRay;
	bool m_IsSwept;
	bool m_UsesWorldAxisTransform;
	std::uint8_t m_Padding4F;
	std::uint32_t m_EngineFlags;
	std::byte m_EngineState54[0xC];
};

static_assert(sizeof(matrix3x4_t) == 0x30);
static_assert(sizeof(RayVector_t) == 0x10);
static_assert(sizeof(Ray_t) == 0x60);
static_assert(offsetof(Ray_t, m_pWorldAxisTransform) == 0x40);
static_assert(offsetof(Ray_t, m_TraceFlags) == 0x48);
static_assert(offsetof(Ray_t, m_IsRay) == 0x4C);
static_assert(offsetof(Ray_t, m_EngineFlags) == 0x50);
