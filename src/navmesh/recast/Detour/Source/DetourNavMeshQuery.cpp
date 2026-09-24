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
#include "Detour/Include/DetourNavMeshQuery.h"
#include "Detour/Include/DetourNavMesh.h"
#include "Detour/Include/DetourNode.h"
#include "Shared/Include/SharedCommon.h"
#include "Shared/Include/SharedAlloc.h"
#include "Shared/Include/SharedAssert.h"

/// @class dtQueryFilter
///
/// <b>The Default Implementation</b>
/// 
/// At construction: All area costs default to 1.0.  All flags are included
/// and none are excluded.
/// 
/// If a polygon has both an include and an exclude flag, it will be excluded.
/// 
/// The way filtering works, a navigation mesh polygon must have at least one flag 
/// set to ever be considered by a query. So a polygon with no flags will never
/// be considered.
///
/// Setting the include flags to 0 will result in all polygons being excluded.
///
/// <b>Custom Implementations</b>
/// 
/// DT_VIRTUAL_QUERYFILTER must be defined in order to extend this class.
/// 
/// Implement a custom query filter by overriding the virtual passFilter(),
/// traverseFilter() and getCost() functions. If this is done, all three
/// functions should be as fast as possible. Use cached local copies of data
/// rather than accessing your own objects where possible.
/// 
/// Custom implementations do not need to adhere to the flags or cost logic 
/// used by the default implementation.  
/// 
/// In order for A* searches to work properly, the cost should be proportional to
/// the travel distance. Implementing a cost modifier less than 1.0 is likely 
/// to lead to problems during pathfinding.
///
/// @see dtNavMeshQuery

dtQueryFilter::dtQueryFilter() :
	m_includeFlags(0xffff),
	m_excludeFlags(0),
	m_traverseFlags(0)
{
	resetTraverseCosts();
}

void dtQueryFilter::resetTraverseCosts()
{
	for (int i = 0; i < DT_MAX_TRAVERSE_TYPES; ++i)
		m_traverseCost[i] = 1.0f;
}

#ifndef DT_VIRTUAL_QUERYFILTER
rdForceInline
#endif
bool dtQueryFilter::passFilter(const dtPolyRef /*ref*/,
									  const dtMeshTile* /*tile*/,
									  const dtPoly* poly) const
{
	return (poly->flags & m_includeFlags) != 0 && (poly->flags & m_excludeFlags) == 0;
}

#ifndef DT_VIRTUAL_QUERYFILTER
rdForceInline
#endif
bool dtQueryFilter::traverseFilter(const dtLink* link,
	const dtMeshTile* /*tile*/,
	const dtPoly* /*poly*/) const
{
	if (link->hasTraverseType())
	{
		if (!(rdBitCellBit(link->getTraverseType()) & m_traverseFlags))
			return false;
	}

	return true;
}

#ifndef DT_VIRTUAL_QUERYFILTER
rdForceInline
#endif
// NOTE: if you wish to uncomment the currently commented parameters, make sure to look around the code for
// DT_VIRTUAL_QUERYFILTER directives as some features have been disabled for non-virtual query filters.
float dtQueryFilter::getCost(const rdVec3D* pa, const rdVec3D* pb, const dtLink* link,
									const dtPolyRef /*prevRef*/, const dtMeshTile* /*prevTile*/, const dtPoly* /*prevPoly*/,
									const dtPolyRef /*curRef*/, const dtMeshTile* /*curTile*/, const dtPoly* /*curPoly*/,
									const dtPolyRef /*nextRef*/, const dtMeshTile* /*nextTile*/, const dtPoly* /*nextPoly*/) const
{
	if (link && link->hasTraverseType())
		return m_traverseCost[link->getTraverseType()];

	return rdVdist(pa, pb);
}
	
static const float H_SCALE = 0.999f; // Search heuristic scale.


dtNavMeshQuery* dtAllocNavMeshQuery()
{
	void* mem = rdAlloc(sizeof(dtNavMeshQuery), RD_ALLOC_PERM);
	if (!mem) return 0;
	return new(mem) dtNavMeshQuery;
}

void dtFreeNavMeshQuery(dtNavMeshQuery* navmesh)
{
	if (!navmesh) return;
	navmesh->~dtNavMeshQuery();
	rdFree(navmesh);
}

//////////////////////////////////////////////////////////////////////////////////////////

/// @class dtNavMeshQuery
///
/// For methods that support undersized buffers, if the buffer is too small 
/// to hold the entire result set the return status of the method will include 
/// the #DT_BUFFER_TOO_SMALL flag.
///
/// Constant member functions can be used by multiple clients without side
/// effects. (E.g. No change to the closed list. No impact on an in-progress
/// sliced path query. Etc.)
/// 
/// Walls and portals: A @e wall is a polygon segment that is 
/// considered impassable. A @e portal is a passable segment between polygons.
/// A portal may be treated as a wall based on the dtQueryFilter used for a query.
///
/// @see dtNavMesh, dtQueryFilter, #dtAllocNavMeshQuery(), #dtAllocNavMeshQuery()

dtNavMeshQuery::dtNavMeshQuery() :
	m_nav(0),
	m_tinyNodePool(0),
	m_nodePool(0),
	m_openList(0)
{
	memset(&m_query, 0, sizeof(dtQueryData));
}

dtNavMeshQuery::~dtNavMeshQuery()
{
	if (m_tinyNodePool)
		m_tinyNodePool->~dtNodePool();
	if (m_nodePool)
		m_nodePool->~dtNodePool();
	if (m_openList)
		m_openList->~dtNodeQueue();

	rdFree(m_tinyNodePool);
	rdFree(m_nodePool);
	rdFree(m_openList);
}

/// @par 
///
/// Must be the first function called after construction, before other
/// functions are used.
///
/// This function can be used multiple times.
dtStatus dtNavMeshQuery::init(const dtNavMesh* nav, const int maxNodes)
{
	if (maxNodes > DT_NULL_IDX || maxNodes > (1 << DT_NODE_PARENT_BITS) - 1)
		return DT_FAILURE | DT_INVALID_PARAM;

	m_nav = nav;
	
	if (!m_nodePool || m_nodePool->getMaxNodes() < maxNodes)
	{
		if (m_nodePool)
		{
			m_nodePool->~dtNodePool();
			rdFree(m_nodePool);
			m_nodePool = 0;
		}
		m_nodePool = new (rdAlloc(sizeof(dtNodePool), RD_ALLOC_PERM)) dtNodePool(maxNodes, rdNextPow2(maxNodes/4));
		if (!m_nodePool)
			return DT_FAILURE | DT_OUT_OF_MEMORY;
	}
	else
	{
		m_nodePool->clear();
	}
	
	if (!m_tinyNodePool)
	{
		m_tinyNodePool = new (rdAlloc(sizeof(dtNodePool), RD_ALLOC_PERM)) dtNodePool(64, 32);
		if (!m_tinyNodePool)
			return DT_FAILURE | DT_OUT_OF_MEMORY;
	}
	else
	{
		m_tinyNodePool->clear();
	}
	
	if (!m_openList || m_openList->getCapacity() < maxNodes)
	{
		if (m_openList)
		{
			m_openList->~dtNodeQueue();
			rdFree(m_openList);
			m_openList = 0;
		}
		m_openList = new (rdAlloc(sizeof(dtNodeQueue), RD_ALLOC_PERM)) dtNodeQueue(maxNodes);
		if (!m_openList)
			return DT_FAILURE | DT_OUT_OF_MEMORY;
	}
	else
	{
		m_openList->clear();
	}
	
	return DT_SUCCESS;
}

dtStatus dtNavMeshQuery::findRandomPoint(const dtQueryFilter* filter, float (*frand)(),
										 dtPolyRef* randomRef, rdVec3D* randomPt) const
{
	rdAssert(m_nav);

	if (!m_nav || !filter || !frand || !randomRef || !randomPt)
		return DT_FAILURE | DT_INVALID_PARAM;

	// Randomly pick one tile. Assume that all tiles cover roughly the same area.
	const dtMeshTile* tile = 0;
	float tsum = 0.0f;
	for (int i = 0; i < m_nav->getMaxTiles(); i++)
	{
		const dtMeshTile* t = m_nav->getTile(i);
		if (!t || !t->header) continue;
		
		// Choose random tile using reservoir sampling.
		const float area = 1.0f; // Could be tile area too.
		tsum += area;
		const float u = frand();
		if (u*tsum <= area)
			tile = t;
	}
	if (!tile)
		return DT_FAILURE;

	// Randomly pick one polygon weighted by polygon area.
	const dtPoly* poly = 0;
	dtPolyRef polyRef = 0;
	const dtPolyRef base = m_nav->getPolyRefBase(tile);

	float areaSum = 0.0f;
	for (int i = 0; i < tile->header->polyCount; ++i)
	{
		const dtPoly* p = &tile->polys[i];
		// Do not return off-mesh connection polygons.
		if (p->getType() != DT_POLYTYPE_GROUND)
			continue;
		// Must pass filter
		const dtPolyRef ref = base | (dtPolyRef)i;
		if (!filter->passFilter(ref, tile, p))
			continue;

		// Calc area of the polygon.
		const float polyArea = dtCalcPolySurfaceArea(p, tile->verts);

		// Choose random polygon weighted by area, using reservoir sampling.
		areaSum += polyArea;
		const float u = frand();
		if (u*areaSum <= polyArea)
		{
			poly = p;
			polyRef = ref;
		}
	}
	
	if (!poly)
		return DT_FAILURE;

	// Randomly pick point on polygon.
	const rdVec3D* v = &tile->verts[poly->verts[0]];
	rdVec3D verts[RD_VERTS_PER_POLYGON];
	float areas[RD_VERTS_PER_POLYGON];
	rdVcopy(&verts[0],v);
	for (int j = 1; j < poly->vertCount; ++j)
	{
		v = &tile->verts[poly->verts[j]];
		rdVcopy(&verts[j],v);
	}
	
	const float s = frand();
	const float t = frand();
	
	rdVec3D pt;
	rdRandomPointInConvexPoly(verts, poly->vertCount, areas, s, t, &pt);
	
	const dtStatus stat = closestPointOnPoly(polyRef, &pt, &pt, NULL);

	if (dtStatusFailed(stat))
		return stat | DT_OUTSIDE_BOUNDS;
	
	rdVcopy(randomPt, &pt);
	*randomRef = polyRef;

	return DT_SUCCESS;
}

dtStatus dtNavMeshQuery::findRandomPointAroundCircle(dtPolyRef startRef, const rdVec3D* centerPos, const float maxRadius,
													 const dtQueryFilter* filter, float (*frand)(),
													 dtPolyRef* randomRef, rdVec3D* randomPt) const
{
	rdAssert(m_nav);
	rdAssert(m_nodePool);
	rdAssert(m_openList);
	
	// Validate input
	if (!m_nav->isValidPolyRef(startRef) ||
		!centerPos || !rdVisfinite(centerPos) ||
		maxRadius < 0 || !rdMathIsfinite(maxRadius) ||
		!filter || !frand || !randomRef || !randomPt)
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}
	
	const dtMeshTile* startTile = 0;
	const dtPoly* startPoly = 0;
	m_nav->getTileAndPolyByRefUnsafe(startRef, &startTile, &startPoly);
	if (!filter->passFilter(startRef, startTile, startPoly))
		return DT_FAILURE | DT_INVALID_PARAM;
	
	m_nodePool->clear();
	m_openList->clear();
	
	dtNode* startNode = m_nodePool->getNode(startRef);
	rdVcopy(&startNode->pos, centerPos);
	startNode->pidx = 0;
	startNode->cost = 0;
	startNode->total = 0;
	startNode->id = startRef;
	startNode->flags = DT_NODE_OPEN;
	m_openList->push(startNode);
	
	dtStatus status = DT_SUCCESS;
	
	const float radiusSqr = rdSqr(maxRadius);
	float areaSum = 0.0f;

	const dtMeshTile* randomTile = 0;
	const dtPoly* randomPoly = 0;
	dtPolyRef randomPolyRef = 0;

	while (!m_openList->empty())
	{
		dtNode* bestNode = m_openList->pop();
		bestNode->flags &= ~DT_NODE_OPEN;
		bestNode->flags |= DT_NODE_CLOSED;
		
		// Get poly and tile.
		// The API input has been checked already, skip checking internal data.
		const dtPolyRef bestRef = bestNode->id;
		const dtMeshTile* bestTile = 0;
		const dtPoly* bestPoly = 0;
		m_nav->getTileAndPolyByRefUnsafe(bestRef, &bestTile, &bestPoly);

		// Place random locations on on ground.
		if (bestPoly->getType() == DT_POLYTYPE_GROUND)
		{
			// Calc area of the polygon.
			const float polyArea = dtCalcPolySurfaceArea(bestPoly, bestTile->verts);

			// Choose random polygon weighted by area, using reservoir sampling.
			areaSum += polyArea;
			const float u = frand();
			if (u*areaSum <= polyArea)
			{
				randomTile = bestTile;
				randomPoly = bestPoly;
				randomPolyRef = bestRef;
			}
		}
		
		// Get parent poly ref.
		const dtPolyRef parentRef = bestNode->pidx ? m_nodePool->getNodeAtIdx(bestNode->pidx)->id : 0;
		
		for (unsigned int i = bestPoly->firstLink; i != DT_NULL_LINK; i = bestTile->links[i].next)
		{
			const dtLink* link = &bestTile->links[i];
			dtPolyRef neighbourRef = link->ref;
			// Skip invalid neighbours and do not follow back to parent.
			if (!neighbourRef || neighbourRef == parentRef)
				continue;
			
			// Expand to neighbour
			const dtMeshTile* neighbourTile = 0;
			const dtPoly* neighbourPoly = 0;
			m_nav->getTileAndPolyByRefUnsafe(neighbourRef, &neighbourTile, &neighbourPoly);
			
			// Do not advance if the polygon is excluded by the filter.
			if (!filter->passFilter(neighbourRef, neighbourTile, neighbourPoly))
				continue;

			if (!filter->traverseFilter(link, neighbourTile, neighbourPoly))
				continue;
			
			// Find edge and calc distance to the edge.
			rdVec3D va, vb;
			if (!getPortalPoints(bestRef, bestPoly, bestTile, neighbourRef, neighbourPoly, neighbourTile, link, &va,&vb))
				continue;
			
			// If the circle is not touching the next polygon, skip it.
			float tseg;
			float distSqr = rdDistancePtSegSqr2D(centerPos, &va,&vb, tseg);
			if (distSqr > radiusSqr)
				continue;
			
			dtNode* neighbourNode = m_nodePool->getNode(neighbourRef);
			if (!neighbourNode)
			{
				status |= DT_OUT_OF_NODES;
				continue;
			}
			
			if (neighbourNode->flags & DT_NODE_CLOSED)
				continue;
			
			// Cost
			if (neighbourNode->flags == 0)
				rdVlerp(&neighbourNode->pos, &va,&vb, 0.5f);
			
			const float total = bestNode->total + rdVdist(&bestNode->pos, &neighbourNode->pos);
			
			// The node is already in open list and the new result is worse, skip.
			if ((neighbourNode->flags & DT_NODE_OPEN) && total >= neighbourNode->total)
				continue;
			
			neighbourNode->id = neighbourRef;
			neighbourNode->flags = (neighbourNode->flags & ~DT_NODE_CLOSED);
			neighbourNode->pidx = m_nodePool->getNodeIdx(bestNode);
			neighbourNode->total = total;
			
			if (neighbourNode->flags & DT_NODE_OPEN)
			{
				m_openList->modify(neighbourNode);
			}
			else
			{
				neighbourNode->flags = DT_NODE_OPEN;
				m_openList->push(neighbourNode);
			}
		}
	}
	
	if (!randomPoly)
		return DT_FAILURE;
	
	// Randomly pick point on polygon.
	const rdVec3D* v = &randomTile->verts[randomPoly->verts[0]];
	rdVec3D verts[RD_VERTS_PER_POLYGON];
	float areas[RD_VERTS_PER_POLYGON];
	rdVcopy(&verts[0],v);
	for (int j = 1; j < randomPoly->vertCount; ++j)
	{
		v = &randomTile->verts[randomPoly->verts[j]];
		rdVcopy(&verts[j],v);
	}
	
	const float s = frand();
	const float t = frand();
	
	rdVec3D pt;
	rdRandomPointInConvexPoly(verts, randomPoly->vertCount, areas, s, t, &pt);

	const dtStatus stat = closestPointOnPoly(randomPolyRef, &pt,&pt, NULL);

	if (dtStatusFailed(stat))
		return stat | DT_OUTSIDE_BOUNDS;
	
	rdVcopy(randomPt, &pt);
	*randomRef = randomPolyRef;
	
	return status;
}


//////////////////////////////////////////////////////////////////////////////////////////

/// @par
///
/// Uses the detail polygons to find the surface height. (Most accurate.)
///
/// @p pos does not have to be within the bounds of the polygon or navigation mesh.
///
/// See closestPointOnPolyBoundary() for a limited but faster option.
///
dtStatus dtNavMeshQuery::closestPointOnPoly(dtPolyRef ref, const rdVec3D* pos, rdVec3D* closest, bool* posOverPoly, float* dist, rdVec3D* normal) const
{
	rdAssert(m_nav);
	if (!m_nav->isValidPolyRef(ref) ||
		!pos || !rdVisfinite(pos) ||
		!closest)
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}

	if (!m_nav->closestPointOnPoly(ref, pos, closest, posOverPoly, dist, normal))
		return DT_FAILURE;

	return DT_SUCCESS;
}

