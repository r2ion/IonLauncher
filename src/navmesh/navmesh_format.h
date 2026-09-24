#pragma once

#include "Detour/Include/DetourNavMesh.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

enum class ValidationSeverity_t : std::uint8_t
{
    Warning,
    Error,
};

struct ValidationIssue_t
{
    ValidationSeverity_t Severity;
    std::string Message;
};

struct NavMeshSummary_t
{
    int Version = 0;
    int TileCount = 0;
    int PolygonCount = 0;
    int OffMeshConnectionCount = 0;
    int TraversalLinkCount = 0;
    int PolygonGroupCount = 0;
    int TraversalTableCount = 0;
    std::uintmax_t FileSize = 0;
};

struct ValidationResult_t
{
    NavMeshSummary_t Summary;
    std::vector<ValidationIssue_t> Issues;

    bool IsValid() const;
};

class CNavMeshFile
{
  public:
    explicit CNavMeshFile(const std::filesystem::path& path);
    explicit CNavMeshFile(const dtNavMesh& mesh);

    ValidationResult_t Validate() const;
    NavMeshSummary_t Save(const std::filesystem::path& path) const;

  private:
    struct TileSections_t
    {
        std::size_t Vertices;
        std::size_t Polygons;
        std::size_t PolygonMap;
        std::size_t Links;
        std::size_t DetailMeshes;
        std::size_t DetailVertices;
        std::size_t DetailTriangles;
        std::size_t BoundingVolumeTree;
        std::size_t OffMeshConnections;
        std::size_t End;
    };

    struct ParsedTile_t
    {
        dtNavMeshTileHeader ContainerHeader;
        dtMeshHeader MeshHeader;
        TileSections_t Sections;
        std::vector<dtPoly> Polygons;
    };

    static std::size_t CheckedAdd(std::size_t left, std::size_t right);
    static std::size_t CheckedMultiply(std::size_t left, std::size_t right);
    static std::size_t NonNegative(std::int32_t value, const char* field);
    static TileSections_t CalculateSections(const dtMeshHeader& header, std::size_t dataOffset);
    static std::vector<std::uint32_t> BuildPolygonMap(std::span<const dtPoly> polygons);

    template <typename T> T ReadObject(std::size_t offset) const;
    void AppendBytes(const void* data, std::size_t size);
    template <typename T> void AppendObject(const T& value);
    void AppendTile(const dtMeshTile& tile, dtTileRef reference);
    void AppendGroupAreas(const dtNavMesh& mesh);
    void Parse();

    std::vector<std::byte> m_Bytes;
    dtNavMeshSetHeader m_Header{};
    std::vector<ParsedTile_t> m_Tiles;
    std::size_t m_GroupAreaOffset = 0;
    std::size_t m_TraversalTablesOffset = 0;
    std::string m_Error;
};
