#include "navmesh_geometry.h"

#include "NavEditor/Include/ChunkyTriMesh.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>

void CNavMeshGeometry::CalculateBounds()
{
    if (m_Vertices.empty())
        throw std::runtime_error("input geometry has no m_Vertices");

    m_BoundsMinimum = m_Vertices.front();
    m_BoundsMaximum = m_Vertices.front();
    for (const rdVec3D& vertex : m_Vertices)
    {
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z))
            throw std::runtime_error("input geometry contains a non-finite vertex");
        rdVmin(&m_BoundsMinimum, &vertex);
        rdVmax(&m_BoundsMaximum, &vertex);
    }
}

void CNavMeshGeometry::BuildChunkyMesh()
{
    if (m_Triangles.empty())
        throw std::runtime_error("input geometry has no m_Triangles");

    auto chunkyMesh = std::make_unique<rcChunkyTriMesh>();
    if (!rcCreateChunkyTriMesh(m_Vertices.data(), m_Triangles.data(), static_cast<int>(m_Triangles.size() / 3), 256, chunkyMesh.get()))
    {
        throw std::runtime_error("failed to build the input-geometry acceleration tree");
    }
    m_ChunkScratch.resize(std::max(m_ChunkScratch.size(), static_cast<std::size_t>(chunkyMesh->nnodes)));
    m_ChunkyMeshes.push_back(std::move(chunkyMesh));
}

CNavMeshGeometry::CNavMeshGeometry()
{
    m_BoundsMinimum.init(0.0f, 0.0f, 0.0f);
    m_BoundsMaximum.init(0.0f, 0.0f, 0.0f);
}

CNavMeshGeometry::~CNavMeshGeometry() = default;

CNavMeshGeometry::CNavMeshGeometry(CNavMeshGeometry&& other) noexcept = default;

CNavMeshGeometry& CNavMeshGeometry::operator=(CNavMeshGeometry&& other) noexcept
{
    if (this == &other)
        return *this;
    m_Vertices = std::move(other.m_Vertices);
    m_Triangles = std::move(other.m_Triangles);
    m_UnwalkableTriangles = std::move(other.m_UnwalkableTriangles);
    m_ClipBrushes = std::move(other.m_ClipBrushes);
    m_OffMeshConnections = std::move(other.m_OffMeshConnections);
    m_AreaVolumes = std::move(other.m_AreaVolumes);
    m_BoundsMinimum = other.m_BoundsMinimum;
    m_BoundsMaximum = other.m_BoundsMaximum;
    m_ChunkyMeshes = std::move(other.m_ChunkyMeshes);
    m_ChunkScratch = std::move(other.m_ChunkScratch);
    return *this;
}