/// @par
///
/// Much faster than closestPointOnPoly().
///
/// If the provided position lies within the polygon's xy-bounds (above or below), 
/// then @p pos and @p closest will be equal.
///
/// The height of @p closest will be the polygon boundary.  The height detail is not used.
/// 
/// @p pos does not have to be within the bounds of the polygon or the navigation mesh.
/// 
dtStatus dtNavMeshQuery::closestPointOnPolyBoundary(dtPolyRef ref, const rdVec3D* pos, rdVec3D* closest, float* dist) const
{
	rdAssert(m_nav);
	
	const dtMeshTile* tile = 0;
	const dtPoly* poly = 0;
	if (dtStatusFailed(m_nav->getTileAndPolyByRef(ref, &tile, &poly)))
		return DT_FAILURE | DT_INVALID_PARAM;

	if (!pos || !rdVisfinite(pos) || !closest)
		return DT_FAILURE | DT_INVALID_PARAM;
	
	// Collect vertices.
	rdVec3D verts[RD_VERTS_PER_POLYGON];
	float edged[RD_VERTS_PER_POLYGON];
	float edget[RD_VERTS_PER_POLYGON];
	int nv = 0;
	for (int i = 0; i < (int)poly->vertCount; ++i)
	{
		rdVcopy(&verts[nv], &tile->verts[poly->verts[i]]);
		nv++;
	}		
	
	const bool inside = rdDistancePtPolyEdgesSqr(pos, verts, nv, edged, edget);
	if (inside)
	{
		// Point is inside the polygon, return the point.
		rdVcopy(closest, pos);

		if (dist)
			*dist = 0.f;
	}
	else
	{
		// Point is outside the polygon, dtClamp to nearest edge.
		float dmin = edged[0];
		int imin = 0;
		for (int i = 1; i < nv; ++i)
		{
			if (edged[i] < dmin)
			{
				dmin = edged[i];
				imin = i;
			}
		}

		if (dist)
			*dist = dmin;

		const rdVec3D* va = &verts[imin];
		const rdVec3D* vb = &verts[(imin+1)%nv];
		rdVlerp(closest, va,vb, edget[imin]);
	}
	
	return DT_SUCCESS;
}

/// @par
///
/// Will return #DT_FAILURE | #DT_INVALID_PARAM if the provided position is outside the xy-bounds 
/// of the polygon.
/// 
dtStatus dtNavMeshQuery::getPolyHeight(dtPolyRef ref, const rdVec3D* pos, float* height, rdVec3D* normal) const
{
	rdAssert(m_nav);

	const dtMeshTile* tile = 0;
	const dtPoly* poly = 0;
	if (dtStatusFailed(m_nav->getTileAndPolyByRef(ref, &tile, &poly)))
		return DT_FAILURE | DT_INVALID_PARAM;

	if (!pos || !rdVisfinite2D(pos))
		return DT_FAILURE | DT_INVALID_PARAM;

	// We used to return success for off-mesh connections, but the
	// getPolyHeight in DetourNavMesh does not do this, so special
	// case it here.
	if (poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
	{
		const rdVec3D* v0 = &tile->verts[poly->verts[0]];
		const rdVec3D* v1 = &tile->verts[poly->verts[1]];
		float t;
		rdDistancePtSegSqr2D(pos, v0, v1, t);
		if (height)
			*height = v0->z + (v1->z - v0->z)*t;

		return DT_SUCCESS;
	}

	return m_nav->getPolyHeight(tile, poly, pos, height, normal)
		? DT_SUCCESS
		: DT_FAILURE | DT_INVALID_PARAM;
}

class dtFindNearestPolyQuery : public dtPolyQuery
{
	const dtNavMeshQuery* m_query;
	const rdVec3D* m_center;
	const rdVec3D* m_halfExtents;
	float m_nearestDistanceSqr;
	dtPolyRef m_nearestRef;
	rdVec3D m_nearestPoint;
	bool m_overPoly;

public:
	dtFindNearestPolyQuery(const dtNavMeshQuery* query, const rdVec3D* center, const rdVec3D* halfExtents)
		: m_query(query), m_center(center), m_halfExtents(halfExtents), m_nearestDistanceSqr(FLT_MAX), m_nearestRef(0), m_nearestPoint(), m_overPoly(false)
	{
	}

	dtPolyRef nearestRef() const { return m_nearestRef; }
	const rdVec3D* nearestPoint() const { return &m_nearestPoint; }
	bool isOverPoly() const { return m_overPoly; }

	void process(const dtMeshTile* tile, dtPoly** polys, dtPolyRef* refs, int count)
	{
		rdIgnoreUnused(polys);
		bool foundInsidePoly = false;

		for (int i = 0; i < count; ++i)
		{
			dtPolyRef ref = refs[i];
			rdVec3D closestPtPoly;
			rdVec3D diff;
			bool posOverPoly = false;
			float d;

			if (dtStatusFailed(m_query->closestPointOnPoly(ref, m_center, &closestPtPoly, &posOverPoly)))
				continue; // Degenerate poly.

			rdVsub(&diff, m_center, &closestPtPoly);

			// Make sure the point resides within our query box.
			// Reject it otherwise.
			bool withinExtents = true;
			for (int j = 0; j < 3; ++j)
			{
				if (rdMathFabsf(diff[j]) > (*m_halfExtents)[j])
				{
					withinExtents = false;
					break;
				}
			}

			if (!withinExtents)
				continue;

			// If a point is directly over a polygon and closer than
			// climb height, favor that instead of straight line nearest point.
			if (posOverPoly)
			{
				d = rdAbs(diff.z) - tile->header->walkableClimb;
				d = d > 0 ? d * d : 0;

				if (!foundInsidePoly)
				{
					// Update the nearest to this polygon.
					m_nearestDistanceSqr = FLT_MAX;
					foundInsidePoly = true;
				}
			}
			else if (!foundInsidePoly)
			{
				d = rdVlenSqr(&diff);
			}
			else
			{
				// We already have a better candidate, reject this one.
				continue;
			}

			if (d < m_nearestDistanceSqr)
			{
				m_nearestPoint = closestPtPoly;
				m_nearestDistanceSqr = d;
				m_nearestRef = ref;
				m_overPoly = posOverPoly;
			}
		}
	}
};

/// @par 
///
/// @note If the search box does not intersect any polygons the search will 
/// return #DT_SUCCESS, but @p nearestRef will be zero. So if in doubt, check 
/// @p nearestRef before using @p nearestPt.
///
/// If center and nearestPt point to an equal position, isOverPoly will be true;
/// however there's also a special case of climb height inside the polygon (see dtFindNearestPolyQuery)
/// 
dtStatus dtNavMeshQuery::findNearestPoly(const rdVec3D* center, const rdVec3D* halfExtents,
										 const dtQueryFilter* filter, dtPolyRef* nearestRef,
										 rdVec3D* nearestPt, bool* isOverPoly) const
{
	rdAssert(m_nav);

	if (!nearestRef)
		return DT_FAILURE | DT_INVALID_PARAM;

	// queryPolygons below will check rest of params
	dtFindNearestPolyQuery query(this, center, halfExtents);
	const dtStatus status = queryPolygonsInArea(center, halfExtents, filter, &query);

	if (dtStatusFailed(status))
		return status;

	*nearestRef = query.nearestRef();
	// Only override nearestPt if we actually found a poly so the nearest point
	// is valid.
	if (nearestPt && *nearestRef)
	{
		rdVcopy(nearestPt, query.nearestPoint());
		if (isOverPoly)
			*isOverPoly = query.isOverPoly();
	}
	
	return DT_SUCCESS;
}

int dtNavMeshQuery::queryPolygonsInTile(const dtMeshTile* tile, const rdVec3D* qmin, const rdVec3D* qmax,
										 const dtQueryFilter* filter, dtPolyQuery* query) const
{
	rdAssert(m_nav);
	static const int batchSize = 32;
	dtPolyRef polyRefs[batchSize];
	dtPoly* polys[batchSize];
	int n = 0;

	if (tile->bvTree)
	{
		const dtBVNode* node = &tile->bvTree[0];
		const dtBVNode* end = &tile->bvTree[tile->header->bvNodeCount];
		const rdVec3D* tbmin = &tile->header->bmin;
		const rdVec3D* tbmax = &tile->header->bmax;
		const float qfac = tile->header->bvQuantFactor;

		// Calculate quantized box
		unsigned short bmin[3], bmax[3];
		// dtClamp query box to world box.
		float minx = -(rdClamp(qmax->x, tbmin->x, tbmax->x) - tbmax->x);
		float miny = rdClamp(qmin->y, tbmin->y, tbmax->y) - tbmin->y;
		float minz = rdClamp(qmin->z, tbmin->z, tbmax->z) - tbmin->z;
		float maxx = -(rdClamp(qmin->x, tbmin->x, tbmax->x) - tbmax->x);
		float maxy = rdClamp(qmax->y, tbmin->y, tbmax->y) - tbmin->y;
		float maxz = rdClamp(qmax->z, tbmin->z, tbmax->z) - tbmin->z;
		// Quantize
		bmin[0] = (unsigned short)(qfac * minx) & 0xfffe;
		bmin[1] = (unsigned short)(qfac * miny) & 0xfffe;
		bmin[2] = (unsigned short)(qfac * minz) & 0xfffe;
		bmax[0] = (unsigned short)(qfac * maxx + 1) | 1;
		bmax[1] = (unsigned short)(qfac * maxy + 1) | 1;
		bmax[2] = (unsigned short)(qfac * maxz + 1) | 1;

		// Traverse tree
		const dtPolyRef base = m_nav->getPolyRefBase(tile);
		while (node < end)
		{
			const bool overlap = rdOverlapQuantBounds(bmin, bmax, node->bmin, node->bmax);
			const bool isLeafNode = node->i >= 0;

			if (isLeafNode && overlap)
			{
				dtPolyRef ref = base | (dtPolyRef)node->i;
				if (filter->passFilter(ref, tile, &tile->polys[node->i]))
				{
					polyRefs[n] = ref;
					polys[n] = &tile->polys[node->i];

					if (n == batchSize - 1)
					{
						query->process(tile, polys, polyRefs, batchSize);
						n = 0;
					}
					else
					{
						n++;
					}
				}
			}

			if (overlap || isLeafNode)
				node++;
			else
			{
				const int escapeIndex = -node->i;
				node += escapeIndex;
			}
		}
	}
	else
	{
		rdVec3D bmin, bmax;
		const dtPolyRef base = m_nav->getPolyRefBase(tile);
		for (int i = 0; i < tile->header->polyCount; ++i)
		{
			dtPoly* p = &tile->polys[i];
			// Do not return off-mesh connection polygons.
			if (p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
				continue;
			// Must pass filter
			const dtPolyRef ref = base | (dtPolyRef)i;
			if (!filter->passFilter(ref, tile, p))
				continue;
			// Calc polygon bounds.
			const rdVec3D* v = &tile->verts[p->verts[0]];
			rdVcopy(&bmin, v);
			rdVcopy(&bmax, v);
			for (int j = 1; j < p->vertCount; ++j)
			{
				v = &tile->verts[p->verts[j]];
				rdVmin(&bmin, v);
				rdVmax(&bmax, v);
			}
			if (rdOverlapBounds(qmin, qmax, &bmin,&bmax))
			{
				polyRefs[n] = ref;
				polys[n] = p;

				if (n == batchSize - 1)
				{
					query->process(tile, polys, polyRefs, batchSize);
					n = 0;
				}
				else
				{
					n++;
				}
			}
		}
	}

	// Process the last polygons that didn't make a full batch.
	if (n > 0)
		query->process(tile, polys, polyRefs, n);

	return n;
}

class dtCollectPolysQuery : public dtPolyQuery
{
	dtPolyRef* m_polys;
	const int m_maxPolys;
	int m_numCollected;
	bool m_overflow;

public:
	dtCollectPolysQuery(dtPolyRef* polys, const int maxPolys)
		: m_polys(polys), m_maxPolys(maxPolys), m_numCollected(0), m_overflow(false)
	{
	}

	int numCollected() const { return m_numCollected; }
	bool overflowed() const { return m_overflow; }

	void process(const dtMeshTile* tile, dtPoly** polys, dtPolyRef* refs, int count)
	{
		rdIgnoreUnused(tile);
		rdIgnoreUnused(polys);

		int numLeft = m_maxPolys - m_numCollected;
		int toCopy = count;
		if (toCopy > numLeft)
		{
			m_overflow = true;
			toCopy = numLeft;
		}

		memcpy(m_polys + m_numCollected, refs, (size_t)toCopy * sizeof(dtPolyRef));
		m_numCollected += toCopy;
	}
};

/// @par 
///
/// If no polygons are found, the function will return #DT_SUCCESS with a
/// @p polyCount of zero.
///
/// If @p polys is too small to hold the entire result set, then the array will 
/// be filled to capacity. The method of choosing which polygons from the 
/// full set are included in the partial result set is undefined.
///
dtStatus dtNavMeshQuery::queryPolygons(const rdVec3D* center, const rdVec3D* halfExtents,
									   const dtQueryFilter* filter,
									   dtPolyRef* polys, int* polyCount, const int maxPolys) const
{
	if (!polys || !polyCount || maxPolys < 0)
		return DT_FAILURE | DT_INVALID_PARAM;

	dtCollectPolysQuery collector(polys, maxPolys);
	const dtStatus status = queryPolygonsInArea(center, halfExtents, filter, &collector);

	if (dtStatusFailed(status))
		return status;

	*polyCount = collector.numCollected();
	return collector.overflowed() ? DT_SUCCESS | DT_BUFFER_TOO_SMALL : DT_SUCCESS;
}

/// @par 
///
/// Selects the best algorithm for the query box,
/// see #queryPolygonsSmallArea
/// and #queryPolygonsLargeArea
///
dtStatus dtNavMeshQuery::queryPolygonsInArea(const rdVec3D* center, const rdVec3D* halfExtents,
											 const dtQueryFilter* filter, dtPolyQuery* query) const
{
	rdAssert(m_nav);
	dtStatus status;

	if (rdVlenSqr2D(halfExtents) > rdSqr(m_nav->getParams()->tileWidth))
		status = queryPolygonsLargeArea(center, halfExtents, filter, query);
	else
		status = queryPolygonsSmallArea(center, halfExtents, filter, query);

	return status;
}

/// @par 
///
/// The query will be invoked with batches of polygons. Polygons passed
/// to the query have bounding boxes that overlap with the center and halfExtents
/// passed to this function. The dtPolyQuery::process function is invoked multiple
/// times until all overlapping polygons have been processed.
///
dtStatus dtNavMeshQuery::queryPolygonsSmallArea(const rdVec3D* center, const rdVec3D* halfExtents,
												const dtQueryFilter* filter, dtPolyQuery* query) const
{
	rdAssert(m_nav);

	if (!center || !rdVisfinite(center) ||
		!halfExtents || !rdVisfinite(halfExtents) ||
		!filter || !query)
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}

	rdVec3D bmin, bmax;
	rdVsub(&bmin, center, halfExtents);
	rdVadd(&bmax, center, halfExtents);
	
	// Find tiles the query touches.
	int minx, miny, maxx, maxy;
	m_nav->calcTileLoc(&bmin, &minx, &miny);
	m_nav->calcTileLoc(&bmax, &maxx, &maxy);

	if (minx > maxx)
		rdSwap(minx, maxx);
	if (miny > maxy)
		rdSwap(miny, maxy);

	static const int MAX_NEIS = 32;
	const dtMeshTile* neis[MAX_NEIS];
	
	for (int y = miny; y <= maxy; ++y)
	{
		for (int x = minx; x <= maxx; ++x)
		{
			const int nneis = m_nav->getTilesAt(x,y,neis,MAX_NEIS);
			for (int j = 0; j < nneis; ++j)
			{
				queryPolygonsInTile(neis[j], &bmin,&bmax, filter, query);
			}
		}
	}
	
	return DT_SUCCESS;
}

/// @par 
///
/// The same as #queryPolygonsSmallArea, but more optimized for @p halfExtents
/// who's squared length extends beyond the squared tile size. The algorithm
/// takes the current start point and spirals outwards. Only use this method
/// over #queryPolygonsSmallArea when the above condition is met, as this is
/// otherwise less efficient and could yield less accurate results too.
///
dtStatus dtNavMeshQuery::queryPolygonsLargeArea(const rdVec3D* center, const rdVec3D* halfExtents,
												const dtQueryFilter* filter, dtPolyQuery* query) const
{
	rdAssert(m_nav);

	if (!center || !rdVisfinite(center) ||
		!halfExtents || !rdVisfinite(halfExtents) ||
		!filter || !query)
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}

	rdVec3D bmin, bmax;
	rdVsub(&bmin, center, halfExtents);
	rdVadd(&bmax, center, halfExtents);

	// Find tiles the query touches.
	int minx, miny, maxx, maxy;
	m_nav->calcTileLoc(&bmin, &minx, &miny);
	m_nav->calcTileLoc(&bmax, &maxx, &maxy);

	if (minx > maxx)
		rdSwap(minx, maxx);

	const int centerX = (minx+maxx) / 2;
	minx -= centerX;

	// Check if the queried area is entirely to the right of the center
	// tile. This strategy assumes symmetry around the center, so drop
	// degenerate ranges here..
	if (minx > 0)
		return DT_SUCCESS;

	maxx -= centerX;

	if (miny > maxy)
		rdSwap(miny, maxy);

	const int centerY = (miny+maxy) / 2;
	miny -= centerY;
	maxy -= centerY;

	int tx = 0, ty = 0, dir = 0;
	int layer = 1, startLayer = INT_MAX;

	static const int MAX_NEIS = 32;
	const dtMeshTile* neis[MAX_NEIS];

	// Spiral clockwise from the center tile outwards.
	while (tx <= maxx && miny <= ty && ty <= maxy)
	{
		const int nneis = m_nav->getTilesAt(tx+centerX, ty+centerY, neis, MAX_NEIS);

		for (int j = 0; j < nneis; ++j)
		{
			const int collected = queryPolygonsInTile(neis[j], &bmin,&bmax, filter, query);

			if (collected > 0 && startLayer == INT_MAX)
				startLayer = layer; // The layer from which we start.
		}

		// Advance through the current spiral layer.
		if (dir == 0)
			dir = ++tx == layer;
		else if (dir == 1)
		{
			if (++ty == layer)
				dir = 2;
		}
		else if (dir == 2)
		{
			if (-(--tx) == layer)
				dir = 3;
		}
		else if (-(--ty) == layer)
		{
			// Expand the spiral layer outwards.
			dir = 0;
			layer++;
		}

		if (layer > startLayer && dir == 1 || minx > tx)
			break;
	}

	return DT_SUCCESS;
}

