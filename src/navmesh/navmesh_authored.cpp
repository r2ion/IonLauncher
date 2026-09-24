#include "navmesh_authored.h"
#include "navmesh_builder.h"

#include "Detour/Include/DetourNavMesh.h"
#include "NavEditor/Include/ChunkyTriMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

const float CNavMeshFloorRepair::MinimumNormalZ = static_cast<float>(std::cos(45.573 * 3.14159265358979323846 / 180.0));

//-----------------------------------------------------------------------------
// Purpose: Read an entity key without inserting a value into the parsed map.
//-----------------------------------------------------------------------------
const std::string& MapEntity_t::GetValue(const char* Key) const
{
    static const std::string Empty;
    const auto Found = Values.find(Key);
    return Found == Values.end() ? Empty : Found->second;
}

//-----------------------------------------------------------------------------
// Purpose: Parse finite Source XYZ vectors; malformed cover nodes are not evidence.
//-----------------------------------------------------------------------------
std::optional<rdVec3D> MapEntity_t::GetVector(const char* Key, const char* Default) const
{
    const std::string& Text = GetValue(Key);
    std::istringstream Stream(Text.empty() ? Default : Text);
    rdVec3D Value;
    std::string Extra;
    if (!(Stream >> Value.x >> Value.y >> Value.z) || (Stream >> Extra) || !std::isfinite(Value.x) || !std::isfinite(Value.y) ||
        !std::isfinite(Value.z))
        return std::nullopt;
    return Value;
}

//-----------------------------------------------------------------------------
// Purpose: Project a point vertically onto a triangle, accepting its boundary.
//-----------------------------------------------------------------------------
std::optional<float> RenderTriangle_t::ProjectedHeight(const rdVec3D& Point, const rdVec3D& A, const rdVec3D& B, const rdVec3D& C)
{
    const double Denominator = (B.y - C.y) * double(A.x - C.x) + (C.x - B.x) * double(A.y - C.y);
    if (std::abs(Denominator) < 1e-8)
        return std::nullopt;
    const double First = ((B.y - C.y) * double(Point.x - C.x) + (C.x - B.x) * double(Point.y - C.y)) / Denominator;
    const double Second = ((C.y - A.y) * double(Point.x - C.x) + (A.x - C.x) * double(Point.y - C.y)) / Denominator;
    const double Third = 1.0 - First - Second;
    if (std::min({First, Second, Third}) < -1e-5)
        return std::nullopt;
    return static_cast<float>(First * A.z + Second * B.z + Third * C.z);
}

//-----------------------------------------------------------------------------
// Purpose: Recover the calibrated longitudinal offsets from retail authored links.
//-----------------------------------------------------------------------------
std::pair<float, float> CNavMeshMap::TraversalOffsets(HullType_t Hull, int Type)
{
    switch (Hull)
    {
    case HullType_t::Small:
        switch (Type)
        {
        case 1:
            return {-25.634f, 70.213f};
        case 3:
            return {-50.063f, 210.580f};
        case 4:
        case 8:
            return {-24.075f, 70.213f};
        case 5:
            return {-25.399f, 94.167f};
        case 10:
            return {-25.411f, 70.198f};
        default:
            return {-25.399f, 69.776f};
        }
    case HullType_t::MediumShort:
        switch (Type)
        {
        case 1:
            return {14.238f, 168.150f};
        case 3:
            return {8.556f, 199.441f};
        case 4:
            return {-12.907f, 157.750f};
        case 5:
            return {-12.914f, 157.739f};
        default:
            return {-12.914f, 122.605f};
        }
    case HullType_t::Medium:
        return Type == 4 || Type == 5 ? std::pair{-0.031f, 263.756f} : std::pair{-0.031f, 172.687f};
    case HullType_t::Large:
        return {-70.733f, 283.520f};
    case HullType_t::ExtraLarge:
        throw std::runtime_error("authored traverse endpoint calibration is unavailable for the extra-large hull");
    }
    throw std::runtime_error("invalid authored traversal hull");
}

