//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#ifndef RECASTDETOURCOMMON_H
#define RECASTDETOURCOMMON_H

#include "Shared/Include/SharedConst.h"
#include "Shared/Include/SharedDefs.h"
#include "Shared/Include/SharedMath.h"
#include "SharedAssert.h"

/**
@defgroup shared Shared

Members in this module are used to create, manipulate, and query navigation 
meshes.

@note This is a summary list of members.  Use the index or search 
feature to find minor members.
*/

/// @name General helper functions
/// @{

/// Used to ignore a function parameter.  VS complains about unused parameters
/// and this silences the warning.
///  @param [in] _ Unused parameter
template<class T> void rdIgnoreUnused(const T&) { }

/// Swaps the values of the two parameters.
///  @param[in,out]	a	Value A
///  @param[in,out]	b	Value B
template<class T> inline void rdSwap(T& a, T& b) { T t = a; a = b; b = t; }

/// Returns the minimum of two values.
///  @param[in]		a	Value A
///  @param[in]		b	Value B
///  @return The minimum of the two values.
template<class T> inline T rdMin(T a, T b) { return a < b ? a : b; }

/// Returns the maximum of two values.
///  @param[in]		a	Value A
///  @param[in]		b	Value B
///  @return The maximum of the two values.
template<class T> inline T rdMax(T a, T b) { return a > b ? a : b; }

/// Clamps the value to the specified range.
///  @param[in]		v	The value to clamp.
///  @param[in]		mn	The minimum permitted return value.
///  @param[in]		mx	The maximum permitted return value.
///  @return The value, clamped to the specified range.
template<class T> inline T rdClamp(T v, T mn, T mx) { return v < mn ? mn : (v > mx ? mx : v); }

/// Returns the absolute value.
///  @param[in]		a	The value.
///  @return The absolute value of the specified value.
template<class T> inline T rdAbs(T a) { return a < 0 ? -a : a; }

/// Returns the square of the value.
///  @param[in]		a	The value.
///  @return The square of the value.
template<class T> inline T rdSqr(T a) { return a * a; }

/// Converts value from Degrees to Radians.
///  @param[in]		x	The value to convert.
///  @return The input value as Radians.
inline float rdDegToRad(const float x) { return x * (RD_PI/180.0f); }

/// Converts value from Radians to Degrees.
///  @param[in]		x	The value to convert.
///  @return The input value as Degrees.
inline float rdRadToDeg(const float x) { return x * (180.0f/RD_PI); }

/// Tests a specific bit in a bit cell
///  @param[in]		i	The bit number
///  @return The offset mask for the bit.
inline int rdBitCellBit(const int i) { return (1 << ((i) & (RD_BITS_PER_BIT_CELL-1))); }

/// @}
/// @name Vector helper functions.
/// @{
/// 

/// A 2-dimensional vector structure.
struct rdVec2D
{
	rdVec2D() = default;
	rdVec2D(const float ix, const float iy)
	{
		init(ix, iy);
	}

	rdForceInline void init(const float ix, const float iy)
	{
		x = ix;
		y = iy;
	}

	rdForceInline float& operator[](const int i)
	{
		rdAssert((i>=0) && (i<2));
		return ((float*)this)[i];
	}

	rdForceInline float operator[](const int i) const
	{
		Assert((i>=0) && (i<2));
		return ((float*)this)[i];
	}

	rdVec2D& operator=(const rdVec2D&) = default;

	float x;
	float y;
};

/// A 3-dimensional vector structure.
/// note(kawe): in the future this shouldn't derive from rdVec2D,
/// but instead it should return an explicit rdVec2D on request.
/// This will be a big change as the library was initially made
/// using float arrays which allows for passing vec3d into vec2d
/// code. If this gets changed, the compiled code should be
/// checked for performance regressions. The current switch to
/// vector structures actually improved the majority of the code
/// performance as the compiler now knows the size ahead of time.
struct rdVec3D : public rdVec2D
{
	rdVec3D() = default;
	rdVec3D(const float ix, const float iy, const float iz)
	{
		init(ix, iy, iz);
	}

	rdVec3D(const rdVec3D& o)
	{
		init(o.x, o.y, o.z);
	}

	rdVec3D(const rdVec3D* o)
	{
		init(o->x, o->y, o->z);
	}

	rdForceInline void init(const float ix, const float iy, const float iz)
	{
		x = ix;
		y = iy;
		z = iz;
	}

	rdForceInline rdVec2D get2D() const
	{
		return rdVec2D(x, y);
	}