/// @par
///
/// If the end polygon cannot be reached through the navigation graph,
/// the last polygon in the path will be the nearest the end polygon.
///
/// If the path array is to small to hold the full result, it will be filled as 
/// far as possible from the start polygon toward the end polygon.
///
/// The start and end positions are used to calculate traversal costs. 
/// (The z-values impact the result.)
///
dtStatus dtNavMeshQuery::findPath(dtPolyRef startRef, dtPolyRef endRef,
								  const rdVec3D* startPos, const rdVec3D* endPos,
								  const dtQueryFilter* filter, dtPolyRef* path,
								  unsigned char* jump, int* pathCount, const int maxPath) const
{
	rdAssert(m_nav);
	rdAssert(m_nodePool);
	rdAssert(m_openList);

	if (!pathCount)
		return DT_FAILURE | DT_INVALID_PARAM;

	*pathCount = 0;
	
	// Validate input
	if (!m_nav->isValidPolyRef(startRef) || !m_nav->isValidPolyRef(endRef) ||
		!startPos || !rdVisfinite(startPos) ||
		!endPos || !rdVisfinite(endPos) ||
		!filter || !path || !jump || maxPath <= 0)
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}

	if (startRef == endRef)
	{
		path[0] = startRef;
		jump[0] = DT_NULL_TRAVERSE_TYPE;
		*pathCount = 1;
		return DT_SUCCESS;
	}
	
	m_nodePool->clear();
	m_openList->clear();
	
	dtNode* startNode = m_nodePool->getNode(startRef);
	startNode->pos = *startPos;
	startNode->pidx = 0;
	startNode->cost = 0;
	startNode->total = rdVdist(startPos, endPos) * H_SCALE;
	startNode->id = startRef;
	startNode->flags = DT_NODE_OPEN;
	m_openList->push(startNode);
	
	dtNode* lastBestNode = startNode;
	float lastBestNodeCost = startNode->total;
	
	bool outOfNodes = false;
	
	while (!m_openList->empty())
	{
		// Remove node from open list and put it in closed list.
		dtNode* bestNode = m_openList->pop();
		bestNode->flags &= ~DT_NODE_OPEN;
		bestNode->flags |= DT_NODE_CLOSED;
		
		// Reached the goal, stop searching.
		if (bestNode->id == endRef)
		{
			lastBestNode = bestNode;
			break;
		}
		
		// Get current poly and tile.
		// The API input has been checked already, skip checking internal data.
		const dtPolyRef bestRef = bestNode->id;
		const dtMeshTile* bestTile = 0;
		const dtPoly* bestPoly = 0;
		m_nav->getTileAndPolyByRefUnsafe(bestRef, &bestTile, &bestPoly);
		
		// Get parent poly and tile.
		dtPolyRef parentRef = 0;
		const dtMeshTile* parentTile = 0;
		const dtPoly* parentPoly = 0;
		if (bestNode->pidx)
			parentRef = m_nodePool->getNodeAtIdx(bestNode->pidx)->id;

		// Default implementation of dtQueryFilter::getCost only needs the start, end
		// positions and the link. The tile and poly are therefore not used. Only
		// obtain them when virtual query filter function overrides are enabled.
#ifdef DT_VIRTUAL_QUERYFILTER
		if (parentRef)
			m_nav->getTileAndPolyByRefUnsafe(parentRef, &parentTile, &parentPoly);
#endif // DT_VIRTUAL_QUERYFILTER
		
		for (unsigned int i = bestPoly->firstLink; i != DT_NULL_LINK; i = bestTile->links[i].next)
		{
			const dtLink& bestLink = bestTile->links[i];
			dtPolyRef neighbourRef = bestLink.ref;
			
			// Skip invalid ids and do not expand back to where we came from.
			if (!neighbourRef || neighbourRef == parentRef)
				continue;
			
			// Get neighbour poly and tile.
			// The API input has been checked already, skip checking internal data.
			const dtMeshTile* neighbourTile = 0;
			const dtPoly* neighbourPoly = 0;
			m_nav->getTileAndPolyByRefUnsafe(neighbourRef, &neighbourTile, &neighbourPoly);
			
			if (!filter->passFilter(neighbourRef, neighbourTile, neighbourPoly))
				continue;

			if (!filter->traverseFilter(&bestLink, neighbourTile, neighbourPoly))
				continue;

			// deal explicitly with crossing tile boundaries
			unsigned char crossSide = 0;
			if (bestLink.side != 0xff)
				crossSide = bestLink.side >> 1;

			// get the node
			dtNode* neighbourNode = m_nodePool->getNode(neighbourRef, crossSide);
			if (!neighbourNode)
			{
				outOfNodes = true;
				continue;
			}
			
			// If the node is visited the first time, calculate node position.
			if (neighbourNode->flags == 0)
			{
				getEdgeMidPoint(bestRef, bestPoly, bestTile,
								neighbourRef, neighbourPoly, neighbourTile,
								&neighbourNode->pos);
			}

			// Calculate cost and heuristic.
			float cost = 0;
			float heuristic = 0;
			
			// Special case for last node.
			if (neighbourRef == endRef)
			{
				// Cost
				const float curCost = filter->getCost(&bestNode->pos, &neighbourNode->pos, &bestLink,
													  parentRef, parentTile, parentPoly,
													  bestRef, bestTile, bestPoly,
													  neighbourRef, neighbourTile, neighbourPoly);
				const float endCost = filter->getCost(&neighbourNode->pos, endPos, 0,
													  bestRef, bestTile, bestPoly,
													  neighbourRef, neighbourTile, neighbourPoly,
													  0, 0, 0);
				
				cost = bestNode->cost + curCost + endCost;
				heuristic = 0;
			}
			else
			{
				// Cost
				const float curCost = filter->getCost(&bestNode->pos, &neighbourNode->pos, &bestLink,
													  parentRef, parentTile, parentPoly,
													  bestRef, bestTile, bestPoly,
													  neighbourRef, neighbourTile, neighbourPoly);
				cost = bestNode->cost + curCost;
				heuristic = rdVdist(&neighbourNode->pos, endPos)*H_SCALE;
			}

			const float total = cost + heuristic;
			
			// The node is already in open list and the new result is worse, skip.
			if ((neighbourNode->flags & DT_NODE_OPEN) && total >= neighbourNode->total)
				continue;
			// The node is already visited and process, and the new result is worse, skip.
			if ((neighbourNode->flags & DT_NODE_CLOSED) && total >= neighbourNode->total)
				continue;
			
			// Add or update the node.
			neighbourNode->pidx = m_nodePool->getNodeIdx(bestNode);
			neighbourNode->id = neighbourRef;
			neighbourNode->flags = (neighbourNode->flags & ~DT_NODE_CLOSED);
			neighbourNode->jump = bestLink.traverseType;
			neighbourNode->cost = cost;
			neighbourNode->total = total;
			
			if (neighbourNode->flags & DT_NODE_OPEN)
			{
				// Already in open, update node location.
				m_openList->modify(neighbourNode);
			}
			else
			{
				// Put the node in open list.
				neighbourNode->flags |= DT_NODE_OPEN;
				m_openList->push(neighbourNode);
			}
			
			// Update nearest node to target so far.
			if (heuristic < lastBestNodeCost)
			{
				lastBestNodeCost = heuristic;
				lastBestNode = neighbourNode;
			}
		}
	}

	dtStatus status = getPathToNode(lastBestNode, path, jump, pathCount, maxPath);

	if (lastBestNode->id != endRef)
		status |= DT_PARTIAL_RESULT;

	if (outOfNodes)
		status |= DT_OUT_OF_NODES;
	
	return status;
}

dtStatus dtNavMeshQuery::getPathToNode(dtNode* endNode, dtPolyRef* path, unsigned char* jump, int* pathCount, int maxPath) const
{
	// Find the length of the entire path.
	dtNode* curNode = endNode;
	int length = 0;
	do
	{
		length++;
		curNode = m_nodePool->getNodeAtIdx(curNode->pidx);
	} while (curNode);

	// If the path cannot be fully stored then advance to the last node we will be able to store.
	curNode = endNode;
	int writeCount;
	for (writeCount = length; writeCount > maxPath; writeCount--)
	{
		rdAssert(curNode);

		curNode = m_nodePool->getNodeAtIdx(curNode->pidx);
	}

	// Write path
	for (int i = writeCount - 1; i >= 0; i--)
	{
		rdAssert(curNode);

		path[i] = curNode->id;
		jump[i] = curNode->jump;
		curNode = m_nodePool->getNodeAtIdx(curNode->pidx);
	}

	rdAssert(!curNode);

	*pathCount = rdMin(length, maxPath);

	if (length > maxPath)
		return DT_SUCCESS | DT_BUFFER_TOO_SMALL;

	return DT_SUCCESS;
}


/// @par
///
/// @warning Calling any non-slice methods before calling finalizeSlicedFindPath() 
/// or finalizeSlicedFindPathPartial() may result in corrupted data!
///
/// The @p filter pointer is stored and used for the duration of the sliced
/// path query.
///
dtStatus dtNavMeshQuery::initSlicedFindPath(dtPolyRef startRef, dtPolyRef endRef,
											const rdVec3D* startPos, const rdVec3D* endPos,
											const unsigned int options)
{
	rdAssert(m_nav);
	rdAssert(m_nodePool);
	rdAssert(m_openList);

	// Init path state.
	memset(&m_query, 0, sizeof(dtQueryData));
	m_query.status = DT_FAILURE;
	m_query.startRef = startRef;
	m_query.endRef = endRef;
	if (startPos)
		m_query.startPos = *startPos;
	if (endPos)
		m_query.endPos = *endPos;
	m_query.options = options;
	m_query.raycastLimitSqr = FLT_MAX;
	
	// Validate input
	if (!m_nav->isValidPolyRef(startRef) || !m_nav->isValidPolyRef(endRef) ||
		!startPos || !rdVisfinite(startPos) ||
		!endPos || !rdVisfinite(endPos))
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}

	// trade quality with performance?
	if (options & DT_FINDPATH_ANY_ANGLE)
	{
		// limiting to several times the character radius yields nice results. It is not sensitive 
		// so it is enough to compute it from the first tile.
		const dtMeshTile* tile = m_nav->getTileByRef(startRef);
		float agentRadius = tile->header->walkableRadius;
		m_query.raycastLimitSqr = rdSqr(agentRadius * DT_RAY_CAST_LIMIT_PROPORTIONS);
	}

	if (startRef == endRef)
	{
		m_query.status = DT_SUCCESS;
		return DT_SUCCESS;
	}
	
	m_nodePool->clear();
	m_openList->clear();
	
	dtNode* startNode = m_nodePool->getNode(startRef);
	startNode->pos = *startPos;
	startNode->pidx = 0;
	startNode->cost = 0;
	startNode->total = rdVdist(startPos, endPos) * H_SCALE;
	startNode->id = startRef;
	startNode->flags = DT_NODE_OPEN;
	m_openList->push(startNode);
	
	m_query.status = DT_IN_PROGRESS;
	m_query.lastBestNode = startNode;
	m_query.lastBestNodeCost = startNode->total;
	
	return m_query.status;
}
	
dtStatus dtNavMeshQuery::updateSlicedFindPath(const int maxIter, int* doneIters, const dtQueryFilter* filter)
{
	rdAssert(filter);

	if (!dtStatusInProgress(m_query.status))
		return m_query.status;

	if (!filter)
		return DT_FAILURE | DT_INVALID_PARAM;

	// Make sure the request is still valid.
	if (!m_nav->isValidPolyRef(m_query.startRef) || !m_nav->isValidPolyRef(m_query.endRef))
	{
		m_query.status = DT_FAILURE;
		return DT_FAILURE;
	}

	dtRaycastHit rayHit;
	rayHit.maxPath = 0;
		
	int iter = 0;
	while (iter < maxIter && !m_openList->empty())
	{
		iter++;
		
		// Remove node from open list and put it in closed list.
		dtNode* bestNode = m_openList->pop();
		bestNode->flags &= ~DT_NODE_OPEN;
		bestNode->flags |= DT_NODE_CLOSED;
		
		// Reached the goal, stop searching.
		if (bestNode->id == m_query.endRef)
		{
			m_query.lastBestNode = bestNode;
			const dtStatus details = m_query.status & DT_STATUS_DETAIL_MASK;
			m_query.status = DT_SUCCESS | details;
			if (doneIters)
				*doneIters = iter;
			return m_query.status;
		}
		
		// Get current poly and tile.
		// The API input has been checked already, skip checking internal data.
		const dtPolyRef bestRef = bestNode->id;
		const dtMeshTile* bestTile = 0;
		const dtPoly* bestPoly = 0;
		if (dtStatusFailed(m_nav->getTileAndPolyByRef(bestRef, &bestTile, &bestPoly)))
		{
			// The polygon has disappeared during the sliced query, fail.
			m_query.status = DT_FAILURE;
			if (doneIters)
				*doneIters = iter;
			return m_query.status;
		}
		
		// Get parent and grand parent poly and tile.
		dtPolyRef parentRef = 0, grandpaRef = 0;
		const dtMeshTile* parentTile = 0;
		const dtPoly* parentPoly = 0;
		dtNode* parentNode = 0;
		if (bestNode->pidx)
		{
			parentNode = m_nodePool->getNodeAtIdx(bestNode->pidx);
			parentRef = parentNode->id;
			if (parentNode->pidx)
				grandpaRef = m_nodePool->getNodeAtIdx(parentNode->pidx)->id;
		}
		if (parentRef)
		{
			bool invalidParent = dtStatusFailed(m_nav->getTileAndPolyByRef(parentRef, &parentTile, &parentPoly));
			if (invalidParent || (grandpaRef && !m_nav->isValidPolyRef(grandpaRef)) )
			{
				// The polygon has disappeared during the sliced query, fail.
				m_query.status = DT_FAILURE;
				if (doneIters)
					*doneIters = iter;
				return m_query.status;
			}
		}

		// decide whether to test raycast to previous nodes
		bool tryLOS = false;
		if (m_query.options & DT_FINDPATH_ANY_ANGLE)
		{
			if ((parentRef != 0) && (rdVdistSqr(&parentNode->pos, &bestNode->pos) < m_query.raycastLimitSqr))
				tryLOS = true;
		}
		
		for (unsigned int i = bestPoly->firstLink; i != DT_NULL_LINK; i = bestTile->links[i].next)
		{
			const dtLink& bestLink = bestTile->links[i];
			dtPolyRef neighbourRef = bestLink.ref;
			
			// Skip invalid ids and do not expand back to where we came from.
			if (!neighbourRef || neighbourRef == parentRef)
				continue;
			
			// Get neighbour poly and tile.
			// The API input has been checked already, skip checking internal data.
			const dtMeshTile* neighbourTile = 0;
			const dtPoly* neighbourPoly = 0;
			m_nav->getTileAndPolyByRefUnsafe(neighbourRef, &neighbourTile, &neighbourPoly);			
			
			if (!filter->passFilter(neighbourRef, neighbourTile, neighbourPoly))
				continue;

			if (!filter->traverseFilter(&bestLink, neighbourTile, neighbourPoly))
				continue;
			
			unsigned char crossSide = 0; // See https://github.com/recastnavigation/recastnavigation/issues/438

			if (bestLink.side != 0xff)
				crossSide = bestLink.side >> 1;

			// get the neighbor node
			dtNode* neighbourNode = m_nodePool->getNode(neighbourRef, crossSide);
			if (!neighbourNode)
			{
				m_query.status |= DT_OUT_OF_NODES;
				continue;
			}
			
			// do not expand to nodes that were already visited from the same parent
			if (neighbourNode->pidx != 0 && neighbourNode->pidx == bestNode->pidx)
				continue;

			// If the node is visited the first time, calculate node position.
			if (neighbourNode->flags == 0)
			{
				getEdgeMidPoint(bestRef, bestPoly, bestTile,
								neighbourRef, neighbourPoly, neighbourTile,
								&neighbourNode->pos);
			}
			
			// Calculate cost and heuristic.
			float cost = 0;
			float heuristic = 0;
			
			// raycast parent
			bool foundShortCut = false;
			rayHit.pathCost = rayHit.t = 0;
			if (tryLOS)
			{
				raycast(parentRef, &parentNode->pos, &neighbourNode->pos, filter, DT_RAYCAST_USE_COSTS, &rayHit, grandpaRef);
				foundShortCut = rayHit.t >= 1.0f;
			}

			// update move cost
			if (foundShortCut)
			{
				// shortcut found using raycast. Using shorter cost instead
				cost = parentNode->cost + rayHit.pathCost;
			}
			else
			{
				// No shortcut found.
				const float curCost = filter->getCost(&bestNode->pos, &neighbourNode->pos, &bestLink,
															  parentRef, parentTile, parentPoly,
															bestRef, bestTile, bestPoly,
															neighbourRef, neighbourTile, neighbourPoly);
				cost = bestNode->cost + curCost;
			}

			// Special case for last node.
			if (neighbourRef == m_query.endRef)
			{
				const float endCost = filter->getCost(&neighbourNode->pos, &m_query.endPos, 0,
															  bestRef, bestTile, bestPoly,
															  neighbourRef, neighbourTile, neighbourPoly,
															  0, 0, 0);
				
				cost = cost + endCost;
				heuristic = 0;
			}
			else
			{
				heuristic = rdVdist(&neighbourNode->pos, &m_query.endPos)*H_SCALE;
			}
			
			const float total = cost + heuristic;
			
			// The node is already in open list and the new result is worse, skip.
			if ((neighbourNode->flags & DT_NODE_OPEN) && total >= neighbourNode->total)
				continue;
			// The node is already visited and process, and the new result is worse, skip.
			if ((neighbourNode->flags & DT_NODE_CLOSED) && total >= neighbourNode->total)
				continue;
			
			// Add or update the node.
			neighbourNode->pidx = foundShortCut ? bestNode->pidx : m_nodePool->getNodeIdx(bestNode);
			neighbourNode->id = neighbourRef;
			neighbourNode->flags = (neighbourNode->flags & ~(DT_NODE_CLOSED | DT_NODE_PARENT_DETACHED));
			neighbourNode->jump = bestLink.traverseType;
			neighbourNode->cost = cost;
			neighbourNode->total = total;
			if (foundShortCut)
				neighbourNode->flags = (neighbourNode->flags | DT_NODE_PARENT_DETACHED);
			
			if (neighbourNode->flags & DT_NODE_OPEN)
			{
				// Already in open, update node location.
				m_openList->modify(neighbourNode);
			}
			else
			{
				// Put the node in open list.
				neighbourNode->flags |= DT_NODE_OPEN;
				m_openList->push(neighbourNode);
			}
			
			// Update nearest node to target so far.
			if (heuristic < m_query.lastBestNodeCost)
			{
				m_query.lastBestNodeCost = heuristic;
				m_query.lastBestNode = neighbourNode;
			}
		}
	}
	
	// Exhausted all nodes, but could not find path.
	if (m_openList->empty())
	{
		const dtStatus details = m_query.status & DT_STATUS_DETAIL_MASK;
		m_query.status = DT_SUCCESS | details;
	}

	if (doneIters)
		*doneIters = iter;

	return m_query.status;
}

