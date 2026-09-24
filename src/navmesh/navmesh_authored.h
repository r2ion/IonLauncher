#pragma once

#include "navmesh_bsp.h"

#include <limits>
#include <map>
#include <set>

// Scratch and connectivity belong to one repair selection, not the loaded map.
class CNavMeshFloorRepair
{
  public:
    CNavMeshFloorRepair(const CNavMeshMap& Map, const dtNavMesh& Baseline) : m_Map(Map), m_Baseline(Baseline)
    {
    }
    CNavMeshGeometry Build();

  private:
    static constexpr float FloorAbove = 24.0f;
    static constexpr float FloorBelow = 128.0f;
    static constexpr float MinimumNodeDistance = 32.0f;
    static constexpr float MaximumNodeDistance = 192.0f;
    static const float MinimumNormalZ;

    struct FloorCandidate_t
    {
        rdVec3D Origin;
        float NavigationDistance;
        std::size_t Seed = std::numeric_limits<std::size_t>::max();
        float FloorHeight = 0.0f;
        float NormalZ = 0.0f;
        float Radius = 0.0f;
    };

    using VertexKey_t = std::array<long long, 3>;
    using EdgeKey_t = std::array<VertexKey_t, 2>;
    using TriangleKey_t = std::array<VertexKey_t, 3>;

    static rdVec3D ClosestOnSegment(const rdVec3D& Point, const rdVec3D& A, const rdVec3D& B);
    static bool WithinRepairRadius(const RenderTriangle_t& Triangle, const FloorCandidate_t& Candidate);
    static VertexKey_t VertexKey(const rdVec3D& Vertex);
    float NavigationDistance(const rdVec3D& Point) const;
    void SelectLocalFloor(const FloorCandidate_t& Candidate);

    const CNavMeshMap& m_Map;
    const dtNavMesh& m_Baseline;
    std::vector<FloorCandidate_t> m_Candidates;
    std::set<std::array<float, 3>> m_Origins;
    std::map<int, int> m_MaterialEvidence;
    std::set<std::size_t> m_Selected;
    std::set<TriangleKey_t> m_Seen;
    std::vector<std::size_t> m_Local;
    std::vector<std::size_t> m_Pending;
    std::map<EdgeKey_t, std::vector<std::size_t>> m_Edges;
    std::vector<bool> m_Visited;
};
