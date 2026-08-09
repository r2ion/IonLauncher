#pragma once

#include <cstddef>
#include <cstdint>

struct con_nprint_s
{
	std::int32_t m_Index;
	float m_TimeToLive;
	float m_Color[3];
	bool m_bFixedWidthFont;
	std::uint8_t m_Padding15[3];
};

static_assert(sizeof(con_nprint_s) == 0x18);
static_assert(offsetof(con_nprint_s, m_TimeToLive) == 0x4);
static_assert(offsetof(con_nprint_s, m_Color) == 0x8);
static_assert(offsetof(con_nprint_s, m_bFixedWidthFont) == 0x14);