dtStatus dtNavMeshQuery::finalizeSlicedFindPath(dtPolyRef* path, unsigned char* jump, int* pathCount, const int maxPath, const dtQueryFilter* filter)
{
	rdAssert(filter);
	rdAssert(pathCount);

	if (!filter || !pathCount)
		return DT_FAILURE | DT_INVALID_PARAM;

	*pathCount = 0;

	if (!path || !jump || maxPath <= 0)
		return DT_FAILURE | DT_INVALID_PARAM;
	
	if (dtStatusFailed(m_query.status))
	{
		// Reset query.
		memset(&m_query, 0, sizeof(dtQueryData));
		return DT_FAILURE;
	}

	int n = 0;

	if (m_query.startRef == m_query.endRef)
	{
		// Special case: the search starts and ends at same poly.
		path[n] = m_query.startRef;
		jump[n] = DT_NULL_TRAVERSE_TYPE;

		n++;
	}
	else
	{
		// Reverse the path.
		rdAssert(m_query.lastBestNode);
		
		if (m_query.lastBestNode->id != m_query.endRef)
			m_query.status |= DT_PARTIAL_RESULT;
		
		dtNode* prev = 0;
		dtNode* node = m_query.lastBestNode;
		int prevRay = 0;
		do
		{
			dtNode* next = m_nodePool->getNodeAtIdx(node->pidx);
			node->pidx = m_nodePool->getNodeIdx(prev);
			prev = node;
			int nextRay = node->flags & DT_NODE_PARENT_DETACHED; // keep track of whether parent is not adjacent (i.e. due to raycast shortcut)
			node->flags = (node->flags & ~DT_NODE_PARENT_DETACHED) | prevRay; // and store it in the reversed path's node
			prevRay = nextRay;
			node = next;
		}
		while (node);
		
		// Store path
		node = prev;
		do
		{
			dtNode* next = m_nodePool->getNodeAtIdx(node->pidx);
			dtStatus status = 0;
			if (node->flags & DT_NODE_PARENT_DETACHED)
			{
				float t;
				int m;
				status = raycast(node->id, &node->pos, &next->pos, filter, &t, nullptr, path+n, &m, maxPath-n);
				n += m;
				// raycast ends on poly boundary and the path might include the next poly boundary.
				if (path[n-1] == next->id)
					n--; // remove to avoid duplicates
			}
			else
			{
				path[n] = node->id;
				jump[n] = node->jump;
				if (++n >= maxPath)
					status = DT_BUFFER_TOO_SMALL;
			}

			if (status & DT_STATUS_DETAIL_MASK)
			{
				m_query.status |= status & DT_STATUS_DETAIL_MASK;
				break;
			}
			node = next;
		}
		while (node);
	}
	
	const dtStatus details = m_query.status & DT_STATUS_DETAIL_MASK;

	// Reset query.
	memset(&m_query, 0, sizeof(dtQueryData));
	
	*pathCount = n;
	
	return DT_SUCCESS | details;
}

dtStatus dtNavMeshQuery::finalizeSlicedFindPathPartial(const dtPolyRef* existing, const int existingSize,
													   dtPolyRef* path, unsigned char* jump, int* pathCount,
													   const int maxPath, const dtQueryFilter* filter)
{
	if (!pathCount)
		return DT_FAILURE | DT_INVALID_PARAM;

	*pathCount = 0;

	if (!existing || existingSize <= 0 || !path || !jump || !pathCount || maxPath <= 0)
		return DT_FAILURE | DT_INVALID_PARAM;
	
	if (dtStatusFailed(m_query.status))
	{
		// Reset query.
		memset(&m_query, 0, sizeof(dtQueryData));
		return DT_FAILURE;
	}
	
	int n = 0;
	
	if (m_query.startRef == m_query.endRef)
	{
		// Special case: the search starts and ends at same poly.
		path[n++] = m_query.startRef;
	}
	else
	{
		// Find furthest existing node that was visited.
		dtNode* prev = 0;
		dtNode* node = 0;
		for (int i = existingSize-1; i >= 0; --i)
		{
			m_nodePool->findNodes(existing[i], &node, 1);
			if (node)
				break;
		}
		
		if (!node)
		{
			m_query.status |= DT_PARTIAL_RESULT;
			rdAssert(m_query.lastBestNode);
			node = m_query.lastBestNode;
		}
		
		// Reverse the path.
		int prevRay = 0;
		do
		{
			dtNode* next = m_nodePool->getNodeAtIdx(node->pidx);
			node->pidx = m_nodePool->getNodeIdx(prev);
			prev = node;
			int nextRay = node->flags & DT_NODE_PARENT_DETACHED; // keep track of whether parent is not adjacent (i.e. due to raycast shortcut)
			node->flags = (node->flags & ~DT_NODE_PARENT_DETACHED) | prevRay; // and store it in the reversed path's node
			prevRay = nextRay;
			node = next;
		}
		while (node);
		
		// Store path
		node = prev;
		do
		{
			dtNode* next = m_nodePool->getNodeAtIdx(node->pidx);
			dtStatus status = 0;
			if (node->flags & DT_NODE_PARENT_DETACHED)
			{
				float t;
				int m;
				status = raycast(node->id, &node->pos, &next->pos, filter, &t, nullptr, path+n, &m, maxPath-n);
				n += m;
				// raycast ends on poly boundary and the path might include the next poly boundary.
				if (path[n-1] == next->id)
					n--; // remove to avoid duplicates
			}
			else
			{
				path[n] = node->id;
				jump[n] = node->jump;
				if (++n >= maxPath)
					status = DT_BUFFER_TOO_SMALL;
			}

			if (status & DT_STATUS_DETAIL_MASK)
			{
				m_query.status |= status & DT_STATUS_DETAIL_MASK;
				break;
			}
			node = next;
		}
		while (node);
	}
	
	const dtStatus details = m_query.status & DT_STATUS_DETAIL_MASK;

	// Reset query.
	memset(&m_query, 0, sizeof(dtQueryData));
	
	*pathCount = n;
	
	return DT_SUCCESS | details;
}


dtStatus dtNavMeshQuery::appendVertex(const rdVec3D* pos, const unsigned char flags, const dtPolyRef ref,
									  const unsigned char jump, rdVec3D* straightPath, unsigned char* straightPathFlags,
									  dtPolyRef* straightPathRefs, unsigned char* straightPathJumps,
									  int* straightPathCount, const int maxStraightPath) const
{
	if ((*straightPathCount) > 0 && ref && rdVequal(&straightPath[((*straightPathCount)-1)], pos))
	{
		// The vertices are equal, update flags and poly.
		if (straightPathFlags)
			straightPathFlags[(*straightPathCount)-1] = flags;
		if (straightPathRefs)
			straightPathRefs[(*straightPathCount)-1] = ref;
		if (straightPathJumps)
			straightPathJumps[(*straightPathCount)-1] = jump;
	}
	else
	{
		// Append new vertex.
		straightPath[(*straightPathCount)] = *pos;
		if (straightPathFlags)
			straightPathFlags[(*straightPathCount)] = flags;
		if (straightPathRefs)
			straightPathRefs[(*straightPathCount)] = ref;
		if (straightPathJumps)
			straightPathJumps[(*straightPathCount)] = jump;
		(*straightPathCount)++;

		// If there is no space to append more vertices, return.
		if ((*straightPathCount) >= maxStraightPath)
		{
			return DT_SUCCESS | DT_BUFFER_TOO_SMALL;
		}

		// If reached end of path, return.
		if (dtIsStraightPathEnd(flags))
		{
			return DT_SUCCESS;
		}
	}
	return DT_IN_PROGRESS;
}

static bool canTraversePortal(const dtMeshTile* fromTile, const dtPoly* fromPoly, const dtPolyRef toRef,
							  const int jumpFilter, const int targetJump)
{
	if (!jumpFilter)
		return false;

	if (!(rdBitCellBit(targetJump) & jumpFilter))
		return false;

	if (fromPoly->firstLink == DT_NULL_LINK)
		return false;

	// Make sure our goal poly has an actual link to our current poly.
	for (unsigned int j = fromPoly->firstLink; j != DT_NULL_LINK; j = fromTile->links[j].next)
	{
		if (fromTile->links[j].ref == toRef)
			return true;
	}

	return false;
}

dtStatus dtNavMeshQuery::appendPortals(const int startIdx, const int endIdx, const rdVec3D* endPos, const dtPolyRef* path,
									  const unsigned char* jumpTypes, rdVec3D* straightPath, unsigned char* straightPathFlags,
									  dtPolyRef* straightPathRefs, unsigned char* straightPathJumps, int* straightPathCount,
									  const int maxStraightPath, const int jumpFilter, const int options) const
{
	const rdVec3D* startPos = &straightPath[(*straightPathCount-1)];
	// Append or update last vertex
	dtStatus stat = 0;
	for (int i = startIdx; i < endIdx; i++)
	{
		// Calculate portal
		const dtPolyRef from = path[i];
		const dtMeshTile* fromTile = 0;
		const dtPoly* fromPoly = 0;
		if (dtStatusFailed(m_nav->getTileAndPolyByRef(from, &fromTile, &fromPoly)))
			return DT_FAILURE | DT_INVALID_PARAM;
		
		const dtPolyRef to = path[i+1];
		const dtMeshTile* toTile = 0;
		const dtPoly* toPoly = 0;
		if (dtStatusFailed(m_nav->getTileAndPolyByRef(to, &toTile, &toPoly)))
			return DT_FAILURE | DT_INVALID_PARAM;
		
		rdVec3D left, right;
		if (dtStatusFailed(getPortalPoints(from, fromPoly, fromTile, to, toPoly, toTile, 0, &left,&right)))
			break;
	
		if (options & DT_STRAIGHTPATH_AREA_CROSSINGS)
		{
			// Skip intersection if only area crossings are requested.
			if (fromPoly->getArea() == toPoly->getArea())
				continue;
		}
		
		// Append intersection
		float s,t;
		if (!rdIntersectSegSeg2D(startPos, endPos, &left,&right, s, t))
			continue;

		rdVec3D pt;
		rdVlerp(&pt, &left,&right, t);

		const unsigned char targetJump = jumpTypes[i+1];

		if (targetJump < DT_MAX_TRAVERSE_TYPES)
		{
			stat = appendVertex(&pt, 0, from, targetJump,
								straightPath, straightPathFlags, straightPathRefs,
								straightPathJumps, straightPathCount, maxStraightPath);
			if (stat != DT_IN_PROGRESS)
				return stat;

			if (dtStatusSucceed(getPortalPoints(to, toPoly, toTile, from, fromPoly, fromTile, 0, &left,&right)))
			{
				if (canTraversePortal(fromTile, fromPoly, to, jumpFilter, targetJump))
				{
					// Note(amos): this isn't the same as startPos as straightPathCount
					// is incremented by the last call to appendVertex.
					const rdVec3D* jumpStartPos = &straightPath[(*straightPathCount-1)];

					const rdVec3D jumpEndPoint(
						((left.y - right.y) * 100.f) + jumpStartPos->x,
						(-(left.x - right.x) * 100.f) + jumpStartPos->y,
						endPos->z);

					// Modify vertex position to take the traverse portal into account
					// for the next call to appendVertex.
					rdIntersectSegSeg2D(jumpStartPos, &jumpEndPoint, &left,&right, s, t);
					rdVlerp(&pt, &left,&right, t);
				}
				else
				{
					// Update vertex position as we advanced since last appendVertex call.
					if (rdIntersectSegSeg2D(startPos, endPos, &left,&right, s, t))
						rdVlerp(&pt, &left,&right, rdClamp(t, 0.f, 1.f));
				}
			}
		}

		stat = appendVertex(&pt, 0, path[i+1], DT_NULL_TRAVERSE_TYPE,
							straightPath, straightPathFlags, straightPathRefs,
							straightPathJumps, straightPathCount, maxStraightPath);
		if (stat != DT_IN_PROGRESS)
			return stat;
	}
	return DT_IN_PROGRESS;
}

dtStatus dtNavMeshQuery::appendPortalVertex(const rdVec3D* startPos, const rdVec3D* endPos, const int startIdx, const dtPolyRef* path,
											rdVec3D* straightPath, unsigned char* straightPathFlags, dtPolyRef* straightPathRefs,
											unsigned char* straightPathJumps, int* straightPathCount, const int maxStraightPath) const
{
	const dtPolyRef from = path[startIdx-1];
	const dtPolyRef goal = path[startIdx];

	rdVec3D left, right;
	unsigned char fromType, toType;
	if (!dtStatusSucceed(getPortalPoints(goal,from, &left,&right, fromType,toType)))
		return DT_FAILURE | DT_PORTAL_BLOCKED;

	float s, t;
	if (!rdIntersectSegSeg2D(endPos,startPos, &left,&right, s, t))
		return DT_FAILURE | DT_PORTAL_BLOCKED;

	rdVec3D pt;
	rdVlerp(&pt, &left,&right, rdClamp(t, 0.f, 1.f));

	straightPathRefs[(*straightPathCount)-1] = from;
	return appendVertex(&pt, 0, goal, DT_NULL_TRAVERSE_TYPE, straightPath, straightPathFlags,
						straightPathRefs, straightPathJumps, straightPathCount, maxStraightPath);
}

// Only checks if non-jump waypoints are too close to the portal,
// we should never advance the path on jump waypoints since that
// would cause the NPC to become stuck at the current waypoint.
static bool checkPortalProximity(const int pathSize, const unsigned char* jumpTypes,
								 const rdVec3D* portalApex, const rdVec3D* left, const rdVec3D* right)
{
	// Look ahead of max 2 points in the corridor to
	// figure out if we are too close to the portal.
	const int skipCount = rdMin(pathSize, 2);
	const float thresh = rdSqr(0.001f);

	float t; // Ignored.
	if (skipCount < 1)
		return rdDistancePtSegSqr2D(portalApex, left, right, t) < thresh;

	int i = 1;
	while (jumpTypes[i++] == DT_NULL_TRAVERSE_TYPE)
	{
		if (i > skipCount)
			return rdDistancePtSegSqr2D(portalApex, left, right, t) < thresh;
	}

	return false;
}

/// @par
/// 
/// This method performs what is often called 'string pulling'.
///
/// The start position is clamped to the first polygon in the path, and the 
/// end position is clamped to the last. So the start and end positions should 
/// normally be within or very near the first and last polygons respectively.
///
/// The returned polygon references represent the reference id of the polygon 
/// that is entered at the associated path position. The reference id associated 
/// with the end point will always be zero.  This allows, for example, matching 
/// off-mesh link points to their representative polygons.
///
/// If the provided result buffers are too small for the entire result set, 
/// they will be filled as far as possible from the start toward the end 
/// position.
///
dtStatus dtNavMeshQuery::findStraightPath(const rdVec3D* startPos, const rdVec3D* endPos,
										  const dtPolyRef* path, const unsigned char* jumpTypes, const int pathSize,
										  rdVec3D* straightPath, unsigned char* straightPathFlags, dtPolyRef* straightPathRefs,
										  unsigned char* straightPathJumps, int* straightPathCount, const int maxStraightPath, 
										  const int jumpFilter, const int options) const
{
	rdAssert(m_nav);

	if (!straightPathCount)
		return DT_FAILURE | DT_INVALID_PARAM;

	*straightPathCount = 0;

	if (!startPos || !rdVisfinite(startPos) ||
		!endPos || !rdVisfinite(endPos) ||
		!path || !jumpTypes || pathSize <= 0 || !path[0] ||
		maxStraightPath <= 0)
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}
	
	dtStatus stat = 0;
	
	// TODO: Should this be callers responsibility?
	rdVec3D closestStartPos;
	if (dtStatusFailed(closestPointOnPolyBoundary(path[0], startPos, &closestStartPos)))
		return DT_FAILURE | DT_INVALID_PARAM;

	rdVec3D closestEndPos; float distToClosest;
	if (dtStatusFailed(closestPointOnPolyBoundary(path[pathSize-1], endPos, &closestEndPos, &distToClosest)))
		return DT_FAILURE | DT_INVALID_PARAM;

	// Start and end positions are too close, no action will be performed.
	if (rdVequal(&closestStartPos, &closestEndPos))
		return DT_FAILURE | DT_INVALID_ACTION;

	// Determine and mark whether the end vertex is our goal position.
	const unsigned char endFlags =
#if DT_NAVMESH_SET_VERSION >= 7
		(distToClosest == 0.0f) ? DT_STRAIGHTPATH_END_GOAL : DT_STRAIGHTPATH_END_PARTIAL;
#else // Titanfall 2 doesn't support partial path end flags.
		DT_STRAIGHTPATH_END_GOAL;
