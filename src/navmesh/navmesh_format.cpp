#include "navmesh_format.h"
#include "navmesh_builder.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <system_error>
#include <unordered_set>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

// Titanfall 2 uses SDK set version 5 and its corresponding packed tile layout.
static_assert(DT_NAVMESH_SET_VERSION == 5);
static_assert(sizeof(dtNavMesh) == 0x88);
static_assert(sizeof(dtMeshTile) == 0x78);
static_assert(sizeof(dtNavMeshSetHeader) == 0x34);
static_assert(sizeof(dtNavMeshTileHeader) == 0x8);
static_assert(sizeof(dtMeshHeader) == 0x68);
static_assert(sizeof(dtPoly) == 0x30);
static_assert(sizeof(dtLink) == 0x10);
static_assert(sizeof(dtPolyDetail) == 0xC);
static_assert(sizeof(dtBVNode) == 0x10);
static_assert(sizeof(dtOffMeshConnection) == 0x34);

std::size_t CNavMeshFile::CheckedAdd(std::size_t left, std::size_t right)
{
    if (right > std::numeric_limits<std::size_t>::max() - left)
        throw std::runtime_error("navmesh size calculation overflow");
    return left + right;
}

std::size_t CNavMeshFile::CheckedMultiply(std::size_t left, std::size_t right)
{
    if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left)
        throw std::runtime_error("navmesh size calculation overflow");
    return left * right;
}

std::size_t CNavMeshFile::NonNegative(std::int32_t value, const char* field)
{
    if (value < 0)
        throw std::runtime_error(std::string("negative ") + field);
    return static_cast<std::size_t>(value);
}

template <typename T> T CNavMeshFile::ReadObject(std::size_t offset) const
{
    if (offset > m_Bytes.size() || sizeof(T) > m_Bytes.size() - offset)
        throw std::runtime_error("truncated navmesh data");
    T value;
    std::memcpy(&value, m_Bytes.data() + offset, sizeof(T));
    return value;
}

CNavMeshFile::CNavMeshFile(const std::filesystem::path& path)
{
    try
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
            throw std::runtime_error("failed to open navmesh file");
        const std::streamoff end = stream.tellg();
        if (end < 0)
            throw std::runtime_error("failed to determine navmesh file size");
        m_Bytes.resize(static_cast<std::size_t>(end));
        stream.seekg(0, std::ios::beg);
        if (!m_Bytes.empty() && !stream.read(reinterpret_cast<char*>(m_Bytes.data()), end))
            throw std::runtime_error("failed to read navmesh file");
        Parse();
    }
    catch (const std::exception& exception)
    {
        m_Error = exception.what();
    }
}

CNavMeshFile::TileSections_t CNavMeshFile::CalculateSections(const dtMeshHeader& header, std::size_t dataOffset)
{
    TileSections_t sections{};
    std::size_t offset = CheckedAdd(dataOffset, sizeof(dtMeshHeader));
    sections.Vertices = offset;
    offset = CheckedAdd(offset, CheckedMultiply(NonNegative(header.vertCount, "vertex count"), sizeof(rdVec3D)));
    sections.Polygons = offset;
    offset = CheckedAdd(offset, CheckedMultiply(NonNegative(header.polyCount, "polygon count"), sizeof(dtPoly)));
    sections.PolygonMap = offset;
    offset = CheckedAdd(offset, CheckedMultiply(CheckedMultiply(NonNegative(header.polyCount, "polygon count"),
                                                                NonNegative(header.polyMapCount, "polygon-map count")),
                                                sizeof(std::uint32_t)));
    sections.Links = offset;
    offset = CheckedAdd(offset, CheckedMultiply(NonNegative(header.maxLinkCount, "link count"), sizeof(dtLink)));
    sections.DetailMeshes = offset;
    offset = CheckedAdd(offset, CheckedMultiply(NonNegative(header.detailMeshCount, "detail-mesh count"), sizeof(dtPolyDetail)));
    sections.DetailVertices = offset;
    offset = CheckedAdd(offset, CheckedMultiply(NonNegative(header.detailVertCount, "detail-vertex count"), sizeof(rdVec3D)));
    sections.DetailTriangles = offset;
    offset = CheckedAdd(offset, CheckedMultiply(NonNegative(header.detailTriCount, "detail-triangle count"), 4));
    sections.BoundingVolumeTree = offset;
    offset = CheckedAdd(offset, CheckedMultiply(NonNegative(header.bvNodeCount, "BV-node count"), sizeof(dtBVNode)));
    sections.OffMeshConnections = offset;
    offset = CheckedAdd(offset, CheckedMultiply(NonNegative(header.offMeshConCount, "off-mesh connection count"), sizeof(dtOffMeshConnection)));
    sections.End = offset;
    return sections;
}

