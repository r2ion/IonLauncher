#pragma once

#include "mathlib/vector.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

enum LightType_t : std::int32_t
{
    MATERIAL_LIGHT_DISABLE = 0,
    MATERIAL_LIGHT_POINT = 1,
    MATERIAL_LIGHT_DIRECTIONAL = 2,
    MATERIAL_LIGHT_SPOT = 3,
};

enum LightType_OptimizationFlags_t : std::uint32_t
{
    LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0 = 1u << 0,
    LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1 = 1u << 1,
    LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2 = 1u << 2,
    LIGHTTYPE_OPTIMIZATIONFLAGS_DERIVED_VALUES_CALCED = 1u << 3,
};

struct LightDesc_t
{
    LightType_t m_Type;
    Vector3 m_Color;
    Vector3 m_Position;
    Vector3 m_Direction;
    float m_Range;
    float m_Falloff;
    float m_Attenuation0;
    float m_Attenuation1;
    float m_Attenuation2;
    float m_Theta;
    float m_Phi;
    float m_ThetaDot;
    float m_PhiDot;
    std::uint32_t m_Flags;
    float OneOver_ThetaDot_Minus_PhiDot;
    float m_RangeSquared;
};

static_assert(sizeof(LightType_t) == 0x4);
static_assert(sizeof(LightDesc_t) == 0x58);
static_assert(offsetof(LightDesc_t, m_Color) == 0x4);
static_assert(offsetof(LightDesc_t, m_Position) == 0x10);
static_assert(offsetof(LightDesc_t, m_Direction) == 0x1C);
static_assert(offsetof(LightDesc_t, m_Range) == 0x28);
static_assert(offsetof(LightDesc_t, m_ThetaDot) == 0x44);
static_assert(offsetof(LightDesc_t, m_Flags) == 0x4C);
static_assert(offsetof(LightDesc_t, OneOver_ThetaDot_Minus_PhiDot) == 0x50);
static_assert(offsetof(LightDesc_t, m_RangeSquared) == 0x54);
static_assert(std::is_standard_layout_v<LightDesc_t>);