//-----------------------------------------------------------------------------
// Purpose: Validate native input and rebuild bounds and raster acceleration.
//-----------------------------------------------------------------------------
void CNavMeshGeometry::Finalize()
{
    if (m_Vertices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        m_Triangles.size() / 3 > static_cast<std::size_t>(std::numeric_limits<int>::max() / 3))
        throw std::runtime_error("input geometry exceeds Recast index limits");
    if (m_Triangles.empty() || m_Triangles.size() % 3 != 0)
        throw std::runtime_error("input geometry requires complete m_Triangles");
    const auto validateIndex = [&](int index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= m_Vertices.size())
            throw std::runtime_error("input geometry contains an invalid triangle index");
    };
    for (int index : m_Triangles)
        validateIndex(index);
    for (std::array<int, 3>& triangle : m_UnwalkableTriangles)
    {
        for (int index : triangle)
            validateIndex(index);
        std::sort(triangle.begin(), triangle.end());
    }
    std::sort(m_UnwalkableTriangles.begin(), m_UnwalkableTriangles.end());
    m_UnwalkableTriangles.erase(std::unique(m_UnwalkableTriangles.begin(), m_UnwalkableTriangles.end()), m_UnwalkableTriangles.end());
    std::sort(m_ClipBrushes.begin(), m_ClipBrushes.end(),
              [](const ClipBrush_t& first, const ClipBrush_t& second) { return first.FirstVertex < second.FirstVertex; });
    int previousEnd = 0;
    for (ClipBrush_t& brush : m_ClipBrushes)
    {
        if (brush.FirstVertex < previousEnd || brush.EndVertex <= brush.FirstVertex ||
            static_cast<std::size_t>(brush.EndVertex) > m_Vertices.size() || brush.FirstIndex % 3 != 0 || brush.IndexCount == 0 ||
            brush.IndexCount % 3 != 0 || brush.FirstIndex > m_Triangles.size() || brush.IndexCount > m_Triangles.size() - brush.FirstIndex)
            throw std::runtime_error("input geometry contains an invalid clip brush");
        for (std::size_t index = brush.FirstIndex; index < brush.FirstIndex + brush.IndexCount; ++index)
            if (m_Triangles[index] < brush.FirstVertex || m_Triangles[index] >= brush.EndVertex)
                throw std::runtime_error("clip brush triangle references another primitive");
        brush.BoundsMinimum = brush.BoundsMaximum = m_Vertices[brush.FirstVertex];
        for (int vertex = brush.FirstVertex + 1; vertex < brush.EndVertex; ++vertex)
        {
            rdVmin(&brush.BoundsMinimum, &m_Vertices[vertex]);
            rdVmax(&brush.BoundsMaximum, &m_Vertices[vertex]);
        }
        previousEnd = brush.EndVertex;
    }
    CalculateBounds();
    m_ChunkyMeshes.clear();
    m_ChunkScratch.clear();
    BuildChunkyMesh();
}

//-----------------------------------------------------------------------------
// Purpose: Transfer supplemental geometry after existing raster batches.
//-----------------------------------------------------------------------------
void CNavMeshGeometry::Append(CNavMeshGeometry&& supplemental)
{
    if (this == &supplemental)
        throw std::runtime_error("cannot append geometry to itself");
    if (!supplemental.m_Triangles.empty() && supplemental.m_ChunkyMeshes.empty())
        supplemental.Finalize();
    if (!m_Triangles.empty() && m_ChunkyMeshes.empty())
        Finalize();
    const std::size_t maximumVertexCount = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (m_Vertices.size() > maximumVertexCount || supplemental.m_Vertices.size() > maximumVertexCount - m_Vertices.size())
        throw std::runtime_error("combined geometry has too many m_Vertices");
    if (supplemental.m_Triangles.empty() && !supplemental.m_Vertices.empty())
        throw std::runtime_error("supplemental geometry has m_Vertices without m_Triangles");
    const std::size_t scratchSize = std::max(m_ChunkScratch.size(), supplemental.m_ChunkScratch.size());
    m_Vertices.reserve(m_Vertices.size() + supplemental.m_Vertices.size());
    m_Triangles.reserve(m_Triangles.size() + supplemental.m_Triangles.size());
    m_UnwalkableTriangles.reserve(m_UnwalkableTriangles.size() + supplemental.m_UnwalkableTriangles.size());
    m_ClipBrushes.reserve(m_ClipBrushes.size() + supplemental.m_ClipBrushes.size());
    m_OffMeshConnections.reserve(m_OffMeshConnections.size() + supplemental.m_OffMeshConnections.size());
    m_AreaVolumes.reserve(m_AreaVolumes.size() + supplemental.m_AreaVolumes.size());
    m_ChunkyMeshes.reserve(m_ChunkyMeshes.size() + supplemental.m_ChunkyMeshes.size());
    m_ChunkScratch.resize(scratchSize);
    const int vertexOffset = static_cast<int>(m_Vertices.size());
    for (int& index : supplemental.m_Triangles)
        index += vertexOffset;
    for (std::array<int, 3>& triangle : supplemental.m_UnwalkableTriangles)
        for (int& index : triangle)
            index += vertexOffset;
    for (ClipBrush_t& brush : supplemental.m_ClipBrushes)
    {
        brush.FirstVertex += vertexOffset;
        brush.EndVertex += vertexOffset;
        brush.FirstIndex += m_Triangles.size();
    }
    for (auto& chunkyMesh : supplemental.m_ChunkyMeshes)
    {
        for (int index = 0; index < chunkyMesh->ntris * 3; ++index)
            chunkyMesh->tris[index] += vertexOffset;
        m_ChunkyMeshes.push_back(std::move(chunkyMesh));
    }
    supplemental.m_ChunkyMeshes.clear();
    if (!supplemental.m_Vertices.empty())
    {
        if (m_Vertices.empty())
        {
            m_BoundsMinimum = supplemental.m_BoundsMinimum;
            m_BoundsMaximum = supplemental.m_BoundsMaximum;
        }
        else
        {
            rdVmin(&m_BoundsMinimum, &supplemental.m_BoundsMinimum);
            rdVmax(&m_BoundsMaximum, &supplemental.m_BoundsMaximum);
        }
    }
    m_Vertices.insert(m_Vertices.end(), supplemental.m_Vertices.begin(), supplemental.m_Vertices.end());
    m_Triangles.insert(m_Triangles.end(), supplemental.m_Triangles.begin(), supplemental.m_Triangles.end());
    m_UnwalkableTriangles.insert(m_UnwalkableTriangles.end(), supplemental.m_UnwalkableTriangles.begin(), supplemental.m_UnwalkableTriangles.end());
    m_ClipBrushes.insert(m_ClipBrushes.end(), supplemental.m_ClipBrushes.begin(), supplemental.m_ClipBrushes.end());
    m_OffMeshConnections.insert(m_OffMeshConnections.end(), supplemental.m_OffMeshConnections.begin(), supplemental.m_OffMeshConnections.end());
    m_AreaVolumes.insert(m_AreaVolumes.end(), std::make_move_iterator(supplemental.m_AreaVolumes.begin()),
                         std::make_move_iterator(supplemental.m_AreaVolumes.end()));
    supplemental.m_Vertices.clear();
    supplemental.m_Triangles.clear();
    supplemental.m_UnwalkableTriangles.clear();
    supplemental.m_ClipBrushes.clear();
    supplemental.m_OffMeshConnections.clear();
    supplemental.m_AreaVolumes.clear();
    supplemental.m_ChunkScratch.clear();
}

