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

#include "Shared/Include/SharedMath.h"
#include "Shared/Include/SharedCommon.h"

//////////////////////////////////////////////////////////////////////////////////////////

float rdCalcSlopeAngle(const rdVec3D* v1, const rdVec3D* v2)
{
	rdVec3D delta;
	rdVsub(&delta, v2, v1);

	const float horizontalDistance = rdMathSqrtf((delta.x*delta.x)+(delta.y*delta.y));
	const float slopeAngleRadians = rdMathAtan2f(delta.z, horizontalDistance);
	const float slopeAngleDegrees = rdRadToDeg(slopeAngleRadians);

	return slopeAngleDegrees;
}

void rdClosestPtPointTriangle(rdVec3D* closest, const rdVec3D* p,
							  const rdVec3D* a, const rdVec3D* b, const rdVec3D* c)
{
	// Check if P in vertex region outside A
	rdVec3D ab, ac, ap;
	rdVsub(&ab, b, a);
	rdVsub(&ac, c, a);
	rdVsub(&ap, p, a);
	const float d1 = rdVdot(&ab, &ap);
	const float d2 = rdVdot(&ac, &ap);
	if (d1 <= 0.0f && d2 <= 0.0f)
	{
		// barycentric coordinates (1,0,0)
		rdVcopy(closest, a);
		return;
	}
	
	// Check if P in vertex region outside B
	rdVec3D bp;
	rdVsub(&bp, p, b);
	const float d3 = rdVdot(&ab, &bp);
	const float d4 = rdVdot(&ac, &bp);
	if (d3 >= 0.0f && d4 <= d3)
	{
		// barycentric coordinates (0,1,0)
		rdVcopy(closest, b);
		return;
	}
	
	// Check if P in edge region of AB, if so return projection of P onto AB
	const float vc = d1*d4 - d3*d2;
	if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
	{
		// barycentric coordinates (1-v,v,0)
		float v = d1 / (d1 - d3);
		closest->x = a->x + v * ab.x;
		closest->y = a->y + v * ab.y;
		closest->z = a->z + v * ab.z;
		return;
	}
	
	// Check if P in vertex region outside C
	rdVec3D cp;
	rdVsub(&cp, p, c);
	const float d5 = rdVdot(&ab, &cp);
	const float d6 = rdVdot(&ac, &cp);
	if (d6 >= 0.0f && d5 <= d6)
	{
		// barycentric coordinates (0,0,1)
		rdVcopy(closest, c);
		return;
	}
	
	// Check if P in edge region of AC, if so return projection of P onto AC
	const float vb = d5*d2 - d1*d6;
	if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
	{
		// barycentric coordinates (1-w,0,w)
		float w = d2 / (d2 - d6);
		closest->x = a->x + w * ac.x;
		closest->y = a->y + w * ac.y;
		closest->z = a->z + w * ac.z;
		return;
	}
	
	// Check if P in edge region of BC, if so return projection of P onto BC
	const float va = d3*d6 - d5*d4;
	if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
	{
		// barycentric coordinates (0,1-w,w)
		float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		closest->x = b->x + w * (c->x - b->x);
		closest->y = b->y + w * (c->y - b->y);
		closest->z = b->z + w * (c->z - b->z);
		return;
	}
	
	// P inside face region. Compute Q through its barycentric coordinates (u,v,w)
	const float denom = 1.0f / (va + vb + vc);
	const float v = vb * denom;
	const float w = vc * denom;
	closest->x = a->x + ab.x * v + ac.x * w;
	closest->y = a->y + ab.y * v + ac.y * w;
	closest->z = a->z + ab.z * v + ac.z * w;
}

