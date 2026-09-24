#include "navmesh_builder.h"

#include "Detour/Include/DetourNavMeshBuilder.h"
#include "NavEditor/Include/ChunkyTriMesh.h"
#include "Recast/Include/Recast.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

struct CNavMeshBuilder::TileData_t
{
    std::unique_ptr<unsigned char, decltype([](unsigned char* bytes) { rdFree(bytes); })> Bytes;
    int Size = 0;
    int RestoredPassageSpanCount = 0;
};

struct CNavMeshBuilder::TraversalTypeSettings_t
{
    float MinimumDistance;
    float MaximumDistance;
    float MinimumElevation;
    float MaximumElevation;
    float MinimumSlope;
    float MaximumSlope;
    float OverlapTrigger;
    bool OverlapExclusive;
};

const std::array<CNavMeshBuilder::TraversalTypeSettings_t, DT_MAX_TRAVERSE_TYPES>& CNavMeshBuilder::TraversalTypes()
{
    static const std::array<TraversalTypeSettings_t, DT_MAX_TRAVERSE_TYPES> types = {{
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 120, 0, 48, 0, 67, 0, false},
        {120, 160, 48, 96, 5, 78, 0, false},
        {160, 220, 0, 128, 0, 38, 0, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {800, 1220, 0, 96, 0, 6.5f, 0, true},
        {70, 220, 48, 220, 19, 84, 0, false},
        {210, 450, 168, 384, 27, 87.5f, 0, false},
        {450, 950, 384, 950, 44, 89.5f, 0, false},
        {410, 800, 0, 56, 0, 7, 0, true},
        {640, 930, 348, 640, 2.2f, 47, 0, true},
        {810, 1220, 256, 640, 5.7f, 58.5f, 0, true},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {220, 410, 0, 104, 0, 12.5f, 0, false},
        {210, 580, 104, 416, 4.6f, 53, 0, true},
        {0, 0, 0, 0, 0, 0, -1, false},
        {210, 450, 168, 384, 34, 89, 0, false},
        {450, 860, 340, 850, 46, 89, 0, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
        {0, 0, 0, 0, 0, 0, -1, false},
    }};
    return types;
}

bool CNavMeshBuilder::SupportsTraversal(unsigned char traversalType) const
{
    if (traversalType >= 32)
        return false;
    const std::uint32_t bit = std::uint32_t{1} << traversalType;
    for (int table = 0; table < m_Hull.TraversalTableCount; ++table)
    {
        if ((m_Hull.TraversalMasks[static_cast<std::size_t>(table)] & bit) != 0)
            return true;
    }
    return false;
}

unsigned char CNavMeshBuilder::SelectTraversalType(void* userData, float distance, float elevation, float slope, bool baseOverlaps, bool landOverlaps)
{
    const auto& context = *static_cast<CNavMeshBuilder*>(userData);
    unsigned char bestType = DT_NULL_TRAVERSE_TYPE;
    float smallestDifference = std::numeric_limits<float>::max();

    for (int index = 31; index >= 0; --index)
    {
        const TraversalTypeSettings_t& type = TraversalTypes()[static_cast<std::size_t>(index)];
        if ((type.MinimumDistance == 0.0f && type.MaximumDistance == 0.0f && type.MinimumElevation == 0.0f && type.MaximumElevation == 0.0f) ||
            !context.SupportsTraversal(static_cast<unsigned char>(index)))
        {
            continue;
        }
        if (distance < type.MinimumDistance || distance > type.MaximumDistance || elevation < type.MinimumElevation ||
            elevation > type.MaximumElevation || slope < type.MinimumSlope || slope > type.MaximumSlope)
        {
            continue;
        }
        if (type.OverlapTrigger >= 0.0f && elevation >= type.OverlapTrigger)
        {
            const bool overlaps = type.OverlapExclusive ? baseOverlaps || landOverlaps : baseOverlaps && landOverlaps;
            if (!overlaps)
                continue;
        }

        const float difference = std::fabs(distance - (type.MinimumDistance + type.MaximumDistance) * 0.5f) +
                                 std::fabs(elevation - (type.MinimumElevation + type.MaximumElevation) * 0.5f) +
                                 std::fabs(slope - (type.MinimumSlope + type.MaximumSlope) * 0.5f);
        if (difference < smallestDifference)
        {
            smallestDifference = difference;
            bestType = static_cast<unsigned char>(index);
        }
    }
    return bestType;
}

bool CNavMeshBuilder::EdgesFaceEachOther(const rdVec2D& firstPosition, const rdVec2D& secondPosition, const rdVec2D& firstNormal,
                                         const rdVec2D& secondNormal) const
{
    rdVec2D delta;
    rdVsub2D(&delta, &secondPosition, &firstPosition);
    return rdVdot2D(&delta, &firstNormal) >= 0.0f && rdVdot2D(&delta, &secondNormal) < 0.0f;
}

bool CNavMeshBuilder::HasVerticalClearance(const rdVec3D& position, float height) const
{
    rdVec3D end = position;
    end.z += height;
    return !m_Geometry.Raycast(position, end);
}

bool CNavMeshBuilder::LinkHasClearance(const rdVec3D& lower, const rdVec3D& higher, float walkableHeight) const
{
    std::array<float, 3> fractions = {0.5f, 0.25f, 0.75f};
    const float distance = rdVdist(&lower, &higher);
    const int rayCount = distance < TripleClearanceRayThreshold ? 1 : 3;
    for (int i = 0; i < rayCount; ++i)
    {
        rdVec3D point;
        point.x = lower.x + (higher.x - lower.x) * fractions[static_cast<std::size_t>(i)];
        point.y = lower.y + (higher.y - lower.y) * fractions[static_cast<std::size_t>(i)];
        point.z = lower.z + (higher.z - lower.z) * fractions[static_cast<std::size_t>(i)];
        if (!HasVerticalClearance(point, walkableHeight))
            return false;
    }
    return true;
}

bool CNavMeshBuilder::TraversalLinkInLineOfSight(void* userData, const rdVec3D* lowerPosition, const rdVec3D* higherPosition,
                                                 const rdVec2D* lowerNormal, const rdVec2D* higherNormal, float walkableHeight, float walkableRadius,
                                                 float slopeAngle)
{
    const auto& context = *static_cast<CNavMeshBuilder*>(userData);
    if (!context.EdgesFaceEachOther(*lowerPosition, *higherPosition, *lowerNormal, *higherNormal))
        return false;

    const float totalLedgeSpan = walkableRadius * 2.0f + TraverseRayExtraOffset;
    const float maximumAngle = rdCalcMaxLOSAngle(totalLedgeSpan, context.m_Hull.CellHeight);
    const float offsetAmount = rdCalcLedgeSpanOffsetAmount(totalLedgeSpan, slopeAngle, maximumAngle);

    rdVec3D offsetHigher = *higherPosition;
    if (offsetAmount > 0.0f)
    {
        offsetHigher.x += higherNormal->x * offsetAmount;
        offsetHigher.y += higherNormal->y * offsetAmount;
        if (context.m_Geometry.Raycast(*higherPosition, offsetHigher))
            return false;
    }
    if (context.m_Geometry.Raycast(offsetHigher, *lowerPosition))
        return false;

    if (rdCalcSlopeAngle(lowerPosition, higherPosition) < context.m_Settings.MaxSlope &&
        !context.LinkHasClearance(*lowerPosition, offsetHigher, walkableHeight))
    {
        return false;
    }
    if (!context.HasVerticalClearance(offsetHigher, walkableHeight * 2.0f) || !context.HasVerticalClearance(*lowerPosition, walkableHeight * 2.0f))
    {
        return false;
    }
    return true;
}

unsigned int* CNavMeshBuilder::FindTraversalPair(void* userData, dtPolyRef basePolygon, dtPolyRef landingPolygon)
{
    auto& context = *static_cast<CNavMeshBuilder*>(userData);
    const auto iterator = context.m_PolygonPairs.find({basePolygon, landingPolygon});
    return iterator == context.m_PolygonPairs.end() ? nullptr : &iterator->second;
}

int CNavMeshBuilder::AddTraversalPair(void* userData, dtPolyRef basePolygon, dtPolyRef landingPolygon, unsigned int traversalTypeBit)
{
    auto& context = *static_cast<CNavMeshBuilder*>(userData);
    try
    {
        const auto [iterator, inserted] = context.m_PolygonPairs.emplace(std::pair{basePolygon, landingPolygon}, traversalTypeBit);
        rdIgnoreUnused(iterator);
        return inserted ? 0 : 1;
    }
    catch (const std::bad_alloc&)
    {
        return -1;
    }
}

bool CNavMeshBuilder::TraversalTableSupportsLink(const dtTraverseTableCreateParams* parameters, const dtLink* link, int tableIndex)
{
    const HullType_t hullType = static_cast<HullType_t>(parameters->navMeshType);
    const HullSettings_t& hull = GetHullSettings(hullType);
    if (tableIndex < 0 || tableIndex >= hull.TraversalTableCount)
        return false;
    const unsigned char traversalType = link->getTraverseType();
    return traversalType < 32 && (hull.TraversalMasks[static_cast<std::size_t>(tableIndex)] & (std::uint32_t{1} << traversalType)) != 0;
}

void CNavMeshBuilder::RasterizeClipVolumes(const std::vector<const ClipBrush_t*>& brushes, rcHeightfield& solid)
{
    if (brushes.empty())
        return;
    rcHeightfield brushField;
    if (!rcCreateHeightfield(this, brushField, solid.width, solid.height, &solid.bmin, &solid.bmax, solid.cs, solid.ch))
        throw std::runtime_error("failed to allocate clip brush heightfield");
    std::fill(m_TriangleAreas.begin(), m_TriangleAreas.end(), ClipOnlyRasterArea);
    for (const ClipBrush_t* brush : brushes)
    {
        // Rasterize one convex brush at a time: joining unrelated brush shells
        // would incorrectly fill free space between vertically stacked clips.
        for (std::size_t first = brush->FirstIndex; first < brush->FirstIndex + brush->IndexCount;)
        {
            const int count = static_cast<int>(std::min(m_TriangleAreas.size(), (brush->FirstIndex + brush->IndexCount - first) / 3));
            if (!rcRasterizeTriangles(this, m_Geometry.m_Vertices.data(), static_cast<int>(m_Geometry.m_Vertices.size()),
                                      &m_Geometry.m_Triangles[first], m_TriangleAreas.data(), count, brushField, 0))
                throw std::runtime_error("failed to rasterize clip brush");
            first += static_cast<std::size_t>(count) * 3;
        }
        const int minimumX = std::clamp(static_cast<int>(std::floor((brush->BoundsMinimum.x - solid.bmin.x) / solid.cs)), 0, solid.width - 1);
        const int minimumY = std::clamp(static_cast<int>(std::floor((brush->BoundsMinimum.y - solid.bmin.y) / solid.cs)), 0, solid.height - 1);
        const int maximumX = std::clamp(static_cast<int>(std::floor((brush->BoundsMaximum.x - solid.bmin.x) / solid.cs)), 0, solid.width - 1);
        const int maximumY = std::clamp(static_cast<int>(std::floor((brush->BoundsMaximum.y - solid.bmin.y) / solid.cs)), 0, solid.height - 1);
        for (int y = minimumY; y <= maximumY; ++y)
            for (int x = minimumX; x <= maximumX; ++x)
            {
                rcSpan*& first = brushField.spans[x + y * solid.width];
                if (!first)
                    continue;
                rcSpan* last = first;
                while (last->next)
                    last = last->next;
                // The entire vertical intersection of a convex brush is solid,
                // not just its bottom/top faces. Floors inside tall clips cannot
                // become support even when the ceiling is above body clearance.
                if (!rcAddSpan(this, solid, x, y, first->smin, last->smax, ClipOnlyRasterArea, m_Configuration.walkableClimb))
                    throw std::runtime_error("failed to rasterize clip volume");
                // Reuse the tile-local pool for the next brush.
                last->next = brushField.freelist;
                brushField.freelist = first;
                first = nullptr;
            }
    }
}

void CNavMeshBuilder::ResolveClipSupport(const rcHeightfield& physicalSupport, rcHeightfield& solid)
{
    for (int column = 0; column < solid.width * solid.height; ++column)
    {
        const rcSpan* support = physicalSupport.spans[column];
        for (rcSpan* span = solid.spans[column]; span; span = span->next)
        {
            if (span->area != ClipOnlyRasterArea)
                continue;
            while (support && static_cast<int>(support->smax) < static_cast<int>(span->smax) - 1)
                support = support->next;
            // Only genuine upward physical triangles count, never a previously
            // resolved clip or low-hanging promotion. One voxel is the raster
            // tolerance; the hull's much larger step height is not support.
            if (support && std::abs(static_cast<int>(support->smax) - static_cast<int>(span->smax)) <= 1)
                span->area = GeneratedWalkableRasterArea;
        }
    }
}

void CNavMeshBuilder::FilterLowHangingWalkableObstacles(int walkableClimb, rcHeightfield& heightfield)
{
    rcScopedTimer timer(this, RC_TIMER_FILTER_LOW_OBSTACLES);
    for (int y = 0; y < heightfield.height; ++y)
    {
        for (int x = 0; x < heightfield.width; ++x)
        {
            rcSpan* previousSpan = nullptr;
            bool previousWasWalkable = false;
            unsigned char previousArea = RC_NULL_AREA;
            for (rcSpan* span = heightfield.spans[x + y * heightfield.width]; span != nullptr; previousSpan = span, span = span->next)
            {
                const bool explicitlyUnwalkable = span->area == ExplicitUnwalkableRasterArea || span->area == ClipOnlyRasterArea;
                const bool walkable = span->area == GeneratedWalkableRasterArea;
                if (!walkable && !explicitlyUnwalkable && previousWasWalkable &&
                    std::abs(static_cast<int>(span->smax) - static_cast<int>(previousSpan->smax)) <= walkableClimb)
                {
                    span->area = previousArea;
                }
                previousWasWalkable = walkable;
                previousArea = span->area;
            }
        }
    }

    for (int y = 0; y < heightfield.height; ++y)
    {
        for (int x = 0; x < heightfield.width; ++x)
        {
            for (rcSpan* span = heightfield.spans[x + y * heightfield.width]; span != nullptr; span = span->next)
            {
                if (span->area == ExplicitUnwalkableRasterArea || span->area == ClipOnlyRasterArea)
                    span->area = RC_NULL_AREA;
                else if (span->area == GeneratedWalkableRasterArea)
                    span->area = RC_WALKABLE_AREA;
            }
        }
    }
}

const rcSpan* CNavMeshBuilder::FindWalkableSpanNearHeight(const rcHeightfield& heightfield, int x, int y, int height, int walkableClimb)
{
    const rcSpan* best = nullptr;
    int bestDifference = walkableClimb + 1;
    for (const rcSpan* span = heightfield.spans[x + y * heightfield.width]; span != nullptr; span = span->next)
    {
        if (span->area != RC_WALKABLE_AREA)
            continue;
        const int difference = std::abs(static_cast<int>(span->smax) - height);
        if (difference <= walkableClimb && difference < bestDifference)
        {
            best = span;
            bestDifference = difference;
        }
    }
    return best;
}

bool CNavMeshBuilder::HasRoomForVirtualFloor(const rcHeightfield& heightfield, int x, int y, int height, int walkableHeight)
{
    const int floorMaximum = std::clamp(height, 1, RC_SPAN_MAX_HEIGHT);
    const int floorMinimum = floorMaximum - 1;
    const int ceiling = std::min(floorMaximum + walkableHeight, RC_SPAN_MAX_HEIGHT);
    for (const rcSpan* span = heightfield.spans[x + y * heightfield.width]; span != nullptr; span = span->next)
    {
        if (static_cast<int>(span->smax) > floorMinimum && static_cast<int>(span->smin) < ceiling)
            return false;
    }
    return true;
}

void CNavMeshBuilder::CollectFloorBridgesAlongAxis(const rcHeightfield& heightfield, int walkableHeight, int walkableClimb, int maximumMissingCells,
                                                   int directionX, int directionY, std::vector<FloorBridgeProposal_t>& proposals)
{
    for (int y = 0; y < heightfield.height; ++y)
    {
        for (int x = 0; x < heightfield.width; ++x)
        {
            for (const rcSpan* start = heightfield.spans[x + y * heightfield.width]; start != nullptr; start = start->next)
            {
                if (start->area != RC_WALKABLE_AREA)
                    continue;

                const int startHeight = static_cast<int>(start->smax);
                const int adjacentX = x + directionX;
                const int adjacentY = y + directionY;
                if (adjacentX < 0 || adjacentX >= heightfield.width || adjacentY < 0 || adjacentY >= heightfield.height ||
                    FindWalkableSpanNearHeight(heightfield, adjacentX, adjacentY, startHeight, walkableClimb))
                {
                    continue;
                }
                if (!HasRoomForVirtualFloor(heightfield, adjacentX, adjacentY, startHeight, walkableHeight))
                    continue;

                for (int distance = 2; distance <= maximumMissingCells + 1; ++distance)
                {
                    const int endX = x + directionX * distance;
                    const int endY = y + directionY * distance;
                    if (endX < 0 || endX >= heightfield.width || endY < 0 || endY >= heightfield.height)
                        break;

                    const rcSpan* end = FindWalkableSpanNearHeight(heightfield, endX, endY, startHeight, walkableClimb);
                    if (!end)
                    {
                        if (!HasRoomForVirtualFloor(heightfield, endX, endY, startHeight, walkableHeight))
                            break;
                        continue;
                    }

                    const int endHeight = static_cast<int>(end->smax);
                    bool clear = true;
                    for (int step = 1; step < distance; ++step)
                    {
                        const int interpolatedHeight =
                            startHeight + static_cast<int>(std::lround(static_cast<double>(endHeight - startHeight) * step / distance));
                        const int bridgeX = x + directionX * step;
                        const int bridgeY = y + directionY * step;
                        if (!HasRoomForVirtualFloor(heightfield, bridgeX, bridgeY, interpolatedHeight, walkableHeight))
                        {
                            clear = false;
                            break;
                        }
                    }
                    if (clear)
                    {
                        for (int step = 1; step < distance; ++step)
                        {
                            const int interpolatedHeight =
                                startHeight + static_cast<int>(std::lround(static_cast<double>(endHeight - startHeight) * step / distance));
                            proposals.push_back({x + directionX * step, y + directionY * step,
                                                 static_cast<unsigned short>(std::clamp(interpolatedHeight, 1, RC_SPAN_MAX_HEIGHT))});
                        }
                    }
                    break;
                }
            }
        }
    }
}

void CNavMeshBuilder::FillNarrowFloorGaps(int walkableHeight, int walkableClimb, rcHeightfield& heightfield)
{
    const int maximumMissingCells = std::max(0, static_cast<int>(std::ceil(m_Hull.Radius * 2.0f / heightfield.cs)) - 1);
    if (maximumMissingCells == 0)
        return;

    m_FloorBridgeProposals.clear();
    auto& proposals = m_FloorBridgeProposals;
    CollectFloorBridgesAlongAxis(heightfield, walkableHeight, walkableClimb, maximumMissingCells, 1, 0, proposals);
    CollectFloorBridgesAlongAxis(heightfield, walkableHeight, walkableClimb, maximumMissingCells, 0, 1, proposals);
    std::sort(proposals.begin(), proposals.end(), [](const FloorBridgeProposal_t& first, const FloorBridgeProposal_t& second)
    {
        if (first.Y != second.Y)
            return first.Y < second.Y;
        if (first.X != second.X)
            return first.X < second.X;
        return first.Height < second.Height;
    });

    for (std::size_t begin = 0; begin < proposals.size();)
    {
        std::size_t end = begin + 1;
        while (end < proposals.size() && proposals[end].X == proposals[begin].X && proposals[end].Y == proposals[begin].Y &&
               static_cast<int>(proposals[end].Height) - static_cast<int>(proposals[end - 1].Height) <= walkableClimb)
        {
            ++end;
        }

        const FloorBridgeProposal_t& proposal = proposals[begin + (end - begin) / 2];
        if (HasRoomForVirtualFloor(heightfield, proposal.X, proposal.Y, proposal.Height, walkableHeight) &&
            !rcAddSpan(this, heightfield, proposal.X, proposal.Y, static_cast<unsigned short>(proposal.Height - 1), proposal.Height, RC_WALKABLE_AREA,
                       walkableClimb))
        {
            throw std::runtime_error("failed to add a virtual floor span");
        }
        begin = end;
    }
}

void CNavMeshBuilder::SelectOffMeshConnections()
{
    m_OffMesh.Vertices.clear();
    m_OffMesh.ReferencePositions.clear();
    m_OffMesh.Radii.clear();
    m_OffMesh.ReferenceYaws.clear();
    m_OffMesh.Directions.clear();
    m_OffMesh.TraversalTypes.clear();
    m_OffMesh.LookupOrders.clear();
    m_OffMesh.Areas.clear();
    m_OffMesh.Flags.clear();
    m_OffMesh.UserIds.clear();
    auto& arrays = m_OffMesh;
    for (const OffMeshConnection_t& connection : m_Geometry.m_OffMeshConnections)
    {
        if (!SupportsTraversal(connection.TraversalType))
            continue;
        arrays.Vertices.push_back(connection.Start);
        arrays.Vertices.push_back(connection.End);
        arrays.ReferencePositions.push_back(connection.ReferencePosition);
        arrays.Radii.push_back(connection.Radius);
        arrays.ReferenceYaws.push_back(connection.ReferenceYaw);
        arrays.Directions.push_back(connection.Bidirectional ? 1 : 0);
        arrays.TraversalTypes.push_back(connection.TraversalType);
        arrays.LookupOrders.push_back(connection.LookupOrder);
        arrays.Areas.push_back(connection.Area);
        arrays.Flags.push_back(connection.Flags);
        arrays.UserIds.push_back(connection.UserId);
    }
}

void CNavMeshBuilder::ApplyAreaVolumes(rcCompactHeightfield& compact)
{
    for (const AreaVolume_t& volume : m_Geometry.m_AreaVolumes)
    {
        switch (volume.Type)
        {
        case AreaVolumeType_t::Box:
            if (volume.Vertices.size() == 2)
                rcMarkBoxArea(this, &volume.Vertices[0], &volume.Vertices[1], volume.Flags, volume.Area, compact);
            break;
        case AreaVolumeType_t::Cylinder:
            if (volume.Vertices.size() == 1)
                rcMarkCylinderArea(this, &volume.Vertices[0], volume.Radius, volume.Height, volume.Flags, volume.Area, compact);
            break;
        case AreaVolumeType_t::Convex:
            if (volume.Vertices.size() >= 3)
                rcMarkConvexPolyArea(this, volume.Vertices.data(), static_cast<int>(volume.Vertices.size()), volume.MinimumHeight,
                                     volume.MaximumHeight, volume.Flags, volume.Area, compact);
            break;
        }
    }
}

class CNavMeshBuilder::CPassageRepair
{
  public:
    CPassageRepair(rcContext& context, const HullSettings_t& hull, const rcHeightfield& heightfield, int walkableHeight, int walkableClimb,
                   rcCompactHeightfield& compact)
        : m_Context(context), m_Hull(hull), m_Heightfield(heightfield), m_WalkableHeight(walkableHeight), m_WalkableClimb(walkableClimb),
          m_Compact(compact)
    {
    }

  private:
    struct PassageConnection_t
    {
        int Distance;
        int FirstOwner;
        int SecondOwner;
        int FirstSpan;
        int SecondSpan;
    };

    // Recast compact links can be one-way across stacked layers. Repair only reciprocal links because later region building consumes the original
    // directed connections; synthesizing reverse adjacency here could restore a connector that never reaches the serialized navmesh.
    using CompactNeighbours_t = std::vector<std::array<int, 4>>;

    CompactNeighbours_t BuildCompactNeighbours(const rcCompactHeightfield& compact)
    {
        CompactNeighbours_t neighbours(static_cast<std::size_t>(compact.spanCount));
        for (std::array<int, 4>& spanNeighbours : neighbours)
            spanNeighbours.fill(-1);

        for (int y = 0; y < compact.height; ++y)
        {
            for (int x = 0; x < compact.width; ++x)
            {
                const rcCompactCell& cell = compact.cells[x + y * compact.width];
                for (unsigned int index = cell.index; index < cell.index + cell.count; ++index)
                {
                    for (int direction = 0; direction < 4; ++direction)
                    {
                        const int connection = rcGetCon(compact.spans[index], direction);
                        if (connection == RC_NOT_CONNECTED)
                            continue;
                        const int neighbourX = x + rcGetDirOffsetX(direction);
                        const int neighbourY = y + rcGetDirOffsetY(direction);
                        if (neighbourX < 0 || neighbourX >= compact.width || neighbourY < 0 || neighbourY >= compact.height)
                            continue;
                        const rcCompactCell& neighbourCell = compact.cells[neighbourX + neighbourY * compact.width];
                        if (connection >= static_cast<int>(neighbourCell.count))
                            continue;
                        const unsigned int neighbour = neighbourCell.index + static_cast<unsigned int>(connection);
                        if (neighbour < static_cast<unsigned int>(compact.spanCount))
                            neighbours[index][static_cast<std::size_t>(direction)] = static_cast<int>(neighbour);
                    }
                }
            }
        }

        const CompactNeighbours_t directedNeighbours = neighbours;
        for (int index = 0; index < compact.spanCount; ++index)
        {
            for (int& neighbour : neighbours[static_cast<std::size_t>(index)])
            {
                if (neighbour < 0)
                    continue;
                const std::array<int, 4>& reverseCandidates = directedNeighbours[static_cast<std::size_t>(neighbour)];
                if (std::find(reverseCandidates.begin(), reverseCandidates.end(), index) == reverseCandidates.end())
                    neighbour = -1;
            }
        }
        return neighbours;
    }

    struct CompactSpanLocation_t
    {
        int X;
        int Y;
        int Z;
    };

    std::vector<CompactSpanLocation_t> BuildCompactSpanLocations(const rcCompactHeightfield& compact)
    {
        std::vector<CompactSpanLocation_t> locations(static_cast<std::size_t>(compact.spanCount));
        for (int y = 0; y < compact.height; ++y)
        {
            for (int x = 0; x < compact.width; ++x)
            {
                const rcCompactCell& cell = compact.cells[x + y * compact.width];
                for (unsigned int index = cell.index; index < cell.index + cell.count; ++index)
                    locations[index] = {x, y, static_cast<int>(compact.spans[index].z)};
            }
        }
        return locations;
    }

    class CWalkableComponents
    {
      public:
        CWalkableComponents(const CompactNeighbours_t& neighbours, const std::vector<unsigned char>& areas)
            : m_Parents(areas.size(), -1), m_Ranks(areas.size(), 0)
        {
            for (int span = 0; span < static_cast<int>(areas.size()); ++span)
            {
                if (areas[static_cast<std::size_t>(span)] != RC_NULL_AREA)
                    m_Parents[static_cast<std::size_t>(span)] = span;
            }
            for (int span = 0; span < static_cast<int>(areas.size()); ++span)
            {
                if (m_Parents[static_cast<std::size_t>(span)] < 0)
                    continue;
                for (const int neighbour : neighbours[static_cast<std::size_t>(span)])
                {
                    if (neighbour >= 0 && m_Parents[static_cast<std::size_t>(neighbour)] >= 0)
                        Merge(span, neighbour);
                }
            }
        }

        bool Connected(int first, int second)
        {
            return Find(first) == Find(second);
        }

        void Activate(int span, const CompactNeighbours_t& neighbours)
        {
            int& parent = m_Parents[static_cast<std::size_t>(span)];
            if (parent >= 0)
                return;
            parent = span;
            for (const int neighbour : neighbours[static_cast<std::size_t>(span)])
            {
                if (neighbour >= 0 && m_Parents[static_cast<std::size_t>(neighbour)] >= 0)
                    Merge(span, neighbour);
            }
        }

      private:
        int Find(int span)
        {
            int& parent = m_Parents[static_cast<std::size_t>(span)];
            if (parent != span)
                parent = Find(parent);
            return parent;
        }

        void Merge(int first, int second)
        {
            first = Find(first);
            second = Find(second);
            if (first == second)
                return;
            if (m_Ranks[static_cast<std::size_t>(first)] < m_Ranks[static_cast<std::size_t>(second)])
                std::swap(first, second);
            m_Parents[static_cast<std::size_t>(second)] = first;
            if (m_Ranks[static_cast<std::size_t>(first)] == m_Ranks[static_cast<std::size_t>(second)])
                ++m_Ranks[static_cast<std::size_t>(first)];
        }

        std::vector<int> m_Parents;
        std::vector<unsigned char> m_Ranks;
    };

    void AddPassageConnection(std::vector<PassageConnection_t>& connections, int distance, int firstOwner, int secondOwner, int firstSpan,
                              int secondSpan)
    {
        if (firstOwner == secondOwner)
            return;
        if (firstOwner > secondOwner)
        {
            std::swap(firstOwner, secondOwner);
            std::swap(firstSpan, secondSpan);
        }
        connections.push_back({distance, firstOwner, secondOwner, firstSpan, secondSpan});
    }

    bool HasWalkablePathWithinDistance(const CompactNeighbours_t& neighbours, const std::vector<CompactSpanLocation_t>& locations,
                                       const std::vector<unsigned char>& areas, int start, int goal, int maximumDistance, std::vector<int>& distances,
                                       std::vector<int>& pending)
    {
        const auto minimumRemainingDistance = [&](int span)
        {
            const CompactSpanLocation_t& current = locations[static_cast<std::size_t>(span)];
            const CompactSpanLocation_t& destination = locations[static_cast<std::size_t>(goal)];
            return std::abs(current.X - destination.X) + std::abs(current.Y - destination.Y);
        };
        if (minimumRemainingDistance(start) > maximumDistance)
            return false;

        pending.clear();
        pending.push_back(start);
        distances[static_cast<std::size_t>(start)] = 0;
        bool found = false;
        for (std::size_t cursor = 0; cursor < pending.size(); ++cursor)
        {
            const int current = pending[cursor];
            if (current == goal)
            {
                found = true;
                break;
            }
            const int nextDistance = distances[static_cast<std::size_t>(current)] + 1;
            if (nextDistance > maximumDistance)
                continue;
            for (const int neighbour : neighbours[static_cast<std::size_t>(current)])
            {
                if (neighbour < 0 || areas[static_cast<std::size_t>(neighbour)] == RC_NULL_AREA ||
                    distances[static_cast<std::size_t>(neighbour)] != -1 || nextDistance + minimumRemainingDistance(neighbour) > maximumDistance)
                {
                    continue;
                }
                distances[static_cast<std::size_t>(neighbour)] = nextDistance;
                pending.push_back(neighbour);
            }
        }
        for (const int visited : pending)
            distances[static_cast<std::size_t>(visited)] = -1;
        return found;
    }

    bool SpanHasPassageClearance(const rcHeightfield& heightfield, const CompactSpanLocation_t& location, int obstacleRadius, int supportRadius,
                                 int walkableHeight, int walkableClimb, signed char& cached)
    {
        if (cached != -1)
            return cached != 0;

        const int scaledObstacleRadiusSquared = obstacleRadius * obstacleRadius * 4;
        const int scaledSupportRadiusSquared = supportRadius * supportRadius * 4;
        const int maximumRadius = std::max(obstacleRadius, supportRadius);
        const int bodyMinimum = location.Z + walkableClimb;
        const int bodyMaximum = location.Z + walkableHeight;
        for (int offsetY = -maximumRadius; offsetY <= maximumRadius; ++offsetY)
        {
            if (location.Y + offsetY < 0 || location.Y + offsetY >= heightfield.height)
            {
                cached = 0;
                return false;
            }
            const int y = location.Y + offsetY;
            for (int offsetX = -maximumRadius; offsetX <= maximumRadius; ++offsetX)
            {
                const int closestX = std::max(0, std::abs(offsetX) * 2 - 1);
                const int closestY = std::max(0, std::abs(offsetY) * 2 - 1);
                const int scaledDistanceSquared = closestX * closestX + closestY * closestY;
                const bool checkObstacles = scaledDistanceSquared <= scaledObstacleRadiusSquared;
                const bool checkSupport = scaledDistanceSquared <= scaledSupportRadiusSquared;
                if (!checkObstacles && !checkSupport)
                    continue;
                if (location.X + offsetX < 0 || location.X + offsetX >= heightfield.width)
                {
                    cached = 0;
                    return false;
                }
                const int x = location.X + offsetX;
                bool hasSupport = !checkSupport;
                for (const rcSpan* span = heightfield.spans[x + y * heightfield.width]; span != nullptr; span = span->next)
                {
                    if (checkSupport && span->area != RC_NULL_AREA && std::abs(static_cast<int>(span->smax) - location.Z) <= walkableClimb)
                    {
                        hasSupport = true;
                    }
                    if (checkObstacles && static_cast<int>(span->smax) > bodyMinimum && static_cast<int>(span->smin) < bodyMaximum)
                    {
                        cached = 0;
                        return false;
                    }
                }
                if (!hasSupport)
                {
                    cached = 0;
                    return false;
                }
            }
        }
        cached = 1;
        return true;
    }

    bool PassageHasClearance(const PassageConnection_t& connection, const std::vector<int>& predecessors,
                             const std::vector<CompactSpanLocation_t>& locations, const rcHeightfield& heightfield, int obstacleRadius,
                             int supportRadius, int walkableHeight, int walkableClimb, std::vector<signed char>& clearanceCache)
    {
        const auto spanIsClear = [&](int span)
        {
            return SpanHasPassageClearance(heightfield, locations[static_cast<std::size_t>(span)], obstacleRadius, supportRadius, walkableHeight,
                                           walkableClimb, clearanceCache[static_cast<std::size_t>(span)]);
        };
        for (int span = connection.FirstSpan; span >= 0; span = predecessors[static_cast<std::size_t>(span)])
        {
            if (!spanIsClear(span))
                return false;
        }
        for (int span = connection.SecondSpan; span >= 0; span = predecessors[static_cast<std::size_t>(span)])
        {
            if (!spanIsClear(span))
                return false;
        }
        return true;
    }

    int RestoreLocalPassageShortcuts(const CompactNeighbours_t& neighbours, const std::vector<CompactSpanLocation_t>& locations,
                                     const rcHeightfield& heightfield, int obstacleClearanceRadius, int supportRadius, int walkableHeight,
                                     int walkableClimb, const std::vector<unsigned char>& originalAreas,
                                     const std::vector<unsigned char>& relaxedAreas, int fullRadius, std::vector<signed char>& clearanceCache,
                                     std::vector<unsigned char>& finalAreas)
    {
        const std::size_t spanCount = finalAreas.size();
        m_Candidates.assign(spanCount, 0);
        auto& candidates = m_Candidates;
        m_Owners.assign(spanCount, -1);
        auto& owners = m_Owners;
        m_Distances.assign(spanCount, -1);
        auto& distances = m_Distances;
        m_Predecessors.assign(spanCount, -1);
        auto& predecessors = m_Predecessors;
        m_Pending.clear();
        auto& pending = m_Pending;
        pending.reserve(spanCount);
        m_Connections.clear();
        auto& connections = m_Connections;

        for (int index = 0; index < static_cast<int>(spanCount); ++index)
        {
            if (finalAreas[static_cast<std::size_t>(index)] != RC_NULL_AREA || relaxedAreas[static_cast<std::size_t>(index)] == RC_NULL_AREA)
                continue;
            candidates[static_cast<std::size_t>(index)] = 1;
            for (const int neighbour : neighbours[static_cast<std::size_t>(index)])
            {
                if (neighbour < 0 || finalAreas[static_cast<std::size_t>(neighbour)] == RC_NULL_AREA)
                    continue;
                int& owner = owners[static_cast<std::size_t>(index)];
                if (owner == -1)
                {
                    owner = neighbour;
                    distances[static_cast<std::size_t>(index)] = 0;
                    pending.push_back(index);
                }
                else
                {
                    AddPassageConnection(connections, 0, owner, neighbour, index, index);
                }
            }
        }

        // A multi-source wave through relaxed-only spans finds the shortest candidate connector for each pair of fully eroded shores.
        for (std::size_t cursor = 0; cursor < pending.size(); ++cursor)
        {
            const int current = pending[cursor];
            for (const int neighbour : neighbours[static_cast<std::size_t>(current)])
            {
                if (neighbour < 0 || candidates[static_cast<std::size_t>(neighbour)] == 0)
                    continue;
                if (owners[static_cast<std::size_t>(neighbour)] == -1)
                {
                    owners[static_cast<std::size_t>(neighbour)] = owners[static_cast<std::size_t>(current)];
                    distances[static_cast<std::size_t>(neighbour)] = distances[static_cast<std::size_t>(current)] + 1;
                    predecessors[static_cast<std::size_t>(neighbour)] = current;
                    pending.push_back(neighbour);
                }
                else
                {
                    AddPassageConnection(connections,
                                         distances[static_cast<std::size_t>(current)] + distances[static_cast<std::size_t>(neighbour)] + 1,
                                         owners[static_cast<std::size_t>(current)], owners[static_cast<std::size_t>(neighbour)], current, neighbour);
                }
            }
        }

        std::sort(connections.begin(), connections.end(), [](const PassageConnection_t& first, const PassageConnection_t& second)
        {
            if (first.FirstOwner != second.FirstOwner)
                return first.FirstOwner < second.FirstOwner;
            if (first.SecondOwner != second.SecondOwner)
                return first.SecondOwner < second.SecondOwner;
            if (first.Distance != second.Distance)
                return first.Distance < second.Distance;
            if (first.FirstSpan != second.FirstSpan)
                return first.FirstSpan < second.FirstSpan;
            return first.SecondSpan < second.SecondSpan;
        });
        // Keep the shortest clearance-safe path per shore pair; the geometrically shortest wave may cross an overhang or unsupported edge.
        m_ClearConnections.clear();
        auto& clearConnections = m_ClearConnections;
        for (std::size_t first = 0; first < connections.size();)
        {
            std::size_t end = first + 1;
            while (end < connections.size() && connections[end].FirstOwner == connections[first].FirstOwner &&
                   connections[end].SecondOwner == connections[first].SecondOwner)
            {
                ++end;
            }
            for (std::size_t candidate = first; candidate < end; ++candidate)
            {
                if (!PassageHasClearance(connections[candidate], predecessors, locations, heightfield, obstacleClearanceRadius, supportRadius,
                                         walkableHeight, walkableClimb, clearanceCache))
                {
                    continue;
                }
                clearConnections.push_back(connections[candidate]);
                break;
            }
            first = end;
        }
        connections.swap(clearConnections);
        std::sort(connections.begin(), connections.end(), [](const PassageConnection_t& first, const PassageConnection_t& second)
        {
            if (first.Distance != second.Distance)
                return first.Distance < second.Distance;
            if (first.FirstOwner != second.FirstOwner)
                return first.FirstOwner < second.FirstOwner;
            if (first.SecondOwner != second.SecondOwner)
                return first.SecondOwner < second.SecondOwner;
            if (first.FirstSpan != second.FirstSpan)
                return first.FirstSpan < second.FirstSpan;
            return first.SecondSpan < second.SecondSpan;
        });

        // Incremental components avoid searching an entire disconnected island for every candidate; only already-connected shores need a path query.
        CWalkableComponents components(neighbours, finalAreas);
        m_PathDistances.assign(spanCount, -1);
        auto& pathDistances = m_PathDistances;
        m_PathPending.clear();
        auto& pathPending = m_PathPending;
        pathPending.reserve(spanCount);
        int restoredSpanCount = 0;
        const auto restorePath = [&](int firstSpan)
        {
            for (int span = firstSpan; span >= 0; span = predecessors[static_cast<std::size_t>(span)])
            {
                unsigned char& finalArea = finalAreas[static_cast<std::size_t>(span)];
                if (finalArea == RC_NULL_AREA)
                {
                    finalArea = originalAreas[static_cast<std::size_t>(span)];
                    ++restoredSpanCount;
                }
                components.Activate(span, neighbours);
            }
        };
        for (const PassageConnection_t& connection : connections)
        {
            const int shortcutDistance = connection.Distance + 2;
            const int acceptableDetour = shortcutDistance * 2 + fullRadius * 2;
            // A distant route must not suppress a missing local doorway. Reject restoration only when a comparably short walkable detour exists.
            if (components.Connected(connection.FirstOwner, connection.SecondOwner) &&
                HasWalkablePathWithinDistance(neighbours, locations, finalAreas, connection.FirstOwner, connection.SecondOwner, acceptableDetour,
                                              pathDistances, pathPending))
            {
                continue;
            }
            restorePath(connection.FirstSpan);
            restorePath(connection.SecondSpan);
        }
        return restoredSpanCount;
    }

  public:
    int ErodeWalkableArea()
    {
        const int globalRadius = static_cast<int>(std::ceil(m_Hull.Radius / m_Compact.cs));
        const std::vector<unsigned char> originalAreas(m_Compact.areas, m_Compact.areas + m_Compact.spanCount);
        if (!rcErodeWalkableArea(&m_Context, globalRadius, m_Compact))
            throw std::runtime_error("failed to erode tile walkable area");
        std::vector<unsigned char> finalAreas(m_Compact.areas, m_Compact.areas + m_Compact.spanCount);
        int restoredSpanCount = 0;

        if (globalRadius > 1)
        {
            const CompactNeighbours_t neighbours = BuildCompactNeighbours(m_Compact);
            const std::vector<CompactSpanLocation_t> locations = BuildCompactSpanLocations(m_Compact);
            std::vector<signed char> clearanceCache(static_cast<std::size_t>(m_Compact.spanCount), -1);
            // Reject nearby collision and unsupported edges while relaxing conservative rasterization by at most two voxels.
            const int minimumRelaxedRadius = std::max(1, globalRadius - 2);
            const int obstacleClearanceRadius = minimumRelaxedRadius;
            const int supportRadius = minimumRelaxedRadius;
            for (int relaxedRadius = globalRadius - 1; relaxedRadius >= minimumRelaxedRadius; --relaxedRadius)
            {
                std::copy(originalAreas.begin(), originalAreas.end(), m_Compact.areas);
                if (!rcErodeWalkableArea(&m_Context, relaxedRadius, m_Compact))
                    throw std::runtime_error("failed to calculate relaxed walkable-area erosion");
                const std::vector<unsigned char> relaxedAreas(m_Compact.areas, m_Compact.areas + m_Compact.spanCount);
                restoredSpanCount +=
                    RestoreLocalPassageShortcuts(neighbours, locations, m_Heightfield, obstacleClearanceRadius, supportRadius, m_WalkableHeight,
                                                 m_WalkableClimb, originalAreas, relaxedAreas, globalRadius, clearanceCache, finalAreas);
            }
        }

        std::copy(finalAreas.begin(), finalAreas.end(), m_Compact.areas);
        return restoredSpanCount;
    }

  private:
    rcContext& m_Context;
    const HullSettings_t& m_Hull;
    const rcHeightfield& m_Heightfield;
    int m_WalkableHeight;
    int m_WalkableClimb;
    rcCompactHeightfield& m_Compact;
    std::vector<unsigned char> m_Candidates;
    std::vector<int> m_Owners;
    std::vector<int> m_Distances;
    std::vector<int> m_Predecessors;
    std::vector<int> m_Pending;
    std::vector<PassageConnection_t> m_Connections;
    std::vector<PassageConnection_t> m_ClearConnections;
    std::vector<int> m_PathDistances;
    std::vector<int> m_PathPending;
};

void CNavMeshBuilder::Configure()
{
    m_Configuration = {};
    rcConfig& configuration = m_Configuration;
    configuration.cs = m_Hull.CellSize;
    configuration.ch = m_Hull.CellHeight;
    configuration.walkableSlopeAngle = m_Settings.MaxSlope;
    configuration.walkableHeight = static_cast<int>(std::ceil(m_Hull.Height / configuration.ch));
    configuration.walkableClimb = static_cast<int>(std::floor(m_Hull.MaxClimb / configuration.ch));
    configuration.walkableRadius = static_cast<int>(std::ceil(m_Hull.Radius / configuration.cs));
    configuration.maxEdgeLen = static_cast<int>(m_Settings.EdgeMaxLength / m_Hull.CellSize);
    configuration.maxSimplificationError = m_Settings.EdgeMaxError;
    configuration.minRegionArea = m_Settings.RegionMinSize * m_Settings.RegionMinSize;
    configuration.mergeRegionArea = m_Settings.RegionMergeSize * m_Settings.RegionMergeSize;
    configuration.maxVertsPerPoly = m_Settings.VerticesPerPolygon;
    configuration.tileSize = m_Settings.TileSize;
    configuration.borderSize = configuration.walkableRadius + 3;
    configuration.width = configuration.tileSize + configuration.borderSize * 2;
    configuration.height = configuration.tileSize + configuration.borderSize * 2;
    configuration.detailSampleDist = m_Settings.DetailSampleDistance < 0.9f ? 0.0f : m_Hull.CellSize * m_Settings.DetailSampleDistance;
    configuration.detailSampleMaxError = m_Hull.CellHeight * m_Settings.DetailSampleMaxError;
    configuration.ignoreWindingOrder = m_Settings.IgnoreWinding;

    std::size_t maximumChunkTriangleCount = 0;
    for (const auto& chunkyMesh : m_Geometry.m_ChunkyMeshes)
        maximumChunkTriangleCount = std::max(maximumChunkTriangleCount, static_cast<std::size_t>(chunkyMesh->maxTrisPerChunk));
    m_TriangleAreas.resize(maximumChunkTriangleCount);
}

CNavMeshBuilder::TileData_t CNavMeshBuilder::BuildTile(int tileX, int tileY, const rdVec3D& tileMinimum, const rdVec3D& tileMaximum)
{
    rcConfig& configuration = m_Configuration;
    configuration.bmin = tileMinimum;
    configuration.bmax = tileMaximum;
    configuration.bmin.x -= configuration.borderSize * configuration.cs;
    configuration.bmin.y -= configuration.borderSize * configuration.cs;
    configuration.bmax.x += configuration.borderSize * configuration.cs;
    configuration.bmax.y += configuration.borderSize * configuration.cs;

    std::unique_ptr<rcHeightfield, decltype([](rcHeightfield* field) { rcFreeHeightField(field); })> solid(rcAllocHeightfield());
    if (!solid || !rcCreateHeightfield(this, *solid, configuration.width, configuration.height, &configuration.bmin, &configuration.bmax,
                                       configuration.cs, configuration.ch))
    {
        throw std::runtime_error("failed to allocate tile heightfield");
    }
    rcHeightfield physicalSupport;
    const bool hasClipBrushes =
        std::any_of(m_Geometry.m_ClipBrushes.begin(), m_Geometry.m_ClipBrushes.end(), [&configuration](const ClipBrush_t& brush)
    {
        return brush.BoundsMinimum.x <= configuration.bmax.x && brush.BoundsMaximum.x >= configuration.bmin.x &&
               brush.BoundsMinimum.y <= configuration.bmax.y && brush.BoundsMaximum.y >= configuration.bmin.y;
    });
    if (hasClipBrushes && !rcCreateHeightfield(this, physicalSupport, configuration.width, configuration.height, &configuration.bmin,
                                               &configuration.bmax, configuration.cs, configuration.ch))
        throw std::runtime_error("failed to allocate physical support heightfield");
    std::vector<const ClipBrush_t*> clipBrushes;

    auto& triangleAreas = m_TriangleAreas;
    rdVec2D queryMinimum(configuration.bmin);
    rdVec2D queryMaximum(configuration.bmax);
    auto& chunkIds = m_ChunkIds;
    int tileTriangleCount = 0;
    for (const auto& chunkyMesh : m_Geometry.m_ChunkyMeshes)
    {
        int currentNode = 0;
        bool done = false;
        do
        {
            int chunkCount = 0;
            done = rcGetChunksOverlappingRect(chunkyMesh.get(), &queryMinimum, &queryMaximum, chunkIds.data(), static_cast<int>(chunkIds.size()),
                                              chunkCount, currentNode) != 0;
            for (int chunk = 0; chunk < chunkCount; ++chunk)
            {
                const rcChunkyTriMeshNode& node = chunkyMesh->nodes[chunkIds[static_cast<std::size_t>(chunk)]];
                const int* indices = &chunkyMesh->tris[node.i * 3];
                tileTriangleCount += node.n;
                std::fill_n(triangleAreas.begin(), node.n, 0);
                rcMarkWalkableTriangles(this, configuration.walkableSlopeAngle, m_Geometry.m_Vertices.data(),
                                        static_cast<int>(m_Geometry.m_Vertices.size()), indices, node.n, triangleAreas.data(),
                                        configuration.ignoreWindingOrder);
                for (int triangle = 0; triangle < node.n; ++triangle)
                {
                    const int* triangleIndices = &indices[triangle * 3];
                    if (m_Geometry.IsTriangleUnwalkable(triangleIndices[0], triangleIndices[1], triangleIndices[2]))
                        triangleAreas[static_cast<std::size_t>(triangle)] = ExplicitUnwalkableRasterArea;
                    else if (const ClipBrush_t* brush = hasClipBrushes ? m_Geometry.FindClipBrush(triangleIndices[0]) : nullptr)
                    {
                        triangleAreas[static_cast<std::size_t>(triangle)] = ClipOnlyRasterArea;
                        clipBrushes.push_back(brush);
                    }
                    else if (triangleAreas[static_cast<std::size_t>(triangle)] != RC_NULL_AREA)
                        triangleAreas[static_cast<std::size_t>(triangle)] = GeneratedWalkableRasterArea;
                }
                if (!rcRasterizeTriangles(this, m_Geometry.m_Vertices.data(), static_cast<int>(m_Geometry.m_Vertices.size()), indices,
                                          triangleAreas.data(), node.n, *solid, configuration.walkableClimb))
                {
                    throw std::runtime_error("failed to rasterize tile geometry");
                }
                if (hasClipBrushes)
                {
                    // Only actual upward physical faces enter this field, not
                    // null faces that could merge support upward by step height.
                    if (configuration.ignoreWindingOrder)
                    {
                        for (int triangle = 0; triangle < node.n; ++triangle)
                        {
                            if (triangleAreas[static_cast<std::size_t>(triangle)] != GeneratedWalkableRasterArea)
                                continue;
                            const int* triangleIndices = &indices[triangle * 3];
                            const rdVec3D& a = m_Geometry.m_Vertices[triangleIndices[0]];
                            const rdVec3D& b = m_Geometry.m_Vertices[triangleIndices[1]];
                            const rdVec3D& c = m_Geometry.m_Vertices[triangleIndices[2]];
                            if ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) <= 0.0f)
                                triangleAreas[static_cast<std::size_t>(triangle)] = RC_NULL_AREA;
                        }
                    }
                    for (int triangle = 0; triangle < node.n;)
                    {
                        if (triangleAreas[static_cast<std::size_t>(triangle)] != GeneratedWalkableRasterArea)
                        {
                            ++triangle;
                            continue;
                        }
                        const int first = triangle++;
                        while (triangle < node.n && triangleAreas[static_cast<std::size_t>(triangle)] == GeneratedWalkableRasterArea)
                            ++triangle;
                        if (!rcRasterizeTriangles(this, m_Geometry.m_Vertices.data(), static_cast<int>(m_Geometry.m_Vertices.size()),
                                                  &indices[first * 3], &triangleAreas[static_cast<std::size_t>(first)], triangle - first,
                                                  physicalSupport, 0))
                            throw std::runtime_error("failed to rasterize physical support");
                    }
                }
            }
        } while (!done);
    }

    if (tileTriangleCount == 0)
        return {};
    if (!clipBrushes.empty())
    {
        std::sort(clipBrushes.begin(), clipBrushes.end());
        clipBrushes.erase(std::unique(clipBrushes.begin(), clipBrushes.end()), clipBrushes.end());
        RasterizeClipVolumes(clipBrushes, *solid);
        ResolveClipSupport(physicalSupport, *solid);
    }

    FilterLowHangingWalkableObstacles(configuration.walkableClimb, *solid);
    FillNarrowFloorGaps(configuration.walkableHeight, configuration.walkableClimb, *solid);
    rcFilterLedgeSpans(this, configuration.walkableHeight, configuration.walkableClimb, false, *solid);
    rcFilterWalkableLowHeightSpans(this, configuration.walkableHeight, *solid);

    // A tile may contain geometry but no walkable spans after filtering (for
    // example, map-boundary cliffs). Recast reports an empty compact
    // heightfield as a build failure, so discard the tile before asking it to
    // compact the remaining spans.
    if (rcGetHeightFieldSpanCount(this, *solid) == 0)
        return {};

    for (int cellIndex = 0; cellIndex < solid->width * solid->height; ++cellIndex)
    {
        int walkableLayerCount = 0;
        for (const rcSpan* span = solid->spans[cellIndex]; span != nullptr; span = span->next)
        {
            if (span->area != RC_NULL_AREA)
                ++walkableLayerCount;
        }
        if (walkableLayerCount > RC_NOT_CONNECTED)
            throw std::runtime_error("heightfield exceeds Recast's 63-layer connection limit");
    }

    std::unique_ptr<rcCompactHeightfield, decltype([](rcCompactHeightfield* field) { rcFreeCompactHeightfield(field); })> compact(
        rcAllocCompactHeightfield());
    if (!compact || !rcBuildCompactHeightfield(this, configuration.walkableHeight, configuration.walkableClimb, *solid, *compact))
    {
        throw std::runtime_error("failed to build compact tile heightfield");
    }

    const int restoredPassageSpanCount =
        CPassageRepair(*this, m_Hull, *solid, configuration.walkableHeight, configuration.walkableClimb, *compact).ErodeWalkableArea();
    solid.reset();
    ApplyAreaVolumes(*compact);

    if (m_Settings.PartitionType == 0)
    {
        if (!rcBuildDistanceField(this, *compact) ||
            !rcBuildRegions(this, *compact, configuration.borderSize, configuration.minRegionArea, configuration.mergeRegionArea))
        {
            throw std::runtime_error("failed to build watershed regions");
        }
    }
    else if (m_Settings.PartitionType == 1)
    {
        if (!rcBuildRegionsMonotone(this, *compact, configuration.borderSize, configuration.minRegionArea, configuration.mergeRegionArea))
            throw std::runtime_error("failed to build monotone regions");
    }
    else if (m_Settings.PartitionType == 2)
    {
        if (!rcBuildLayerRegions(this, *compact, configuration.borderSize, configuration.minRegionArea))
            throw std::runtime_error("failed to build layered regions");
    }
    else
    {
        throw std::runtime_error("partition type must be 0, 1, or 2");
    }

    std::unique_ptr<rcContourSet, decltype([](rcContourSet* contours) { rcFreeContourSet(contours); })> contours(rcAllocContourSet());
    if (!contours || !rcBuildContours(this, *compact, configuration.maxSimplificationError, configuration.maxEdgeLen, *contours))
    {
        throw std::runtime_error("failed to build tile contours");
    }
    if (contours->nconts == 0)
        return {};

    std::unique_ptr<rcPolyMesh, decltype([](rcPolyMesh* mesh) { rcFreePolyMesh(mesh); })> polygonMesh(rcAllocPolyMesh());
    if (!polygonMesh || !rcBuildPolyMesh(this, *contours, configuration.maxVertsPerPoly, *polygonMesh))
    {
        throw std::runtime_error("failed to build tile polygon mesh");
    }
    std::unique_ptr<rcPolyMeshDetail, decltype([](rcPolyMeshDetail* mesh) { rcFreePolyMeshDetail(mesh); })> detailMesh(rcAllocPolyMeshDetail());
    if (!detailMesh ||
        !rcBuildPolyMeshDetail(this, *polygonMesh, *compact, configuration.detailSampleDist, configuration.detailSampleMaxError, *detailMesh))
    {
        throw std::runtime_error("failed to build tile detail mesh");
    }

    if (polygonMesh->nverts >= 0xffff)
        throw std::runtime_error("tile contains too many vertices");
    for (int polygonIndex = 0; polygonIndex < polygonMesh->npolys; ++polygonIndex)
    {
        if (polygonMesh->areas[polygonIndex] == RC_WALKABLE_AREA)
            polygonMesh->areas[polygonIndex] = DT_POLYAREA_GROUND;
        if (polygonMesh->areas[polygonIndex] == DT_POLYAREA_GROUND)
            polygonMesh->flags[polygonIndex] |= DT_POLYFLAGS_WALK;
        if (polygonMesh->surfa[polygonIndex] <= PolygonSurfaceAreaTooSmall)
            polygonMesh->flags[polygonIndex] |= DT_POLYFLAGS_TOO_SMALL;

        const unsigned short* polygon = &polygonMesh->polys[polygonIndex * polygonMesh->nvp * 2];
        for (int edge = 0; edge < polygonMesh->nvp; ++edge)
        {
            if (polygon[edge] == RD_MESH_NULL_IDX)
                break;
            if ((polygon[polygonMesh->nvp + edge] & 0x8000) != 0 && (polygon[polygonMesh->nvp + edge] & 0xf) != 0xf)
            {
                polygonMesh->flags[polygonIndex] |= DT_POLYFLAGS_HAS_NEIGHBOUR;
            }
        }
    }

    dtNavMeshCreateParams parameters{};
    parameters.verts = polygonMesh->verts;
    parameters.vertCount = polygonMesh->nverts;
    parameters.polys = polygonMesh->polys;
    parameters.polyFlags = polygonMesh->flags;
    parameters.polyAreas = polygonMesh->areas;
    parameters.surfAreas = polygonMesh->surfa;
    parameters.polyCount = polygonMesh->npolys;
    parameters.nvp = polygonMesh->nvp;
    parameters.cellResolution = m_Hull.PolygonCellResolution;
    parameters.detailMeshes = detailMesh->meshes;
    parameters.detailVerts = detailMesh->verts;
    parameters.detailVertsCount = detailMesh->nverts;
    parameters.detailTris = detailMesh->tris;
    parameters.detailTriCount = detailMesh->ntris;
    parameters.offMeshConVerts = m_OffMesh.Vertices.empty() ? nullptr : m_OffMesh.Vertices.data();
    parameters.offMeshConRefPos = m_OffMesh.ReferencePositions.empty() ? nullptr : m_OffMesh.ReferencePositions.data();
    parameters.offMeshConRad = m_OffMesh.Radii.empty() ? nullptr : m_OffMesh.Radii.data();
    parameters.offMeshConRefYaw = m_OffMesh.ReferenceYaws.empty() ? nullptr : m_OffMesh.ReferenceYaws.data();
    parameters.offMeshConDir = m_OffMesh.Directions.empty() ? nullptr : m_OffMesh.Directions.data();
    parameters.offMeshConJumps = m_OffMesh.TraversalTypes.empty() ? nullptr : m_OffMesh.TraversalTypes.data();
    parameters.offMeshConOrders = m_OffMesh.LookupOrders.empty() ? nullptr : m_OffMesh.LookupOrders.data();
    parameters.offMeshConAreas = m_OffMesh.Areas.empty() ? nullptr : m_OffMesh.Areas.data();
    parameters.offMeshConFlags = m_OffMesh.Flags.empty() ? nullptr : m_OffMesh.Flags.data();
    parameters.offMeshConUserID = m_OffMesh.UserIds.empty() ? nullptr : m_OffMesh.UserIds.data();
    parameters.offMeshConCount = static_cast<int>(m_OffMesh.Radii.size());
    parameters.walkableHeight = m_Hull.Height;
    parameters.walkableRadius = m_Hull.Radius;
    parameters.walkableClimb = m_Hull.MaxClimb;
    parameters.tileX = tileX;
    parameters.tileY = tileY;
    parameters.tileLayer = 0;
    parameters.bmin = polygonMesh->bmin;
    parameters.bmax = polygonMesh->bmax;
    parameters.cs = configuration.cs;
    parameters.ch = configuration.ch;
    parameters.buildBvTree = true;

    TileData_t output;
    output.RestoredPassageSpanCount = restoredPassageSpanCount;
    unsigned char* bytes = nullptr;
    const bool created = dtCreateNavMeshData(&parameters, &bytes, &output.Size);
    output.Bytes.reset(bytes);
    if (!created)
        throw std::runtime_error("failed to create Detour tile data");
    return output;
}