	rdForceInline float& operator[](const int i)
	{
		rdAssert((i>=0) && (i<3));
		return ((float*)this)[i];
	}

	rdForceInline float operator[](const int i) const
	{
		Assert((i>=0) && (i<3));
		return ((float*)this)[i];
	}

	rdVec3D& operator=(const rdVec3D&) = default;

	float z;
};

/// Derives the cross product of two vectors. (@p v1 x @p v2)
///  @param[out]	dest	The cross product. [(x, y, z)]
///  @param[in]		v1		A Vector [(x, y, z)]
///  @param[in]		v2		A vector [(x, y, z)]
inline void rdVcross(rdVec3D* dest, const rdVec3D* v1, const rdVec3D* v2)
{
	dest->x = v1->y*v2->z - v1->z*v2->y;
	dest->y = v1->z*v2->x - v1->x*v2->z;
	dest->z = v1->x*v2->y - v1->y*v2->x;
}

/// Derives the xy-plane 2D perp product of the two vectors. (uy*vx - ux*vy)
///  @param[in]		u		The LHV vector [(x, y)]
///  @param[in]		v		The RHV vector [(x, y)]
/// @return The perp dot product on the xy-plane.
///
/// The vectors are projected onto the xy-plane, so the z-values are ignored.
inline float rdVperp2D(const rdVec2D* u, const rdVec2D* v)
{
	return u->x*v->y - u->y*v->x;
}

/// Derives the dot product of two vectors. (@p v1 . @p v2)
///  @param[in]		v1	A Vector [(x, y, z)]
///  @param[in]		v2	A vector [(x, y, z)]
/// @return The dot product.
inline float rdVdot(const rdVec3D* v1, const rdVec3D* v2)
{
	return v1->x*v2->x + v1->y*v2->y + v1->z*v2->z;
}

/// Derives the dot product of two vectors on the xy-plane. (@p u . @p v)
///  @param[in]		u		A vector [(x, y)]
///  @param[in]		v		A vector [(x, y)]
/// @return The dot product on the xy-plane.
///
/// The vectors are projected onto the xy-plane, so the z-values are ignored.
inline float rdVdot2D(const rdVec2D* u, const rdVec2D* v)
{
	return u->x*v->x + u->y*v->y;
}

/// Performs a scaled vector addition. (@p v1 + (@p v2 * @p s))
///  @param[out]	dest	The result vector. [(x, y, z)]
///  @param[in]		v1		The base vector. [(x, y, z)]
///  @param[in]		v2		The vector to scale and add to @p v1. [(x, y, z)]
///  @param[in]		s		The amount to scale @p v2 by before adding to @p v1.
inline void rdVmad(rdVec3D* dest, const rdVec3D* v1, const rdVec3D* v2, const float s)
{
	dest->x = v1->x+v2->x*s;
	dest->y = v1->y+v2->y*s;
	dest->z = v1->z+v2->z*s;
}

/// Performs a scaled vector addition. ((@p v1 + @p v2) * @p s)
///  @param[out]	dest	The result vector. [(x, y, z)]
///  @param[in]		v1		The base vector. [(x, y, z)]
///  @param[in]		v2		The vector to add to @p v1. [(x, y, z)]
///  @param[in]		s		The amount to scale the addition result of @p v1 and @p v2.
inline void rdVsad(rdVec3D* dest, const rdVec3D* v1, const rdVec3D* v2, const float s)
{
	dest->x = (v1->x+v2->x)*s;
	dest->y = (v1->y+v2->y)*s;
	dest->z = (v1->z+v2->z)*s;
}

/// Performs a linear interpolation between two vectors. (@p v1 toward @p v2)
///  @param[out]	dest	The result vector. [(x, y, x)]
///  @param[in]		v1		The starting vector.
///  @param[in]		v2		The destination vector.
///	 @param[in]		t		The interpolation factor. [Limits: 0 <= value <= 1.0]
inline void rdVlerp(rdVec3D* dest, const rdVec3D* v1, const rdVec3D* v2, const float t)
{
	dest->x = v1->x+(v2->x-v1->x)*t;
	dest->y = v1->y+(v2->y-v1->y)*t;
	dest->z = v1->z+(v2->z-v1->z)*t;
}

