#pragma once

#include <cstddef>

struct VMatrix
{
	float m_Elements[4][4];
};

static_assert(sizeof(VMatrix) == 0x40);