#endif
	
	// Add start point.
	stat = appendVertex(&closestStartPos, DT_STRAIGHTPATH_START, path[0], jumpTypes[0],
						straightPath, straightPathFlags, straightPathRefs,
						straightPathJumps, straightPathCount, maxStraightPath);
	if (stat != DT_IN_PROGRESS)
		return stat;
	
	if (pathSize > 1)
	{
		rdVec3D portalApex, portalLeft, portalRight;
		rdVcopy(&portalApex, &closestStartPos);
		rdVcopy(&portalLeft, &portalApex);
		rdVcopy(&portalRight, &portalApex);
		int apexIndex = 0;
		int leftIndex = 0;
		int rightIndex = 0;
		bool apexCrossTile = false;
		
		unsigned char leftPolyType = 0;
		unsigned char rightPolyType = 0;
		
		unsigned char leftJumpType = jumpTypes[0];
		unsigned char rightJumpType = jumpTypes[0];

		dtPolyRef leftPolyRef = path[0];
		dtPolyRef rightPolyRef = path[0];
		
		for (int i = 0; i < pathSize; ++i)
		{
			rdVec3D left, right;
			unsigned char toType;
			
			if (i+1 < pathSize)
			{
				// If from poly is the same as to poly, advance.
				if (path[i] == path[i+1])
					continue;

				unsigned char fromType; // fromType is ignored.

				// Next portal.
				if (dtStatusFailed(getPortalPoints(path[i], path[i+1], &left,&right, fromType, toType)))
				{
					// Failed to get portal points, in practice this means that path[i+1] is invalid polygon.
					// Clamp the end point to path[i], and return the path so far.
					
					if (dtStatusFailed(closestPointOnPolyBoundary(path[i], endPos, &closestEndPos)))
					{
						// This should only happen when the first polygon is invalid.
						return DT_FAILURE | DT_INVALID_PARAM;
					}

					const bool shouldCross = options & (DT_STRAIGHTPATH_AREA_CROSSINGS | DT_STRAIGHTPATH_ALL_CROSSINGS);

					// Append portals along the current straight path segment.
					if (shouldCross)
					{
						stat = appendPortals(apexIndex, i, &closestEndPos, path, jumpTypes,
											 straightPath, straightPathFlags, straightPathRefs,
											 straightPathJumps, straightPathCount, maxStraightPath, jumpFilter, options);
					}

					if (!shouldCross || stat == DT_IN_PROGRESS)
					{
						// Ignore status return value as we're just about to return anyway.
						appendVertex(&closestEndPos, 0, path[i], jumpTypes[i],
									 straightPath, straightPathFlags, straightPathRefs,
									 straightPathJumps, straightPathCount, maxStraightPath);

						return DT_SUCCESS | DT_PARTIAL_RESULT | ((*straightPathCount >= maxStraightPath) ? DT_BUFFER_TOO_SMALL : 0);
					}

					return stat;
				}
				
				// If starting really close to the portal, advance.
				if (i == 0)
				{
					if (checkPortalProximity(pathSize, jumpTypes, &portalApex, &left,&right))
						continue;
				}
			}
			else
			{
				// End of the path.
				rdVcopy(&left, &closestEndPos);
				rdVcopy(&right, &closestEndPos);
				
				toType = DT_POLYTYPE_GROUND;
			}

			// note(kawe): we need to be at least on the second vertex
			// in the path corridor before we can append jump vertices.
			if (i == 0)
				rdAssert(jumpTypes[i] == DT_NULL_TRAVERSE_TYPE);

			// Jump vertex.
			if (i > 0 && jumpTypes[i] != DT_NULL_TRAVERSE_TYPE && !dtIsTraverseTypeOffMesh(jumpTypes[i]))
			{
				if (rdTriArea2D(&portalLeft, &portalRight, &left) < 0.0f ||
					rdTriArea2D(&portalLeft, &portalRight, &right) < 0.0f)
				{
					rdVec3D jumpPt;
					rdVlerp(&jumpPt, &portalLeft, &portalRight, 0.5f);

					// Append 'from' jump vertex.
					stat = appendVertex(&jumpPt, 0, path[i-1], jumpTypes[i],
						straightPath, straightPathFlags, straightPathRefs,
						straightPathJumps, straightPathCount, maxStraightPath);

					const bool contAfterJmp = (options & DT_STRAIGHTPATH_CONTINUE_AFTER_JUMP) != 0;

					// Append portals along the current straight path segment.
					if (!(stat & DT_BUFFER_TOO_SMALL))
					{
						stat = appendPortalVertex(&jumpPt, &closestEndPos, i, path,
												  straightPath, straightPathFlags, straightPathRefs,
												  straightPathJumps, straightPathCount, maxStraightPath);

						if ((contAfterJmp && stat != DT_IN_PROGRESS) || (!contAfterJmp && stat == DT_IN_PROGRESS))
							return DT_SUCCESS;
					}

					if (!contAfterJmp)
						return stat;
				}
			}
			
			enum AddStraightPathSegments
			{
				SP_SEGMENT_ADD_LEFT, SP_SEGMENT_ADD_RIGHT,
				SP_SEGMENT_COUNT, // Left is checked and added first.
			}; 
			bool addSegments[SP_SEGMENT_COUNT] = { false, false };

			// Left vertex.
			if (rdTriArea2D(&portalApex, &portalLeft, &left) <= 0.0f)
			{
				if (rdVequal(&portalApex, &portalLeft) || rdVequal(&left, &portalLeft) || rdTriArea2D(&portalApex, &portalRight, &left) >= 0.0f)
				{
					rdVcopy(&portalLeft, &left);
					leftPolyRef = (i+1 < pathSize) ? path[i+1] : 0;
					leftJumpType = (i+1 < pathSize) ? jumpTypes[i+1] : DT_NULL_TRAVERSE_TYPE;
					leftPolyType = toType;
					leftIndex = i;
				}
				else
				{
					// note(kawe): we must delay 'right' after 'left' since we
					// changed the coordinate system (XZY -> XYZ, Z is up now),
					// 'left' however must know its poly, jump and index in
					// case its vertex lies outside the current funnel.
					addSegments[SP_SEGMENT_ADD_RIGHT] = true;
				}
			}
			
			// Right vertex.
			if (rdTriArea2D(&portalApex, &portalRight, &right) >= 0.0f)
			{
				if (rdVequal(&portalApex, &portalRight) || rdVequal(&right, &portalRight) || rdTriArea2D(&portalApex, &portalLeft, &right) <= 0.0f)
				{
					rdVcopy(&portalRight, &right);
					rightPolyRef = (i+1 < pathSize) ? path[i+1] : 0;
					rightJumpType = (i+1 < pathSize) ? jumpTypes[i+1] : DT_NULL_TRAVERSE_TYPE;
					rightPolyType = toType;
					rightIndex = i;
				}
				else
				{
					// note(kawe): see the same branch for the right vertex.
					addSegments[SP_SEGMENT_ADD_LEFT] = true;
				}
			}

			bool shouldContinue = false;

			for (int segIdx = 0; segIdx < SP_SEGMENT_COUNT; segIdx++)
			{
				const bool add = addSegments[segIdx];

				if (!add)
					continue;

				const bool isLeft = segIdx == SP_SEGMENT_ADD_LEFT;

				const rdVec3D* portalTarget = isLeft ? &portalLeft : &portalRight;
				const dtPolyRef targetPolyRef = isLeft ? leftPolyRef : rightPolyRef;
				const unsigned char targetJumpType = isLeft ? leftJumpType : rightJumpType;
				const unsigned char targetPolyType = isLeft ? leftPolyType : rightPolyType;
				const int targetIndex = isLeft ? leftIndex : rightIndex;

				// Append portals along the current straight path segment.
				if (options & (DT_STRAIGHTPATH_AREA_CROSSINGS | DT_STRAIGHTPATH_ALL_CROSSINGS))
				{
					stat = appendPortals(apexIndex, targetIndex, portalTarget, path, jumpTypes,
										 straightPath, straightPathFlags, straightPathRefs,
										 straightPathJumps, straightPathCount, maxStraightPath, jumpFilter, options);
					if (stat != DT_IN_PROGRESS)
						return stat;
				}

				if (!apexCrossTile && (m_nav->decodePolyIdTile(path[apexIndex]) != m_nav->decodePolyIdTile(path[targetIndex])))
					apexCrossTile = true;

				rdVcopy(&portalApex, portalTarget);
				apexIndex = targetIndex;
				
				unsigned char flags = 0;
				if (!targetPolyRef)
					flags = endFlags;
				else if (targetPolyType == DT_POLYTYPE_OFFMESH_CONNECTION)
					flags = DT_STRAIGHTPATH_OFFMESH_CONNECTION;

				// Append or update vertex
				stat = appendVertex(&portalApex, flags, targetPolyRef, targetJumpType,
									straightPath, straightPathFlags, straightPathRefs,
									straightPathJumps, straightPathCount, maxStraightPath);
				if (stat != DT_IN_PROGRESS)
					return stat;

				// Append portals along the current straight path segment.
				if (i > 0 && jumpTypes[i] < DT_MAX_TRAVERSE_TYPES)
				{
					stat = appendPortalVertex(&portalApex, &closestEndPos, i, path,
											  straightPath, straightPathFlags, straightPathRefs,
											  straightPathJumps, straightPathCount, maxStraightPath);

					if (stat != DT_SUCCESS && stat != DT_IN_PROGRESS)
						return stat;

					if (!(options & DT_STRAIGHTPATH_CONTINUE_AFTER_JUMP))
						return DT_SUCCESS;
				}
				
				rdVcopy(&portalLeft, &portalApex);
				rdVcopy(&portalRight, &portalApex);
				leftIndex = apexIndex;
				rightIndex = apexIndex;
				
				// Restart.
				shouldContinue = true;
				i = apexIndex;

				break;
			}

			if (shouldContinue)
				continue;

			// Prevent occasional errors when portalApex, portalLeft and portalRight
			// form an angle larger than 180 degrees.
			// See: https://github.com/recastnavigation/recastnavigation/issues/515
			// And: https://github.com/recastnavigation/recastnavigation/issues/735
			if (apexCrossTile && rdTriArea2D(&portalApex, &portalLeft, &portalRight) < 0.0f)
			{
				const float l = rdVdist2DSqr(&portalApex, &portalLeft);
				const float r = rdVdist2DSqr(&portalApex, &portalRight);
				if (l < r)
				{
					rdVcopy(&portalApex, &portalLeft);
					apexIndex = leftIndex;
				}
				else if (r < l)
				{
					rdVcopy(&portalApex, &portalRight);
					apexIndex = rightIndex;
				}
			}

			apexCrossTile = false;
		}

		// Append portals along the current straight path segment.
		if (options & (DT_STRAIGHTPATH_AREA_CROSSINGS | DT_STRAIGHTPATH_ALL_CROSSINGS))
		{
			stat = appendPortals(apexIndex, pathSize-1, &closestEndPos, path, jumpTypes,
								 straightPath, straightPathFlags, straightPathRefs,
								 straightPathJumps, straightPathCount, maxStraightPath, jumpFilter, options);
			if (stat != DT_IN_PROGRESS)
				return stat;
		}
	}

	// Ignore status return value as we're just about to return anyway.
	appendVertex(&closestEndPos, endFlags, 0, DT_NULL_TRAVERSE_TYPE,
						straightPath, straightPathFlags, straightPathRefs,
						straightPathJumps, straightPathCount, maxStraightPath);
	
	return DT_SUCCESS | ((*straightPathCount >= maxStraightPath) ? DT_BUFFER_TOO_SMALL : 0);
}

dtStatus dtNavMeshQuery::findStraightPath(const rdVec3D* startPos, const rdVec3D* endPos, const dtPolyRef* path,
										  const unsigned char* jumpTypes, const int pathSize, dtStraightPathResult& result,
										  const int maxStraightPath, const int jumpFilter, const int options) const
{
	const dtStatus stat = findStraightPath(startPos, endPos, path, jumpTypes, pathSize,
										   result.path, result.flags, result.polys, result.jumps, &result.pathCount,
										   maxStraightPath, jumpFilter, options);

	if (dtStatusSucceed(stat))
		result.pathEndIsGoal = (result.flags[result.pathCount-1] & DT_STRAIGHTPATH_END_GOAL) != 0;

	return stat;
}

/// @par
///
/// This method is optimized for small delta movement and a small number of 
/// polygons. If used for too great a distance, the result set will form an 
/// incomplete path.
///
/// @p resultPos will equal the @p endPos if the end is reached. 
/// Otherwise the closest reachable position will be returned.
/// 
/// @p resultPos is not projected onto the surface of the navigation 
/// mesh. Use #getPolyHeight if this is needed.
///
/// This method treats the end position in the same manner as 
/// the #raycast method. (As a 2D point.) See that method's documentation 
/// for details.
/// 
/// If the @p visited array is too small to hold the entire result set, it will 
/// be filled as far as possible from the start position toward the end 
/// position.
///
dtStatus dtNavMeshQuery::moveAlongSurface(dtPolyRef startRef, const rdVec3D* startPos, const rdVec3D* endPos,
										  const dtQueryFilter* filter, rdVec3D* resultPos, dtPolyRef* visitedPolys,
										  int* visitedCount, const int maxVisitedSize, const unsigned char options) const
{
	rdAssert(m_nav);
	rdAssert(m_nodePool);
	rdAssert(m_tinyNodePool);

	if (!visitedCount)
		return DT_FAILURE | DT_INVALID_PARAM;

	*visitedCount = 0;

	if (!m_nav->isValidPolyRef(startRef) ||
		!startPos || !rdVisfinite(startPos) ||
		!endPos || !rdVisfinite(endPos) ||
		!filter || !resultPos || maxVisitedSize <= 0)
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}
	
	dtStatus status = DT_SUCCESS;
	
	static const int MAX_STACK = 256;
	dtNode* stack[MAX_STACK];
	int nstack = 0;

	dtNodePool* nodePool = (options & DT_MOVEALONGSURFACE_USE_REGULAR_NODE_POOL) ? m_nodePool : m_tinyNodePool;
	nodePool->clear();
	
	dtNode* startNode = nodePool->getNode(startRef);
	startNode->pidx = 0;
	startNode->cost = 0;
	startNode->total = 0;
	startNode->id = startRef;
	startNode->flags = DT_NODE_CLOSED;
	stack[nstack++] = startNode;
	
	dtNode* bestNode = 0;
	rdVec3D bestPos = *startPos;
	float bestDist = FLT_MAX;
	
	// Search constraints
	rdVec3D searchPos;
	rdVlerp(&searchPos, startPos, endPos, 0.5f);
	const float searchRadSqr = rdSqr(rdVdist(startPos, endPos)/2.0f + 0.001f);
	
	rdVec3D verts[RD_VERTS_PER_POLYGON];
	
	while (nstack)
	{
		// Pop front.
		dtNode* curNode = stack[0];
		for (int i = 0; i < nstack-1; ++i)
			stack[i] = stack[i+1];
		nstack--;
		
		// Get poly and tile.
		// The API input has been checked already, skip checking internal data.
		const dtPolyRef curRef = curNode->id;
		const dtMeshTile* curTile = 0;
		const dtPoly* curPoly = 0;
		m_nav->getTileAndPolyByRefUnsafe(curRef, &curTile, &curPoly);			
		
		// Collect vertices.
		const int nverts = curPoly->vertCount;
		for (int i = 0; i < nverts; ++i)
			rdVcopy(&verts[i], &curTile->verts[curPoly->verts[i]]);
		
		// If target is inside the poly, stop search.
		if (rdPointInPolygon(endPos, verts, nverts))
		{
			bestNode = curNode;
			rdVcopy(&bestPos, endPos);
			break;
		}
		
		// Find wall edges and find nearest point inside the walls.
		for (int i = 0, j = (int)curPoly->vertCount-1; i < (int)curPoly->vertCount; j = i++)
		{
			// Find links to neighbours.
			static const int MAX_NEIS = 8;
			int nneis = 0;
			dtPolyRef neis[MAX_NEIS];
			
			if (curPoly->neis[j] & DT_EXT_LINK)
			{
				// Tile border.
				for (unsigned int k = curPoly->firstLink; k != DT_NULL_LINK; k = curTile->links[k].next)
				{
					const dtLink* link = &curTile->links[k];
					if (link->edge == j)
					{
						if (link->ref != 0)
						{
							const dtMeshTile* neiTile = 0;
							const dtPoly* neiPoly = 0;
							m_nav->getTileAndPolyByRefUnsafe(link->ref, &neiTile, &neiPoly);

							if (filter->passFilter(link->ref, neiTile, neiPoly) && 
								filter->traverseFilter(link, neiTile, neiPoly))
							{
								if (nneis < MAX_NEIS)
									neis[nneis++] = link->ref;
							}
						}
					}
				}
			}
			else if (curPoly->neis[j])
			{
				const unsigned int idx = (unsigned int)(curPoly->neis[j]-1);
				const dtPolyRef ref = m_nav->getPolyRefBase(curTile) | idx;
				if (filter->passFilter(ref, curTile, &curTile->polys[idx]))
				{
					// Internal edge, encode id.
					neis[nneis++] = ref;
				}
			}
			
			if (!nneis)
			{
				// Wall edge, calc distance.
				const rdVec3D* vj = &verts[j];
				const rdVec3D* vi = &verts[i];
				float tseg;
				const float distSqr = rdDistancePtSegSqr2D(endPos, vj, vi, tseg);
				if (distSqr < bestDist)
				{
                    // Update nearest distance.
					rdVlerp(&bestPos, vj,vi, tseg);
					bestDist = distSqr;
					bestNode = curNode;
				}
			}
			else
			{
				for (int k = 0; k < nneis; ++k)
				{
					// Skip if no node can be allocated.
					dtNode* neighbourNode = nodePool->getNode(neis[k]);
					if (!neighbourNode)
						continue;
					// Skip if already visited.
					if (neighbourNode->flags & DT_NODE_CLOSED)
						continue;
					
					// Skip the link if it is too far from search constraint.
					// TODO: Maybe should use getPortalPoints(), but this one is way faster.
					const rdVec3D* vj = &verts[j];
					const rdVec3D* vi = &verts[i];
					float tseg;
					float distSqr = rdDistancePtSegSqr2D(&searchPos, vj, vi, tseg);
					if (distSqr > searchRadSqr)
						continue;
					
					// Mark as the node as visited and push to queue.
					if (nstack < MAX_STACK)
					{
						neighbourNode->pidx = nodePool->getNodeIdx(curNode);
						neighbourNode->flags |= DT_NODE_CLOSED;
						stack[nstack++] = neighbourNode;
					}
				}
			}
		}
	}
	
	int n = 0;
	if (bestNode)
	{
		if (!(options & DT_MOVEALONGSURFACE_DONT_VISIT_POLYGONS))
		{
			rdAssert(visitedPolys);

			// Reverse the path.
			dtNode* prev = 0;
			dtNode* node = bestNode;
			do
			{
				dtNode* next = nodePool->getNodeAtIdx(node->pidx);
				node->pidx = nodePool->getNodeIdx(prev);
				prev = node;
				node = next;
			} while (node);

			// Store result
			node = prev;
			do
			{
				visitedPolys[n] = node->id;

				if (++n >= maxVisitedSize)
				{
					status |= DT_BUFFER_TOO_SMALL;
					break;
				}
				node = nodePool->getNodeAtIdx(node->pidx);
			} while (node);
		}

		if (options & DT_MOVEALONGSURFACE_USE_POLY_HEIGHT_FOR_RESULT)
		{
			rdVec3D closestPoint; // If it fails we just use the currently stored height.
			if (dtStatusSucceed(closestPointOnPoly(bestNode->id, &bestPos, &closestPoint, 0)))
				bestPos.z = closestPoint.z;
		}
	}

	*visitedCount = n;
	rdVcopy(resultPos, &bestPos);

	return status;
}