/// Performs a vector addition. (@p v1 + @p v2)
///  @param[out]	dest	The result vector. [(x, y, z)]
///  @param[in]		v1		The base vector. [(x, y, z)]
///  @param[in]		v2		The vector to add to @p v1. [(x, y, z)]
inline void rdVadd(rdVec3D* dest, const rdVec3D* v1, const rdVec3D* v2)
{
	dest->x = v1->x+v2->x;
	dest->y = v1->y+v2->y;
	dest->z = v1->z+v2->z;
}

/// Performs a vector subtraction. (@p v1 - @p v2)
///  @param[out]	dest	The result vector. [(x, y, z)]
///  @param[in]		v1		The base vector. [(x, y, z)]
///  @param[in]		v2		The vector to subtract from @p v1. [(x, y, z)]
inline void rdVsub(rdVec3D* dest, const rdVec3D* v1, const rdVec3D* v2)
{
	dest->x = v1->x-v2->x;
	dest->y = v1->y-v2->y;
	dest->z = v1->z-v2->z;
}

/// Performs a vector subtraction on the xy-plane. (@p v1 - @p v2)
///  @param[out]	dest	The result vector. [(x, y)]
///  @param[in]		v1		The base vector. [(x, y)]
///  @param[in]		v2		The vector to subtract from @p v1. [(x, y)]
inline void rdVsub2D(rdVec2D* dest, const rdVec2D* v1, const rdVec2D* v2)
{
	dest->x = v1->x - v2->x;
	dest->y = v1->y - v2->y;
}

/// Scales the vector by the specified value. (@p v * @p t)
///  @param[out]	dest	The result vector. [(x, y, z)]
///  @param[in]		v		The vector to scale. [(x, y, z)]
///  @param[in]		t		The scaling factor.
inline void rdVscale(rdVec3D* dest, const rdVec3D* v, const float t)
{
	dest->x = v->x*t;
	dest->y = v->y*t;
	dest->z = v->z*t;
}

/// Selects the minimum value of each element from the specified vectors.
///  @param[in,out]	mn	A vector.  (Will be updated with the result.) [(x, y, z)]
///  @param[in]	v	A vector. [(x, y, z)]
inline void rdVmin(rdVec3D* mn, const rdVec3D* v)
{
	mn->x = rdMin(mn->x, v->x);
	mn->y = rdMin(mn->y, v->y);
	mn->z = rdMin(mn->z, v->z);
}

/// Selects the maximum value of each element from the specified vectors.
///  @param[in,out]	mx	A vector.  (Will be updated with the result.) [(x, y, z)]
///  @param[in]		v	A vector. [(x, y, z)]
inline void rdVmax(rdVec3D* mx, const rdVec3D* v)
{
	mx->x = rdMax(mx->x, v->x);
	mx->y = rdMax(mx->y, v->y);
	mx->z = rdMax(mx->z, v->z);
}

/// Sets the vector elements to the specified values.
///  @param[out]	dest	The result vector. [(x, y, z)]
///  @param[in]		x		The x-value of the vector.
///  @param[in]		y		The y-value of the vector.
///  @param[in]		z		The z-value of the vector.
inline void rdVset(rdVec3D* dest, const float x, const float y, const float z)
{
	dest->x = x; dest->y = y; dest->z = z;
}

/// Performs a vector copy.
///  @param[out]	dest	The result. [(x, y, z)]
///  @param[in]		a		The vector to copy. [(x, y, z)]
inline void rdVcopy(rdVec3D* dest, const rdVec3D* a)
{
	dest->x = a->x;
	dest->y = a->y;
	dest->z = a->z;
}

/// Derives the scalar length of the vector.
///  @param[in]		v The vector. [(x, y, z)]
/// @return The scalar length of the vector.
inline float rdVlen(const rdVec3D* v)
{
	return rdMathSqrtf(v->x*v->x + v->y*v->y + v->z*v->z);
}

/// Derives the square of the scalar length of the vector. (len * len)
///  @param[in]		v The vector. [(x, y, z)]
/// @return The square of the scalar length of the vector.
inline float rdVlenSqr(const rdVec3D* v)
{
	return v->x*v->x + v->y*v->y + v->z*v->z;
}

/// Derives the scalar length of the vector on the xy-plane.
///  @param[in]		v The vector. [(x, y)]
/// @return The scalar length of the vector.
inline float rdVlen2D(const rdVec2D* v)
{
	return rdMathSqrtf(v->x*v->x + v->y*v->y);
}

/// Derives the square of the scalar length of the vector on the xy-plane. (len * len)
///  @param[in]		v The vector. [(x, y)]
/// @return The square of the scalar length of the vector.
inline float rdVlenSqr2D(const rdVec2D* v)
{
	return v->x*v->x + v->y*v->y;
}

