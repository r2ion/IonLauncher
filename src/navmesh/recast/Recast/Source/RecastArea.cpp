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

#include "Recast/Include/Recast.h"
#include "Shared/Include/SharedAlloc.h"
#include "Shared/Include/SharedAssert.h"

/// @par 
/// 
/// Basically, any spans that are closer to a boundary or obstruction than the specified radius 
/// are marked as unwalkable.
///
/// This method is usually called immediately after the heightfield has been built.
///
/// @see rcCompactHeightfield, rcBuildCompactHeightfield, rcConfig::walkableRadius
bool rcErodeWalkableArea(rcContext* ctx, int radius, rcCompactHeightfield& chf)
{
	rdAssert(ctx);
	
	const int w = chf.width;
	const int h = chf.height;
	
	rcScopedTimer timer(ctx, RC_TIMER_ERODE_AREA);
	
	unsigned char* dist = (unsigned char*)rdAlloc(sizeof(unsigned char)*chf.spanCount, RD_ALLOC_TEMP);
	if (!dist)
	{
		ctx->log(RC_LOG_ERROR, "erodeWalkableArea: Out of memory 'dist' (%d).", chf.spanCount);
		return false;
	}
	
	// Init distance.
	memset(dist, 0xff, sizeof(unsigned char)*chf.spanCount);
	
	// Mark boundary cells.
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				if (chf.areas[i] == RC_NULL_AREA)
				{
					dist[i] = 0;
				}
				else
				{
					const rcCompactSpan& s = chf.spans[i];
					int nc = 0;
					for (int dir = 0; dir < 4; ++dir)
					{
						if (rcGetCon(s, dir) != RC_NOT_CONNECTED)
						{
							const int nx = x + rcGetDirOffsetX(dir);
							const int ny = y + rcGetDirOffsetY(dir);
							const int nidx = (int)chf.cells[nx+ny*w].index + rcGetCon(s, dir);
							if (chf.areas[nidx] != RC_NULL_AREA)
							{
								nc++;
							}
						}
					}
					// At least one missing neighbour.
					if (nc != 4)
						dist[i] = 0;
				}
			}
		}
	}
	
	unsigned char nd;
	
	// Pass 1
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				const rcCompactSpan& s = chf.spans[i];
				
				if (rcGetCon(s, 0) != RC_NOT_CONNECTED)
				{
					// (-1,0)
					const int ax = x + rcGetDirOffsetX(0);
					const int ay = y + rcGetDirOffsetY(0);
					const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, 0);
					const rcCompactSpan& as = chf.spans[ai];
					nd = (unsigned char)rdMin((int)dist[ai]+2, 255);
					if (nd < dist[i])
						dist[i] = nd;
					
					// (-1,-1)
					if (rcGetCon(as, 3) != RC_NOT_CONNECTED)
					{
						const int aax = ax + rcGetDirOffsetX(3);
						const int aay = ay + rcGetDirOffsetY(3);
						const int aai = (int)chf.cells[aax+aay*w].index + rcGetCon(as, 3);
						nd = (unsigned char)rdMin((int)dist[aai]+3, 255);
						if (nd < dist[i])
							dist[i] = nd;
					}
				}
				if (rcGetCon(s, 3) != RC_NOT_CONNECTED)
				{
					// (0,-1)
					const int ax = x + rcGetDirOffsetX(3);
					const int ay = y + rcGetDirOffsetY(3);
					const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, 3);
					const rcCompactSpan& as = chf.spans[ai];
					nd = (unsigned char)rdMin((int)dist[ai]+2, 255);
					if (nd < dist[i])
						dist[i] = nd;
					
					// (1,-1)
					if (rcGetCon(as, 2) != RC_NOT_CONNECTED)
					{
						const int aax = ax + rcGetDirOffsetX(2);
						const int aay = ay + rcGetDirOffsetY(2);
						const int aai = (int)chf.cells[aax+aay*w].index + rcGetCon(as, 2);
						nd = (unsigned char)rdMin((int)dist[aai]+3, 255);
						if (nd < dist[i])
							dist[i] = nd;
					}
				}
			}
		}
	}
	
	// Pass 2
	for (int y = h-1; y >= 0; --y)
	{
		for (int x = w-1; x >= 0; --x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				const rcCompactSpan& s = chf.spans[i];
				
				if (rcGetCon(s, 2) != RC_NOT_CONNECTED)
				{
					// (1,0)
					const int ax = x + rcGetDirOffsetX(2);
					const int ay = y + rcGetDirOffsetY(2);
					const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, 2);
					const rcCompactSpan& as = chf.spans[ai];
					nd = (unsigned char)rdMin((int)dist[ai]+2, 255);
					if (nd < dist[i])
						dist[i] = nd;
					
					// (1,1)
					if (rcGetCon(as, 1) != RC_NOT_CONNECTED)
					{
						const int aax = ax + rcGetDirOffsetX(1);
						const int aay = ay + rcGetDirOffsetY(1);
						const int aai = (int)chf.cells[aax+aay*w].index + rcGetCon(as, 1);
						nd = (unsigned char)rdMin((int)dist[aai]+3, 255);
						if (nd < dist[i])
							dist[i] = nd;
					}
				}
				if (rcGetCon(s, 1) != RC_NOT_CONNECTED)
				{
					// (0,1)
					const int ax = x + rcGetDirOffsetX(1);
					const int ay = y + rcGetDirOffsetY(1);
					const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, 1);
					const rcCompactSpan& as = chf.spans[ai];
					nd = (unsigned char)rdMin((int)dist[ai]+2, 255);
					if (nd < dist[i])
						dist[i] = nd;
					
					// (-1,1)
					if (rcGetCon(as, 0) != RC_NOT_CONNECTED)
					{
						const int aax = ax + rcGetDirOffsetX(0);
						const int aay = ay + rcGetDirOffsetY(0);
						const int aai = (int)chf.cells[aax+aay*w].index + rcGetCon(as, 0);
						nd = (unsigned char)rdMin((int)dist[aai]+3, 255);
						if (nd < dist[i])
							dist[i] = nd;
					}
				}
			}
		}
	}
	
	const unsigned char thr = (unsigned char)(radius*2);
	for (int i = 0; i < chf.spanCount; ++i)
		if (dist[i] < thr)
			chf.areas[i] = RC_NULL_AREA;
	
	rdFree(dist);
	
	return true;
}

