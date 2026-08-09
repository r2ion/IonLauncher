#pragma once

#include <cstdint>

using edict_t = std::uint16_t;

inline constexpr edict_t INVALID_EDICT = 0xFFFF;

static_assert(sizeof(edict_t) == 0x2);
