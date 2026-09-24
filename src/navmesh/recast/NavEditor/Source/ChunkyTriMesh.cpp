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
#include "NavEditor/Include/ChunkyTriMesh.h"

struct BoundsItem
{
	rdVec3D bmin;
	rdVec3D bmax;
	int i;
};

static int compareItemX(const void* va, const void* vb)
{
	const BoundsItem* a = (const BoundsItem*)va;
	const BoundsItem* b = (const BoundsItem*)vb;
	if (a->bmin.x < b->bmin.x)
		return -1;
	if (a->bmin.x > b->bmin.x)
		return 1;
	return 0;
}

static int compareItemY(const void* va, const void* vb)
{
	const BoundsItem* a = (const BoundsItem*)va;
	const BoundsItem* b = (const BoundsItem*)vb;
	if (a->bmin.y < b->bmin.y)
		return -1;
	if (a->bmin.y > b->bmin.y)
		return 1;
	return 0;
}

static int compareItemZ(const void* va, const void* vb)
{
	const BoundsItem* a = (const BoundsItem*)va;
	const BoundsItem* b = (const BoundsItem*)vb;
	if (a->bmin.z < b->bmin.z)
		return -1;
	if (a->bmin.z > b->bmin.z)
		return 1;
	return 0;
}

static void calcExtends(const BoundsItem* items, const int /*nitems*/,
						const int imin, const int imax,
						rdVec3D* bmin, rdVec3D* bmax)
{
	bmin->x = items[imin].bmin.x;
	bmin->y = items[imin].bmin.y;
	bmin->z = items[imin].bmin.z;
	
	bmax->x = items[imin].bmax.x;
	bmax->y = items[imin].bmax.y;
	bmax->z = items[imin].bmax.z;
	
	for (int i = imin+1; i < imax; ++i)
	{
		const BoundsItem& it = items[i];
		if (it.bmin.x < bmin->x) bmin->x = it.bmin.x;
		if (it.bmin.y < bmin->y) bmin->y = it.bmin.y;
		if (it.bmin.z < bmin->z) bmin->z = it.bmin.z;
		
		if (it.bmax.x > bmax->x) bmax->x = it.bmax.x;
		if (it.bmax.y > bmax->y) bmax->y = it.bmax.y;
		if (it.bmax.z > bmax->z) bmax->z = it.bmax.z;
	}
}

inline int longestAxis(float x, float y, float z)
{
	int	axis = 0;
	float maxVal = x;
	if (y > maxVal)
	{
		axis = 1;
		maxVal = y;
	}
	if (z > maxVal)
	{
		axis = 2;
	}
	return axis;
}

static void subdivide(BoundsItem* items, int nitems, int imin, int imax, int trisPerChunk,
					  int& curNode, rcChunkyTriMeshNode* nodes, const int maxNodes,
					  int& curTri, int* outTris, const int* inTris)
{
	int inum = imax - imin;
	int icur = curNode;
	
	if (curNode >= maxNodes)
		return;

	rcChunkyTriMeshNode& node = nodes[curNode++];
	
	if (inum <= trisPerChunk)
	{
		// Leaf
		calcExtends(items, nitems, imin, imax, &node.bmin, &node.bmax);
		
		// Copy triangles.
		node.i = curTri;
		node.n = inum;
		
		for (int i = imin; i < imax; ++i)
		{
			const int* src = &inTris[items[i].i*3];
			int* dst = &outTris[curTri*3];
			curTri++;
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
		}
	}
	else
	{
		// Split
		calcExtends(items, nitems, imin, imax, &node.bmin, &node.bmax);
		
		const int	axis = longestAxis(node.bmax.x - node.bmin.x,
							   node.bmax.y - node.bmin.y,
							   node.bmax.z - node.bmin.z);
		
		if (axis == 0)
		{
			// Sort along x-axis
			qsort(items+imin, static_cast<size_t>(inum), sizeof(BoundsItem), compareItemX);
		}
		else if (axis == 1)
		{
			// Sort along y-axis
			qsort(items+imin, static_cast<size_t>(inum), sizeof(BoundsItem), compareItemY);
		}
		else
		{
			// Sort along z-axis
			qsort(items+imin, static_cast<size_t>(inum), sizeof(BoundsItem), compareItemZ);
		}
		
		int isplit = imin+inum/2;
		
		// Left
		subdivide(items, nitems, imin, isplit, trisPerChunk, curNode, nodes, maxNodes, curTri, outTris, inTris);
		// Right
		subdivide(items, nitems, isplit, imax, trisPerChunk, curNode, nodes, maxNodes, curTri, outTris, inTris);
		
		int iescape = curNode - icur;
		// Negative index means escape.
		node.i = -iescape;
	}
}