static void insertSort(unsigned char* a, const int n)
{
	int i, j;
	for (i = 1; i < n; i++)
	{
		const unsigned char value = a[i];
		for (j = i - 1; j >= 0 && a[j] > value; j--)
			a[j+1] = a[j];
		a[j+1] = value;
	}
}

/// @par
///
/// This filter is usually applied after applying area id's using functions
/// such as #rcMarkBoxArea, #rcMarkConvexPolyArea, and #rcMarkCylinderArea.
/// 
/// @see rcCompactHeightfield
bool rcMedianFilterWalkableArea(rcContext* ctx, rcCompactHeightfield& chf)
{
	rdAssert(ctx);
	
	const int w = chf.width;
	const int h = chf.height;
	
	rcScopedTimer timer(ctx, RC_TIMER_MEDIAN_AREA);
	
	unsigned char* areas = (unsigned char*)rdAlloc(sizeof(unsigned char)*chf.spanCount, RD_ALLOC_TEMP);
	if (!areas)
	{
		ctx->log(RC_LOG_ERROR, "medianFilterWalkableArea: Out of memory 'areas' (%d).", chf.spanCount);
		return false;
	}
	
	// Init distance.
	memset(areas, 0xff, sizeof(unsigned char)*chf.spanCount);
	
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				const rcCompactSpan& s = chf.spans[i];
				if (chf.areas[i] == RC_NULL_AREA)
				{
					areas[i] = chf.areas[i];
					continue;
				}
				
				unsigned char nei[9];
				for (int j = 0; j < 9; ++j)
					nei[j] = chf.areas[i];
				
				for (int dir = 0; dir < 4; ++dir)
				{
					if (rcGetCon(s, dir) != RC_NOT_CONNECTED)
					{
						const int ax = x + rcGetDirOffsetX(dir);
						const int ay = y + rcGetDirOffsetY(dir);
						const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, dir);
						if (chf.areas[ai] != RC_NULL_AREA)
							nei[dir*2+0] = chf.areas[ai];
						
						const rcCompactSpan& as = chf.spans[ai];
						const int dir2 = (dir+1) & 0x3;
						if (rcGetCon(as, dir2) != RC_NOT_CONNECTED)
						{
							const int ax2 = ax + rcGetDirOffsetX(dir2);
							const int ay2 = ay + rcGetDirOffsetY(dir2);
							const int ai2 = (int)chf.cells[ax2+ay2*w].index + rcGetCon(as, dir2);
							if (chf.areas[ai2] != RC_NULL_AREA)
								nei[dir*2+1] = chf.areas[ai2];
						}
					}
				}
				insertSort(nei, 9);
				areas[i] = nei[4];
			}
		}
	}
	
	memcpy(chf.areas, areas, sizeof(unsigned char)*chf.spanCount);
	
	rdFree(areas);
	
	return true;
}

