#pragma once

#include "Shared/Include/SharedCommon.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct rcChunkyTriMesh;

enum class HullType_t : std::uint8_t
{
    Small,
    MediumShort,
    Medium,
    Large,
    ExtraLarge,
};

struct HullSettings_t
{
    HullType_t Type;
    const char* Suffix;
    float Radius;
    float Height;
    float MaxClimb;
    float CellSize;
    float CellHeight;
    int PolygonCellResolution;
    std::array<std::uint32_t, 4> TraversalMasks;
    int TraversalTableCount;
};

struct BuildSettings_t
{
    float MaxSlope = 45.573f;
    int RegionMinSize = 4;
    int RegionMergeSize = 8;
    int EdgeMaxLength = 0;
    float EdgeMaxError = 1.3f;
    int VerticesPerPolygon = RD_VERTS_PER_POLYGON;
    float DetailSampleDistance = 6.0f;
    float DetailSampleMaxError = 3.0f;
    int TileSize = 64;
    int PartitionType = 0;
    bool IgnoreWinding = false;
    bool BuildTraversalLinks = true;
};

struct OffMeshConnection_t
{
    rdVec3D Start;
    rdVec3D End;
    rdVec3D ReferencePosition;
    float Radius = 0.0f;
    float ReferenceYaw = 0.0f;
    std::uint16_t UserId = 0;
    std::uint16_t Flags = 0;
    std::uint8_t TraversalType = 0;
    std::uint8_t LookupOrder = 0;
    std::uint8_t Area = 0;
    bool Bidirectional = false;
};

enum class AreaVolumeType_t : std::uint8_t
{
    Box,
    Cylinder,
    Convex,
};

struct AreaVolume_t
{
    AreaVolumeType_t Type;
    std::vector<rdVec3D> Vertices;
    float MinimumHeight = 0.0f;
    float MaximumHeight = 0.0f;
    float Radius = 0.0f;
    float Height = 0.0f;
    std::uint16_t Flags = 0;
    std::uint8_t Area = 0;
};

// Convex clip brushes own contiguous vertices and triangle indices. Keep their
// closed volumes distinct from hard surface exclusions and physical support.
struct ClipBrush_t
{
    int FirstVertex;
    int EndVertex;
    std::size_t FirstIndex;
    std::size_t IndexCount;
    rdVec3D BoundsMinimum;
    rdVec3D BoundsMaximum;
};

class CNavMeshGeometry
{
  public:
    CNavMeshGeometry();
    ~CNavMeshGeometry();
    CNavMeshGeometry(CNavMeshGeometry&& other) noexcept;
    CNavMeshGeometry& operator=(CNavMeshGeometry&& other) noexcept;
    CNavMeshGeometry(const CNavMeshGeometry&) = delete;
    CNavMeshGeometry& operator=(const CNavMeshGeometry&) = delete;

    std::vector<rdVec3D> m_Vertices;
    std::vector<int> m_Triangles;
    std::vector<std::array<int, 3>> m_UnwalkableTriangles;
    std::vector<ClipBrush_t> m_ClipBrushes;
    std::vector<OffMeshConnection_t> m_OffMeshConnections;
    std::vector<AreaVolume_t> m_AreaVolumes;
    rdVec3D m_BoundsMinimum;
    rdVec3D m_BoundsMaximum;
    std::vector<std::unique_ptr<rcChunkyTriMesh>> m_ChunkyMeshes;
    mutable std::vector<int> m_ChunkScratch;

    void Finalize();
    void Append(CNavMeshGeometry&& Supplemental);
    bool IsTriangleUnwalkable(int first, int second, int third) const;
    const ClipBrush_t* FindClipBrush(int vertex) const;
    bool Raycast(const rdVec3D& start, const rdVec3D& end) const;

  private:
    void CalculateBounds();
    void BuildChunkyMesh();
};