bool rcCreateChunkyTriMesh(const rdVec3D* verts, const int* tris, int ntris,
						   int trisPerChunk, rcChunkyTriMesh* cm)
{
	int nchunks = (ntris + trisPerChunk-1) / trisPerChunk;

	cm->nodes = new rcChunkyTriMeshNode[nchunks*4];
	if (!cm->nodes)
		return false;
		
	cm->tris = new int[ntris*3];
	if (!cm->tris)
		return false;
		
	cm->ntris = ntris;

	// Build tree
	BoundsItem* items = new BoundsItem[ntris];
	if (!items)
		return false;

	for (int i = 0; i < ntris; i++)
	{
		const int* t = &tris[i*3];
		BoundsItem& it = items[i];
		it.i = i;
		// Calc triangle XYZ bounds.
		it.bmin.x = it.bmax.x = verts[t[0]].x;
		it.bmin.y = it.bmax.y = verts[t[0]].y;
		it.bmin.z = it.bmax.z = verts[t[0]].z;
		for (int j = 1; j < 3; ++j)
		{
			const rdVec3D* v = &verts[t[j]];
			if (v->x < it.bmin.x) it.bmin.x = v->x;
			if (v->y < it.bmin.y) it.bmin.y = v->y;
			if (v->z < it.bmin.z) it.bmin.z = v->z;

			if (v->x > it.bmax.x) it.bmax.x = v->x;
			if (v->y > it.bmax.y) it.bmax.y = v->y;
			if (v->z > it.bmax.z) it.bmax.z = v->z;
		}
	}

	int curTri = 0;
	int curNode = 0;
	subdivide(items, ntris, 0, ntris, trisPerChunk, curNode, cm->nodes, nchunks*4, curTri, cm->tris, tris);
	
	delete [] items;
	
	cm->nnodes = curNode;
	
	// Calc max tris per node.
	cm->maxTrisPerChunk = 0;
	for (int i = 0; i < cm->nnodes; ++i)
	{
		rcChunkyTriMeshNode& node = cm->nodes[i];
		const bool isLeaf = node.i >= 0;
		if (!isLeaf) continue;
		if (node.n > cm->maxTrisPerChunk)
			cm->maxTrisPerChunk = node.n;
	}
	 
	return true;
}


static inline bool checkOverlapRect(const rdVec2D* amin, const rdVec2D* amax,
							 const rdVec2D* bmin, const rdVec2D* bmax)
{
	bool overlap = true;
	overlap = (amin->x > bmax->x || amax->x < bmin->x) ? false : overlap;
	overlap = (amin->y > bmax->y || amax->y < bmin->y) ? false : overlap;
	return overlap;
}

int rcGetChunksOverlappingRect(const rcChunkyTriMesh* cm,
							   rdVec2D* bmin, rdVec2D* bmax,
							   int* ids, const int maxIds)
{
	// Traverse tree
	int i = 0;
	int n = 0;
	while (i < cm->nnodes)
	{
		const rcChunkyTriMeshNode* node = &cm->nodes[i];
		const bool overlap = checkOverlapRect(bmin, bmax, &node->bmin, &node->bmax);
		const bool isLeafNode = node->i >= 0;
		
		if (isLeafNode && overlap)
		{
			if (n < maxIds)
			{
				ids[n] = i;
				n++;
			}
		}
		
		if (overlap || isLeafNode)
			i++;
		else
		{
			const int escapeIndex = -node->i;
			i += escapeIndex;
		}
	}
	
	return n;
}

int rcGetChunksOverlappingRect(const rcChunkyTriMesh* cm, const rdVec2D* bmin, const rdVec2D* bmax, int* ids, const int maxIds, int& currentCount, int& currentNode)
{
	// Traverse tree
	while (currentNode < cm->nnodes)
	{
		const rcChunkyTriMeshNode* node = &cm->nodes[currentNode];
		const bool overlap = checkOverlapRect(bmin, bmax, &node->bmin, &node->bmax);
		const bool isLeafNode = node->i >= 0;

		if (isLeafNode && overlap)
		{
			if (currentCount < maxIds)
			{
				ids[currentCount] = currentNode;
				currentCount++;
			}
		}

		if (overlap || isLeafNode)
			currentNode++;
		else
		{
			const int escapeIndex = -node->i;
			currentNode += escapeIndex;
		}
		if (currentCount == maxIds)
		{
			return 0;
		}
	}
	//done with tree
	return 1;
}



static bool checkOverlapSegment(const rdVec3D* p, const rdVec3D* q,
								const rdVec3D* bmin, const rdVec3D* bmax)
{
	float tmin = 0;
	float tmax = 1;
	rdVec3D d;
	d.x = q->x - p->x;
	d.y = q->y - p->y;
	d.z = q->z - p->z;
	
	for (int i = 0; i < 3; i++)
	{
		if (rdMathFabsf(d[i]) < RD_EPS)
		{
			// Ray is parallel to slab. No hit if origin not within slab
			if ((*p)[i] < (*bmin)[i] || (*p)[i] > (*bmax)[i])
				return false;
		}
		else
		{
			// Compute intersection t value of ray with near and far plane of slab
			float ood = 1.0f / d[i];
			float t1 = ((*bmin)[i] - (*p)[i]) * ood;
			float t2 = ((*bmax)[i] - (*p)[i]) * ood;
			if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
			if (t1 > tmin) tmin = t1;
			if (t2 < tmax) tmax = t2;
			if (tmin > tmax) return false;
		}
	}
	return true;
}


int rcGetChunksOverlappingSegment(const rcChunkyTriMesh* cm,
								  const rdVec3D* p, const rdVec3D* q,
								  int* ids, const int maxIds)
{
	// Traverse tree
	int i = 0;
	int n = 0;
	while (i < cm->nnodes)
	{
		const rcChunkyTriMeshNode* node = &cm->nodes[i];
		const bool overlap = checkOverlapSegment(p, q, &node->bmin, &node->bmax);
		const bool isLeafNode = node->i >= 0;
		
		if (isLeafNode && overlap)
		{
			if (n < maxIds)
			{
				ids[n] = i;
				n++;
			}
		}
		
		if (overlap || isLeafNode)
			i++;
		else
		{
			const int escapeIndex = -node->i;
			i += escapeIndex;
		}
	}
	
	return n;
}