//-----------------------------------------------------------------------------
// Purpose: Find the highest non-excluded collision surface beneath a landing.
//-----------------------------------------------------------------------------
std::optional<float> CNavMeshMap::LandingHeight(const rdVec3D& Point) const
{
    float Highest = -std::numeric_limits<float>::infinity();
    const auto Visit = [&](int A, int B, int C)
    {
        if (m_Geometry.IsTriangleUnwalkable(A, B, C) || m_Geometry.FindClipBrush(A))
            return;
        const auto Height = RenderTriangle_t::ProjectedHeight(Point, m_Geometry.m_Vertices[A], m_Geometry.m_Vertices[B], m_Geometry.m_Vertices[C]);
        if (Height && *Height <= Point.z)
            Highest = std::max(Highest, *Height);
    };
    for (const auto& Mesh : m_Geometry.m_ChunkyMeshes)
    {
        for (int Index = 0; Index < Mesh->nnodes;)
        {
            const rcChunkyTriMeshNode& Node = Mesh->nodes[Index];
            const bool Overlaps = Point.x >= Node.bmin.x && Point.x <= Node.bmax.x && Point.y >= Node.bmin.y && Point.y <= Node.bmax.y;
            if (Node.i >= 0 && Overlaps)
                for (int Triangle = Node.i; Triangle < Node.i + Node.n; ++Triangle)
                    Visit(Mesh->tris[Triangle * 3], Mesh->tris[Triangle * 3 + 1], Mesh->tris[Triangle * 3 + 2]);
            Index += !Overlaps && Node.i < 0 ? -Node.i : 1;
        }
    }
    if (!std::isfinite(Highest))
        return std::nullopt;
    return Highest;
}

//-----------------------------------------------------------------------------
// Purpose: Add calibrated hull-filtered authored traversal; retain caller data.
//-----------------------------------------------------------------------------
void CNavMeshMap::AddAuthoredTraversal(const HullSettings_t& Hull)
{
    unsigned int NextUserId = 1000;
    for (const OffMeshConnection_t& Existing : m_Geometry.m_OffMeshConnections)
        NextUserId = std::max(NextUserId, static_cast<unsigned int>(Existing.UserId) + 1);
    for (const MapEntity_t& Entity : m_Entities)
    {
        if (Entity.GetValue("classname") != "traverse")
            continue;
        const std::string& TypeText = Entity.GetValue("traverseType");
        std::size_t Consumed = 0;
        const int Type = TypeText.empty() ? 0 : std::stoi(TypeText, &Consumed);
        if ((!TypeText.empty() && Consumed != TypeText.size()) || Type < 0 || Type >= 32)
            throw std::runtime_error("invalid BSP traverseType");
        if (Type == 0 || !(Hull.TraversalMasks[0] & (std::uint32_t{1} << Type)))
            continue;
        const auto Origin = Entity.GetVector("origin");
        const auto Angles = Entity.GetVector("angles", "0 0 0");
        if (!Origin || !Angles)
            throw std::runtime_error("invalid BSP traverse origin or angles");
        const auto [StartOffset, EndOffset] = TraversalOffsets(Hull.Type, Type);
        const float Radians = rdDegToRad(Angles->y);
        const float ForwardX = std::cos(Radians);
        const float ForwardY = std::sin(Radians);
        OffMeshConnection_t Connection;
        Connection.Start = rdVec3D(Origin->x + ForwardX * StartOffset, Origin->y + ForwardY * StartOffset, Origin->z);
        Connection.End = rdVec3D(Origin->x + ForwardX * EndOffset, Origin->y + ForwardY * EndOffset, Origin->z + 1.0f);
        const auto Height = LandingHeight(Connection.End);
        // Authored markers can land outside collision; reject that link, not the map.
        if (!Height)
            continue;
        Connection.End.z = *Height;
        Connection.ReferencePosition = *Origin;
        Connection.ReferenceYaw = Angles->y;
        Connection.Radius = Hull.Radius;
        Connection.TraversalType = static_cast<std::uint8_t>(Type);
        Connection.LookupOrder = 1;
        Connection.Area = 1;
        Connection.Flags = 1;
        Connection.Bidirectional = Type != 4 && Type != 5;
        if (NextUserId >= 0xffff)
            throw std::runtime_error("too many authored off-mesh connections");
        Connection.UserId = static_cast<std::uint16_t>(NextUserId++);
        m_Geometry.m_OffMeshConnections.push_back(Connection);
    }
}

