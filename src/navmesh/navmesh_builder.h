#pragma once

#include "navmesh_format.h"
#include "navmesh_geometry.h"

#include "Detour/Include/DetourNavMesh.h"
#include "Recast/Include/Recast.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>

struct dtTraverseTableCreateParams;

struct BuildStatistics_t
{
    int TileCount = 0;
    int PolygonCount = 0;
    int TraversalLinkCount = 0;
    int PolygonGroupCount = 0;
    int RestoredPassageSpanCount = 0;
};

class CGeneratedNavMesh
{
  public:
    CGeneratedNavMesh() = default;
    explicit CGeneratedNavMesh(dtNavMesh* mesh);
    ~CGeneratedNavMesh() = default;
    CGeneratedNavMesh(CGeneratedNavMesh&& other) noexcept = default;
    CGeneratedNavMesh& operator=(CGeneratedNavMesh&& other) noexcept = default;
    CGeneratedNavMesh(const CGeneratedNavMesh&) = delete;
    CGeneratedNavMesh& operator=(const CGeneratedNavMesh&) = delete;

    dtNavMesh* Get() const;
    const BuildStatistics_t& Statistics() const;
    NavMeshSummary_t Save(const std::filesystem::path& path) const;

  private:
    friend class CNavMeshBuilder;
    void CollectStatistics();

    std::unique_ptr<dtNavMesh, decltype([](dtNavMesh* mesh) { dtFreeNavMesh(mesh); })> m_Mesh;
    BuildStatistics_t m_Statistics;
};

class CNavMeshBuilder final : public rcContext
{
  public:
    // Geometry is borrowed and must remain alive for the builder's lifetime.
    CNavMeshBuilder(const CNavMeshGeometry& geometry, const HullSettings_t& hull, const BuildSettings_t& settings);
    CNavMeshBuilder(const CNavMeshBuilder&) = delete;
    CNavMeshBuilder& operator=(const CNavMeshBuilder&) = delete;

    CGeneratedNavMesh Build();
    static const std::array<HullSettings_t, 5>& GetHullSettings();
    static const HullSettings_t& GetHullSettings(HullType_t type);
    static std::optional<HullType_t> ParseHullType(std::string value);

  protected:
    void doLog(rcLogCategory category, const char* message, rdSizeType length) override;

  private:
    struct TileData_t;
    struct TraversalTypeSettings_t;
    class CPassageRepair;

    struct OffMeshArrays_t
    {
        std::vector<rdVec3D> Vertices;
        std::vector<rdVec3D> ReferencePositions;
        std::vector<float> Radii;
        std::vector<float> ReferenceYaws;
        std::vector<unsigned char> Directions;
        std::vector<unsigned char> TraversalTypes;
        std::vector<unsigned char> LookupOrders;
        std::vector<unsigned char> Areas;
        std::vector<unsigned short> Flags;
        std::vector<unsigned short> UserIds;
    };

    struct FloorBridgeProposal_t
    {
        int X;
        int Y;
        unsigned short Height;
    };

    // Recast normally represents both slope-rejected and explicitly authored
    // unwalkable triangles as RC_NULL_AREA.  Its low-hanging-obstacle filter may
    // promote null spans back to walkable, which is useful for ordinary geometry
    // but violates explicitly unwalkable collision triangles. Keep the cases distinct
    // until that filter has run.  ExplicitUnwalkableRasterArea has the higher
    // merge priority so authored exclusions win when coplanar triangles overlap.
    static constexpr unsigned char GeneratedWalkableRasterArea = 1;
    static constexpr unsigned char ClipOnlyRasterArea = 2;
    static constexpr unsigned char ExplicitUnwalkableRasterArea = RC_WALKABLE_AREA;
    static constexpr unsigned short PolygonSurfaceAreaTooSmall = 120;
    static constexpr float TraverseRayExtraOffset = 4.0f;
    static constexpr float TraversePortalMaximumAlignment = 0.5f;
    static constexpr float TripleClearanceRayThreshold = 100.0f;

    static const std::array<TraversalTypeSettings_t, DT_MAX_TRAVERSE_TYPES>& TraversalTypes();
    bool SupportsTraversal(unsigned char traversalType) const;
    static unsigned char SelectTraversalType(void* userData, float distance, float elevation, float slope, bool baseOverlaps, bool landOverlaps);
    bool EdgesFaceEachOther(const rdVec2D& firstPosition, const rdVec2D& secondPosition, const rdVec2D& firstNormal,
                            const rdVec2D& secondNormal) const;
    bool HasVerticalClearance(const rdVec3D& position, float height) const;
    bool LinkHasClearance(const rdVec3D& lower, const rdVec3D& higher, float walkableHeight) const;
    static bool TraversalLinkInLineOfSight(void* userData, const rdVec3D* lowerPosition, const rdVec3D* higherPosition, const rdVec2D* lowerNormal,
                                           const rdVec2D* higherNormal, float walkableHeight, float walkableRadius, float slopeAngle);
    static unsigned int* FindTraversalPair(void* userData, dtPolyRef basePolygon, dtPolyRef landingPolygon);
    static int AddTraversalPair(void* userData, dtPolyRef basePolygon, dtPolyRef landingPolygon, unsigned int traversalTypeBit);
    static bool TraversalTableSupportsLink(const dtTraverseTableCreateParams* parameters, const dtLink* link, int tableIndex);
    void FilterLowHangingWalkableObstacles(int walkableClimb, rcHeightfield& heightfield);
    void RasterizeClipVolumes(const std::vector<const ClipBrush_t*>& brushes, rcHeightfield& solid);
    void ResolveClipSupport(const rcHeightfield& physicalSupport, rcHeightfield& solid);
    const rcSpan* FindWalkableSpanNearHeight(const rcHeightfield& heightfield, int x, int y, int height, int walkableClimb);
    bool HasRoomForVirtualFloor(const rcHeightfield& heightfield, int x, int y, int height, int walkableHeight);
    void CollectFloorBridgesAlongAxis(const rcHeightfield& heightfield, int walkableHeight, int walkableClimb, int maximumMissingCells,
                                      int directionX, int directionY, std::vector<FloorBridgeProposal_t>& proposals);
    void FillNarrowFloorGaps(int walkableHeight, int walkableClimb, rcHeightfield& heightfield);
    void SelectOffMeshConnections();
    void ApplyAreaVolumes(rcCompactHeightfield& compact);
    void Configure();
    TileData_t BuildTile(int tileX, int tileY, const rdVec3D& tileMinimum, const rdVec3D& tileMaximum);
    void ConnectOffMeshLinks(dtNavMesh& mesh);
    void ConnectTraversalLinks(dtNavMesh& mesh);
    void CreateStaticPathingData(dtNavMesh& mesh);

    const CNavMeshGeometry& m_Geometry;
    const HullSettings_t m_Hull;
    const BuildSettings_t m_Settings;
    rcConfig m_Configuration{};
    OffMeshArrays_t m_OffMesh;
    std::vector<unsigned char> m_TriangleAreas;
    std::array<int, 1024> m_ChunkIds{};
    std::vector<FloorBridgeProposal_t> m_FloorBridgeProposals;
    std::map<std::pair<dtPolyRef, dtPolyRef>, std::uint32_t> m_PolygonPairs;
};
