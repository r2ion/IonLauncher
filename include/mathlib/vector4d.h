#pragma once

#include <cstddef>
#include <type_traits>

class Vector4D
{
  public:
    float x;
    float y;
    float z;
    float w;
};

static_assert(sizeof(Vector4D) == 0x10);
static_assert(offsetof(Vector4D, w) == 0xC);
static_assert(std::is_standard_layout_v<Vector4D>);