int rcOffsetPoly(const rdVec3D* verts, const int nverts, const float offset,
				 rdVec3D* outVerts, const int maxOutVerts)
{
	const float	MITER_LIMIT = 1.20f;

	int n = 0;

	for (int i = 0; i < nverts; i++)
	{
		const int a = (i+nverts-1) % nverts;
		const int b = i;
		const int c = (i+1) % nverts;
		const rdVec3D* va = &verts[a];
		const rdVec3D* vb = &verts[b];
		const rdVec3D* vc = &verts[c];
		float dx0 = vb->x - va->x;
		float dy0 = vb->y - va->y;
		float d0 = dx0*dx0 + dy0*dy0;
		if (d0 > RD_EPS)
		{
			d0 = 1.0f/rdMathSqrtf(d0);
			dx0 *= d0;
			dy0 *= d0;
		}
		float dx1 = vc->x - vb->x;
		float dy1 = vc->y - vb->y;
		float d1 = dx1*dx1 + dy1*dy1;
		if (d1 > RD_EPS)
		{
			d1 = 1.0f/rdMathSqrtf(d1);
			dx1 *= d1;
			dy1 *= d1;
		}
		const float dlx0 = -dy0;
		const float dly0 = dx0;
		const float dlx1 = -dy1;
		const float dly1 = dx1;
		float cross = dx1*dy0 - dx0*dy1;
		float dmx = (dlx0 + dlx1) * 0.5f;
		float dmy = (dly0 + dly1) * 0.5f;
		float dmr2 = dmx*dmx + dmy*dmy;
		bool bevel = dmr2 * MITER_LIMIT*MITER_LIMIT < 1.0f;
		if (dmr2 > RD_EPS)
		{
			const float scale = 1.0f / dmr2;
			dmx *= scale;
			dmy *= scale;
		}

		if (bevel && cross < 0.0f)
		{
			if (n+2 > maxOutVerts)
				return 0;
			float d = (1.0f - (dx0*dx1 + dy0*dy1))*0.5f;
			outVerts[n].x = vb->x + (-dlx0+dx0*d)*offset;
			outVerts[n].y = vb->y + (-dly0+dy0*d)*offset;
			outVerts[n].z = vb->z;
			n++;
			outVerts[n].x = vb->x + (-dlx1-dx1*d)*offset;
			outVerts[n].y = vb->y + (-dly1-dy1*d)*offset;
			outVerts[n].z = vb->z;
			n++;
		}
		else
		{
			if (n+1 > maxOutVerts)
				return 0;
			outVerts[n].x = vb->x - dmx*offset;
			outVerts[n].y = vb->y - dmy*offset;
			outVerts[n].z = vb->z;
			n++;
		}
	}
	
	return n;
}

/// @par
///
/// The value of spacial parameters are in world units.
/// 
/// @see rcCompactHeightfield, rcMedianFilterWalkableArea
void rcMarkBoxArea(rcContext* ctx, const rdVec3D* bmin, const rdVec3D* bmax,
				   unsigned short flags, unsigned char areaId,
				   rcCompactHeightfield& chf)
{
	rdAssert(ctx);
	
	rcScopedTimer timer(ctx, RC_TIMER_MARK_BOX_AREA);

	int minx = (int)((bmin->x-chf.bmin.x)/chf.cs);
	int miny = (int)((bmin->y-chf.bmin.y)/chf.cs);
	int minz = (int)((bmin->z-chf.bmin.z)/chf.ch);
	int maxx = (int)((bmax->x-chf.bmin.x)/chf.cs);
	int maxy = (int)((bmax->y-chf.bmin.y)/chf.cs);
	int maxz = (int)((bmax->z-chf.bmin.z)/chf.ch);
	
	if (maxx < 0) return;
	if (minx >= chf.width) return;
	if (maxy < 0) return;
	if (miny >= chf.height) return;

	if (minx < 0) minx = 0;
	if (maxx >= chf.width) maxx = chf.width-1;
	if (miny < 0) miny = 0;
	if (maxy >= chf.height) maxy = chf.height-1;
	
	for (int y = miny; y <= maxy; ++y)
	{
		for (int x = minx; x <= maxx; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*chf.width];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				rcCompactSpan& s = chf.spans[i];
				if ((int)s.z >= minz && (int)s.z <= maxz)
				{
					if (chf.areas[i] != RC_NULL_AREA)
					{
						chf.flags[i] = flags;
						chf.areas[i] = areaId;
					}
				}
			}
		}
	}
}