void CNavMeshBuilder::ConnectOffMeshLinks(dtNavMesh& mesh)
{
    for (int tileIndex = 0; tileIndex < mesh.getMaxTiles(); ++tileIndex)
    {
        const dtMeshTile* tile = mesh.getTile(tileIndex);
        if (tile && tile->header && tile->header->offMeshConCount != 0)
        {
            const dtStatus status = mesh.connectOffMeshLinks(mesh.getTileRef(tile));
            if (dtStatusFailed(status))
                throw std::runtime_error("failed to connect an off-mesh link");
        }
    }
}

void CNavMeshBuilder::ConnectTraversalLinks(dtNavMesh& mesh)
{
    dtTraverseLinkConnectParams parameters{};
    parameters.getTraverseType = SelectTraversalType;
    parameters.traverseLinkInLOS = TraversalLinkInLineOfSight;
    parameters.findPolyLink = FindTraversalPair;
    parameters.addPolyLink = AddTraversalPair;
    parameters.userData = this;
    parameters.minEdgeOverlap = RD_EPS;
    parameters.maxPortalAlign = TraversePortalMaximumAlignment;
    parameters.singlePortalPerPair = false;

    for (int tileIndex = 0; tileIndex < mesh.getMaxTiles(); ++tileIndex)
    {
        const dtMeshTile* tile = mesh.getTile(tileIndex);
        if (!tile || !tile->header)
            continue;
        const dtTileRef reference = mesh.getTileRef(tile);
        parameters.linkToNeighbor = false;
        const dtStatus localStatus = mesh.connectTraverseLinks(reference, parameters);
        if (dtStatusFailed(localStatus) && (localStatus & DT_INVALID_ACTION) == 0)
            throw std::runtime_error("failed to create local traversal links");
        parameters.linkToNeighbor = true;
        const dtStatus neighbourStatus = mesh.connectTraverseLinks(reference, parameters);
        if (dtStatusFailed(neighbourStatus) && (neighbourStatus & DT_INVALID_ACTION) == 0)
            throw std::runtime_error("failed to create cross-tile traversal links");
    }
}