dtStatus dtNavMeshQuery::getPortalPoints(dtPolyRef from, dtPolyRef to, rdVec3D* left, rdVec3D* right,
										 unsigned char& fromType, unsigned char& toType) const
{
	rdAssert(m_nav);
	
	const dtMeshTile* fromTile = 0;
	const dtPoly* fromPoly = 0;
	if (dtStatusFailed(m_nav->getTileAndPolyByRef(from, &fromTile, &fromPoly)))
		return DT_FAILURE | DT_INVALID_PARAM;
	fromType = fromPoly->getType();

	const dtMeshTile* toTile = 0;
	const dtPoly* toPoly = 0;
	if (dtStatusFailed(m_nav->getTileAndPolyByRef(to, &toTile, &toPoly)))
		return DT_FAILURE | DT_INVALID_PARAM;
	toType = toPoly->getType();
		
	return getPortalPoints(from, fromPoly, fromTile, to, toPoly, toTile, 0, left, right);
}

// Returns portal points between two polygons.
dtStatus dtNavMeshQuery::getPortalPoints(dtPolyRef from, const dtPoly* fromPoly, const dtMeshTile* fromTile,
										 dtPolyRef to, const dtPoly* toPoly, const dtMeshTile* toTile,
										 const dtLink* inLink, rdVec3D* left, rdVec3D* right) const
{
	const dtLink* link = inLink;
	if (!link)
	{
		// Find the link that points to the 'to' polygon.
		for (unsigned int i = fromPoly->firstLink; i != DT_NULL_LINK; i = fromTile->links[i].next)
		{
			if (fromTile->links[i].ref == to)
			{
				link = &fromTile->links[i];
				break;
			}
		}
	}
	if (!link)
		return DT_FAILURE | DT_INVALID_PARAM;

	// Copy the 'to' vertices over, this traverse type is reserved and
	// used by the engine for special activities such as ziplines.
	if (dtIsTraverseTypeReserved(link->traverseType))
	{
		rdVcopy(left, &toTile->verts[toPoly->verts[0]]);
		rdVcopy(right, &toTile->verts[toPoly->verts[0]]);
		return DT_SUCCESS;
	}
	
	// Handle off-mesh connections.
	if (fromPoly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
	{
		// Find link that points to first vertex.
		for (unsigned int i = fromPoly->firstLink; i != DT_NULL_LINK; i = fromTile->links[i].next)
		{
			if (fromTile->links[i].ref == to)
			{
				const int v = fromTile->links[i].edge;
				rdVcopy(left, &fromTile->verts[fromPoly->verts[v]]);
				rdVcopy(right, &fromTile->verts[fromPoly->verts[v]]);
				return DT_SUCCESS;
			}
		}
		return DT_FAILURE | DT_INVALID_PARAM;
	}
	
	if (toPoly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
	{
		for (unsigned int i = toPoly->firstLink; i != DT_NULL_LINK; i = toTile->links[i].next)
		{
			if (toTile->links[i].ref == from)
			{
				const int v = toTile->links[i].edge;
				rdVcopy(left, &toTile->verts[toPoly->verts[v]]);
				rdVcopy(right, &toTile->verts[toPoly->verts[v]]);
				return DT_SUCCESS;
			}
		}
		return DT_FAILURE | DT_INVALID_PARAM;
	}
	
	// Find portal vertices.
	const int v0 = fromPoly->verts[link->edge];
	const int v1 = fromPoly->verts[(link->edge+1) % (int)fromPoly->vertCount];
	rdVcopy(left, &fromTile->verts[v0]);
	rdVcopy(right, &fromTile->verts[v1]);
	
	// If the link is at tile boundary, dtClamp the vertices to
	// the link width.
	if (link->side != 0xff)
	{
		// Unpack portal limits.
		if (link->bmin != 0 || link->bmax != 255)
		{
			const float s = 1.0f/255.0f;
			const float tmin = link->bmin*s;
			const float tmax = link->bmax*s;
			rdVlerp(left, &fromTile->verts[v0], &fromTile->verts[v1], tmin);
			rdVlerp(right, &fromTile->verts[v0], &fromTile->verts[v1], tmax);
		}
	}
	
	return DT_SUCCESS;
}

// Returns edge mid point between two polygons.
dtStatus dtNavMeshQuery::getEdgeMidPoint(dtPolyRef from, dtPolyRef to, rdVec3D* mid) const
{
	rdVec3D left, right;
	unsigned char fromType, toType;
	if (dtStatusFailed(getPortalPoints(from, to, &left,&right, fromType, toType)))
		return DT_FAILURE | DT_INVALID_PARAM;
	rdVsad(mid, &left,&right, 0.5f);
	return DT_SUCCESS;
}

dtStatus dtNavMeshQuery::getEdgeMidPoint(dtPolyRef from, const dtPoly* fromPoly, const dtMeshTile* fromTile,
										 dtPolyRef to, const dtPoly* toPoly, const dtMeshTile* toTile,
										 rdVec3D* mid) const
{
	rdVec3D left, right;
	if (dtStatusFailed(getPortalPoints(from, fromPoly, fromTile, to, toPoly, toTile, 0, &left,&right)))
		return DT_FAILURE | DT_INVALID_PARAM;
	rdVsad(mid, &left,&right, 0.5f);
	return DT_SUCCESS;
}

dtStatus dtNavMeshQuery::getEdgeNormal(dtPolyRef from, dtPolyRef to, rdVec2D* norm) const
{
	rdVec3D left, right;
	unsigned char fromType, toType;
	if (dtStatusFailed(getPortalPoints(from, to, &left,&right, fromType, toType)))
		return DT_FAILURE | DT_INVALID_PARAM;
	rdVec3D dir;
	rdVsub(&dir, &right,&left);
	rdCalcEdgeNormal2D(&dir, norm);
	return DT_SUCCESS;
}

dtStatus dtNavMeshQuery::getEdgeNormal(dtPolyRef from, const dtPoly* fromPoly, const dtMeshTile* fromTile,
										 dtPolyRef to, const dtPoly* toPoly, const dtMeshTile* toTile,
										 rdVec2D* norm) const
{
	rdVec3D left, right;
	if (dtStatusFailed(getPortalPoints(from, fromPoly, fromTile, to, toPoly, toTile, 0, &left,&right)))
		return DT_FAILURE | DT_INVALID_PARAM;
	rdVec3D dir;
	rdVsub(&dir, &right,&left);
	rdCalcEdgeNormal2D(&dir, norm);
	return DT_SUCCESS;
}

/// @par
///
/// This method is meant to be used for quick, short distance checks.
///
/// If the path array is too small to hold the result, it will be filled as 
/// far as possible from the start position toward the end position.
///
/// <b>Using the Hit Parameter (t)</b>
/// 
/// If the hit parameter is a very high value (FLT_MAX), then the ray has hit 
/// the end position. In this case the path represents a valid corridor to the 
/// end position and the value of @p hitNormal is undefined.
///
/// If the hit parameter is zero, then the start position is on the wall that 
/// was hit and the value of @p hitNormal is undefined.
///
/// If 0 < t < 1.0 then the following applies:
///
/// @code
/// distanceToHitBorder = distanceToEndPosition * t
/// hitPoint = startPos + (endPos - startPos) * t
/// @endcode
///
/// <b>Use Case Restriction</b>
///
/// The raycast ignores the z-value of the end position. (2D check.) This 
/// places significant limits on how it can be used. For example:
///
/// Consider a scene where there is a main floor with a second floor balcony 
/// that hangs over the main floor. So the first floor mesh extends below the 
/// balcony mesh. The start position is somewhere on the first floor. The end 
/// position is on the balcony.
///
/// The raycast will search toward the end position along the first floor mesh. 
/// If it reaches the end position's xy-coordinates it will indicate FLT_MAX
/// (no wall hit), meaning it reached the end position. This is one example of why
/// this method is meant for short distance checks.
///
dtStatus dtNavMeshQuery::raycast(dtPolyRef startRef, const rdVec3D* startPos, const rdVec3D* endPos,
								 const dtQueryFilter* filter, float* t, rdVec3D* hitNormal, dtPolyRef* path,
								 int* pathCount, const int maxPath) const
{
	dtRaycastHit hit;
	hit.path = path;
	hit.maxPath = maxPath;

	const dtStatus status = raycast(startRef, startPos, endPos, filter, 0, &hit);
	
	*t = hit.t;
	if (hitNormal)
		*hitNormal = hit.hitNormal;
	if (pathCount)
		*pathCount = hit.pathCount;

	return status;
}


/// @par
///
/// This method is meant to be used for quick, short distance checks.
///
/// If the path array is too small to hold the result, it will be filled as 
/// far as possible from the start position toward the end position.
///
/// <b>Using the Hit Parameter t of RaycastHit</b>
/// 
/// If the hit parameter is a very high value (FLT_MAX), then the ray has hit 
/// the end position. In this case the path represents a valid corridor to the 
/// end position and the value of @p hitNormal is undefined.
///
/// If the hit parameter is zero, then the start position is on the wall that 
/// was hit and the value of @p hitNormal is undefined.
///
/// If 0 < t < 1.0 then the following applies:
///
/// @code
/// distanceToHitBorder = distanceToEndPosition * t
/// hitPoint = startPos + (endPos - startPos) * t
/// @endcode
///
/// <b>Use Case Restriction</b>
///
/// The raycast ignores the z-value of the end position. (2D check.) This 
/// places significant limits on how it can be used. For example:
///
/// Consider a scene where there is a main floor with a second floor balcony 
/// that hangs over the main floor. So the first floor mesh extends below the 
/// balcony mesh. The start position is somewhere on the first floor. The end 
/// position is on the balcony.
///
/// The raycast will search toward the end position along the first floor mesh. 
/// If it reaches the end position's xy-coordinates it will indicate FLT_MAX
/// (no wall hit), meaning it reached the end position. This is one example of why
/// this method is meant for short distance checks.
///
dtStatus dtNavMeshQuery::raycast(dtPolyRef startRef, const rdVec3D* startPos, const rdVec3D* endPos,
								 const dtQueryFilter* filter, const unsigned int options,
								 dtRaycastHit* hit, dtPolyRef prevRef) const
{
	rdAssert(m_nav);

	if (!hit)
		return DT_FAILURE | DT_INVALID_PARAM;

	hit->t = 0;
	hit->pathCount = 0;
	hit->pathCost = 0;

	// Validate input
	if (!m_nav->isValidPolyRef(startRef) ||
		!startPos || !rdVisfinite(startPos) ||
		!endPos || !rdVisfinite(endPos) ||
		!filter ||
		(prevRef && !m_nav->isValidPolyRef(prevRef)))
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}
	
	rdVec3D dir, curPos, lastPos;
	rdVec3D verts[RD_VERTS_PER_POLYGON+1];
	int n = 0;

	rdVcopy(&curPos, startPos);
	rdVsub(&dir, endPos, startPos);
	hit->hitNormal.init(0, 0, 0);

	dtStatus status = DT_SUCCESS;

	const dtMeshTile* prevTile, *tile, *nextTile;
	const dtPoly* prevPoly, *poly, *nextPoly;
	dtPolyRef curRef;

	// The API input has been checked already, skip checking internal data.
	curRef = startRef;
	tile = 0;
	poly = 0;
	m_nav->getTileAndPolyByRefUnsafe(curRef, &tile, &poly);
	nextTile = prevTile = tile;
	nextPoly = prevPoly = poly;
	if (prevRef)
		m_nav->getTileAndPolyByRefUnsafe(prevRef, &prevTile, &prevPoly);

	while (curRef)
	{
		// Cast ray against current polygon.
		
		// Collect vertices.
		int nv = 0;
		for (int i = 0; i < (int)poly->vertCount; ++i)
		{
			rdVcopy(&verts[nv], &tile->verts[poly->verts[i]]);
			nv++;
		}
		
		float tmin, tmax;
		int segMin, segMax;
		if (!rdIntersectSegmentPoly2D(startPos, endPos, verts, nv, tmin, tmax, segMin, segMax))
		{
			// Could not hit the polygon, keep the old t and report hit.
			hit->pathCount = n;
			return status;
		}

		hit->startRef = startRef;

		// Keep track of furthest t so far.
		if (tmax > hit->t)
			hit->t = tmax;
		
		// Store visited polygons.
		if (n < hit->maxPath)
			hit->path[n++] = curRef;
		else
			status |= DT_BUFFER_TOO_SMALL;

		// Ray end is completely inside the polygon.
		if (segMax == -1)
		{
			hit->t = FLT_MAX;
			hit->pathCount = n;
			
			// add the cost
			if (options & DT_RAYCAST_USE_COSTS)
				hit->pathCost += filter->getCost(&curPos, endPos, 0, prevRef, prevTile, prevPoly, curRef, tile, poly, curRef, tile, poly);
			return status;
		}

		// Follow neighbours.
		dtPolyRef nextRef = 0;
		
		for (unsigned int i = poly->firstLink; i != DT_NULL_LINK; i = tile->links[i].next)
		{
			const dtLink* link = &tile->links[i];
			
			// Find link which contains this edge.
			if ((int)link->edge != segMax || link->hasTraverseType())
				continue;
			
			// Get pointer to the next polygon.
			nextTile = 0;
			nextPoly = 0;
			m_nav->getTileAndPolyByRefUnsafe(link->ref, &nextTile, &nextPoly);
			
			if (options & DT_RAYCAST_SKIP_OFFMESH_CONNECTION)
			{
				// Skip off-mesh connections.
				if (nextPoly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
					continue;
			}

			// Skip links based on filter.
			if (!filter->passFilter(link->ref, nextTile, nextPoly))
				continue;
			
			// If the link is internal, just return the ref.
			if (link->side == 0xff)
			{
				nextRef = link->ref;
				break;
			}
			
			// If the link is at tile boundary,
			
			// Check if the link spans the whole edge, and accept.
			if (link->bmin == 0 && link->bmax == 255)
			{
				nextRef = link->ref;
				break;
			}
			
			// Check for partial edge links.
			const int v0 = poly->verts[link->edge];
			const int v1 = poly->verts[(link->edge+1) % poly->vertCount];
			const rdVec3D* left = &tile->verts[v0];
			const rdVec3D* right = &tile->verts[v1];
			
			// Check that the intersection lies inside the link portal.
			if (link->side == 0 || link->side == 4)
			{
				// Calculate link size.
				const float s = 1.0f/255.0f;
				float lmin = left->y + (right->y - left->y)*(link->bmin*s);
				float lmax = left->y + (right->y - left->y)*(link->bmax*s);
				if (lmin > lmax) rdSwap(lmin, lmax);
				
				// Find Y intersection.
				const float y = startPos->y + (endPos->y-startPos->y)*tmax;
				if (y >= lmin && y <= lmax)
				{
					nextRef = link->ref;
					break;
				}
			}
			else if (link->side == 2 || link->side == 6)
			{
				// Calculate link size.
				const float s = 1.0f/255.0f;
				float lmin = left->x + (right->x - left->x)*(link->bmin*s);
				float lmax = left->x + (right->x - left->x)*(link->bmax*s);
				if (lmin > lmax) rdSwap(lmin, lmax);
				
				// Find X intersection.
				const float x = startPos->x + (endPos->x-startPos->x)*tmax;
				if (x >= lmin && x <= lmax)
				{
					nextRef = link->ref;
					break;
				}
			}
		}
		
		// add the cost
		if (options & DT_RAYCAST_USE_COSTS)
		{
			// compute the intersection point at the furthest end of the polygon
			// and correct the height (since the raycast moves in 2d)
			rdVcopy(&lastPos, &curPos);
			rdVmad(&curPos, startPos, &dir, hit->t);
			rdVec3D* e1 = &verts[segMax];
			rdVec3D* e2 = &verts[((segMax+1)%nv)];
			rdVec3D eDir, diff;
			rdVsub(&eDir, e2, e1);
			rdVsub(&diff, &curPos, e1);
			float s = rdSqr(eDir.x) > rdSqr(eDir.y) ? diff.x / eDir.x : diff.y / eDir.y;
			curPos.z = e1->z + eDir.z * s;

			hit->pathCost += filter->getCost(&lastPos, &curPos, 0, prevRef, prevTile, prevPoly, curRef, tile, poly, nextRef, nextTile, nextPoly);
		}

		if (!nextRef)
		{
			// No neighbour, we hit a wall.
			
			// Calculate hit normal.
			const int a = segMax;
			const int b = segMax+1 < nv ? segMax+1 : 0;
			const rdVec3D* va = &verts[a];
			const rdVec3D* vb = &verts[b];
			const float dx = vb->x - va->x;
			const float dy = vb->y - va->y;
			hit->hitNormal.x = -dy;
			hit->hitNormal.y = dx;
			hit->hitNormal.z = 0.0f;
			rdVnormalize(&hit->hitNormal);
			
			hit->pathCount = n;
			return status;
		}

		// No hit, advance to neighbour polygon.
		prevRef = curRef;
		curRef = nextRef;
		prevTile = tile;
		tile = nextTile;
		prevPoly = poly;
		poly = nextPoly;

		if (status & DT_BUFFER_TOO_SMALL)
		{
			status |= DT_PARTIAL_RESULT;
			break;
		}
	}
	
	hit->pathCount = n;
	
	return status;
}

/// @par
///
/// At least one result array must be provided.
///
/// The order of the result set is from least to highest cost to reach the polygon.
///
/// A common use case for this method is to perform Dijkstra searches. 
/// Candidate polygons are found by searching the graph beginning at the start polygon.
///
/// If a polygon is not found via the graph search, even if it intersects the 
/// search circle, it will not be included in the result set. For example:
///
/// polyA is the start polygon.
/// polyB shares an edge with polyA. (Is adjacent.)
/// polyC shares an edge with polyB, but not with polyA
/// Even if the search circle overlaps polyC, it will not be included in the 
/// result set unless polyB is also in the set.
/// 
/// The value of the center point is used as the start position for cost 
/// calculations. It is not projected onto the surface of the mesh, so its 
/// z-value will affect the costs.
///
/// Intersection tests occur in 2D. All polygons and the search circle are 
/// projected onto the xy-plane. So the z-value of the center point does not 
/// affect intersection tests.
///
/// If the result arrays are to small to hold the entire result set, they will be 
/// filled to capacity.
/// 
dtStatus dtNavMeshQuery::findPolysAroundCircle(dtPolyRef startRef, const rdVec3D* centerPos, const float radius,
											   const dtQueryFilter* filter,
											   dtPolyRef* resultRef, dtPolyRef* resultParent, float* resultCost,
											   int* resultCount, const int maxResult) const
{
	rdAssert(m_nav);
	rdAssert(m_nodePool);
	rdAssert(m_openList);

	if (!resultCount)
		return DT_FAILURE | DT_INVALID_PARAM;

	*resultCount = 0;

	if (!m_nav->isValidPolyRef(startRef) ||
		!centerPos || !rdVisfinite(centerPos) ||
		radius < 0 || !rdMathIsfinite(radius) ||
		!filter || maxResult < 0)
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}
	
	m_nodePool->clear();
	m_openList->clear();
	
	dtNode* startNode = m_nodePool->getNode(startRef);
	rdVcopy(&startNode->pos, centerPos);
	startNode->pidx = 0;
	startNode->cost = 0;
	startNode->total = 0;
	startNode->id = startRef;
	startNode->flags = DT_NODE_OPEN;
	m_openList->push(startNode);
	
	dtStatus status = DT_SUCCESS;
	
	int n = 0;
	
	const float radiusSqr = rdSqr(radius);
	
	while (!m_openList->empty())
	{
		dtNode* bestNode = m_openList->pop();
		bestNode->flags &= ~DT_NODE_OPEN;
		bestNode->flags |= DT_NODE_CLOSED;
		
		// Get poly and tile.
		// The API input has been checked already, skip checking internal data.
		const dtPolyRef bestRef = bestNode->id;
		const dtMeshTile* bestTile = 0;
		const dtPoly* bestPoly = 0;
		m_nav->getTileAndPolyByRefUnsafe(bestRef, &bestTile, &bestPoly);
		
		// Get parent poly and tile.
		dtPolyRef parentRef = 0;
		const dtMeshTile* parentTile = 0;
		const dtPoly* parentPoly = 0;
		if (bestNode->pidx)
			parentRef = m_nodePool->getNodeAtIdx(bestNode->pidx)->id;

		// Default implementation of dtQueryFilter::getCost only needs the start, end
		// positions and the link. The tile and poly are therefore not used. Only
		// obtain them when virtual query filter function overrides are enabled.
#ifdef DT_VIRTUAL_QUERYFILTER
		if (parentRef)
			m_nav->getTileAndPolyByRefUnsafe(parentRef, &parentTile, &parentPoly);
#endif // DT_VIRTUAL_QUERYFILTER

		// Make sure our polygon isn't too small, reject those.
		if (!(bestPoly->flags & DT_POLYFLAGS_TOO_SMALL))
		{
			if (n < maxResult)
			{
				if (resultRef)
					resultRef[n] = bestRef;
				if (resultParent)
					resultParent[n] = parentRef;
				if (resultCost)
					resultCost[n] = bestNode->total;
				++n;
			}
			else
			{
				status |= DT_BUFFER_TOO_SMALL;
			}
		}
		
		for (unsigned int i = bestPoly->firstLink; i != DT_NULL_LINK; i = bestTile->links[i].next)
		{
			const dtLink* link = &bestTile->links[i];
			dtPolyRef neighbourRef = link->ref;
			// Skip invalid neighbours and do not follow back to parent.
			if (!neighbourRef || neighbourRef == parentRef)
				continue;
			
			// Expand to neighbour
			const dtMeshTile* neighbourTile = 0;
			const dtPoly* neighbourPoly = 0;
			m_nav->getTileAndPolyByRefUnsafe(neighbourRef, &neighbourTile, &neighbourPoly);
		
			// Do not advance if the polygon is excluded by the filter.
			if (!filter->passFilter(neighbourRef, neighbourTile, neighbourPoly))
				continue;

			if (!filter->traverseFilter(link, neighbourTile, neighbourPoly))
				continue;
			
			// Find edge and calc distance to the edge.
			rdVec3D va, vb;
			if (!getPortalPoints(bestRef, bestPoly, bestTile, neighbourRef, neighbourPoly, neighbourTile, link, &va,&vb))
				continue;
			
			// If the circle is not touching the next polygon, skip it.
			float tseg;
			float distSqr = rdDistancePtSegSqr2D(centerPos, &va,&vb, tseg);
			if (distSqr > radiusSqr)
				continue;
			
			dtNode* neighbourNode = m_nodePool->getNode(neighbourRef);
			if (!neighbourNode)
			{
				status |= DT_OUT_OF_NODES;
				continue;
			}
				
			if (neighbourNode->flags & DT_NODE_CLOSED)
				continue;
			
			// Cost
			if (neighbourNode->flags == 0)
				rdVlerp(&neighbourNode->pos, &va,&vb, 0.5f);
			
			const float cost = filter->getCost(
				&bestNode->pos, &neighbourNode->pos, nullptr,
				parentRef, parentTile, parentPoly,
				bestRef, bestTile, bestPoly,
				neighbourRef, neighbourTile, neighbourPoly);

			const float total = bestNode->total + cost;
			
			// The node is already in open list and the new result is worse, skip.
			if ((neighbourNode->flags & DT_NODE_OPEN) && total >= neighbourNode->total)
				continue;
			
			neighbourNode->id = neighbourRef;
			neighbourNode->pidx = m_nodePool->getNodeIdx(bestNode);
			neighbourNode->total = total;
			
			if (neighbourNode->flags & DT_NODE_OPEN)
			{
				m_openList->modify(neighbourNode);
			}
			else
			{
				neighbourNode->flags = DT_NODE_OPEN;
				m_openList->push(neighbourNode);
			}
		}
	}
	
	*resultCount = n;
	
	return status;
}

/// @par
///
/// The order of the result set is from least to highest cost.
/// 
/// At least one result array must be provided.
///
/// A common use case for this method is to perform Dijkstra searches. 
/// Candidate polygons are found by searching the graph beginning at the start 
/// polygon.
/// 
/// The same intersection test restrictions that apply to findPolysAroundCircle()
/// method apply to this method.
/// 
/// The 3D centroid of the search polygon is used as the start position for cost 
/// calculations.
/// 
/// Intersection tests occur in 2D. All polygons are projected onto the 
/// xy-plane. So the z-values of the vertices do not affect intersection tests.
/// 
/// If the result arrays are is too small to hold the entire result set, they will 
/// be filled to capacity.
///
dtStatus dtNavMeshQuery::findPolysAroundShape(dtPolyRef startRef, const rdVec3D* verts, const int nverts,
											  const dtQueryFilter* filter,
											  dtPolyRef* resultRef, dtPolyRef* resultParent, float* resultCost,
											  int* resultCount, const int maxResult) const
{
	rdAssert(m_nav);
	rdAssert(m_nodePool);
	rdAssert(m_openList);

	if (!resultCount)
		return DT_FAILURE | DT_INVALID_PARAM;

	*resultCount = 0;

	if (!m_nav->isValidPolyRef(startRef) ||
		!verts || nverts < 3 ||
		!filter || maxResult < 0)
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}
	
	// Validate input
	if (!startRef || !m_nav->isValidPolyRef(startRef))
		return DT_FAILURE | DT_INVALID_PARAM;
	
	m_nodePool->clear();
	m_openList->clear();
	
	rdVec3D centerPos(0,0,0);
	for (int i = 0; i < nverts; ++i)
		rdVadd(&centerPos,&centerPos,&verts[i]);
	rdVscale(&centerPos,&centerPos,1.0f/nverts);

	dtNode* startNode = m_nodePool->getNode(startRef);
	startNode->pos = centerPos;
	startNode->pidx = 0;
	startNode->cost = 0;
	startNode->total = 0;
	startNode->id = startRef;
	startNode->flags = DT_NODE_OPEN;
	m_openList->push(startNode);
	
	dtStatus status = DT_SUCCESS;

	int n = 0;
	
	while (!m_openList->empty())
	{
		dtNode* bestNode = m_openList->pop();
		bestNode->flags &= ~DT_NODE_OPEN;
		bestNode->flags |= DT_NODE_CLOSED;
		
		// Get poly and tile.
		// The API input has been checked already, skip checking internal data.
		const dtPolyRef bestRef = bestNode->id;
		const dtMeshTile* bestTile = 0;
		const dtPoly* bestPoly = 0;
		m_nav->getTileAndPolyByRefUnsafe(bestRef, &bestTile, &bestPoly);
		
		// Get parent poly and tile.
		dtPolyRef parentRef = 0;
		const dtMeshTile* parentTile = 0;
		const dtPoly* parentPoly = 0;
		if (bestNode->pidx)
			parentRef = m_nodePool->getNodeAtIdx(bestNode->pidx)->id;

		// Default implementation of dtQueryFilter::getCost only needs the start, end
		// positions and the link. The tile and poly are therefore not used. Only
		// obtain them when virtual query filter function overrides are enabled.
#ifdef DT_VIRTUAL_QUERYFILTER
		if (parentRef)
			m_nav->getTileAndPolyByRefUnsafe(parentRef, &parentTile, &parentPoly);
#endif // DT_VIRTUAL_QUERYFILTER

		// Make sure our polygon isn't too small, reject those.
		if (!(bestPoly->flags & DT_POLYFLAGS_TOO_SMALL))
		{
			if (n < maxResult)
			{
				if (resultRef)
					resultRef[n] = bestRef;
				if (resultParent)
					resultParent[n] = parentRef;
				if (resultCost)
					resultCost[n] = bestNode->total;
				++n;
			}
			else
			{
				status |= DT_BUFFER_TOO_SMALL;
			}
		}
		
		for (unsigned int i = bestPoly->firstLink; i != DT_NULL_LINK; i = bestTile->links[i].next)
		{
			const dtLink* link = &bestTile->links[i];
			dtPolyRef neighbourRef = link->ref;
			// Skip invalid neighbours and do not follow back to parent.
			if (!neighbourRef || neighbourRef == parentRef)
				continue;
			
			// Expand to neighbour
			const dtMeshTile* neighbourTile = 0;
			const dtPoly* neighbourPoly = 0;
			m_nav->getTileAndPolyByRefUnsafe(neighbourRef, &neighbourTile, &neighbourPoly);

			// Do not advance if the polygon is excluded by the filter.
			if (!filter->passFilter(neighbourRef, neighbourTile, neighbourPoly))
				continue;

			if (!filter->traverseFilter(link, neighbourTile, neighbourPoly))
				continue;
			
			// Find edge and calc distance to the edge.
			rdVec3D va, vb;
			if (!getPortalPoints(bestRef, bestPoly, bestTile, neighbourRef, neighbourPoly, neighbourTile, link, &va,&vb))
				continue;
			
			// If the poly is not touching the edge to the next polygon, skip the connection it.
			float tmin, tmax;
			int segMin, segMax;
			if (!rdIntersectSegmentPoly2D(&va,&vb, verts, nverts, tmin, tmax, segMin, segMax))
				continue;
			if (tmin > 1.0f || tmax < 0.0f)
				continue;
			
			dtNode* neighbourNode = m_nodePool->getNode(neighbourRef);
			if (!neighbourNode)
			{
				status |= DT_OUT_OF_NODES;
				continue;
			}
			
			if (neighbourNode->flags & DT_NODE_CLOSED)
				continue;
			
			// Cost
			if (neighbourNode->flags == 0)
				rdVlerp(&neighbourNode->pos, &va,&vb, 0.5f);
			
			const float cost = filter->getCost(
				&bestNode->pos, &neighbourNode->pos, nullptr,
				parentRef, parentTile, parentPoly,
				bestRef, bestTile, bestPoly,
				neighbourRef, neighbourTile, neighbourPoly);

			const float total = bestNode->total + cost;
			
			// The node is already in open list and the new result is worse, skip.
			if ((neighbourNode->flags & DT_NODE_OPEN) && total >= neighbourNode->total)
				continue;
			
			neighbourNode->id = neighbourRef;
			neighbourNode->pidx = m_nodePool->getNodeIdx(bestNode);
			neighbourNode->total = total;
			
			if (neighbourNode->flags & DT_NODE_OPEN)
			{
				m_openList->modify(neighbourNode);
			}
			else
			{
				neighbourNode->flags = DT_NODE_OPEN;
				m_openList->push(neighbourNode);
			}
		}
	}
	
	*resultCount = n;
	
	return status;
}