/// Returns the distance between two points.
///  @param[in]		v1	A point. [(x, y, z)]
///  @param[in]		v2	A point. [(x, y, z)]
/// @return The distance between the two points.
inline float rdVdist(const rdVec3D* v1, const rdVec3D* v2)
{
	const float dx = v2->x - v1->x;
	const float dy = v2->y - v1->y;
	const float dz = v2->z - v1->z;
	return rdMathSqrtf(dx*dx + dy*dy + dz*dz);
}

/// Returns the square of the distance between two points.
///  @param[in]		v1	A point. [(x, y, z)]
///  @param[in]		v2	A point. [(x, y, z)]
/// @return The square of the distance between the two points.
inline float rdVdistSqr(const rdVec3D* v1, const rdVec3D* v2)
{
	const float dx = v2->x - v1->x;
	const float dy = v2->y - v1->y;
	const float dz = v2->z - v1->z;
	return dx*dx + dy*dy + dz*dz;
}

/// Derives the distance between the specified points on the xy-plane.
///  @param[in]		v1	A point. [(x, y)]
///  @param[in]		v2	A point. [(x, y)]
/// @return The distance between the point on the xy-plane.
///
/// The vectors are projected onto the xy-plane, so the z-values are ignored.
inline float rdVdist2D(const rdVec2D* v1, const rdVec2D* v2)
{
	const float dx = v2->x - v1->x;
	const float dy = v2->y - v1->y;
	return rdMathSqrtf(dx*dx + dy*dy);
}

/// Derives the square of the distance between the specified points on the xy-plane.
///  @param[in]		v1	A point. [(x, y)]
///  @param[in]		v2	A point. [(x, y)]
/// @return The square of the distance between the point on the xy-plane.
inline float rdVdist2DSqr(const rdVec2D* v1, const rdVec2D* v2)
{
	const float dx = v2->x - v1->x;
	const float dy = v2->y - v1->y;
	return dx*dx + dy*dy;
}

/// Normalizes the vector.
///  @param[in,out]	v	The vector to normalize. [(x, y, z)]
inline void rdVnormalize(rdVec3D* v)
{
	const float s = rdMathSqrtf(rdSqr(v->x) + rdSqr(v->y) + rdSqr(v->z));
	if (rdLikely(s > 0))
	{
		const float d = 1.0f / s;
		v->x *= d;
		v->y *= d;
		v->z *= d;
	}
}

/// Normalizes the vector on the xy-plane.
///  @param[in,out]	v	The vector to normalize. [(x, y, z)]
inline void rdVnormalize2D(rdVec2D* v)
{
	const float s = rdMathSqrtf(rdSqr(v->x) + rdSqr(v->y));
	if (rdLikely(s > 0))
	{
		const float d = 1.0f / s;
		v->x *= d;
		v->y *= d;
	}
}

/// Derives the magnitude of the vector.
///  @param[in]		v	A vector. [(x, y, z)]
/// @return The magnitude of the vector.
inline float rdVmag(const rdVec3D* v)
{
	return rdMathSqrtf(rdVdot(v, v));
}

/// Derives the magnitude of the vector on the xy-plane.
///  @param[in]		v	A vector. [(x, y)]
/// @return The magnitude of the vector on the xy-plane.
inline float rdVmag2D(const rdVec2D* v)
{
	return rdMathSqrtf(rdVdot2D(v, v));
}

/// Derives the scalar projection of the specified point into the vector.
///  @param[in]		p	A point. [(x, y, z)]
///  @param[in]		v	A vector. [(x, y, z)]
/// @return The scalar projection of the specified point into the vector.
inline float rdVproj(const rdVec3D* p, const rdVec3D* v)
{
	return rdVdot(p, v) / rdVmag(v);
}

/// Derives the scalar projection of the specified point into the vector on the xy-plane.
///  @param[in]		p	A point. [(x, y)]
///  @param[in]		v	A vector. [(x, y)]
/// @return The scalar projection of the specified point into the vector on the xy-plane.
inline float rdVproj2D(const rdVec2D* p, const rdVec2D* v)
{
	return rdVdot2D(p, v) / rdVmag2D(v);
}