void CNavMeshBuilder::CreateStaticPathingData(dtNavMesh& mesh)
{
    std::array<dtDisjointSet, 4> sets;
    dtTraverseTableCreateParams parameters{};
    parameters.nav = &mesh;
    parameters.sets = sets.data();
    parameters.tableCount = m_Hull.TraversalTableCount;
    parameters.navMeshType = static_cast<int>(m_Hull.Type);
    parameters.canTraverse = TraversalTableSupportsLink;
    parameters.collapseGroups = false;
    if (!dtCreateDisjointPolyGroups(&parameters))
        throw std::runtime_error("too many disjoint polygon groups");
    if (!dtCreateTraverseTableData(&parameters))
        throw std::runtime_error("failed to create static traversal tables");
}

void CGeneratedNavMesh::CollectStatistics()
{
    m_Statistics = {};
    const dtNavMesh& mesh = *m_Mesh;
    m_Statistics.TileCount = mesh.getTileCount();
    m_Statistics.PolygonGroupCount = mesh.getParams()->polyGroupCount;
    for (int tileIndex = 0; tileIndex < mesh.getMaxTiles(); ++tileIndex)
    {
        const dtMeshTile* tile = mesh.getTile(tileIndex);
        if (!tile || !tile->header)
            continue;
        m_Statistics.PolygonCount += tile->header->polyCount;
        for (int polygonIndex = 0; polygonIndex < tile->header->polyCount; ++polygonIndex)
        {
            for (unsigned int linkIndex = tile->polys[polygonIndex].firstLink; linkIndex != DT_NULL_LINK; linkIndex = tile->links[linkIndex].next)
            {
                if (tile->links[linkIndex].traverseType != DT_NULL_TRAVERSE_TYPE)
                    ++m_Statistics.TraversalLinkCount;
            }
        }
    }
}