//-----------------------------------------------------------------------------
// Purpose: Find the nearest point in XY on a segment without allocating vertices.
//-----------------------------------------------------------------------------
rdVec3D CNavMeshFloorRepair::ClosestOnSegment(const rdVec3D& Point, const rdVec3D& A, const rdVec3D& B)
{
    const float X = B.x - A.x;
    const float Y = B.y - A.y;
    const float LengthSquared = X * X + Y * Y;
    const float Amount = LengthSquared == 0.0f ? 0.0f : std::clamp(((Point.x - A.x) * X + (Point.y - A.y) * Y) / LengthSquared, 0.0f, 1.0f);
    return rdVec3D(A.x + Amount * X, A.y + Amount * Y, 0.0f);
}

//-----------------------------------------------------------------------------
// Purpose: Match the old ground-polygon XY distance and planar height query.
//-----------------------------------------------------------------------------
float CNavMeshFloorRepair::NavigationDistance(const rdVec3D& Point) const
{
    float Best = std::numeric_limits<float>::infinity();
    for (int TileIndex = 0; TileIndex < m_Baseline.getMaxTiles(); ++TileIndex)
    {
        const dtMeshTile* Tile = m_Baseline.getTile(TileIndex);
        if (!Tile || !Tile->header || Tile->header->bmin.x > Point.x + MaximumNodeDistance || Tile->header->bmax.x < Point.x - MaximumNodeDistance ||
            Tile->header->bmin.y > Point.y + MaximumNodeDistance || Tile->header->bmax.y < Point.y - MaximumNodeDistance)
            continue;
        for (int Index = 0; Index < Tile->header->polyCount; ++Index)
        {
            const dtPoly& Polygon = Tile->polys[Index];
            if (Polygon.getType() != DT_POLYTYPE_GROUND || Polygon.vertCount < 3)
                continue;
            int Sign = 0;
            bool Inside = true;
            float Distance = std::numeric_limits<float>::infinity();
            rdVec3D Nearest = Point;
            for (int Edge = 0; Edge < Polygon.vertCount; ++Edge)
            {
                const rdVec3D& A = Tile->verts[Polygon.verts[Edge]];
                const rdVec3D& B = Tile->verts[Polygon.verts[(Edge + 1) % Polygon.vertCount]];
                const float Cross = (B.x - A.x) * (Point.y - A.y) - (B.y - A.y) * (Point.x - A.x);
                if (std::abs(Cross) >= 1e-4f)
                {
                    const int Current = Cross > 0.0f ? 1 : -1;
                    if (Sign && Sign != Current)
                        Inside = false;
                    Sign = Current;
                }
                const rdVec3D Closest = ClosestOnSegment(Point, A, B);
                const float EdgeDistance = std::hypot(Point.x - Closest.x, Point.y - Closest.y);
                if (EdgeDistance < Distance)
                {
                    Distance = EdgeDistance;
                    Nearest = Closest;
                }
            }
            if (Inside)
            {
                Distance = 0.0f;
                Nearest = Point;
            }
            if (Distance > MaximumNodeDistance || Distance >= Best)
                continue;
            const rdVec3D& A = Tile->verts[Polygon.verts[0]];
            const rdVec3D& B = Tile->verts[Polygon.verts[1]];
            const rdVec3D& C = Tile->verts[Polygon.verts[2]];
            rdVec3D AB, AC, Normal;
            rdVsub(&AB, &B, &A);
            rdVsub(&AC, &C, &A);
            rdVcross(&Normal, &AB, &AC);
            float Height = 0.0f;
            if (std::abs(Normal.z) < 1e-5f)
            {
                for (int Vertex = 0; Vertex < Polygon.vertCount; ++Vertex)
                    Height += Tile->verts[Polygon.verts[Vertex]].z;
                Height /= Polygon.vertCount;
            }
            else
                Height = A.z - (Normal.x * (Nearest.x - A.x) + Normal.y * (Nearest.y - A.y)) / Normal.z;
            const float Vertical = Point.z - Height;
            if (Vertical >= -FloorAbove && Vertical <= FloorBelow)
                Best = Distance;
        }
    }
    return Best;
}