// note(amos): based on the Möller–Trumbore algorithm, see:
// https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
bool rdIntersectSegmentTriangle(const rdVec3D* sp, const rdVec3D* sq,
								const rdVec3D* a, const rdVec3D* b, const rdVec3D* c, float& t)
{
	rdVec3D ab, ac, qp;
	rdVsub(&ab, b, a);
	rdVsub(&ac, c, a);
	rdVsub(&qp, sq, sp);

	rdVec3D h;
	rdVcross(&h, &qp, &ac);

	const float d = rdVdot(&ab, &h);

	if (d > -RD_EPS && d < RD_EPS)
		return false; // Ray is parallel to the triangle plane

	rdVec3D s;
	rdVsub(&s, sp, a);

	const float id = 1.0f / d;
	const float u = rdVdot(&s, &h) * id;

	if (u < 0.0f || u > 1.0f)
		return false;

	rdVec3D q;
	rdVcross(&q, &s, &ab);

	const float v = rdVdot(&qp, &q) * id;

	if (v < 0.0f || u+v > 1.0f)
		return false;

	t = rdVdot(&ac, &q)*id;

	if (t < 0.0f || t > 1.0f)
		return false;

	// Segment/ray intersects triangle
	return true;
}

bool rdIntersectSegmentPoly2D(const rdVec3D* p0, const rdVec3D* p1,
							  const rdVec3D* verts, int nverts,
							  float& tmin, float& tmax,
							  int& segMin, int& segMax)
{
	tmin = 0;
	tmax = 1;
	segMin = -1;
	segMax = -1;
	
	rdVec3D dir;
	rdVsub(&dir, p1, p0);
	
	for (int i = 0, j = nverts-1; i < nverts; j=i++)
	{
		rdVec3D edge, diff;
		rdVsub(&edge, &verts[i], &verts[j]);
		rdVsub(&diff, p0, &verts[j]);
		const float n = rdVperp2D(&edge, &diff);
		const float d = rdVperp2D(&dir, &edge);
		if (rdMathFabsf(d) < RD_EPS)
		{
			// S is nearly parallel to this edge
			if (n < 0)
				return false;
			else
				continue;
		}
		const float t = n / d;
		if (d < 0)
		{
			// segment S is entering across this edge
			if (t > tmin)
			{
				tmin = t;
				segMin = j;
				// S enters after leaving polygon
				if (tmin > tmax)
					return false;
			}
		}
		else
		{
			// segment S is leaving across this edge
			if (t < tmax)
			{
				tmax = t;
				segMax = j;
				// S leaves before entering polygon
				if (tmax < tmin)
					return false;
			}
		}
	}
	
	return true;
}

bool rdIntersectSegmentAABB(const rdVec3D* sp, const rdVec3D* sq,
						 const rdVec3D* amin, const rdVec3D* amax,
						 float& tmin, float& tmax)
{
	rdVec3D d;
	rdVsub(&d, sq, sp);
	tmin = 0; // set to 0 to get first hit on line
	tmax = 1; // set to max distance ray can travel (for segment)
	
	// For all three slabs
	for (int i = 0; i < 3; i++)
	{
		if (rdMathFabsf(d[i]) < RD_EPS)
		{
			// Ray is parallel to slab. No hit if origin not within slab
			if ((*sp)[i] < (*amin)[i] || (*sp)[i] > (*amax)[i])
				return false;
		}
		else
		{
			// Compute intersection t value of ray with near and far plane of slab
			const float ood = 1.0f / d[i];
			float t1 = ((*amin)[i]-(*sp)[i]) * ood;
			float t2 = ((*amax)[i]-(*sp)[i]) * ood;
			// Make t1 be intersection with near plane, t2 with far plane
			if (t1 > t2) rdSwap(t1, t2);
			// Compute the intersection of slab intersections intervals
			if (t1 > tmin) tmin = t1;
			if (t2 < tmax) tmax = t2;
			// Exit with no collision as soon as slab intersection becomes empty
			if (tmin > tmax) return false;
		}
	}
	
	return true;
}

