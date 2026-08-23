#pragma once

#include <cstddef>

struct VMatrix
{
	float m_Elements[4][4];
};

inline void MatrixMultiply(const VMatrix& source1, const VMatrix& source2, VMatrix& destination)
{
	VMatrix result{};
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			for (int index = 0; index < 4; ++index)
				result.m_Elements[row][column] += source1.m_Elements[row][index] * source2.m_Elements[index][column];
		}
	}
	destination = result;
}

static_assert(sizeof(VMatrix) == 0x40);