bool CNavMeshGeometry::IsTriangleUnwalkable(int first, int second, int third) const
{
    std::array<int, 3> key = {first, second, third};
    std::sort(key.begin(), key.end());
    return std::binary_search(m_UnwalkableTriangles.begin(), m_UnwalkableTriangles.end(), key);
}

const ClipBrush_t* CNavMeshGeometry::FindClipBrush(int vertex) const
{
    const auto after = std::upper_bound(m_ClipBrushes.begin(), m_ClipBrushes.end(), vertex,
                                        [](int index, const ClipBrush_t& brush) { return index < brush.FirstVertex; });
    if (after == m_ClipBrushes.begin())
        return nullptr;
    const ClipBrush_t& brush = *std::prev(after);
    return vertex < brush.EndVertex ? &brush : nullptr;
}

bool CNavMeshGeometry::Raycast(const rdVec3D& start, const rdVec3D& end) const
{
    if (m_ChunkScratch.empty())
        return false;
    for (const auto& chunkyMesh : m_ChunkyMeshes)
    {
        const int chunkCount =
            rcGetChunksOverlappingSegment(chunkyMesh.get(), &start, &end, m_ChunkScratch.data(), static_cast<int>(m_ChunkScratch.size()));
        for (int i = 0; i < chunkCount; ++i)
        {
            const rcChunkyTriMeshNode& node = chunkyMesh->nodes[m_ChunkScratch[static_cast<std::size_t>(i)]];
            const int* indices = &chunkyMesh->tris[node.i * 3];
            for (int triangle = 0; triangle < node.n; ++triangle)
            {
                const rdVec3D& a = m_Vertices[static_cast<std::size_t>(indices[triangle * 3])];
                const rdVec3D& b = m_Vertices[static_cast<std::size_t>(indices[triangle * 3 + 1])];
                const rdVec3D& c = m_Vertices[static_cast<std::size_t>(indices[triangle * 3 + 2])];
                float hitFraction = 1.0f;
                if (rdIntersectSegmentTriangle(&start, &end, &a, &b, &c, hitFraction))
                    return true;
            }
        }
    }
    return false;
}