bool rdIntersectSegmentCylinder(const rdVec3D* sp, const rdVec3D* sq, const rdVec3D* position,
								const float radius, const float height,
								float& tmin, float& tmax)
{
	tmin = 0;
	tmax = 1;

	const float cx = position->x;
	const float cy = position->y;
	const float cz = position->z;
	const float topZ = cz + height;

	// Horizontal (x-y plane) intersection test with infinite cylinder
	const float dx = sq->x-sp->x;
	const float dy = sq->y-sp->y;

	const float px = sp->x-cx;
	const float py = sp->y-cy;

	const float a = dx*dx + dy*dy;
	const float b = 2.0f * (px*dx + py*dy);
	const float c = rdSqr(px) + rdSqr(py) - radius*radius;

	if (a > 0.0f)
	{
		// Discriminant for solving quadratic equation
		float disc = b*b - 4.0f * a*c;

		if (disc < RD_EPS)
			return false; // No intersection in the horizontal plane

		disc = rdMathSqrtf(disc);
		float t0 = (-b-disc) / (2.0f*a);
		float t1 = (-b+disc) / (2.0f*a);

		if (t0 > t1) rdSwap(t0, t1);

		tmin = rdMax(tmin, t0);
		tmax = rdMin(tmax, t1);

		if (tmin > tmax)
			return false; // No intersection in the [tmin, tmax] range
	}
	else
	{
		// There is no shift in the start and end point on the x-y plane,
		// ensure the starting point is within the radius of the cylinder
		// before checking for vertical intersection
		if (px*px + py*py > radius*radius) 
			return false;
	}

	// Vertical (z-axis) intersection test
	const float dz = sq->z-sp->z;

	if (dz != 0.0f)
	{
		float tCapMin = (cz-sp->z) / dz;
		float tCapMax = (topZ-sp->z) / dz;

		if (tCapMin > tCapMax) rdSwap(tCapMin, tCapMax);

		// Update tmin and tmax for cap intersections
		tmin = rdMax(tmin, tCapMin);
		tmax = rdMin(tmax, tCapMax);

		if (tmin > tmax)
			return false;
	}

	const float z0 = sp->z + tmin*dz;
	const float z1 = sp->z + tmax*dz;

	if ((z0 < cz && z1 < cz) || (z0 > topZ && z1 > topZ))
		return false; // No intersection with the vertical height of the cylinder

	return true;
}

bool rdIntersectSegmentConvexHull(const rdVec3D* sp, const rdVec3D* sq,
								  const rdVec3D* verts, const int nverts,
								  const float hmin, const float hmax,
								  float& tmin, float& tmax)
{
	int segMin, segMax;
	if (!rdIntersectSegmentPoly2D(sp, sq, verts, nverts, tmin, tmax, segMin, segMax))
		return false; // No intersection with the polygon base

	tmin = rdMax(0.0f, tmin);
	tmax = rdMin(1.0f, tmax);

	if (tmin > tmax)
		return false; // No valid intersection range

	// Vertical (z-axis) intersection test
	const float dz = sq->z-sp->z;

	if (dz != 0.0f)
	{
		float tCapMin = (hmin-sp->z) / dz;
		float tCapMax = (hmax-sp->z) / dz;

		if (tCapMin > tCapMax) rdSwap(tCapMin, tCapMax);

		// Update tmin and tmax for cap intersections
		tmin = rdMax(tmin, tCapMin);
		tmax = rdMin(tmax, tCapMax);

		if (tmin > tmax)
			return false;
	}

	const float z0 = sp->z + tmin*dz;
	const float z1 = sp->z + tmax*dz;

	if ((z0 < hmin && z1 < hmin) || (z0 > hmax && z1 > hmax))
		return false; // No intersection within the vertical bounds

	return true;
}

float rdDistancePtSegSqr2D(const rdVec3D* pt, const rdVec3D* p, const rdVec3D* q, float& t)
{
	const float pqx = q->x - p->x;
	const float pqy = q->y - p->y;
	float dx = pt->x - p->x;
	float dy = pt->y - p->y;
	const float d = rdSqr(pqx) + rdSqr(pqy);
	t = pqx*dx + pqy*dy;
	if (d > 0) t /= d;
	if (t < 0) t = 0;
	else if (t > 1) t = 1;
	dx = p->x + t*pqx - pt->x;
	dy = p->y + t*pqy - pt->y;
	return dx*dx + dy*dy;
}

void rdCalcPolyCenter(rdVec3D* tc, const unsigned short* idx, int nidx, const rdVec3D* verts)
{
	tc->x = 0.0f;
	tc->y = 0.0f;
	tc->z = 0.0f;
	for (int j = 0; j < nidx; ++j)
	{
		const rdVec3D* v = &verts[idx[j]];
		tc->x += v->x;
		tc->y += v->y;
		tc->z += v->z;
	}
	const float s = 1.0f / nidx;
	tc->x *= s;
	tc->y *= s;
	tc->z *= s;
}