dtStatus dtNavMeshQuery::getPathFromDijkstraSearch(dtPolyRef endRef, dtPolyRef* path, unsigned char* jump, int* pathCount, int maxPath) const
{
	if (!m_nav->isValidPolyRef(endRef) || !path || !jump || !pathCount || maxPath < 0)
		return DT_FAILURE | DT_INVALID_PARAM;

	*pathCount = 0;

	dtNode* endNode;
	if (m_nodePool->findNodes(endRef, &endNode, 1) != 1 ||
		(endNode->flags & DT_NODE_CLOSED) == 0)
		return DT_FAILURE | DT_INVALID_PARAM;

	return getPathToNode(endNode, path, jump, pathCount, maxPath);
}

/// @par
///
/// This method is optimized for a small search radius and small number of result 
/// polygons.
///
/// Candidate polygons are found by searching the navigation graph beginning at 
/// the start polygon.
///
/// The same intersection test restrictions that apply to the findPolysAroundCircle 
/// method applies to this method.
///
/// The value of the center point is used as the start point for cost calculations. 
/// It is not projected onto the surface of the mesh, so its z-value will affect 
/// the costs.
/// 
/// Intersection tests occur in 2D. All polygons and the search circle are 
/// projected onto the xy-plane. So the z-value of the center point does not 
/// affect intersection tests.
/// 
/// If the result arrays are is too small to hold the entire result set, they will 
/// be filled to capacity.
/// 
dtStatus dtNavMeshQuery::findLocalNeighbourhood(dtPolyRef startRef, const rdVec3D* centerPos, const float radius,
												const dtQueryFilter* filter,
												dtPolyRef* resultRef, dtPolyRef* resultParent,
												int* resultCount, const int maxResult) const
{
	rdAssert(m_nav);
	rdAssert(m_tinyNodePool);

	if (!resultCount)
		return DT_FAILURE | DT_INVALID_PARAM;

	*resultCount = 0;

	if (!m_nav->isValidPolyRef(startRef) ||
		!centerPos || !rdVisfinite(centerPos) ||
		radius < 0 || !rdMathIsfinite(radius) ||
		!filter || maxResult < 0)
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}

	static const int MAX_STACK = 256;
	dtNode* stack[MAX_STACK];
	int nstack = 0;
	
	m_tinyNodePool->clear();
	
	dtNode* startNode = m_tinyNodePool->getNode(startRef);
	startNode->pidx = 0;
	startNode->id = startRef;
	startNode->flags = DT_NODE_CLOSED;
	stack[nstack++] = startNode;
	
	const float radiusSqr = rdSqr(radius);
	
	rdVec3D pa[RD_VERTS_PER_POLYGON];
	rdVec3D pb[RD_VERTS_PER_POLYGON];
	
	dtStatus status = DT_SUCCESS;
	
	int n = 0;
	if (n < maxResult)
	{
		resultRef[n] = startNode->id;
		if (resultParent)
			resultParent[n] = 0;
		++n;
	}
	else
	{
		status |= DT_BUFFER_TOO_SMALL;
	}
	
	while (nstack)
	{
		// Pop front.
		dtNode* curNode = stack[0];
		for (int i = 0; i < nstack-1; ++i)
			stack[i] = stack[i+1];
		nstack--;
		
		// Get poly and tile.
		// The API input has been checked already, skip checking internal data.
		const dtPolyRef curRef = curNode->id;
		const dtMeshTile* curTile = 0;
		const dtPoly* curPoly = 0;
		m_nav->getTileAndPolyByRefUnsafe(curRef, &curTile, &curPoly);
		
		for (unsigned int i = curPoly->firstLink; i != DT_NULL_LINK; i = curTile->links[i].next)
		{
			const dtLink* link = &curTile->links[i];
			dtPolyRef neighbourRef = link->ref;
			// Skip invalid neighbours.
			if (!neighbourRef)
				continue;
			
			// Skip if cannot allocate more nodes.
			dtNode* neighbourNode = m_tinyNodePool->getNode(neighbourRef);
			if (!neighbourNode)
				continue;
			// Skip visited.
			if (neighbourNode->flags & DT_NODE_CLOSED)
				continue;
			
			// Expand to neighbour
			const dtMeshTile* neighbourTile = 0;
			const dtPoly* neighbourPoly = 0;
			m_nav->getTileAndPolyByRefUnsafe(neighbourRef, &neighbourTile, &neighbourPoly);
			
			// Skip off-mesh connections.
			if (neighbourPoly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
				continue;
			
			// Do not advance if the polygon is excluded by the filter.
			if (!filter->passFilter(neighbourRef, neighbourTile, neighbourPoly))
				continue;
			
			if (!filter->traverseFilter(link, neighbourTile, neighbourPoly))
				continue;
			
			// Find edge and calc distance to the edge.
			rdVec3D va, vb;
			if (!getPortalPoints(curRef, curPoly, curTile, neighbourRef, neighbourPoly, neighbourTile, link, &va,&vb))
				continue;
			
			// If the circle is not touching the next polygon, skip it.
			float tseg;
			float distSqr = rdDistancePtSegSqr2D(centerPos, &va,&vb, tseg);
			if (distSqr > radiusSqr)
				continue;
			
			// Mark node visited, this is done before the overlap test so that
			// we will not visit the poly again if the test fails.
			neighbourNode->flags |= DT_NODE_CLOSED;
			neighbourNode->pidx = m_tinyNodePool->getNodeIdx(curNode);
			
			// Check that the polygon does not collide with existing polygons.
			
			// Collect vertices of the neighbour poly.
			const int npa = neighbourPoly->vertCount;
			for (int k = 0; k < npa; ++k)
				rdVcopy(&pa[k], &neighbourTile->verts[neighbourPoly->verts[k]]);
			
			bool overlap = false;
			for (int j = 0; j < n; ++j)
			{
				dtPolyRef pastRef = resultRef[j];
				
				// Connected polys do not overlap.
				bool connected = false;
				for (unsigned int k = curPoly->firstLink; k != DT_NULL_LINK; k = curTile->links[k].next)
				{
					if (curTile->links[k].ref == pastRef)
					{
						connected = true;
						break;
					}
				}
				if (connected)
					continue;
				
				// Potentially overlapping.
				const dtMeshTile* pastTile = 0;
				const dtPoly* pastPoly = 0;
				m_nav->getTileAndPolyByRefUnsafe(pastRef, &pastTile, &pastPoly);
				
				// Get vertices and test overlap
				const int npb = pastPoly->vertCount;
				for (int k = 0; k < npb; ++k)
					rdVcopy(&pb[k], &pastTile->verts[pastPoly->verts[k]]);
				
				if (rdOverlapPolyPoly2D(pa,npa, pb,npb))
				{
					overlap = true;
					break;
				}
			}

			if (overlap)
				continue;

			// Make sure our polygon isn't too small, reject those.
			if (!(neighbourPoly->flags & DT_POLYFLAGS_TOO_SMALL))
			{
				// This poly is fine, store and advance to the poly.
				if (n < maxResult)
				{
					resultRef[n] = neighbourRef;
					if (resultParent)
						resultParent[n] = curRef;
					++n;
				}
				else
				{
					status |= DT_BUFFER_TOO_SMALL;
				}
			}
			
			if (nstack < MAX_STACK)
			{
				stack[nstack++] = neighbourNode;
			}
		}
	}
	
	*resultCount = n;
	
	return status;
}