/// Performs a 'sloppy' collocation check of the specified points.
///  @param[in]		p0	A point. [(x, y, z)]
///  @param[in]		p1	A point. [(x, y, z)]
/// @return True if the points are considered to be at the same location.
///
/// Basically, this function will return true if the specified points are 
/// close enough to each other to be considered collocated.
inline bool rdVequal(const rdVec3D* p0, const rdVec3D* p1)
{
	const float thr = rdSqr(1.0f/16384.0f);
	const float d = rdVdistSqr(p0, p1);
	return d < thr;
}

/// Checks that the specified vector's components are all finite.
///  @param[in]		v	A point. [(x, y, z)]
/// @return True if all of the point's components are finite, i.e. not NaN
/// or any of the infinities.
inline bool rdVisfinite(const rdVec3D* v)
{
	const bool result =
		rdMathIsfinite(v->x) &&
		rdMathIsfinite(v->y) &&
		rdMathIsfinite(v->z);

	return result;
}

/// Checks that the specified vector's 2D components are finite.
///  @param[in]		v	A point. [(x, y)]
inline bool rdVisfinite2D(const rdVec2D* v)
{
	const bool result = rdMathIsfinite(v->x) && rdMathIsfinite(v->y);
	return result;
}

/// @}
/// @name Computational geometry helper functions.
/// @{

/// Derives the signed xy-plane area of the triangle ABC, or the relationship of line AB to point C.
///  @param[in]		a		Vertex A. [(x, y)]
///  @param[in]		b		Vertex B. [(x, y)]
///  @param[in]		c		Vertex C. [(x, y)]
/// @return The signed xy-plane area of the triangle.
inline float rdTriArea2D(const rdVec2D* a, const rdVec2D* b, const rdVec2D* c)
{
	const float abx = b->x - a->x;
	const float aby = b->y - a->y;
	const float acx = c->x - a->x;
	const float acy = c->y - a->y;
	return acx*aby - abx*acy;
}

/// Derives the normal of the triangle ABC.
///  @param[in]		a		Vertex A. [(x, y, z)]
///  @param[in]		b		Vertex B. [(x, y, z)]
///  @param[in]		c		Vertex C. [(x, y, z)]
///  @param[out]	out		The resulting normal. [(x, y, z)]
inline void rdTriNormal(const rdVec3D* v0, const rdVec3D* v1, const rdVec3D* v2, rdVec3D* out)
{
	rdVec3D e0, e1;
	rdVsub(&e0, v1, v0);
	rdVsub(&e1, v2, v0);
	rdVcross(out, &e0, &e1);
	rdVnormalize(out);
}

/// Determines if two axis-aligned bounding boxes overlap.
///  @param[in]		amin	Minimum bounds of box A. [(x, y, z)]
///  @param[in]		amax	Maximum bounds of box A. [(x, y, z)]
///  @param[in]		bmin	Minimum bounds of box B. [(x, y, z)]
///  @param[in]		bmax	Maximum bounds of box B. [(x, y, z)]
/// @return True if the two AABB's overlap.
/// @see rdOverlapBounds
inline bool rdOverlapQuantBounds(const unsigned short amin[3], const unsigned short amax[3],
								 const unsigned short bmin[3], const unsigned short bmax[3])
{
	bool overlap = true;
	overlap = (amin[0] > bmax[0] || amax[0] < bmin[0]) ? false : overlap;
	overlap = (amin[1] > bmax[1] || amax[1] < bmin[1]) ? false : overlap;
	overlap = (amin[2] > bmax[2] || amax[2] < bmin[2]) ? false : overlap;
	return overlap;
}

/// Determines if two axis-aligned bounding boxes overlap.
///  @param[in]		amin	Minimum bounds of box A. [(x, y, z)]
///  @param[in]		amax	Maximum bounds of box A. [(x, y, z)]
///  @param[in]		bmin	Minimum bounds of box B. [(x, y, z)]
///  @param[in]		bmax	Maximum bounds of box B. [(x, y, z)]
/// @return True if the two AABB's overlap.
/// @see rdOverlapQuantBounds
inline bool rdOverlapBounds(const rdVec3D* amin, const rdVec3D* amax,
							const rdVec3D* bmin, const rdVec3D* bmax)
{
	bool overlap = true;
	overlap = (amin->x > bmax->x || amax->x < bmin->x) ? false : overlap;
	overlap = (amin->y > bmax->y || amax->y < bmin->y) ? false : overlap;
	overlap = (amin->z > bmax->z || amax->z < bmin->z) ? false : overlap;
	return overlap;
}