bool rdClosestHeightPointTriangle(const rdVec3D* p, const rdVec3D* a, const rdVec3D* b, const rdVec3D* c, float& h)
{
	rdVec3D v0, v1, v2;

	rdVsub(&v0, c, a);
	rdVsub(&v1, b, a);
	rdVsub(&v2, p, a);

	// Compute scaled barycentric coordinates
	float denom = v0.x * v1.y - v0.y * v1.x;
	if (rdMathFabsf(denom) < RD_EPS)
		return false;

	float u = v1.y * v2.x - v1.x * v2.y;
	float v = v0.x * v2.y - v0.y * v2.x;

	if (denom < 0) {
		denom = -denom;
		u = -u;
		v = -v;
	}

	// If point lies inside the triangle, return interpolated zcoord.
	if (u >= 0.0f && v >= 0.0f && (u + v) <= denom) {
		h = a->z + (v0.z * u + v1.z * v) / denom;
		return true;
	}
	return false;
}

bool rdPointInAABB(const rdVec3D* pt, const rdVec3D* bmin, const rdVec3D* bmax)
{
	if (pt->x >= bmin->x && pt->x <= bmax->x &&
		pt->y >= bmin->y && pt->y <= bmax->y &&
		pt->z >= bmin->z && pt->z <= bmax->z)
	{
		return true;
	}

	return false;
}

bool rdPointInCylinder(const rdVec3D* pt, const rdVec3D* pos, const float radius, const float height)
{
	const float dx = pt->x - pos->x;
	const float dy = pt->y - pos->y;
	const float distSquared = rdSqr(dx) + rdSqr(dy);

	if (distSquared <= rdSqr(radius) &&
		pt->z >= pos->z && pt->z <= (pos->z + height))
	{
		return true;
	}

	return false;
}

/// @par
///
/// All points are projected onto the xy-plane, so the z-values are ignored.
bool rdPointInPolygon(const rdVec3D* pt, const rdVec3D* verts, const int nverts)
{
	// TODO: Replace pnpoly with triArea2D tests?
	int i, j;
	bool c = false;
	for (i = 0, j = nverts-1; i < nverts; j = i++)
	{
		const rdVec3D* vi = &verts[i];
		const rdVec3D* vj = &verts[j];
		if (((vi->y > pt->y) != (vj->y > pt->y)) &&
			(pt->x < (vj->x-vi->x) * (pt->y-vi->y) / (vj->y-vi->y) + vi->x) )
			c = !c;
	}
	return c;
}

bool rdDistancePtPolyEdgesSqr(const rdVec3D* pt, const rdVec3D* verts, const int nverts,
							  float* ed, float* et)
{
	// TODO: Replace pnpoly with triArea2D tests?
	int i, j;
	bool c = false;
	for (i = 0, j = nverts-1; i < nverts; j = i++)
	{
		const rdVec3D* vi = &verts[i];
		const rdVec3D* vj = &verts[j];
		if (((vi->y > pt->y) != (vj->y > pt->y)) &&
			(pt->x < (vj->x-vi->x) * (pt->y-vi->y) / (vj->y-vi->y) + vi->x) )
			c = !c;
		ed[j] = rdDistancePtSegSqr2D(pt, vj, vi, et[j]);
	}
	return c;
}

static void projectPoly(const rdVec2D* axis, const rdVec3D* poly, const int npoly,
						float& rmin, float& rmax)
{
	rmin = rmax = rdVdot2D(axis, &poly[0]);
	for (int i = 1; i < npoly; ++i)
	{
		const float d = rdVdot2D(axis, &poly[i]);
		rmin = rdMin(rmin, d);
		rmax = rdMax(rmax, d);
	}
}

inline bool overlapRange(const float amin, const float amax,
						 const float bmin, const float bmax,
						 const float eps)
{
	return ((amin+eps) > bmax || (amax-eps) < bmin) ? false : true;
}