struct dtSegInterval
{
	dtPolyRef ref;
	short tmin, tmax;
};

static void insertInterval(dtSegInterval* ints, int& nints, const int maxInts,
						   const short tmin, const short tmax, const dtPolyRef ref)
{
	if (nints+1 > maxInts) return;
	// Find insertion point.
	int idx = 0;
	while (idx < nints)
	{
		if (tmax <= ints[idx].tmin)
			break;
		idx++;
	}
	// Move current results.
	if (nints-idx)
		memmove(ints+idx+1, ints+idx, sizeof(dtSegInterval)*(nints-idx));
	// Store
	ints[idx].ref = ref;
	ints[idx].tmin = tmin;
	ints[idx].tmax = tmax;
	nints++;
}

/// @par
///
/// If the @p segmentRefs parameter is provided, then all polygon segments will be returned. 
/// Otherwise only the wall segments are returned.
/// 
/// A segment that is normally a portal will be included in the result set as a 
/// wall if the @p filter results in the neighbor polygon becoming impassable.
/// 
/// The @p segmentVerts and @p segmentRefs buffers should normally be sized for the 
/// maximum segments per polygon of the source navigation mesh.
/// 
dtStatus dtNavMeshQuery::getPolyWallSegments(dtPolyRef ref, const dtQueryFilter* filter,
											 dtPolyWallSegment* segments, dtPolyRef* segmentRefs, int* segmentCount,
											 const int maxSegments) const
{
	rdAssert(m_nav);

	if (!segmentCount)
		return DT_FAILURE | DT_INVALID_PARAM;
	
	*segmentCount = 0;

	const dtMeshTile* tile = 0;
	const dtPoly* poly = 0;
	if (dtStatusFailed(m_nav->getTileAndPolyByRef(ref, &tile, &poly)))
		return DT_FAILURE | DT_INVALID_PARAM;

	if (!filter || !segments || maxSegments < 0)
		return DT_FAILURE | DT_INVALID_PARAM;
	
	int n = 0;
	static const int MAX_INTERVAL = 16;
	dtSegInterval ints[MAX_INTERVAL];
	int nints;
	
	const bool storePortals = segmentRefs != 0;
	
	dtStatus status = DT_SUCCESS;
	
	for (int i = 0, j = (int)poly->vertCount-1; i < (int)poly->vertCount; j = i++)
	{
		// Skip non-solid edges.
		nints = 0;
		if (poly->neis[j] & DT_EXT_LINK)
		{
			// Tile border.
			for (unsigned int k = poly->firstLink; k != DT_NULL_LINK; k = tile->links[k].next)
			{
				const dtLink* link = &tile->links[k];
				if (link->edge == j)
				{
					if (link->ref != 0)
					{
						const dtMeshTile* neiTile = 0;
						const dtPoly* neiPoly = 0;
						m_nav->getTileAndPolyByRefUnsafe(link->ref, &neiTile, &neiPoly);
						if (filter->passFilter(link->ref, neiTile, neiPoly) &&
							filter->traverseFilter(link, neiTile, neiPoly))
						{
							insertInterval(ints, nints, MAX_INTERVAL, link->bmin, link->bmax, link->ref);
						}
					}
				}
			}
		}
		else
		{
			// Internal edge
			dtPolyRef neiRef = 0;
			if (poly->neis[j])
			{
				const unsigned int idx = (unsigned int)(poly->neis[j]-1);
				neiRef = m_nav->getPolyRefBase(tile) | idx;
				if (!filter->passFilter(neiRef, tile, &tile->polys[idx]))
					neiRef = 0;
			}

			// If the edge leads to another polygon and portals are not stored, skip.
			if (neiRef != 0 && !storePortals)
				continue;
			
			if (n < maxSegments)
			{
				const rdVec3D* vj = &tile->verts[poly->verts[j]];
				const rdVec3D* vi = &tile->verts[poly->verts[i]];
				dtPolyWallSegment* seg = &segments[n];
				rdVcopy(&seg->verta, vj);
				rdVcopy(&seg->vertb, vi);
				if (segmentRefs)
					segmentRefs[n] = neiRef;
				n++;
			}
			else
			{
				status |= DT_BUFFER_TOO_SMALL;
			}
			
			continue;
		}
		
		// Add sentinels
		insertInterval(ints, nints, MAX_INTERVAL, -1, 0, 0);
		insertInterval(ints, nints, MAX_INTERVAL, 255, 256, 0);
		
		// Store segments.
		const rdVec3D* vj = &tile->verts[poly->verts[j]];
		const rdVec3D* vi = &tile->verts[poly->verts[i]];
		for (int k = 1; k < nints; ++k)
		{
			// Portal segment.
			if (storePortals && ints[k].ref)
			{
				const float tmin = ints[k].tmin/255.0f; 
				const float tmax = ints[k].tmax/255.0f; 
				if (n < maxSegments)
				{
					dtPolyWallSegment* seg = &segments[n];
					rdVlerp(&seg->verta, vj,vi, tmin);
					rdVlerp(&seg->vertb, vj,vi, tmax);
					if (segmentRefs)
						segmentRefs[n] = ints[k].ref;
					n++;
				}
				else
				{
					status |= DT_BUFFER_TOO_SMALL;
				}
			}

			// Wall segment.
			const int imin = ints[k-1].tmax;
			const int imax = ints[k].tmin;
			if (imin != imax)
			{
				const float tmin = imin/255.0f; 
				const float tmax = imax/255.0f; 
				if (n < maxSegments)
				{
					dtPolyWallSegment* seg = &segments[n];
					rdVlerp(&seg->verta, vj,vi, tmin);
					rdVlerp(&seg->vertb, vj,vi, tmax);
					if (segmentRefs)
						segmentRefs[n] = 0;
					n++;
				}
				else
				{
					status |= DT_BUFFER_TOO_SMALL;
				}
			}
		}
	}
	
	*segmentCount = n;
	
	return status;
}

/// @par
///
/// @p hitPos is not adjusted using the height detail data.
///
/// @p hitDist will equal the search radius if there is no wall within the 
/// radius. In this case the values of @p hitPos and @p hitNormal are
/// undefined.
///
/// The normal will become unpredictable if @p hitDist is a very small number.
///
dtStatus dtNavMeshQuery::findDistanceToWall(dtPolyRef startRef, const rdVec3D* centerPos, const float maxRadius,
											const dtQueryFilter* filter,
											float* hitDist, rdVec3D* hitPos, rdVec3D* hitNormal) const
{
	rdAssert(m_nav);
	rdAssert(m_nodePool);
	rdAssert(m_openList);
	
	// Validate input
	if (!m_nav->isValidPolyRef(startRef) ||
		!centerPos || !rdVisfinite(centerPos) ||
		maxRadius < 0 || !rdMathIsfinite(maxRadius) ||
		!filter || !hitDist || !hitPos || !hitNormal)
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}
	
	m_nodePool->clear();
	m_openList->clear();
	
	dtNode* startNode = m_nodePool->getNode(startRef);
	startNode->pos = *centerPos;
	startNode->pidx = 0;
	startNode->cost = 0;
	startNode->total = 0;
	startNode->id = startRef;
	startNode->flags = DT_NODE_OPEN;
	m_openList->push(startNode);
	
	float radiusSqr = rdSqr(maxRadius);
	
	dtStatus status = DT_SUCCESS;
	
	while (!m_openList->empty())
	{
		dtNode* bestNode = m_openList->pop();
		bestNode->flags &= ~DT_NODE_OPEN;
		bestNode->flags |= DT_NODE_CLOSED;
		
		// Get poly and tile.
		// The API input has been checked already, skip checking internal data.
		const dtPolyRef bestRef = bestNode->id;
		const dtMeshTile* bestTile = 0;
		const dtPoly* bestPoly = 0;
		m_nav->getTileAndPolyByRefUnsafe(bestRef, &bestTile, &bestPoly);
		
		// Get parent ref.
		const dtPolyRef parentRef = bestNode->pidx ? m_nodePool->getNodeAtIdx(bestNode->pidx)->id : 0;
		
		// Hit test walls.
		for (int i = 0, j = (int)bestPoly->vertCount-1; i < (int)bestPoly->vertCount; j = i++)
		{
			// Skip non-solid edges.
			if (bestPoly->neis[j] & DT_EXT_LINK)
			{
				// Tile border.
				bool solid = true;
				for (unsigned int k = bestPoly->firstLink; k != DT_NULL_LINK; k = bestTile->links[k].next)
				{
					const dtLink* link = &bestTile->links[k];
					if (link->edge == j)
					{
						if (link->ref != 0)
						{
							const dtMeshTile* neiTile = 0;
							const dtPoly* neiPoly = 0;
							m_nav->getTileAndPolyByRefUnsafe(link->ref, &neiTile, &neiPoly);
							if (filter->passFilter(link->ref, neiTile, neiPoly) && 
								filter->traverseFilter(link, neiTile, neiPoly))
								solid = false;
						}
						break;
					}
				}
				if (!solid) continue;
			}
			else if (bestPoly->neis[j])
			{
				// Internal edge
				const unsigned int idx = (unsigned int)(bestPoly->neis[j]-1);
				const dtPolyRef ref = m_nav->getPolyRefBase(bestTile) | idx;
				if (filter->passFilter(ref, bestTile, &bestTile->polys[idx]))
					continue;
			}
			
			// Calc distance to the edge.
			const rdVec3D* vj = &bestTile->verts[bestPoly->verts[j]];
			const rdVec3D* vi = &bestTile->verts[bestPoly->verts[i]];
			float tseg;
			float distSqr = rdDistancePtSegSqr2D(centerPos, vj, vi, tseg);
			
			// Edge is too far, skip.
			if (distSqr > radiusSqr)
				continue;
			
			// Hit wall, update radius.
			radiusSqr = distSqr;
			// Calculate hit pos.
			hitPos->x = vj->x + (vi->x - vj->x)*tseg;
			hitPos->y = vj->y + (vi->y - vj->y)*tseg;
			hitPos->z = vj->z + (vi->z - vj->z)*tseg;
		}
		
		for (unsigned int i = bestPoly->firstLink; i != DT_NULL_LINK; i = bestTile->links[i].next)
		{
			const dtLink* link = &bestTile->links[i];
			dtPolyRef neighbourRef = link->ref;
			// Skip invalid neighbours and do not follow back to parent.
			if (!neighbourRef || neighbourRef == parentRef)
				continue;
			
			// Expand to neighbour.
			const dtMeshTile* neighbourTile = 0;
			const dtPoly* neighbourPoly = 0;
			m_nav->getTileAndPolyByRefUnsafe(neighbourRef, &neighbourTile, &neighbourPoly);
			
			// Skip off-mesh connections.
			if (neighbourPoly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
				continue;
			
			// Calc distance to the edge.
			const rdVec3D* va = &bestTile->verts[bestPoly->verts[link->edge]];
			const rdVec3D* vb = &bestTile->verts[bestPoly->verts[(link->edge+1) % bestPoly->vertCount]];
			float tseg;
			float distSqr = rdDistancePtSegSqr2D(centerPos, va,vb, tseg);
			
			// If the circle is not touching the next polygon, skip it.
			if (distSqr > radiusSqr)
				continue;
			
			if (!filter->passFilter(neighbourRef, neighbourTile, neighbourPoly))
				continue;

			if (!filter->traverseFilter(link, neighbourTile, neighbourPoly))
				continue;

			dtNode* neighbourNode = m_nodePool->getNode(neighbourRef);
			if (!neighbourNode)
			{
				status |= DT_OUT_OF_NODES;
				continue;
			}
			
			if (neighbourNode->flags & DT_NODE_CLOSED)
				continue;
			
			// Cost
			if (neighbourNode->flags == 0)
			{
				getEdgeMidPoint(bestRef, bestPoly, bestTile,
								neighbourRef, neighbourPoly, neighbourTile, &neighbourNode->pos);
			}
			
			const float total = bestNode->total + rdVdist(&bestNode->pos, &neighbourNode->pos);
			
			// The node is already in open list and the new result is worse, skip.
			if ((neighbourNode->flags & DT_NODE_OPEN) && total >= neighbourNode->total)
				continue;
			
			neighbourNode->id = neighbourRef;
			neighbourNode->flags = (neighbourNode->flags & ~DT_NODE_CLOSED);
			neighbourNode->pidx = m_nodePool->getNodeIdx(bestNode);
			neighbourNode->total = total;
				
			if (neighbourNode->flags & DT_NODE_OPEN)
			{
				m_openList->modify(neighbourNode);
			}
			else
			{
				neighbourNode->flags |= DT_NODE_OPEN;
				m_openList->push(neighbourNode);
			}
		}
	}
	
	// Calc hit normal.
	rdVsub(hitNormal, centerPos, hitPos);
	rdVnormalize(hitNormal);
	
	*hitDist = rdMathSqrtf(radiusSqr);
	
	return status;
}

bool dtNavMeshQuery::isGoalPolyReachable(const dtPolyRef fromRef, const dtPolyRef goalRef,
	const bool checkDisjointGroupsOnly, const int traverseTableIndex) const
{
	rdAssert(m_nav);
	return m_nav->isGoalPolyReachable(fromRef, goalRef, checkDisjointGroupsOnly, traverseTableIndex);
}

bool dtNavMeshQuery::isValidPolyRef(dtPolyRef ref, const dtQueryFilter* filter) const
{
	const dtMeshTile* tile = 0;
	const dtPoly* poly = 0;
	dtStatus status = m_nav->getTileAndPolyByRef(ref, &tile, &poly);
	// If cannot get polygon, assume it does not exists and boundary is invalid.
	if (dtStatusFailed(status))
		return false;
	// If cannot pass filter, assume flags has changed and boundary is invalid.
	if (!filter->passFilter(ref, tile, poly))
		return false;
	return true;
}

/// @par
///
/// The closed list is the list of polygons that were fully evaluated during 
/// the last navigation graph search. (A* or Dijkstra)
/// 
bool dtNavMeshQuery::isInClosedList(dtPolyRef ref) const
{
	if (!m_nodePool) return false;
	
	dtNode* nodes[DT_MAX_STATES_PER_NODE];
	int n= m_nodePool->findNodes(ref, nodes, DT_MAX_STATES_PER_NODE);

	for (int i=0; i<n; i++)
	{
		if (nodes[i]->flags & DT_NODE_CLOSED)
			return true;
	}		

	return false;
}