//-----------------------------------------------------------------------------
// Purpose: Measure slope independently of render winding, as in the exporter.
//-----------------------------------------------------------------------------
float RenderTriangle_t::GetNormalZ() const
{
    const auto& V = Vertices;
    const double AX = V[1].x - V[0].x, AY = V[1].y - V[0].y, AZ = V[1].z - V[0].z;
    const double BX = V[2].x - V[0].x, BY = V[2].y - V[0].y, BZ = V[2].z - V[0].z;
    const double X = AY * BZ - AZ * BY, Y = AZ * BX - AX * BZ, Z = AX * BY - AY * BX;
    const double Magnitude = std::sqrt(X * X + Y * Y + Z * Z);
    return Magnitude == 0.0 ? 0.0f : static_cast<float>(std::abs(Z) / Magnitude);
}

//-----------------------------------------------------------------------------
// Purpose: Reject local triangles whose nearest XY point exceeds the gap radius.
//-----------------------------------------------------------------------------
bool CNavMeshFloorRepair::WithinRepairRadius(const RenderTriangle_t& Triangle, const FloorCandidate_t& Candidate)
{
    const auto& V = Triangle.Vertices;
    if (RenderTriangle_t::ProjectedHeight(Candidate.Origin, V[0], V[1], V[2]))
        return true;
    for (int Edge = 0; Edge < 3; ++Edge)
    {
        const rdVec3D Closest = ClosestOnSegment(Candidate.Origin, V[Edge], V[(Edge + 1) % 3]);
        if (std::hypot(Closest.x - Candidate.Origin.x, Closest.y - Candidate.Origin.y) <= Candidate.Radius)
            return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// Purpose: Weld render edges at the exporter's millimetric coordinate precision.
//-----------------------------------------------------------------------------
CNavMeshFloorRepair::VertexKey_t CNavMeshFloorRepair::VertexKey(const rdVec3D& Vertex)
{
    return {std::llround(double(Vertex.x) * 1000.0), std::llround(double(Vertex.y) * 1000.0), std::llround(double(Vertex.z) * 1000.0)};
}

//-----------------------------------------------------------------------------
// Purpose: Traverse only same-mesh local connected floor triangles near one node.
//-----------------------------------------------------------------------------
void CNavMeshFloorRepair::SelectLocalFloor(const FloorCandidate_t& Candidate)
{
    const int Mesh = m_Map.m_RenderTriangles[Candidate.Seed].Mesh;
    m_Local.clear();
    m_Pending.clear();
    m_Edges.clear();
    for (std::size_t Index = 0; Index < m_Map.m_RenderTriangles.size(); ++Index)
    {
        const RenderTriangle_t& Triangle = m_Map.m_RenderTriangles[Index];
        if (Triangle.Mesh != Mesh || Triangle.GetNormalZ() < MinimumNormalZ || !WithinRepairRadius(Triangle, Candidate))
            continue;
        const std::size_t LocalIndex = m_Local.size();
        m_Local.push_back(Index);
        const auto& V = Triangle.Vertices;
        for (int Edge = 0; Edge < 3; ++Edge)
        {
            EdgeKey_t Key{VertexKey(V[Edge]), VertexKey(V[(Edge + 1) % 3])};
            std::sort(Key.begin(), Key.end());
            m_Edges[Key].push_back(LocalIndex);
        }
        const auto Height = RenderTriangle_t::ProjectedHeight(Candidate.Origin, V[0], V[1], V[2]);
        if (Height && std::abs(*Height - Candidate.FloorHeight) <= 0.25f)
            m_Pending.push_back(LocalIndex);
    }
    m_Visited.assign(m_Local.size(), false);
    for (const std::size_t Seed : m_Pending)
        m_Visited[Seed] = true;
    while (!m_Pending.empty())
    {
        const std::size_t Index = m_Pending.back();
        m_Pending.pop_back();
        m_Selected.insert(m_Local[Index]);
        const auto& V = m_Map.m_RenderTriangles[m_Local[Index]].Vertices;
        for (int Edge = 0; Edge < 3; ++Edge)
        {
            EdgeKey_t Key{VertexKey(V[Edge]), VertexKey(V[(Edge + 1) % 3])};
            std::sort(Key.begin(), Key.end());
            for (const std::size_t Neighbour : m_Edges.at(Key))
            {
                if (m_Visited[Neighbour])
                    continue;
                m_Visited[Neighbour] = true;
                m_Pending.push_back(Neighbour);
            }
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Restore only local render floors corroborated by uncovered cover nodes.
//-----------------------------------------------------------------------------
CNavMeshGeometry CNavMeshFloorRepair::Build()
{
    CNavMeshGeometry Repair;
    m_Candidates.clear();
    m_Origins.clear();
    m_MaterialEvidence.clear();
    m_Selected.clear();
    m_Seen.clear();
    for (const MapEntity_t& Entity : m_Map.m_Entities)
    {
        if (!Entity.GetValue("classname").starts_with("info_node_cover_"))
            continue;
        const auto Origin = Entity.GetVector("origin");
        if (!Origin || !m_Origins.insert({Origin->x, Origin->y, Origin->z}).second)
            continue;
        const float Distance = NavigationDistance(*Origin);
        if (std::isfinite(Distance) && Distance > MinimumNodeDistance)
            m_Candidates.push_back({*Origin, Distance});
    }
    if (m_Candidates.size() < 2)
        return Repair;
    for (std::size_t Index = 0; Index < m_Map.m_RenderTriangles.size(); ++Index)
    {
        const RenderTriangle_t& Triangle = m_Map.m_RenderTriangles[Index];
        const float NormalZ = Triangle.GetNormalZ();
        if (NormalZ < MinimumNormalZ)
            continue;
        for (FloorCandidate_t& Candidate : m_Candidates)
        {
            const auto& V = Triangle.Vertices;
            const auto Height = RenderTriangle_t::ProjectedHeight(Candidate.Origin, V[0], V[1], V[2]);
            if (!Height)
                continue;
            const float Vertical = Candidate.Origin.z - *Height;
            if (Vertical < -FloorAbove || Vertical > FloorBelow)
                continue;
            if (Candidate.Seed == std::numeric_limits<std::size_t>::max() ||
                std::pair{std::abs(Vertical), -NormalZ} < std::pair{std::abs(Candidate.Origin.z - Candidate.FloorHeight), -Candidate.NormalZ})
            {
                Candidate.Seed = Index;
                Candidate.FloorHeight = *Height;
                Candidate.NormalZ = NormalZ;
            }
        }
    }
    for (const FloorCandidate_t& Candidate : m_Candidates)
        if (Candidate.Seed != std::numeric_limits<std::size_t>::max())
            ++m_MaterialEvidence[m_Map.m_RenderTriangles[Candidate.Seed].Material];
    for (FloorCandidate_t& Candidate : m_Candidates)
    {
        if (Candidate.Seed == std::numeric_limits<std::size_t>::max() || m_MaterialEvidence[m_Map.m_RenderTriangles[Candidate.Seed].Material] < 2)
            continue;
        Candidate.Radius = std::max(64.0f, Candidate.NavigationDistance + 48.0f);
        SelectLocalFloor(Candidate);
    }
    for (const std::size_t Index : m_Selected)
    {
        const auto& V = m_Map.m_RenderTriangles[Index].Vertices;
        TriangleKey_t Key{VertexKey(V[0]), VertexKey(V[1]), VertexKey(V[2])};
        std::sort(Key.begin(), Key.end());
        if (!m_Seen.insert(Key).second)
            continue;
        if (Repair.m_Vertices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) - 3)
            throw std::runtime_error("render-floor repair exceeds geometry index capacity");
        const int First = static_cast<int>(Repair.m_Vertices.size());
        Repair.m_Vertices.insert(Repair.m_Vertices.end(), V.begin(), V.end());
        Repair.m_Triangles.insert(Repair.m_Triangles.end(), {First, First + 1, First + 2});
    }
    if (!Repair.m_Triangles.empty())
        Repair.Finalize();
    return Repair;
}

CNavMeshGeometry CNavMeshMap::CreateRenderFloorRepair(const dtNavMesh& Baseline) const
{
    return CNavMeshFloorRepair(*this, Baseline).Build();
}