void CNavMeshFile::Parse()
{
    m_Header = ReadObject<dtNavMeshSetHeader>(0);
    if (m_Header.magic != DT_NAVMESH_SET_MAGIC)
        throw std::runtime_error("invalid navmesh-set magic");
    if (m_Header.version != DT_NAVMESH_SET_VERSION)
        throw std::runtime_error("unsupported navmesh-set version");
    if (m_Header.numTiles < 0)
        throw std::runtime_error("negative tile count");
    if (m_Header.params.maxTiles <= 0 || m_Header.params.maxPolys <= 0)
        throw std::runtime_error("invalid navmesh reference limits");
    if (m_Header.numTiles > m_Header.params.maxTiles)
        throw std::runtime_error("tile count exceeds maxTiles");

    std::size_t offset = sizeof(dtNavMeshSetHeader);
    const std::size_t maximumStoredTiles = (m_Bytes.size() - sizeof(dtNavMeshSetHeader)) / (sizeof(dtNavMeshTileHeader) + sizeof(dtMeshHeader));
    m_Tiles.reserve(std::min(static_cast<std::size_t>(m_Header.numTiles), maximumStoredTiles));
    for (int i = 0; i < m_Header.numTiles; ++i)
    {
        ParsedTile_t tile;
        tile.ContainerHeader = ReadObject<dtNavMeshTileHeader>(offset);
        offset = CheckedAdd(offset, sizeof(dtNavMeshTileHeader));
        if (tile.ContainerHeader.tileRef == 0 || tile.ContainerHeader.dataSize <= 0)
            throw std::runtime_error("invalid tile container header at index " + std::to_string(i));
        const std::size_t tileSize = static_cast<std::size_t>(tile.ContainerHeader.dataSize);
        if (offset > m_Bytes.size() || tileSize > m_Bytes.size() - offset)
            throw std::runtime_error("truncated tile at index " + std::to_string(i));
        tile.MeshHeader = ReadObject<dtMeshHeader>(offset);
        tile.Sections = CalculateSections(tile.MeshHeader, offset);
        if (tile.Sections.End != offset + tileSize)
            throw std::runtime_error("tile data size does not match its section counts at index " + std::to_string(i));
        tile.Polygons.resize(static_cast<std::size_t>(tile.MeshHeader.polyCount));
        if (!tile.Polygons.empty())
            std::memcpy(tile.Polygons.data(), m_Bytes.data() + tile.Sections.Polygons, tile.Polygons.size() * sizeof(dtPoly));
        m_Tiles.push_back(std::move(tile));
        offset += tileSize;
    }

    NonNegative(m_Header.params.traverseTableSize, "traversal-table size");
    NonNegative(m_Header.params.traverseTableCount, "traversal-table count");
    m_GroupAreaOffset = offset;
    offset = CheckedAdd(offset, CheckedMultiply(NonNegative(m_Header.params.polyGroupCount, "polygon-group count"), sizeof(std::uint32_t)));
    m_TraversalTablesOffset = offset;
    if (m_Header.params.polyGroupCount >= DT_MIN_POLY_GROUP_COUNT)
    {
        offset = CheckedAdd(offset, CheckedMultiply(NonNegative(m_Header.params.traverseTableSize, "traversal-table size"),
                                                    NonNegative(m_Header.params.traverseTableCount, "traversal-table count")));
    }
    if (offset != m_Bytes.size())
        throw std::runtime_error(offset < m_Bytes.size() ? "unexpected trailing navmesh data" : "truncated navmesh metadata");
}