/// @par
///
/// All vertices are projected onto the xy-plane, so the z-values are ignored.
bool rdOverlapPolyPoly2D(const rdVec3D* polya, const int npolya,
						 const rdVec3D* polyb, const int npolyb)
{
	const float eps = 1e-4f;
	
	for (int i = 0, j = npolya-1; i < npolya; j=i++)
	{
		const rdVec3D* va = &polya[j];
		const rdVec3D* vb = &polya[i];
		const rdVec2D n(-(vb->y-va->y), vb->x-va->x);
		float amin,amax,bmin,bmax;
		projectPoly(&n, polya, npolya, amin,amax);
		projectPoly(&n, polyb, npolyb, bmin,bmax);
		if (!overlapRange(amin,amax, bmin,bmax, eps))
		{
			// Found separating axis
			return false;
		}
	}
	for (int i = 0, j = npolyb-1; i < npolyb; j=i++)
	{
		const rdVec3D* va = &polyb[j];
		const rdVec3D* vb = &polyb[i];
		const rdVec2D n(-(vb->y-va->y), vb->x-va->x);
		float amin,amax,bmin,bmax;
		projectPoly(&n, polya, npolya, amin,amax);
		projectPoly(&n, polyb, npolyb, bmin,bmax);
		if (!overlapRange(amin,amax, bmin,bmax, eps))
		{
			// Found separating axis
			return false;
		}
	}
	return true;
}

// Returns a random point in a convex polygon.
// Adapted from Graphics Gems article.
void rdRandomPointInConvexPoly(const rdVec3D* pts, const int npts, float* areas,
							   const float s, const float t, rdVec3D* out)
{
	// Calc triangle areas
	float areasum = 0.0f;
	for (int i = 2; i < npts; i++) {
		areas[i] = rdTriArea2D(pts, &pts[i], &pts[(i-1)]);
		areasum += rdMax(0.001f, areas[i]);
	}
	// Find sub triangle weighted by area.
	const float thr = s*areasum;
	float acc = 0.0f;
	float u = 1.0f;
	int tri = npts - 1;
	for (int i = 2; i < npts; i++) {
		const float dacc = areas[i];
		if (thr >= acc && thr < (acc+dacc))
		{
			u = (thr - acc) / dacc;
			tri = i;
			break;
		}
		acc += dacc;
	}
	
	const float v = rdMathSqrtf(t);
	
	const float a = 1 - v;
	const float b = (1 - u) * v;
	const float c = u * v;

	const rdVec3D* pa = pts;
	const rdVec3D* pb = &pts[tri];
	const rdVec3D* pc = &pts[tri-1];
	
	out->x = a*pa->x + b*pb->x + c*pc->x;
	out->y = a*pa->y + b*pb->y + c*pc->y;
	out->z = a*pa->z + b*pb->z + c*pc->z;
}

bool rdIntersectSegSeg2D(const rdVec2D* ap, const rdVec2D* aq,
						 const rdVec2D* bp, const rdVec2D* bq,
						 float& s, float& t)
{
	rdVec2D u, v, w;
	rdVsub2D(&u,aq,ap);
	rdVsub2D(&v,bq,bp);
	rdVsub2D(&w,ap,bp);
	float d = rdVperp2D(&u,&v);
	if (rdMathFabsf(d) < RD_EPS) return false;
	s = rdVperp2D(&v,&w) / d;
	t = rdVperp2D(&u,&w) / d;
	return true;
}

float rdDistancePtLine2D(const rdVec2D* pt, const rdVec2D* p, const rdVec2D* q)
{
	const float pqx = q->x - p->x;
	const float pqy = q->y - p->y;
	float dx = pt->x - p->x;
	float dy = pt->y - p->y;
	const float d = rdSqr(pqx) + rdSqr(pqy);
	float t = pqx * dx + pqy * dy;
	if (d != 0) t /= d;
	dx = p->x + t * pqx - pt->x;
	dy = p->y + t * pqy - pt->y;
	return rdSqr(dx) + rdSqr(dy);
}

void rdCalcEdgeNormal2D(const rdVec2D* dir, rdVec2D* out)
{
	out->x = dir->y;
	out->y = -dir->x;
	rdVnormalize2D(out);
}