/// Derives the slope angle from 2 points.
///  @param[in]		v1	The start vector. [(x, y, z)]
///  @param[in]		v2	The end vector. [(x, y, z)]
/// @return The slope angle between the 2 points.
float rdCalcSlopeAngle(const rdVec3D* v1, const rdVec3D* v2);

/// Derives the closest point on a triangle from the specified reference point.
///  @param[out]	closest	The closest point on the triangle.	
///  @param[in]		p		The reference point from which to test. [(x, y, z)]
///  @param[in]		a		Vertex A of triangle ABC. [(x, y, z)]
///  @param[in]		b		Vertex B of triangle ABC. [(x, y, z)]
///  @param[in]		c		Vertex C of triangle ABC. [(x, y, z)]
void rdClosestPtPointTriangle(rdVec3D* closest, const rdVec3D* p,
							  const rdVec3D* a, const rdVec3D* b, const rdVec3D* c);

/// Derives the z-axis height of the closest point on the triangle from the specified reference point.
///  @param[in]		p		The reference point from which to test. [(x, y, z)]
///  @param[in]		a		Vertex A of triangle ABC. [(x, y, z)]
///  @param[in]		b		Vertex B of triangle ABC. [(x, y, z)]
///  @param[in]		c		Vertex C of triangle ABC. [(x, y, z)]
///  @param[out]	h		The resulting height.
bool rdClosestHeightPointTriangle(const rdVec3D* p, const rdVec3D* a, const rdVec3D* b, const rdVec3D* c, float& h);

bool rdIntersectSegmentTriangle(const rdVec3D* sp, const rdVec3D* sq,
								const rdVec3D* a, const rdVec3D* b, const rdVec3D* c, float& t);

bool rdIntersectSegmentPoly2D(const rdVec3D* p0, const rdVec3D* p1,
							  const rdVec3D* verts, int nverts,
							  float& tmin, float& tmax,
							  int& segMin, int& segMax);

bool rdIntersectSegmentAABB(const rdVec3D* sp, const rdVec3D* sq,
						 const rdVec3D* amin, const rdVec3D* amax,
						 float& tmin, float& tmax);

bool rdIntersectSegmentCylinder(const rdVec3D* sp, const rdVec3D* sq, const rdVec3D* position,
								const float radius, const float height,
								float& tmin, float& tmax);

bool rdIntersectSegmentConvexHull(const rdVec3D* sp, const rdVec3D* sq, const rdVec3D* verts,
								  int nverts, float hmin, float hmax,
								  float& tmin, float& tmax);

bool rdIntersectSegSeg2D(const rdVec2D* ap, const rdVec2D* aq,
						 const rdVec2D* bp, const rdVec2D* bq,
						 float& s, float& t);

float rdDistancePtLine2D(const rdVec2D* pt, const rdVec2D* p, const rdVec2D* q);

/// Derives the normal of an edge
///  @param[in]		dir		The direction of the edge. [(x, y)]
///  @param[out]	out		The resulting normal. [(x, y)]
void rdCalcEdgeNormal2D(const rdVec2D* dir, rdVec2D* out);

/// Derives the normal of an edge
///  @param[in]		v1		First vert of the polygon edge. [(x, y, z)]
///  @param[in]		v2		Second vert of the polygon edge. [(x, y, z)]
///  @param[out]	out		The resulting normal. [(x, y)]
void rdCalcEdgeNormalPt2D(const rdVec2D* v1, const rdVec2D* v2, rdVec2D* out);

/// Derives the sub-edge area of an edge.
///  @param[in]		edgeStart		First vert of the polygon edge. [(x, y, z)]
///  @param[in]		edgeEnd			Second vert of the polygon edge. [(x, y, z)]
///  @param[in]		subEdgeStart	First vert of the detail edge. [(x, y, z)]
///  @param[in]		subEdgeEnd		Second vert of the detail edge. [(x, y, z)]
///  @param[out]	tmin			The normalized distance ratio from polygon edge start to detail edge start.
///  @param[out]	tmax			The normalized distance ratio from polygon edge start to detail edge end.
void rdCalcSubEdgeArea2D(const rdVec2D* edgeStart, const rdVec2D* edgeEnd, const rdVec2D* subEdgeStart,
	const rdVec2D* subEdgeEnd, float& tmin, float& tmax);