std::vector<std::uint32_t> CNavMeshFile::BuildPolygonMap(std::span<const dtPoly> polygons)
{
    const std::size_t polygonCount = polygons.size();
    const std::size_t wordCount = (polygonCount + 31) / 32;
    std::vector<int> component(polygonCount, -1);
    std::vector<std::vector<std::size_t>> members;

    for (std::size_t start = 0; start < polygonCount; ++start)
    {
        if (component[start] != -1)
            continue;
        const int componentIndex = static_cast<int>(members.size());
        members.emplace_back();
        std::vector<std::size_t> pending = {start};
        component[start] = componentIndex;
        while (!pending.empty())
        {
            const std::size_t current = pending.back();
            pending.pop_back();
            members.back().push_back(current);
            const dtPoly& polygon = polygons[current];
            if (polygon.vertCount < 2 || polygon.vertCount > RD_VERTS_PER_POLYGON)
                throw std::runtime_error("polygon has an invalid vertex count");
            for (std::size_t edge = 0; edge < polygon.vertCount; ++edge)
            {
                const std::uint16_t neighbour = polygon.neis[edge];
                if (neighbour == 0 || (neighbour & DT_EXT_LINK) != 0)
                    continue;
                const std::size_t neighbourIndex = static_cast<std::size_t>(neighbour - 1);
                if (neighbourIndex >= polygonCount || component[neighbourIndex] != -1)
                    continue;
                component[neighbourIndex] = componentIndex;
                pending.push_back(neighbourIndex);
            }
        }
    }

    std::vector<std::vector<std::uint32_t>> componentMasks(members.size(), std::vector<std::uint32_t>(wordCount));
    for (std::size_t i = 0; i < members.size(); ++i)
    {
        for (const std::size_t polygon : members[i])
            componentMasks[i][polygon / 32] |= std::uint32_t{1} << (polygon & 31);
    }

    std::vector<std::uint32_t> map(CheckedMultiply(polygonCount, wordCount));
    for (std::size_t polygon = 0; polygon < polygonCount; ++polygon)
    {
        const auto& mask = componentMasks[static_cast<std::size_t>(component[polygon])];
        std::copy(mask.begin(), mask.end(), map.begin() + polygon * wordCount);
    }
    return map;
}

void CNavMeshFile::AppendBytes(const void* data, std::size_t size)
{
    const std::size_t oldSize = m_Bytes.size();
    m_Bytes.resize(CheckedAdd(oldSize, size));
    if (size != 0)
        std::memcpy(m_Bytes.data() + oldSize, data, size);
}

template <typename T> void CNavMeshFile::AppendObject(const T& value)
{
    AppendBytes(&value, sizeof(value));
}