CNavMeshBuilder::CNavMeshBuilder(const CNavMeshGeometry& geometry, const HullSettings_t& hull, const BuildSettings_t& settings)
    : rcContext(true), m_Geometry(geometry), m_Hull(hull), m_Settings(settings)
{
}

void CNavMeshBuilder::doLog(rcLogCategory category, const char* message, rdSizeType length)
{
    if (category == RC_LOG_ERROR || category == RC_LOG_WARNING)
        std::cerr.write(message, static_cast<std::streamsize>(length)) << '\n';
}

CGeneratedNavMesh::CGeneratedNavMesh(dtNavMesh* mesh) : m_Mesh(mesh)
{
}

dtNavMesh* CGeneratedNavMesh::Get() const
{
    return m_Mesh.get();
}

const BuildStatistics_t& CGeneratedNavMesh::Statistics() const
{
    return m_Statistics;
}

CGeneratedNavMesh CNavMeshBuilder::Build()
{
    m_PolygonPairs.clear();
    if (m_Geometry.m_ChunkyMeshes.empty() || m_Geometry.m_Vertices.empty() || m_Geometry.m_Triangles.empty())
        throw std::runtime_error("input geometry is not initialized");
    if (m_Settings.TileSize <= 0 || m_Settings.VerticesPerPolygon < 3 || m_Settings.VerticesPerPolygon > RD_VERTS_PER_POLYGON)
    {
        throw std::runtime_error("invalid navmesh build settings");
    }
    if (!std::isfinite(m_Hull.CellSize) || m_Hull.CellSize <= 0.0f || !std::isfinite(m_Hull.CellHeight) || m_Hull.CellHeight <= 0.0f ||
        !std::isfinite(m_Hull.Radius) || m_Hull.Radius <= 0.0f || !std::isfinite(m_Hull.Height) || m_Hull.Height <= 0.0f ||
        !std::isfinite(m_Hull.MaxClimb) || m_Hull.MaxClimb < 0.0f || m_Hull.PolygonCellResolution <= 0)
    {
        throw std::runtime_error("invalid navmesh hull settings");
    }
    const double radiusCells = std::ceil(static_cast<double>(m_Hull.Radius) / m_Hull.CellSize);
    const double heightCells = std::ceil(static_cast<double>(m_Hull.Height) / m_Hull.CellHeight);
    const double climbCells = std::floor(static_cast<double>(m_Hull.MaxClimb) / m_Hull.CellHeight);
    if (radiusCells < 1.0 || radiusCells > 127.0 || heightCells < 3.0 || heightCells > RC_SPAN_MAX_HEIGHT || climbCells < 0.0 ||
        climbCells > RC_SPAN_MAX_HEIGHT)
    {
        throw std::runtime_error("navmesh hull settings exceed Recast voxel limits");
    }

    int gridWidth = 0;
    int gridHeight = 0;
    rcCalcGridSize(&m_Geometry.m_BoundsMinimum, &m_Geometry.m_BoundsMaximum, m_Hull.CellSize, &gridWidth, &gridHeight);
    const int tileWidth = (gridWidth + m_Settings.TileSize - 1) / m_Settings.TileSize;
    const int tileHeight = (gridHeight + m_Settings.TileSize - 1) / m_Settings.TileSize;
    if (tileWidth <= 0 || tileHeight <= 0)
        throw std::runtime_error("input geometry has empty navmesh bounds");
    const std::uint64_t tileCount = static_cast<std::uint64_t>(tileWidth) * tileHeight;
    if (tileCount > (std::uint64_t{1} << 14))
        throw std::runtime_error("input requires more than Titanfall 2's 16384 tile limit");

    const int tileBits = rdIlog2(rdNextPow2(static_cast<std::uint32_t>(tileCount)));
    const int polygonBits = 22 - tileBits;
    if (polygonBits <= 0)
        throw std::runtime_error("input leaves no reference bits for polygons");

    dtNavMeshParams parameters{};
    parameters.orig.x = m_Geometry.m_BoundsMaximum.x;
    parameters.orig.y = m_Geometry.m_BoundsMinimum.y;
    parameters.orig.z = m_Geometry.m_BoundsMinimum.z;
    parameters.tileWidth = m_Settings.TileSize * m_Hull.CellSize;
    parameters.tileHeight = m_Settings.TileSize * m_Hull.CellSize;
    parameters.maxTiles = 1 << tileBits;
    parameters.maxPolys = 1 << polygonBits;
    parameters.polyGroupCount = 0;
    parameters.traverseTableSize = 0;
    parameters.traverseTableCount = 0;

    dtNavMesh* rawMesh = dtAllocNavMesh();
    if (!rawMesh)
        throw std::bad_alloc();
    CGeneratedNavMesh generated(rawMesh);
    if (dtStatusFailed(rawMesh->init(&parameters)))
        throw std::runtime_error("failed to initialize Detour navmesh");

    Configure();
    SelectOffMeshConnections();
    int restoredPassageSpanCount = 0;
    for (int tileY = 0; tileY < tileHeight; ++tileY)
    {
        for (int tileX = 0; tileX < tileWidth; ++tileX)
        {
            const float tileWorldSize = m_Settings.TileSize * m_Hull.CellSize;
            rdVec3D tileMinimum;
            tileMinimum.x = m_Geometry.m_BoundsMaximum.x - (tileX + 1) * tileWorldSize;
            tileMinimum.y = m_Geometry.m_BoundsMinimum.y + tileY * tileWorldSize;
            tileMinimum.z = m_Geometry.m_BoundsMinimum.z;
            rdVec3D tileMaximum;
            tileMaximum.x = m_Geometry.m_BoundsMaximum.x - tileX * tileWorldSize;
            tileMaximum.y = m_Geometry.m_BoundsMinimum.y + (tileY + 1) * tileWorldSize;
            tileMaximum.z = m_Geometry.m_BoundsMaximum.z;

            TileData_t tile = BuildTile(tileX, tileY, tileMinimum, tileMaximum);
            if (!tile.Bytes)
                continue;
            restoredPassageSpanCount += tile.RestoredPassageSpanCount;
            dtTileRef tileReference = 0;
            const dtStatus addStatus = rawMesh->addTile(tile.Bytes.get(), tile.Size, DT_TILE_FREE_DATA, 0, &tileReference);
            if (dtStatusFailed(addStatus))
                throw std::runtime_error("failed to add generated tile to navmesh");
            tile.Bytes.release();
            if (dtStatusFailed(rawMesh->connectTile(tileReference)))
                throw std::runtime_error("failed to connect generated tile");
        }
    }
    if (rawMesh->getTileCount() == 0)
        throw std::runtime_error("generation produced no walkable tiles");

    ConnectOffMeshLinks(*rawMesh);
    if (m_Settings.BuildTraversalLinks)
        ConnectTraversalLinks(*rawMesh);
    CreateStaticPathingData(*rawMesh);
    generated.CollectStatistics();
    generated.m_Statistics.RestoredPassageSpanCount = restoredPassageSpanCount;
    return generated;
}

