#pragma once

inline float FastSqrt(float x)
{
	__m128 root = _mm_sqrt_ss(_mm_load_ss(&x));
	return *(reinterpret_cast<float*>(&root));
}

inline uint32_t SmallestPowerOfTwoGreaterOrEqual(uint32_t x)
{
	x -= 1;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

// return the largest power of two <= x. Will return 0 if passed 0
inline uint32_t LargestPowerOfTwoLessThanOrEqual(uint32_t x)
{
	if (x >= 0x80000000)
		return 0x80000000;

	return SmallestPowerOfTwoGreaterOrEqual(x + 1) >> 1;
}