/// @par
///
/// The value of spacial parameters are in world units.
/// 
/// The z-values of the polygon vertices are ignored. So the polygon is effectively 
/// projected onto the xy-plane at @p hmin, then extruded to @p hmax.
/// 
/// @see rcCompactHeightfield, rcMedianFilterWalkableArea
void rcMarkConvexPolyArea(rcContext* ctx, const rdVec3D* verts, const int nverts,
						  const float hmin, const float hmax,
						  unsigned short flags, unsigned char areaId,
						  rcCompactHeightfield& chf)
{
	rdAssert(ctx);
	
	rcScopedTimer timer(ctx, RC_TIMER_MARK_CONVEXPOLY_AREA);

	rdVec3D bmin(verts);
	rdVec3D bmax(verts);
	for (int i = 1; i < nverts; ++i)
	{
		rdVmin(&bmin, &verts[i]);
		rdVmax(&bmax, &verts[i]);
	}
	bmin.z = hmin;
	bmax.z = hmax;

	int minx = (int)((bmin.x-chf.bmin.x)/chf.cs);
	int miny = (int)((bmin.y-chf.bmin.y)/chf.cs);
	int minz = (int)((bmin.z-chf.bmin.z)/chf.ch);
	int maxx = (int)((bmax.x-chf.bmin.x)/chf.cs);
	int maxy = (int)((bmax.y-chf.bmin.y)/chf.cs);
	int maxz = (int)((bmax.z-chf.bmin.z)/chf.ch);
	
	if (maxx < 0) return;
	if (minx >= chf.width) return;
	if (maxy < 0) return;
	if (miny >= chf.height) return;
	
	if (minx < 0) minx = 0;
	if (maxx >= chf.width) maxx = chf.width-1;
	if (miny < 0) miny = 0;
	if (maxy >= chf.height) maxy = chf.height-1;
	
	
	// TODO: Optimize.
	for (int y = miny; y <= maxy; ++y)
	{
		for (int x = minx; x <= maxx; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*chf.width];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				rcCompactSpan& s = chf.spans[i];
				if (chf.areas[i] == RC_NULL_AREA)
					continue;
				if ((int)s.z >= minz && (int)s.z <= maxz)
				{
					rdVec3D p;
					p.x = chf.bmin.x + (x+0.5f)*chf.cs;
					p.y = chf.bmin.y + (y+0.5f)*chf.cs;
					p.z = 0; 

					if (rdPointInPolygon(&p, verts, nverts))
					{
						chf.flags[i] = flags;
						chf.areas[i] = areaId;
					}
				}
			}
		}
	}
}


/// @par
///
/// The value of spacial parameters are in world units.
/// 
/// @see rcCompactHeightfield, rcMedianFilterWalkableArea
void rcMarkCylinderArea(rcContext* ctx, const rdVec3D* pos,
						const float r, const float h, 
						unsigned short flags, unsigned char areaId,
						rcCompactHeightfield& chf)
{
	rdAssert(ctx);
	
	rcScopedTimer timer(ctx, RC_TIMER_MARK_CYLINDER_AREA);
	
	rdVec3D bmin, bmax;
	bmin.x = pos->x - r;
	bmin.y = pos->y - r;
	bmin.z = pos->z;
	bmax.x = pos->x + r;
	bmax.y = pos->y + r;
	bmax.z = pos->z + h;
	const float r2 = r*r;
	
	int minx = (int)((bmin.x-chf.bmin.x)/chf.cs);
	int miny = (int)((bmin.y-chf.bmin.y)/chf.cs);
	int minz = (int)((bmin.z-chf.bmin.z)/chf.ch);
	int maxx = (int)((bmax.x-chf.bmin.x)/chf.cs);
	int maxy = (int)((bmax.y-chf.bmin.y)/chf.cs);
	int maxz = (int)((bmax.z-chf.bmin.z)/chf.ch);
	
	if (maxx < 0) return;
	if (minx >= chf.width) return;
	if (maxy < 0) return;
	if (miny >= chf.height) return;
	
	if (minx < 0) minx = 0;
	if (maxx >= chf.width) maxx = chf.width-1;
	if (miny < 0) miny = 0;
	if (maxy >= chf.height) maxy = chf.height-1;
	
	
	for (int y = miny; y <= maxy; ++y)
	{
		for (int x = minx; x <= maxx; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*chf.width];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				rcCompactSpan& s = chf.spans[i];
				
				if (chf.areas[i] == RC_NULL_AREA)
					continue;
				
				if ((int)s.z >= minz && (int)s.z <= maxz)
				{
					const float sx = chf.bmin.x + (x+0.5f)*chf.cs; 
					const float sy = chf.bmin.y + (y+0.5f)*chf.cs; 
					const float dx = sx - pos->x;
					const float dy = sy - pos->y;
					
					if (dx*dx + dy*dy < r2)
					{
						chf.flags[i] = flags;
						chf.areas[i] = areaId;
					}
				}
			}
		}
	}
}