void rdCalcEdgeNormalPt2D(const rdVec2D* v1, const rdVec2D* v2, rdVec2D* out)
{
	rdVec2D dir;
	rdVsub2D(&dir, v2, v1);
	rdCalcEdgeNormal2D(&dir, out);
}

void rdCalcSubEdgeArea2D(const rdVec2D* edgeStart, const rdVec2D* edgeEnd, const rdVec2D* subEdgeStart,
	const rdVec2D* subEdgeEnd, float& tmin, float& tmax)
{
	const float edgeLen = rdVdist2D(edgeStart, edgeEnd);
	const float subEdgeStartDist = rdVdist2D(edgeStart, subEdgeStart);
	const float subEdgeEndDist = rdVdist2D(edgeStart, subEdgeEnd);

	tmax = subEdgeStartDist / edgeLen;
	tmin = subEdgeEndDist / edgeLen;

	// Can happen when the sub edge equals the main edge.
	if (tmin > tmax)
		rdSwap(tmin, tmax);
}

float rdCalcEdgeOverlap2D(const rdVec2D* edge1Start, const rdVec2D* edge1End,
	const rdVec2D* edge2Start, const rdVec2D* edge2End, const rdVec2D* targetEdgeVec)
{
	float min1 = rdVproj2D(edge1Start, targetEdgeVec);
	float max1 = rdVproj2D(edge1End, targetEdgeVec);

	if (min1 > max1)
		rdSwap(min1, max1);

	float min2 = rdVproj2D(edge2Start, targetEdgeVec);
	float max2 = rdVproj2D(edge2End, targetEdgeVec);

	if (min2 > max2)
		rdSwap(min2, max2);

	const float start = rdMax(min1, min2);
	const float end = rdMin(max1, max2);

	return rdMax(0.0f, end - start);
}

float rdCalcMaxLOSAngle(const float ledgeSpan, const float objectHeight)
{
	const float angleRad = rdMathAtan2f(objectHeight, ledgeSpan);
	const float angleDeg = rdRadToDeg(angleRad);

	return angleDeg;
}

float rdCalcLedgeSpanOffsetAmount(const float ledgeSpan, const float slopeAngle, const float maxAngle)
{
	const float clampedAngle = rdClamp(slopeAngle, slopeAngle, maxAngle);
	const float offset = ledgeSpan * (clampedAngle / maxAngle);

	return offset;
}

static const unsigned char XP = 1 << 0;
static const unsigned char YP = 1 << 1;
static const unsigned char XM = 1 << 2;
static const unsigned char YM = 1 << 3;

unsigned char rdClassifyPointOutsideBounds(const rdVec2D* pt, const rdVec2D* bmin, const rdVec2D* bmax)
{
	unsigned char outcode = 0; 
	outcode |= (pt->x >= bmax->x) ? XM : 0;
	outcode |= (pt->y >= bmax->y) ? YP : 0;
	outcode |= (pt->x < bmin->x)  ? XP : 0;
	outcode |= (pt->y < bmin->y)  ? YM : 0;

	switch (outcode)
	{
	case XP: return 0;
	case XP|YP: return 1;
	case YP: return 2;
	case XM|YP: return 3;
	case XM: return 4;
	case XM|YM: return 5;
	case YM: return 6;
	case XP|YM: return 7;
	};

	return 0xff;
}

unsigned char rdClassifyPointInsideBounds(const rdVec2D* pt, const rdVec2D* bmin, const rdVec2D* bmax)
{
	const rdVec2D center((bmin->x+bmax->x) * 0.5f, (bmin->y+bmax->y) * 0.5f);
	const rdVec2D boxSize(bmax->x - bmin->x, bmax->y - bmin->y);

	rdVec2D dir(pt->x-center.x, pt->y-center.y);
	const float len = rdMathSqrtf(dir.x*dir.x + dir.y*dir.y);

	if (len > 0)
	{
		dir.x /= len;
		dir.y /= len;
	}

	const rdVec2D newPt(center.x+dir.x * boxSize.x, center.y+dir.y * boxSize.y);
	return rdClassifyPointOutsideBounds(&newPt, bmin, bmax);
}
