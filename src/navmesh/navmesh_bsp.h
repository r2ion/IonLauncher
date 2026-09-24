#pragma once

#include "navmesh_geometry.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class dtNavMesh;
struct HullSettings_t;
enum class HullType_t : std::uint8_t;
using ReadFileFn = std::string (*)(const char* Path);

struct MapEntity_t
{
    std::unordered_map<std::string, std::string> Values;

    const std::string& GetValue(const char* Key) const;
    std::optional<rdVec3D> GetVector(const char* Key, const char* Default = "") const;
};

struct RenderTriangle_t
{
    std::array<rdVec3D, 3> Vertices;
    int Material = 0;
    int Mesh = 0;

    float GetNormalZ() const;
    static std::optional<float> ProjectedHeight(const rdVec3D& Point, const rdVec3D& A, const rdVec3D& B, const rdVec3D& C);
};

struct MapGeometryStatistics_t
{
    int BrushCount = 0;
    int TriangleCollisionCount = 0;
    int StaticPropCount = 0;
    int StaticPropModelCount = 0;
    int NonCollidingPropCount = 0;
};

class CNavMeshMap
{
  public:
    // Reads collision and authored map data through mounted GAME search paths.
    CNavMeshMap(const char* MapName, bool TitanCollision, ReadFileFn ReadFile);

    // Retains existing caller-authored links and exclusions.
    void AddAuthoredTraversal(const HullSettings_t& Hull);
    // Baseline must be the unrepaired small-hull mesh; the result is additive.
    CNavMeshGeometry CreateRenderFloorRepair(const dtNavMesh& Baseline) const;

    CNavMeshGeometry m_Geometry;
    std::vector<MapEntity_t> m_Entities;
    std::vector<RenderTriangle_t> m_RenderTriangles;
    MapGeometryStatistics_t m_Statistics;

  private:
    static std::pair<float, float> TraversalOffsets(HullType_t Hull, int Type);
    std::optional<float> LandingHeight(const rdVec3D& Point) const;
};