/// Derives the overlap between 2 edges.
///  @param[in]		edge1Start		Start vert of the first edge. [(x, y, z)]
///  @param[in]		edge1End		End vert of the first edge. [(x, y, z)]
///  @param[in]		edge2Start		Start vert of the second edge. [(x, y, z)]
///  @param[in]		edge2End		End vert of the second edge. [(x, y, z)]
///  @param[in]		targetEdgeVec	The projection direction. [(x, y, z)]
/// @return The length of the overlap.
float rdCalcEdgeOverlap2D(const rdVec2D* edge1Start, const rdVec2D* edge1End,
	const rdVec2D* edge2Start, const rdVec2D* edge2End, const rdVec2D* targetEdgeVec);

/// Derives the maximum angle in which an object on an elevated surface can be seen from below.
///  @param[in]		ledgeSpan		The distance between the edge of the object and the edge of the ledge.
///  @param[in]		objectHeight	The height of the object.
/// @return The maximum angle before LOS gets blocked.
float rdCalcMaxLOSAngle(const float ledgeSpan, const float objectHeight);

/// Determines the amount we need to offset an object to maintain LOS from an angle, with a maximum.
///  @param[in]		ledgeSpan	The distance between the edge of the object and the edge of the ledge.
///  @param[in]		slopeAngle	The slope angle to test.
///  @param[in]		maxAngle	The maximum angle in degrees.
/// @return The amount we need to offset to maintain LOS.
float rdCalcLedgeSpanOffsetAmount(const float ledgeSpan, const float slopeAngle, const float maxAngle);

unsigned char rdClassifyPointOutsideBounds(const rdVec2D* pt, const rdVec2D* bmin, const rdVec2D* bmax);
unsigned char rdClassifyPointInsideBounds(const rdVec2D* pt, const rdVec2D* bmin, const rdVec2D* bmax);

/// Determines if the specified point is inside the axis-aligned bounding box.
///  @param[in]		pt		The point to check. [(x, y, z)]
///  @param[in]		bmin	Minimum bounds of the box. [(x, y, z)]
///  @param[in]		bmax	Maximum bounds of the box. [(x, y, z)]
/// @return True if the point is inside the axis-aligned bounding box.
bool rdPointInAABB(const rdVec3D* pt, const rdVec3D* bmin, const rdVec3D* bmax);

/// Determines if the specified point is inside the cylinder on the xy-plane.
///  @param[in]		pt		The point to check. [(x, y, z)]
///  @param[in]		pos		The position of the cylinder. [(x, y, z)]
///  @param[in]		radius	The radius of the cylinder.
///  @param[in]		height	The height of the cylinder.
/// @return True if the point is inside the cylinder.
bool rdPointInCylinder(const rdVec3D* pt, const rdVec3D* pos, const float radius, const float height);

/// Determines if the specified point is inside the convex polygon on the xy-plane.
///  @param[in]		pt		The point to check. [(x, y, z)]
///  @param[in]		verts	The polygon vertices. [(x, y, z) * @p nverts]
///  @param[in]		nverts	The number of vertices. [Limit: >= 3]
/// @return True if the point is inside the polygon.
bool rdPointInPolygon(const rdVec3D* pt, const rdVec3D* verts, const int nverts);

bool rdDistancePtPolyEdgesSqr(const rdVec3D* pt, const rdVec3D* verts, const int nverts,
							  float* ed, float* et);

float rdDistancePtSegSqr2D(const rdVec3D* pt, const rdVec3D* p, const rdVec3D* q, float& t);

/// Derives the centroid of a convex polygon.
///  @param[out]	tc		The centroid of the polygon. [(x, y, z)]
///  @param[in]		idx		The polygon indices. [(vertIndex) * @p nidx]
///  @param[in]		nidx	The number of indices in the polygon. [Limit: >= 3]
///  @param[in]		verts	The polygon vertices. [(x, y, z) * vertCount]
void rdCalcPolyCenter(rdVec3D* tc, const unsigned short* idx, int nidx, const rdVec3D* verts);

/// Determines if the two convex polygons overlap on the xy-plane.
///  @param[in]		polya		Polygon A vertices.	[(x, y, z) * @p npolya]
///  @param[in]		npolya		The number of vertices in polygon A.
///  @param[in]		polyb		Polygon B vertices.	[(x, y, z) * @p npolyb]
///  @param[in]		npolyb		The number of vertices in polygon B.
/// @return True if the two polygons overlap.
bool rdOverlapPolyPoly2D(const rdVec3D* polya, const int npolya,
						 const rdVec3D* polyb, const int npolyb);

/// @}
/// @name Miscellaneous functions.
/// @{