void CNavMeshFile::AppendTile(const dtMeshTile& tile, dtTileRef reference)
{
    if (!tile.header)
        throw std::runtime_error("cannot serialize an empty tile");
    dtMeshHeader header = *tile.header;
    header.polyMapCount = static_cast<int>((NonNegative(header.polyCount, "polygon count") + 31) / 32);
    const TileSections_t sections = CalculateSections(header, 0);
    if (sections.End > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::runtime_error("generated tile is too large");
    const std::vector<std::uint32_t> polygonMap = BuildPolygonMap(std::span<const dtPoly>(tile.polys, static_cast<std::size_t>(header.polyCount)));
    AppendObject(dtNavMeshTileHeader{reference, static_cast<int>(sections.End)});
    AppendObject(header);
    AppendBytes(tile.verts, static_cast<std::size_t>(header.vertCount) * sizeof(rdVec3D));
    AppendBytes(tile.polys, static_cast<std::size_t>(header.polyCount) * sizeof(dtPoly));
    AppendBytes(polygonMap.data(), polygonMap.size() * sizeof(std::uint32_t));
    AppendBytes(tile.links, static_cast<std::size_t>(header.maxLinkCount) * sizeof(dtLink));
    AppendBytes(tile.detailMeshes, static_cast<std::size_t>(header.detailMeshCount) * sizeof(dtPolyDetail));
    AppendBytes(tile.detailVerts, static_cast<std::size_t>(header.detailVertCount) * sizeof(rdVec3D));
    AppendBytes(tile.detailTris, static_cast<std::size_t>(header.detailTriCount) * 4);
    AppendBytes(tile.bvTree, static_cast<std::size_t>(header.bvNodeCount) * sizeof(dtBVNode));
    AppendBytes(tile.offMeshCons, static_cast<std::size_t>(header.offMeshConCount) * sizeof(dtOffMeshConnection));
}

void CNavMeshFile::AppendGroupAreas(const dtNavMesh& mesh)
{
    const dtNavMeshParams* parameters = mesh.getParams();
    if (parameters->polyGroupCount < 0)
        throw std::runtime_error("negative polygon-group count");
    std::vector<std::uint64_t> wideAreas(static_cast<std::size_t>(parameters->polyGroupCount));
    for (int tileIndex = 0; tileIndex < mesh.getMaxTiles(); ++tileIndex)
    {
        const dtMeshTile* tile = mesh.getTile(tileIndex);
        if (!tile || !tile->header)
            continue;
        for (int polygonIndex = 0; polygonIndex < tile->header->polyCount; ++polygonIndex)
        {
            const dtPoly& polygon = tile->polys[polygonIndex];
            if (polygon.groupId >= wideAreas.size())
                throw std::runtime_error("polygon group ID exceeds the set header count");
            if (polygon.groupId >= DT_FIRST_USABLE_POLY_GROUP)
                wideAreas[polygon.groupId] += polygon.surfaceArea;
        }
    }

    for (std::size_t i = 0; i < wideAreas.size(); ++i)
    {
        if (wideAreas[i] > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("polygon-group surface area overflow");
        AppendObject(static_cast<std::uint32_t>(wideAreas[i]));
    }
}

CNavMeshFile::CNavMeshFile(const dtNavMesh& mesh)
{
    try
    {
        const dtNavMeshParams* parameters = mesh.getParams();
        m_Header = {DT_NAVMESH_SET_MAGIC, DT_NAVMESH_SET_VERSION, mesh.getTileCount(), *parameters};
        if (parameters->polyGroupCount < 0 || parameters->traverseTableCount < 0 || parameters->traverseTableSize < 0)
            throw std::runtime_error("invalid generated navmesh metadata");

        // Reserve the final file once; tile sections are appended directly to this buffer.
        std::size_t fileSize =
            CheckedAdd(sizeof(dtNavMeshSetHeader), CheckedMultiply(static_cast<std::size_t>(parameters->polyGroupCount), sizeof(std::uint32_t)));
        if (parameters->polyGroupCount >= DT_MIN_POLY_GROUP_COUNT)
            fileSize = CheckedAdd(fileSize, CheckedMultiply(static_cast<std::size_t>(parameters->traverseTableCount),
                                                            static_cast<std::size_t>(parameters->traverseTableSize)));
        for (int tileIndex = 0; tileIndex < mesh.getMaxTiles(); ++tileIndex)
        {
            const dtMeshTile* tile = mesh.getTile(tileIndex);
            if (!tile || !tile->header)
                continue;
            dtMeshHeader header = *tile->header;
            header.polyMapCount = static_cast<int>((NonNegative(header.polyCount, "polygon count") + 31) / 32);
            fileSize = CheckedAdd(fileSize, CheckedAdd(sizeof(dtNavMeshTileHeader), CalculateSections(header, 0).End));
        }
        m_Bytes.reserve(fileSize);
        AppendObject(m_Header);
        for (int tileIndex = 0; tileIndex < mesh.getMaxTiles(); ++tileIndex)
        {
            const dtMeshTile* tile = mesh.getTile(tileIndex);
            if (tile && tile->header)
                AppendTile(*tile, mesh.getTileRef(tile));
        }
        AppendGroupAreas(mesh);
        if (parameters->polyGroupCount >= DT_MIN_POLY_GROUP_COUNT)
        {
            int** tables = mesh.getTraverseTables();
            if (parameters->traverseTableCount != 0 && !tables)
                throw std::runtime_error("generated navmesh has no traversal-table storage");
            for (int tableIndex = 0; tableIndex < parameters->traverseTableCount; ++tableIndex)
            {
                if (!tables[tableIndex])
                    throw std::runtime_error("generated navmesh has an empty traversal table");
                AppendBytes(tables[tableIndex], static_cast<std::size_t>(parameters->traverseTableSize));
            }
        }

        Parse();
    }
    catch (const std::exception& exception)
    {
        m_Error = exception.what();
    }
}

bool ValidationResult_t::IsValid() const
{
    return std::none_of(Issues.begin(), Issues.end(), [](const ValidationIssue_t& issue) { return issue.Severity == ValidationSeverity_t::Error; });
}

ValidationResult_t CNavMeshFile::Validate() const
{
    ValidationResult_t result;
    try
    {
        if (!m_Error.empty())
            throw std::runtime_error(m_Error);
        result.Summary.Version = m_Header.version;
        result.Summary.TileCount = m_Header.numTiles;
        result.Summary.PolygonGroupCount = m_Header.params.polyGroupCount;
        result.Summary.TraversalTableCount = m_Header.params.traverseTableCount;
        result.Summary.FileSize = m_Bytes.size();

        const std::size_t groupCount = static_cast<std::size_t>(m_Header.params.polyGroupCount);
        const std::size_t expectedTableSize = CheckedMultiply(CheckedMultiply(groupCount, (groupCount + 31) / 32), sizeof(std::uint32_t));
        if (groupCount >= DT_MIN_POLY_GROUP_COUNT && expectedTableSize != static_cast<std::size_t>(m_Header.params.traverseTableSize))
        {
            result.Issues.push_back({ValidationSeverity_t::Error, "traversal-table size does not match polygon-group count"});
        }

        std::unordered_set<std::uint32_t> polygonReferences;
        std::unordered_set<std::uint32_t> tileReferences;
        std::vector<std::uint64_t> expectedGroupAreas(groupCount);

        for (std::size_t tileIndex = 0; tileIndex < m_Tiles.size(); ++tileIndex)
        {
            const ParsedTile_t& tile = m_Tiles[tileIndex];
            const dtMeshHeader& header = tile.MeshHeader;
            const std::string prefix = "tile " + std::to_string(tileIndex) + ": ";
            if (!tileReferences.insert(tile.ContainerHeader.tileRef).second)
                result.Issues.push_back({ValidationSeverity_t::Error, prefix + "duplicate tile reference"});
            if (header.magic != static_cast<int>(DT_NAVMESH_MAGIC))
                result.Issues.push_back({ValidationSeverity_t::Error, prefix + "invalid tile magic"});
            if (header.version != DT_NAVMESH_VERSION)
                result.Issues.push_back({ValidationSeverity_t::Error, prefix + "unsupported tile version"});
            if (header.polyCount <= 0 || header.polyCount > m_Header.params.maxPolys)
                result.Issues.push_back({ValidationSeverity_t::Error, prefix + "invalid polygon count"});
            if (header.offMeshBase < 0 || header.offMeshBase > header.polyCount)
            {
                result.Issues.push_back({ValidationSeverity_t::Error, prefix + "off-mesh polygon base is out of range"});
            }
            else
            {
                if (header.detailMeshCount != header.offMeshBase)
                {
                    result.Issues.push_back({ValidationSeverity_t::Error, prefix + "detail-mesh count differs from ground-polygon count"});
                }
                if (header.offMeshConCount != header.polyCount - header.offMeshBase)
                {
                    result.Issues.push_back({ValidationSeverity_t::Error, prefix + "off-mesh connection count differs from off-mesh polygon count"});
                }
            }
            const std::size_t expectedMapCount = (static_cast<std::size_t>(header.polyCount) + 31) / 32;
            if (header.polyMapCount != expectedMapCount)
                result.Issues.push_back({ValidationSeverity_t::Error, prefix + "polygon reachability map has the wrong width"});

            const std::vector<dtPoly>& polygons = tile.Polygons;
            if (static_cast<std::size_t>(header.polyMapCount) == expectedMapCount)
            {
                const std::vector<std::uint32_t> expectedMap = BuildPolygonMap(polygons);
                if (std::memcmp(expectedMap.data(), m_Bytes.data() + tile.Sections.PolygonMap, expectedMap.size() * sizeof(std::uint32_t)) != 0)
                {
                    result.Issues.push_back({ValidationSeverity_t::Error, prefix + "polygon reachability map does not match local adjacency"});
                }
            }

            std::vector<bool> claimedLinks(static_cast<std::size_t>(std::max(header.maxLinkCount, 0)));
            for (std::size_t polygonIndex = 0; polygonIndex < polygons.size(); ++polygonIndex)
            {
                const dtPoly& polygon = polygons[polygonIndex];
                ++result.Summary.PolygonCount;
                polygonReferences.insert(tile.ContainerHeader.tileRef | static_cast<std::uint32_t>(polygonIndex));
                if (polygon.vertCount < 2 || polygon.vertCount > RD_VERTS_PER_POLYGON)
                    result.Issues.push_back({ValidationSeverity_t::Error, prefix + "polygon has an invalid vertex count"});
                for (std::size_t edge = 0; edge < std::min<int>(polygon.vertCount, RD_VERTS_PER_POLYGON); ++edge)
                {
                    if (polygon.verts[edge] >= header.vertCount)
                        result.Issues.push_back({ValidationSeverity_t::Error, prefix + "polygon references an invalid vertex"});
                    const std::uint16_t neighbour = polygon.neis[edge];
                    if (neighbour != 0 && (neighbour & DT_EXT_LINK) == 0 && neighbour > header.polyCount)
                        result.Issues.push_back({ValidationSeverity_t::Error, prefix + "polygon has an invalid local neighbour"});
                }
                if (polygon.groupId >= groupCount)
                {
                    result.Issues.push_back({ValidationSeverity_t::Error, prefix + "polygon group ID is out of range"});
                }
                else if (polygon.groupId >= DT_FIRST_USABLE_POLY_GROUP)
                {
                    expectedGroupAreas[polygon.groupId] += polygon.surfaceArea;
                }

                std::uint32_t linkIndex = polygon.firstLink;
                std::size_t chainLength = 0;
                while (linkIndex != DT_NULL_LINK)
                {
                    if (linkIndex >= claimedLinks.size())
                    {
                        result.Issues.push_back({ValidationSeverity_t::Error, prefix + "polygon link index is out of range"});
                        break;
                    }
                    if (claimedLinks[linkIndex])
                    {
                        result.Issues.push_back({ValidationSeverity_t::Error, prefix + "polygon link chain is cyclic or shared"});
                        break;
                    }
                    claimedLinks[linkIndex] = true;
                    const dtLink link = ReadObject<dtLink>(tile.Sections.Links + linkIndex * sizeof(dtLink));
                    if (link.traverseType != DT_NULL_TRAVERSE_TYPE)
                        ++result.Summary.TraversalLinkCount;
                    linkIndex = link.next;
                    if (++chainLength > claimedLinks.size())
                    {
                        result.Issues.push_back({ValidationSeverity_t::Error, prefix + "polygon link chain does not terminate"});
                        break;
                    }
                }
                if (polygon.firstLink == DT_NULL_LINK && polygon.groupId != DT_UNLINKED_POLY_GROUP)
                    result.Issues.push_back({ValidationSeverity_t::Error, prefix + "unlinked polygon is not in reserved group 1"});
            }
            result.Summary.OffMeshConnectionCount += header.offMeshConCount;
        }

        for (const ParsedTile_t& tile : m_Tiles)
        {
            const std::vector<dtPoly>& polygons = tile.Polygons;
            for (const dtPoly& polygon : polygons)
            {
                std::uint32_t linkIndex = polygon.firstLink;
                std::size_t remaining = static_cast<std::size_t>(std::max(tile.MeshHeader.maxLinkCount, 0));
                while (linkIndex != DT_NULL_LINK && remaining-- != 0)
                {
                    if (linkIndex >= static_cast<std::uint32_t>(tile.MeshHeader.maxLinkCount))
                        break;
                    const dtLink link = ReadObject<dtLink>(tile.Sections.Links + linkIndex * sizeof(dtLink));
                    if (link.ref != 0 && !polygonReferences.contains(link.ref))
                        result.Issues.push_back({ValidationSeverity_t::Error, "link references a polygon that is not present"});
                    linkIndex = link.next;
                }
            }
        }

        for (std::size_t group = 0; group < groupCount; ++group)
        {
            const std::uint32_t stored = ReadObject<std::uint32_t>(m_GroupAreaOffset + group * sizeof(std::uint32_t));
            if (expectedGroupAreas[group] > std::numeric_limits<std::uint32_t>::max() ||
                stored != static_cast<std::uint32_t>(expectedGroupAreas[group]))
            {
                result.Issues.push_back(
                    {ValidationSeverity_t::Error, "polygon-group surface area does not match polygon data for group " + std::to_string(group)});
            }
        }

        if (groupCount >= DT_MIN_POLY_GROUP_COUNT && expectedTableSize == static_cast<std::size_t>(m_Header.params.traverseTableSize))
        {
            const std::size_t wordsPerRow = (groupCount + 31) / 32;
            for (int table = 0; table < m_Header.params.traverseTableCount; ++table)
            {
                const std::size_t tableOffset = m_TraversalTablesOffset + static_cast<std::size_t>(table) * expectedTableSize;
                for (std::size_t group = DT_FIRST_USABLE_POLY_GROUP; group < groupCount; ++group)
                {
                    const std::size_t word = group / 32;
                    const std::uint32_t value = ReadObject<std::uint32_t>(tableOffset + (group * wordsPerRow + word) * sizeof(std::uint32_t));
                    if ((value & (std::uint32_t{1} << (group & 31))) == 0)
                    {
                        result.Issues.push_back({ValidationSeverity_t::Error, "traversal table " + std::to_string(table) + " is not reflexive"});
                        break;
                    }
                }
            }
        }
    }
    catch (const std::exception& exception)
    {
        result.Issues.push_back({ValidationSeverity_t::Error, exception.what()});
    }
    return result;
}

NavMeshSummary_t CNavMeshFile::Save(const std::filesystem::path& outputPath) const
{
    const ValidationResult_t validation = Validate();
    if (!validation.IsValid())
    {
        std::string message = "navmesh validation failed";
        for (const ValidationIssue_t& issue : validation.Issues)
        {
            if (issue.Severity == ValidationSeverity_t::Error)
                message += ": " + issue.Message;
        }
        throw std::runtime_error(message);
    }
    std::filesystem::path temporaryPath = outputPath;
    temporaryPath += L".tmp";
    try
    {
        std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw std::runtime_error("failed to open temporary output file");
        if (!m_Bytes.empty())
            stream.write(reinterpret_cast<const char*>(m_Bytes.data()), static_cast<std::streamsize>(m_Bytes.size()));
        stream.flush();
        if (!stream)
            throw std::runtime_error("failed to write navmesh output");
        stream.close();
        if (!stream)
            throw std::runtime_error("failed to close navmesh output");
#ifdef _WIN32
        if (!MoveFileExW(temporaryPath.c_str(), outputPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "failed to replace navmesh output");
#else
        std::filesystem::rename(temporaryPath, outputPath);
#endif
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);
        throw;
    }
    return validation.Summary;
}

NavMeshSummary_t CGeneratedNavMesh::Save(const std::filesystem::path& path) const
{
    if (!Get())
        throw std::runtime_error("cannot save an empty generated navmesh");
    return CNavMeshFile(*Get()).Save(path);
}