const std::array<HullSettings_t, 5>& CNavMeshBuilder::GetHullSettings()
{
    static const std::array<HullSettings_t, 5> settings = {{
        {HullType_t::Small, "small", 16.0f, 72.0f, 18.0f, 8.0f, 4.0f, 16, {0x0000013f, 0x000bff7e, 0x001bdf7f, 0x001bffff}, 4},
        {HullType_t::MediumShort, "med_short", 40.0f, 72.0f, 18.0f, 8.0f, 4.0f, 8, {0x00033fb7, 0, 0, 0}, 1},
        {HullType_t::Medium, "medium", 48.0f, 150.0f, 32.0f, 8.0f, 4.0f, 8, {0x00033fb2, 0, 0, 0}, 1},
        {HullType_t::Large, "large", 60.0f, 235.0f, 80.0f, 15.0f, 7.5f, 4, {0x00000030, 0, 0, 0}, 1},
        {HullType_t::ExtraLarge, "extra_large", 80.0f, 235.0f, 80.0f, 15.0f, 7.5f, 4, {0x00000030, 0, 0, 0}, 1},
    }};
    return settings;
}

const HullSettings_t& CNavMeshBuilder::GetHullSettings(HullType_t type)
{
    for (const HullSettings_t& settings : GetHullSettings())
    {
        if (settings.Type == type)
            return settings;
    }
    throw std::logic_error("unknown hull type");
}

std::optional<HullType_t> CNavMeshBuilder::ParseHullType(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    if (value == "small")
        return HullType_t::Small;
    if (value == "med_short" || value == "medium_short")
        return HullType_t::MediumShort;
    if (value == "medium")
        return HullType_t::Medium;
    if (value == "large")
        return HullType_t::Large;
    if (value == "extra_large" || value == "xlarge" || value == "goliath")
        return HullType_t::ExtraLarge;
    return std::nullopt;
}