inline unsigned int rdNextPow2(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

inline unsigned int rdIlog2(unsigned int v)
{
	unsigned int r;
	unsigned int shift;
	r = (v > 0xffff) << 4; v >>= r;
	shift = (v > 0xff) << 3; v >>= shift; r |= shift;
	shift = (v > 0xf) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3) << 1; v >>= shift; r |= shift;
	r |= (v >> 1);
	return r;
}

inline int rdAlign4(int x) { return (x+3) & ~3; }

inline unsigned char rdWrapTileSide(const int side) { return side & 0x7; }
inline unsigned char rdOppositeTile(const int side) { return rdWrapTileSide(side+4); }

inline void rdSwapByte(unsigned char* a, unsigned char* b)
{
	unsigned char tmp = *a;
	*a = *b;
	*b = tmp;
}

inline void rdSwapEndian(unsigned short* v)
{
	unsigned char* x = (unsigned char*)v;
	rdSwapByte(x+0, x+1);
}

inline void rdSwapEndian(short* v)
{
	unsigned char* x = (unsigned char*)v;
	rdSwapByte(x+0, x+1);
}

inline void rdSwapEndian(unsigned int* v)
{
	unsigned char* x = (unsigned char*)v;
	rdSwapByte(x+0, x+3); rdSwapByte(x+1, x+2);
}

inline void rdSwapEndian(int* v)
{
	unsigned char* x = (unsigned char*)v;
	rdSwapByte(x+0, x+3); rdSwapByte(x+1, x+2);
}

inline void rdSwapEndian(float* v)
{
	unsigned char* x = (unsigned char*)v;
	rdSwapByte(x+0, x+3); rdSwapByte(x+1, x+2);
}

inline void rdSwapEndian(rdVec3D* v)
{
	for (int i = 0; i < 3; i++)
		rdSwapEndian(&v[i]);
}

void rdRandomPointInConvexPoly(const rdVec3D* pts, const int npts, float* areas,
							   const float s, const float t, rdVec3D* out);

/// Counts the number of vertices in the polygon.
///  @param[in]		p	The polygon.
///  @param[in]		nvp	The total number of verts per polygon.
/// @return The number of vertices in the polygon.
inline int rdCountPolyVerts(const unsigned short* p, const int nvp)
{
	for (int i = 0; i < nvp; ++i)
		if (p[i] == RD_MESH_NULL_IDX)
			return i;
	return nvp;
}

template<typename TypeToRetrieveAs>
TypeToRetrieveAs* rdGetThenAdvanceBufferPointer(const unsigned char*& buffer, const rdSizeType distanceToAdvance)
{
	TypeToRetrieveAs* returnPointer = reinterpret_cast<TypeToRetrieveAs*>(buffer);
	buffer += distanceToAdvance;
	return returnPointer;
}

template<typename TypeToRetrieveAs>
TypeToRetrieveAs* rdGetThenAdvanceBufferPointer(unsigned char*& buffer, const rdSizeType distanceToAdvance)
{
	TypeToRetrieveAs* returnPointer = reinterpret_cast<TypeToRetrieveAs*>(buffer);
	buffer += distanceToAdvance;
	return returnPointer;
}


/// @}

#endif // RECASTDETOURCOMMON_H

///////////////////////////////////////////////////////////////////////////

// This section contains detailed documentation for members that don't have
// a source file. It reduces clutter in the main section of the header.

/**

@fn float rdTriArea2D(const rdVec3D* a, const rdVec3D* b, const rdVec3D* c)
@par

The vertices are projected onto the xy-plane, so the z-values are ignored.

This is a low cost function than can be used for various purposes.  Its main purpose
is for point/line relationship testing.

In all cases: A value of zero indicates that all vertices are collinear or represent the same point.
(On the xy-plane.)

When used for point/line relationship tests, AB usually represents a line against which
the C point is to be tested.  In this case:

A positive value indicates that point C is to the left of line AB, looking from A toward B.<br/>
A negative value indicates that point C is to the right of lineAB, looking from A toward B.

When used for evaluating a triangle:

The absolute value of the return value is two times the area of the triangle when it is
projected onto the xy-plane.

A positive return value indicates:

<ul>
<li>The vertices are wrapped in the normal Detour wrap direction.</li>
<li>The triangle's 3D face normal is in the general up direction.</li>
</ul>

A negative return value indicates:

<ul>
<li>The vertices are reverse wrapped. (Wrapped opposite the normal Detour wrap direction.)</li>
<li>The triangle's 3D face normal is in the general down direction.</li>
</ul>

*/