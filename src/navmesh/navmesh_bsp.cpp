#include "navmesh_bsp.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

// R2 layouts follow bsp_tool.branches.respawn.titanfall{,2}; all disk reads
// use memcpy rather than casting potentially unaligned packed file storage.
// The reader owns mounted bytes, lump views and model caches for one map load.
class CNavMeshBspReader
{
    enum Lump_t : unsigned
    {
        Entities = 0,
        Planes = 1,
        TextureData = 2,
        Vertices = 3,
        EntityPartitions = 24,
        GameLump = 35,
        TextureNames = 43,
        TricollTriangles = 66,
        TricollHeaders = 69,
        VertexUnlit = 71,
        VertexLitFlat = 72,
        VertexLitBump = 73,
        VertexUnlitTs = 74,
        MeshIndices = 79,
        Meshes = 80,
        MeshBounds = 81,
        MaterialSorts = 82,
        Grid = 85,
        GridCells = 86,
        GeoSets = 87,
        Primitives = 89,
        UniqueContents = 91,
        Brushes = 92,
        BrushPlaneOffsets = 93,
        BrushSideProperties = 94,
    };

    struct LumpHeader_t
    {
        std::uint32_t Offset, Length, Version, FourCC;
    };
    struct Plane_t
    {
        float Normal[3];
        float Distance;
    };
    struct Grid_t
    {
        float Scale;
        std::int32_t Offset[2], Count[2], StraddleGroups, FirstBrushPlane;
    };
    struct GridCell_t
    {
        std::uint16_t FirstGeoSet, GeoSetCount;
    };
    struct GeoSet_t
    {
        std::uint16_t StraddleGroup, PrimitiveCount;
        std::uint32_t Primitive;
    };
    struct Brush_t
    {
        float Origin[3];
        std::uint8_t NonAxialCount, PlaneOffsetCount;
        std::uint16_t Index;
        float Extents[3];
        std::int32_t SideOffset;
    };
    struct TricollHeader_t
    {
        std::int16_t Flags, TextureFlags, Texture, VertexCount, TriangleCount, BevelCount;
        std::int32_t FirstVertex, FirstTriangle, FirstNode, FirstBevel;
        float Origin[3], Scale;
    };
    struct TextureData_t
    {
        float Reflectivity[3];
        std::int32_t Name, Size[2], View[2], Flags;
    };
    struct GameLumpHeader_t
    {
        char Id[4];
        std::uint16_t Flags, Version;
        std::int32_t Offset, Length;
    };
    struct StaticProp_t
    {
        float Origin[3], Angles[3], Scale;
        std::uint16_t Model;
        std::uint8_t SolidMode, Flags;
        std::uint16_t Skin, Cubemap;
        float FadeScale, LightingOrigin[3];
        std::int8_t CpuLevel[2], GpuLevel[2];
        std::uint8_t DiffuseModulation[4];
        std::uint16_t CollisionFlags[2];
    };
    struct Mesh_t
    {
        std::uint32_t FirstIndex;
        std::uint16_t TriangleCount, FirstVertex, VertexCount;
        std::int8_t VertexType, Cubemap, Styles[4];
        std::int16_t LuxelOrigin[2];
        std::uint8_t LuxelMaximum[2];
        std::uint16_t MaterialSort;
        std::uint32_t Flags;
    };
    struct MeshBounds_t
    {
        float Origin[3], Radius, Extents[3], TanYaw;
    };
    struct MaterialSort_t
    {
        std::int16_t Texture, Lightmap, Cubemap, LastVertex;
        std::int32_t VertexOffset;
    };
    struct ModelHull_t
    {
        std::uint32_t FacePlaneCount, PlaneCount, EdgeCount, VertexCount, DataOffset;
    };
    static_assert(sizeof(LumpHeader_t) == 16 && sizeof(Plane_t) == 16 && sizeof(Grid_t) == 28);
    static_assert(sizeof(GridCell_t) == 4 && sizeof(GeoSet_t) == 8 && sizeof(Brush_t) == 32);
    static_assert(sizeof(TricollHeader_t) == 44 && sizeof(TextureData_t) == 36);
    static_assert(sizeof(GameLumpHeader_t) == 16 && sizeof(StaticProp_t) == 64);
    static_assert(sizeof(Mesh_t) == 28 && sizeof(MeshBounds_t) == 32 && sizeof(MaterialSort_t) == 12);
    static_assert(sizeof(ModelHull_t) == 20);

    //-----------------------------------------------------------------------------
    // Purpose: Bounds-checked, non-owning view of a disk structure or lump.
    //-----------------------------------------------------------------------------
    class CBinaryView
    {
      public:
        CBinaryView(std::string_view Data, std::string_view Path) : m_Data(Data), m_Path(Path)
        {
        }

        [[noreturn]] void Fail(const std::string& Reason) const
        {
            throw std::runtime_error(std::string(m_Path) + ": " + Reason);
        }

        void Check(std::size_t Offset, std::size_t Count, std::size_t Stride = 1) const
        {
            if (Offset > m_Data.size() || Stride == 0 || Count > (m_Data.size() - Offset) / Stride)
                Fail("structure or index outside file bounds");
        }

        template <typename T> T Read(std::size_t Offset) const
        {
            Check(Offset, 1, sizeof(T));
            T Value;
            std::memcpy(&Value, m_Data.data() + Offset, sizeof(T));
            return Value;
        }

        template <typename T> std::size_t Count() const
        {
            if (m_Data.size() % sizeof(T))
                Fail("partial packed record");
            return m_Data.size() / sizeof(T);
        }

        template <typename T> T At(std::size_t Index) const
        {
            if (Index >= Count<T>())
                Fail("record index outside lump bounds");
            return Read<T>(Index * sizeof(T));
        }

        CBinaryView Slice(std::size_t Offset, std::size_t Length) const
        {
            Check(Offset, Length);
            return CBinaryView(m_Data.substr(Offset, Length), m_Path);
        }

        std::string String(std::size_t Offset, std::size_t Maximum) const
        {
            Check(Offset, Maximum);
            const std::string_view Value = m_Data.substr(Offset, Maximum);
            const auto End = Value.find('\0');
            if (End == std::string_view::npos)
                Fail("unterminated string");
            return std::string(Value.substr(0, End));
        }

        std::string_view Data() const
        {
            return m_Data;
        }

      private:
        std::string_view m_Data;
        std::string_view m_Path;
    };

    using Point_t = std::array<double, 3>;
    using Face_t = std::vector<Point_t>;
    using ModelFaces_t = std::vector<Face_t>;

  public:
    CNavMeshBspReader(std::string Path, ReadFileFn ReadFile, CNavMeshMap& Map)
        : m_Path(std::move(Path)), m_ReadFile(ReadFile), m_File(m_ReadFile(m_Path.c_str())), m_Map(Map)
    {
        const CBinaryView Header(m_File, m_Path);
        if (m_File.empty())
            Header.Fail("required BSP is unavailable");
        Header.Check(0, 16 + 128 * sizeof(LumpHeader_t));
        if (m_File.compare(0, 4, "rBSP") != 0)
            Header.Fail("expected rBSP header");
        if (Header.Read<std::uint32_t>(4) != 37)
            Header.Fail("unsupported BSP version (expected Titanfall 2 v37)");
        if (Header.Read<std::uint32_t>(12) != 127)
            Header.Fail("unsupported BSP lump directory");
        for (unsigned Index = 0; Index < m_Headers.size(); ++Index)
            m_Headers[Index] = Header.Read<LumpHeader_t>(16 + Index * sizeof(LumpHeader_t));
    }

    CBinaryView Lump(unsigned Index)
    {
        const auto& Header = m_Headers.at(Index);
        if (!m_Loaded[Index])
        {
            char Suffix[32];
            std::snprintf(Suffix, sizeof(Suffix), ".%04x.bsp_lump", Index);
            m_LumpPaths[Index] = m_Path + Suffix;
            m_External[Index] = m_ReadFile(m_LumpPaths[Index].c_str());
            // R2 inherits both lit-vertex v1 layouts from Titanfall; unlit
            // vertices remain v0 (bsp_tool respawn/titanfall.py LUMP_CLASSES).
            unsigned ExpectedVersion = 0;
            switch (Index)
            {
            case Planes:
            case TextureData:
            case TricollHeaders:
            case VertexLitFlat:
            case VertexLitBump:
                ExpectedVersion = 1;
                break;
            case TricollTriangles:
                ExpectedVersion = 2;
                break;
            }
            if ((Header.Length || !m_External[Index].empty()) && Header.Version != ExpectedVersion)
                throw std::runtime_error(m_LumpPaths[Index] + ": unsupported lump version " + std::to_string(Header.Version));
            if (Header.FourCC)
                throw std::runtime_error(m_LumpPaths[Index] + ": compressed lumps are unsupported");
            if (!m_External[Index].empty())
                m_LumpData[Index] = m_External[Index];
            else if (Header.Length)
            {
                if (Header.Offset > m_File.size() || Header.Length > m_File.size() - Header.Offset)
                    throw std::runtime_error(m_LumpPaths[Index] + ": required external lump is unavailable");
                m_LumpData[Index] = std::string_view(m_File).substr(Header.Offset, Header.Length);
                m_LumpPaths[Index] = m_Path + " lump " + std::to_string(Index);
            }
            m_Loaded[Index] = true;
        }
        return CBinaryView(m_LumpData[Index], m_LumpPaths[Index]);
    }

    std::uint32_t LumpOffset(unsigned Index) const
    {
        return m_Headers.at(Index).Offset;
    }
    std::string ReadFile(const std::string& Path) const
    {
        return m_ReadFile(Path.c_str());
    }

    void Load(bool TitanCollision)
    {
        LoadEntities(m_Path.substr(0, m_Path.size() - 4), m_Map.m_Entities);
        m_Materials = LoadMaterials();
        LoadCollision(TitanCollision);
        LoadRenderTriangles();
        m_Map.m_Geometry.Finalize();
    }

  private:
    static Point_t Subtract(const Point_t& A, const Point_t& B)
    {
        return {A[0] - B[0], A[1] - B[1], A[2] - B[2]};
    }
    static Point_t Cross(const Point_t& A, const Point_t& B)
    {
        return {A[1] * B[2] - A[2] * B[1], A[2] * B[0] - A[0] * B[2], A[0] * B[1] - A[1] * B[0]};
    }
    static double Dot(const Point_t& A, const Point_t& B)
    {
        return A[0] * B[0] + A[1] * B[1] + A[2] * B[2];
    }
    static Point_t Point(const float* Value)
    {
        Point_t Result{Value[0], Value[1], Value[2]};
        for (double Component : Result)
            if (!std::isfinite(Component))
                throw std::runtime_error("non-finite geometry coordinate");
        return Result;
    }
    static Point_t Normal(const Plane_t& Plane)
    {
        const Point_t Result = Point(Plane.Normal);
        if (!std::isfinite(Plane.Distance) || Dot(Result, Result) < 1e-14)
            throw std::runtime_error("invalid collision plane");
        return Result;
    }
    static void AddUnique(Face_t& Points, const Point_t& Value)
    {
        for (const auto& Other : Points)
        {
            const auto Delta = Subtract(Value, Other);
            if (Dot(Delta, Delta) <= 0.05 * 0.05)
                return;
        }
        Points.push_back(Value);
    }

    //-----------------------------------------------------------------------------
    // Purpose: Order a convex face counter-clockwise around its outward normal.
    //-----------------------------------------------------------------------------
    static void OrderFace(Face_t& Face, const Point_t& FaceNormal)
    {
        Point_t Center{};
        for (const auto& Vertex : Face)
            for (unsigned Axis = 0; Axis < 3; ++Axis)
                Center[Axis] += Vertex[Axis] / Face.size();
        const Point_t Reference = std::abs(FaceNormal[2]) < 0.9 ? Point_t{0, 0, 1} : Point_t{0, 1, 0};
        Point_t Tangent = Cross(Reference, FaceNormal);
        const double Length = std::sqrt(Dot(Tangent, Tangent));
        for (double& Component : Tangent)
            Component /= Length;
        const Point_t Bitangent = Cross(FaceNormal, Tangent);
        std::sort(Face.begin(), Face.end(), [&](const Point_t& A, const Point_t& B)
        {
            const auto LocalA = Subtract(A, Center), LocalB = Subtract(B, Center);
            return std::atan2(Dot(LocalA, Bitangent), Dot(LocalA, Tangent)) < std::atan2(Dot(LocalB, Bitangent), Dot(LocalB, Tangent));
        });
    }

    static int AddVertex(CNavMeshGeometry& Geometry, const Point_t& Vertex)
    {
        if (Geometry.m_Vertices.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error("collision geometry exceeds vertex index range");
        const int Index = static_cast<int>(Geometry.m_Vertices.size());
        for (double Component : Vertex)
            if (!std::isfinite(Component) || std::abs(Component) > std::numeric_limits<float>::max())
                throw std::runtime_error("collision coordinate exceeds finite float range");
        Geometry.m_Vertices.push_back({static_cast<float>(Vertex[0]), static_cast<float>(Vertex[1]), static_cast<float>(Vertex[2])});
        return Index;
    }
    static void AddTriangle(CNavMeshGeometry& Geometry, int A, int B, int C, bool Unwalkable = false)
    {
        Geometry.m_Triangles.insert(Geometry.m_Triangles.end(), {A, B, C});
        if (Unwalkable)
            Geometry.m_UnwalkableTriangles.push_back({A, B, C});
    }
    static void AddFace(CNavMeshGeometry& Geometry, const Face_t& Face, bool Unwalkable = false)
    {
        if (Face.size() < 3)
            return;
        const int First = AddVertex(Geometry, Face[0]);
        int Previous = AddVertex(Geometry, Face[1]);
        for (std::size_t Index = 2; Index < Face.size(); ++Index)
        {
            const int Next = AddVertex(Geometry, Face[Index]);
            AddTriangle(Geometry, First, Previous, Next, Unwalkable);
            Previous = Next;
        }
    }

    //-----------------------------------------------------------------------------
    // Purpose: Recover world brush faces from axial bounds and non-axial planes.
    //-----------------------------------------------------------------------------
    void AddBrush(const Grid_t& GridData, unsigned Index, CNavMeshGeometry& Geometry, bool ClipOnly)
    {
        const auto Brush = Lump(Brushes).At<Brush_t>(Index);
        const Point_t Origin = Point(Brush.Origin), Extents = Point(Brush.Extents);
        const int FirstVertex = static_cast<int>(Geometry.m_Vertices.size());
        const std::size_t FirstIndex = Geometry.m_Triangles.size();
        std::vector<Plane_t> BrushPlanes;
        BrushPlanes.reserve(6 + Brush.PlaneOffsetCount);
        for (unsigned Axis = 0; Axis < 3; ++Axis)
        {
            if (Extents[Axis] < 0)
                throw std::runtime_error("negative brush extent");
            Plane_t Positive{}, Negative{};
            Positive.Normal[Axis] = 1;
            Negative.Normal[Axis] = -1;
            Positive.Distance = static_cast<float>(Origin[Axis] + Extents[Axis]);
            Negative.Distance = static_cast<float>(Extents[Axis] - Origin[Axis]);
            BrushPlanes.push_back(Positive);
            BrushPlanes.push_back(Negative);
        }
        const auto Offsets = Lump(BrushPlaneOffsets), PlaneData = Lump(Planes);
        for (unsigned Side = 0; Side < Brush.PlaneOffsetCount; ++Side)
        {
            const std::int64_t Offset = static_cast<std::int64_t>(Brush.SideOffset) + Side;
            const auto BackOffset = Offsets.At<std::uint16_t>(static_cast<std::size_t>(Offset));
            const std::int64_t PlaneIndex = static_cast<std::int64_t>(GridData.FirstBrushPlane) + Offset - BackOffset;
            BrushPlanes.push_back(PlaneData.At<Plane_t>(static_cast<std::size_t>(PlaneIndex)));
        }
        std::vector<Point_t> Normals;
        for (const auto& Plane : BrushPlanes)
            Normals.push_back(Normal(Plane));
        Face_t Vertices;
        for (std::size_t A = 0; A < BrushPlanes.size(); ++A)
            for (std::size_t B = A + 1; B < BrushPlanes.size(); ++B)
                for (std::size_t C = B + 1; C < BrushPlanes.size(); ++C)
                {
                    const auto BC = Cross(Normals[B], Normals[C]);
                    const double Determinant = Dot(Normals[A], BC);
                    if (std::abs(Determinant) < 1e-7)
                        continue;
                    const auto CA = Cross(Normals[C], Normals[A]), AB = Cross(Normals[A], Normals[B]);
                    Point_t Vertex;
                    for (unsigned Axis = 0; Axis < 3; ++Axis)
                        Vertex[Axis] =
                            (BrushPlanes[A].Distance * BC[Axis] + BrushPlanes[B].Distance * CA[Axis] + BrushPlanes[C].Distance * AB[Axis]) /
                            Determinant;
                    bool Inside = true;
                    for (std::size_t Side = 0; Side < BrushPlanes.size(); ++Side)
                        if (Dot(Normals[Side], Vertex) - BrushPlanes[Side].Distance > 0.1)
                        {
                            Inside = false;
                            break;
                        }
                    if (Inside)
                        AddUnique(Vertices, Vertex);
                }
        const auto Properties = Lump(BrushSideProperties), Textures = Lump(TextureData);
        const std::int64_t FirstSide = static_cast<std::int64_t>(Brush.Index) * 6 + Brush.SideOffset;
        if (FirstSide < 0)
            Properties.Fail("negative brush side index");
        Properties.Check(static_cast<std::size_t>(FirstSide) * sizeof(std::uint16_t), BrushPlanes.size(), sizeof(std::uint16_t));
        for (std::size_t Side = 0; Side < BrushPlanes.size(); ++Side)
        {
            Face_t Face;
            for (const auto& Vertex : Vertices)
                if (std::abs(Dot(Normals[Side], Vertex) - BrushPlanes[Side].Distance) <= 0.1)
                    Face.push_back(Vertex);
            if (Face.size() >= 3)
            {
                OrderFace(Face, Normals[Side]);
                // R2 engine.dll RVA 0x147DD4 / 0x147E23 masks both axial and
                // non-axial side properties to a fourteen-bit surface index.
                // SKY/SKY2D collide, but must never supply navigation support.
                const auto Property = Properties.At<std::uint16_t>(static_cast<std::size_t>(FirstSide) + Side);
                const auto Texture = Textures.At<TextureData_t>(Property & 0x3FFF);
                AddFace(Geometry, Face, (Texture.Flags & 0x6) != 0);
            }
        }
        if (ClipOnly && Geometry.m_Triangles.size() != FirstIndex)
            Geometry.m_ClipBrushes.push_back(
                {FirstVertex, static_cast<int>(Geometry.m_Vertices.size()), FirstIndex, Geometry.m_Triangles.size() - FirstIndex, {}, {}});
    }

    //-----------------------------------------------------------------------------
    // Purpose: Decode v53 inline convex collision; zero hulls legitimately means
    //          no collision. Missing models never become guessed OBB obstacles.
    //-----------------------------------------------------------------------------
    ModelFaces_t LoadModelCollision(const std::string& Path)
    {
        const std::string Bytes = ReadFile(Path);
        const CBinaryView Model(Bytes, Path);
        if (Bytes.empty())
            Model.Fail("required colliding model is unavailable");
        Model.Check(0, 0x1D4);
        if (Bytes.compare(0, 4, "IDST") != 0 || Model.Read<std::int32_t>(4) != 53)
            Model.Fail("expected Titanfall 2 MDL v53");
        const auto CollisionCount = Model.Read<std::int32_t>(0x1D0);
        if (CollisionCount == 0)
            return {};
        const auto PhysicsOffset = Model.Read<std::int32_t>(0x1B8), PhysicsSize = Model.Read<std::int32_t>(0x1C8);
        const auto CollisionOffset = Model.Read<std::int32_t>(0x1CC);
        if (CollisionCount < 0 || PhysicsSize <= 0 || PhysicsOffset < 0 || CollisionOffset < 0)
            Model.Fail("VPhysics/static-collision metadata disagrees");
        Model.Check(PhysicsOffset, PhysicsSize);
        Model.Check(CollisionOffset, CollisionCount, sizeof(ModelHull_t));
        ModelFaces_t Faces;
        for (int HullIndex = 0; HullIndex < CollisionCount; ++HullIndex)
        {
            const std::size_t HeaderOffset = static_cast<std::size_t>(CollisionOffset) + HullIndex * sizeof(ModelHull_t);
            const auto Hull = Model.Read<ModelHull_t>(HeaderOffset);
            if (Hull.FacePlaneCount > Hull.PlaneCount || Hull.VertexCount < 3)
                Model.Fail("invalid static collision hull " + std::to_string(HullIndex));
            const std::size_t PlaneOffset = HeaderOffset + Hull.DataOffset;
            Model.Check(PlaneOffset, Hull.PlaneCount, sizeof(Plane_t));
            const std::size_t EdgeOffset = PlaneOffset + static_cast<std::size_t>(Hull.PlaneCount) * sizeof(Plane_t);
            Model.Check(EdgeOffset, Hull.EdgeCount, 0x1C);
            const std::size_t VertexOffset = EdgeOffset + static_cast<std::size_t>(Hull.EdgeCount) * 0x1C;
            Model.Check(VertexOffset, Hull.VertexCount, sizeof(std::array<float, 3>));
            Face_t Vertices;
            Vertices.reserve(Hull.VertexCount);
            for (unsigned Index = 0; Index < Hull.VertexCount; ++Index)
                Vertices.push_back(Point(Model.Read<std::array<float, 3>>(VertexOffset + static_cast<std::size_t>(Index) * 12).data()));
            for (unsigned Index = 0; Index < Hull.FacePlaneCount; ++Index)
            {
                const auto Plane = Model.Read<Plane_t>(PlaneOffset + Index * sizeof(Plane_t));
                const auto FaceNormal = Normal(Plane);
                Face_t Face;
                for (const auto& Vertex : Vertices)
                    if (std::abs(Dot(FaceNormal, Vertex) - Plane.Distance) <= 0.1)
                        AddUnique(Face, Vertex);
                if (Face.size() < 3)
                    Model.Fail("could not reconstruct static collision hull " + std::to_string(HullIndex) + " face " + std::to_string(Index));
                OrderFace(Face, FaceNormal);
                Faces.push_back(std::move(Face));
            }
        }
        return Faces;
    }

    static std::string NormalizePath(std::string Path)
    {
        for (char& Character : Path)
        {
            if (Character == '\\')
                Character = '/';
            if (Character >= 'A' && Character <= 'Z')
                Character += 'a' - 'A';
        }
        return Path;
    }

    //-----------------------------------------------------------------------------
    // Purpose: Read the actual BSP sprp model dictionary and transformed instances.
    //-----------------------------------------------------------------------------
    void LoadStaticProps(std::vector<std::string>& Models, std::vector<StaticProp_t>& Props)
    {
        const auto Game = Lump(GameLump);
        if (Game.Data().empty())
            return;
        const auto Count = Game.Read<std::uint32_t>(0);
        Game.Check(4, Count, sizeof(GameLumpHeader_t));
        for (unsigned Index = 0; Index < Count; ++Index)
        {
            const auto Header = Game.Read<GameLumpHeader_t>(4 + Index * sizeof(GameLumpHeader_t));
            if (std::memcmp(Header.Id, "prps", 4) != 0)
                continue;
            if (Header.Version != 13)
                Game.Fail("unsupported sprp version (expected v13)");
            const std::int64_t RelativeOffset = static_cast<std::int64_t>(Header.Offset) - LumpOffset(GameLump);
            const auto Data = Game.Slice(static_cast<std::size_t>(RelativeOffset), static_cast<std::size_t>(Header.Length));
            if (Data.Data().substr(0, 4) == "LZMA")
                Data.Fail("compressed static props are unsupported");
            const auto ModelCount = Data.Read<std::uint32_t>(0);
            Data.Check(4, ModelCount, 128);
            for (unsigned Model = 0; Model < ModelCount; ++Model)
            {
                auto Name = NormalizePath(Data.String(4 + Model * std::size_t(128), 128));
                if (Name.compare(0, 7, "models/") != 0 || Name.find(':') != std::string::npos || Name.find("..") != std::string::npos)
                    Data.Fail("invalid mounted model path " + Name);
                Models.push_back(std::move(Name));
            }
            const std::size_t PropHeader = 4 + ModelCount * std::size_t(128);
            const auto PropCount = Data.Read<std::uint32_t>(PropHeader);
            Data.Check(PropHeader, 12);
            const std::size_t PropOffset = PropHeader + 12;
            Data.Check(PropOffset, PropCount, sizeof(StaticProp_t));
            Props.reserve(PropCount);
            for (unsigned Prop = 0; Prop < PropCount; ++Prop)
                Props.push_back(Data.Read<StaticProp_t>(PropOffset + Prop * sizeof(StaticProp_t)));
            const std::size_t Tail = PropOffset + PropCount * sizeof(StaticProp_t);
            if (Tail == Data.Data().size() && ModelCount == 0 && PropCount == 0 && Data.Read<std::uint32_t>(PropHeader + 4) == 0 &&
                Data.Read<std::uint32_t>(PropHeader + 8) == 0)
                return;
            const auto UnknownCount = Data.Read<std::uint32_t>(Tail);
            Data.Check(Tail + 4, UnknownCount, 64);
            if (Tail + 4 + UnknownCount * std::size_t(64) != Data.Data().size())
                Data.Fail("unexpected sprp trailing data");
            return;
        }
    }

    static void AddStaticProp(const StaticProp_t& Prop, const ModelFaces_t& Faces, CNavMeshGeometry& Geometry)
    {
        const auto Origin = Point(Prop.Origin), Angles = Point(Prop.Angles);
        if (!std::isfinite(Prop.Scale) || Prop.Scale <= 0)
            throw std::runtime_error("invalid static prop scale");
        constexpr double Radians = 3.14159265358979323846 / 180.0;
        const double Pitch = Angles[0] * Radians, Yaw = Angles[1] * Radians, Roll = Angles[2] * Radians;
        const double SP = std::sin(Pitch), CP = std::cos(Pitch), SY = std::sin(Yaw), CY = std::cos(Yaw), SR = std::sin(Roll), CR = std::cos(Roll);
        const std::array<Point_t, 3> Matrix{{{CP * CY, SR * SP * CY - CR * SY, CR * SP * CY + SR * SY},
                                             {CP * SY, SR * SP * SY + CR * CY, CR * SP * SY - SR * CY},
                                             {-SP, SR * CP, CR * CP}}};
        for (const auto& Face : Faces)
        {
            Face_t Transformed;
            Transformed.reserve(Face.size());
            for (const auto& Vertex : Face)
            {
                Point_t World;
                for (unsigned Axis = 0; Axis < 3; ++Axis)
                    World[Axis] = Origin[Axis] + Dot(Matrix[Axis], Vertex) * Prop.Scale;
                Transformed.push_back(World);
            }
            AddFace(Geometry, Transformed);
        }
    }

    //-----------------------------------------------------------------------------
    // Purpose: Parse Valve entity key/value syntax including comments and escapes.
    //-----------------------------------------------------------------------------
    static void ParseEntities(const CBinaryView& Data, std::vector<MapEntity_t>& Entities)
    {
        const auto Text = Data.Data();
        std::size_t Cursor = 0;
        auto SkipSpace = [&]()
        {
            while (Cursor < Text.size())
            {
                if (static_cast<unsigned char>(Text[Cursor]) <= ' ')
                    ++Cursor;
                else if (Text[Cursor] == '/' && Cursor + 1 < Text.size() && Text[Cursor + 1] == '/')
                {
                    while (Cursor < Text.size() && Text[Cursor] != '\n')
                        ++Cursor;
                }
                else
                    break;
            }
        };
        auto ReadQuoted = [&]()
        {
            SkipSpace();
            if (Cursor == Text.size() || Text[Cursor++] != '"')
                Data.Fail("expected quoted entity key/value");
            std::string Result;
            while (Cursor < Text.size())
            {
                char Character = Text[Cursor++];
                if (Character == '"')
                    return Result;
                if (Character == '\0')
                    Data.Fail("unterminated entity string");
                if (Character == '\\' && Cursor < Text.size() && (Text[Cursor] == '"' || Text[Cursor] == '\\'))
                    Character = Text[Cursor++];
                Result.push_back(Character);
            }
            Data.Fail("unterminated entity string");
        };
        SkipSpace();
        while (Cursor < Text.size())
        {
            if (Text[Cursor++] != '{')
                Data.Fail("expected entity opening brace");
            MapEntity_t Entity;
            for (;;)
            {
                SkipSpace();
                if (Cursor == Text.size())
                    Data.Fail("unterminated entity");
                if (Text[Cursor] == '}')
                {
                    ++Cursor;
                    break;
                }
                auto Key = ReadQuoted();
                auto Value = ReadQuoted();
                Entity.Values.insert_or_assign(std::move(Key), std::move(Value));
            }
            Entities.push_back(std::move(Entity));
            SkipSpace();
        }
    }

    void LoadEntities(const std::string& MapPath, std::vector<MapEntity_t>& EntitiesOut)
    {
        ParseEntities(Lump(Entities), EntitiesOut);
        const auto PartitionData = Lump(EntityPartitions);
        std::string PartitionText(PartitionData.Data());
        std::replace(PartitionText.begin(), PartitionText.end(), '\0', ' ');
        std::istringstream Partitions(PartitionText);
        std::unordered_set<std::string> Required;
        for (std::string Partition; Partitions >> Partition;)
        {
            if (Partition == "01*")
                continue;
            if (Partition != "env" && Partition != "fx" && Partition != "script" && Partition != "snd" && Partition != "spawn")
                PartitionData.Fail("unsupported entity partition " + Partition);
            Required.insert(Partition);
        }
        for (const char* Partition : {"env", "fx", "script", "snd", "spawn"})
        {
            const std::string Path = MapPath + "_" + Partition + ".ent";
            const std::string Bytes = ReadFile(Path);
            if (Bytes.empty())
            {
                if (Required.count(Partition))
                    throw std::runtime_error(Path + ": required entity partition is unavailable");
                continue;
            }
            const CBinaryView Data(Bytes, Path);
            const auto Newline = Bytes.find('\n');
            if (Newline == std::string::npos || (Bytes.substr(0, Newline) != "ENTITIES01" && Bytes.substr(0, Newline) != "ENTITIES01\r"))
                Data.Fail("unsupported entity partition header");
            ParseEntities(Data.Slice(Newline + 1, Bytes.size() - Newline - 1), EntitiesOut);
        }
    }

    std::vector<std::string> LoadMaterials()
    {
        const auto Names = Lump(TextureNames);
        std::vector<std::string> Strings;
        std::size_t Offset = 0;
        while (Offset < Names.Data().size())
        {
            auto Value = Names.String(Offset, Names.Data().size() - Offset);
            Offset += Value.size() + 1;
            Strings.push_back(NormalizePath(std::move(Value)));
        }
        const auto Textures = Lump(TextureData);
        std::vector<std::string> Materials;
        Materials.reserve(Textures.Count<TextureData_t>());
        for (std::size_t Index = 0; Index < Textures.Count<TextureData_t>(); ++Index)
        {
            const auto Texture = Textures.At<TextureData_t>(Index);
            if (Texture.Name < 0 || static_cast<std::size_t>(Texture.Name) >= Strings.size())
                Textures.Fail("material name index outside string table");
            Materials.push_back(Strings[Texture.Name]);
        }
        return Materials;
    }

    //-----------------------------------------------------------------------------
    // Purpose: Keep only floor-capable render triangles from meshes near authored
    //          cover nodes; material identities are shared integers, never strings
    //          per triangle. Source XYZ and mesh-local connectivity are preserved.
    //-----------------------------------------------------------------------------
    void LoadRenderTriangles()
    {
        std::vector<Point_t> Nodes;
        for (const auto& Entity : m_Map.m_Entities)
        {
            const auto Class = Entity.Values.find("classname"), Origin = Entity.Values.find("origin");
            if (Class == Entity.Values.end() || Class->second.compare(0, 16, "info_node_cover_") != 0 || Origin == Entity.Values.end())
                continue;
            std::istringstream Values(Origin->second);
            Point_t Node;
            std::string Extra;
            if (Values >> Node[0] >> Node[1] >> Node[2] && !(Values >> Extra) && std::isfinite(Node[0]) && std::isfinite(Node[1]) &&
                std::isfinite(Node[2]))
                Nodes.push_back(Node);
        }
        if (Nodes.empty())
            return;
        std::unordered_map<std::string, int> MaterialIds;
        std::vector<int> Ids;
        for (const auto& Material : m_Materials)
        {
            const int Next = static_cast<int>(MaterialIds.size());
            Ids.push_back(MaterialIds.emplace(Material, Next).first->second);
        }
        const auto Bounds = Lump(MeshBounds), MeshData = Lump(Meshes), Sorts = Lump(MaterialSorts);
        const auto Indices = Lump(MeshIndices), Positions = Lump(Vertices);
        if (Bounds.Count<MeshBounds_t>() != MeshData.Count<Mesh_t>())
            Bounds.Fail("mesh bounds count does not match meshes");
        const double MinimumNormal = std::cos(45.573 * 3.14159265358979323846 / 180.0);
        for (std::size_t Index = 0; Index < MeshData.Count<Mesh_t>(); ++Index)
        {
            const auto Bound = Bounds.At<MeshBounds_t>(Index);
            const auto Origin = Point(Bound.Origin), Extents = Point(Bound.Extents);
            bool NearNode = false;
            for (const auto& Node : Nodes)
                if (Node[0] >= Origin[0] - Extents[0] && Node[0] <= Origin[0] + Extents[0] && Node[1] >= Origin[1] - Extents[1] &&
                    Node[1] <= Origin[1] + Extents[1] && Origin[2] - Extents[2] <= Node[2] + 24 && Origin[2] + Extents[2] >= Node[2] - 128)
                {
                    NearNode = true;
                    break;
                }
            if (!NearNode)
                continue;
            const auto Mesh = MeshData.At<Mesh_t>(Index);
            const auto Sort = Sorts.At<MaterialSort_t>(Mesh.MaterialSort);
            if (Sort.Texture < 0 || static_cast<std::size_t>(Sort.Texture) >= m_Materials.size())
                Sorts.Fail("invalid mesh texture index");
            if (m_Materials[Sort.Texture].compare(0, 6, "tools/") == 0)
                continue;
            unsigned VertexLump = VertexLitFlat, Stride = 36;
            switch (Mesh.Flags & 0x600)
            {
            case 0x200:
                VertexLump = VertexLitBump;
                Stride = 44;
                break;
            case 0x400:
                VertexLump = VertexUnlit;
                Stride = 20;
                break;
            case 0x600:
                VertexLump = VertexUnlitTs;
                Stride = 28;
                break;
            }
            const auto VertexData = Lump(VertexLump);
            if (VertexData.Data().size() % Stride)
                VertexData.Fail("partial render vertex record");
            Indices.Check(static_cast<std::size_t>(Mesh.FirstIndex) * 2, Mesh.TriangleCount * 3, 2);
            for (unsigned Triangle = 0; Triangle < Mesh.TriangleCount; ++Triangle)
            {
                std::array<Point_t, 3> Points;
                for (unsigned Corner = 0; Corner < 3; ++Corner)
                {
                    const auto LocalIndex = Indices.At<std::uint16_t>(static_cast<std::size_t>(Mesh.FirstIndex) + Triangle * 3 + Corner);
                    const std::int64_t VertexIndex = static_cast<std::int64_t>(Sort.VertexOffset) + LocalIndex;
                    if (VertexIndex < 0 || static_cast<std::uint64_t>(VertexIndex) >= VertexData.Data().size() / Stride)
                        VertexData.Fail("render vertex index outside lump bounds");
                    const auto Position = VertexData.Read<std::uint32_t>(static_cast<std::size_t>(VertexIndex) * Stride);
                    Points[Corner] = Point(Positions.At<std::array<float, 3>>(Position).data());
                }
                const auto FaceNormal = Cross(Subtract(Points[1], Points[0]), Subtract(Points[2], Points[0]));
                const double Length = std::sqrt(Dot(FaceNormal, FaceNormal));
                if (Length == 0 || std::abs(FaceNormal[2]) / Length < MinimumNormal)
                    continue;
                if (FaceNormal[2] < 0)
                    std::swap(Points[1], Points[2]);
                RenderTriangle_t Output;
                Output.Material = Ids[Sort.Texture];
                Output.Mesh = static_cast<int>(Index);
                for (unsigned Corner = 0; Corner < 3; ++Corner)
                    Output.Vertices[Corner] = {static_cast<float>(Points[Corner][0]), static_cast<float>(Points[Corner][1]),
                                               static_cast<float>(Points[Corner][2])};
                m_Map.m_RenderTriangles.push_back(Output);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // Purpose: Traverse only worldspawn collision cells, deduplicating the first
    //          occurrence of each primitive exactly as the verified exporter does.
    //-----------------------------------------------------------------------------
    void LoadCollision(bool TitanCollision)
    {
        const auto GridData = Lump(Grid).At<Grid_t>(0);
        const auto Cells = Lump(GridCells), Sets = Lump(GeoSets), PrimitiveData = Lump(Primitives);
        const auto Contents = Lump(UniqueContents), Positions = Lump(Vertices);
        if (GridData.Count[0] <= 0 || GridData.Count[1] <= 0)
            Cells.Fail("invalid world collision grid dimensions");
        const std::size_t CellCount = static_cast<std::size_t>(GridData.Count[0]) * GridData.Count[1];
        Cells.Check(0, CellCount, sizeof(GridCell_t));
        std::unordered_set<std::uint32_t> Seen;
        std::vector<std::uint32_t> Included;
        constexpr std::uint32_t PhysicalContents = 0xB; // Solid, window and grate.
        const std::uint32_t Mask = PhysicalContents | (TitanCollision ? 0x00200000 : 0x00020000);
        auto Include = [&](std::uint32_t Primitive)
        {
            // R2: type in high byte, index in middle 16 bits, contents in low byte.
            if (!Seen.insert(Primitive >> 8).second)
                return;
            if (Contents.At<std::uint32_t>(Primitive & 0xFF) & Mask)
                Included.push_back(Primitive);
        };
        for (std::size_t CellIndex = 0; CellIndex < CellCount; ++CellIndex)
        {
            const auto Cell = Cells.At<GridCell_t>(CellIndex);
            for (unsigned Offset = 0; Offset < Cell.GeoSetCount; ++Offset)
            {
                const auto Set = Sets.At<GeoSet_t>(static_cast<unsigned>(Cell.FirstGeoSet) + Offset);
                if (Set.PrimitiveCount == 1)
                    Include(Set.Primitive);
                else
                    for (unsigned Offset = 0; Offset < Set.PrimitiveCount; ++Offset)
                        Include(PrimitiveData.At<std::uint32_t>(((Set.Primitive >> 8) & 0xFFFF) + Offset));
            }
        }
        // Raster tie-breaking must retain exporter order: tricolls, brushes, props.
        std::stable_sort(Included.begin(), Included.end(), [](std::uint32_t A, std::uint32_t B)
        {
            const auto Rank = [](std::uint32_t Primitive)
            {
                const unsigned Type = Primitive >> 24;
                return Type == 0x40 ? 0 : Type == 0 ? 1 : Type == 0x60 ? 2 : 3;
            };
            return Rank(A) < Rank(B);
        });
        const auto Headers = Lump(TricollHeaders), Triangles = Lump(TricollTriangles);
        for (const auto Primitive : Included)
        {
            const unsigned Type = Primitive >> 24, Index = (Primitive >> 8) & 0xFFFF;
            if (Type == 0)
            {
                // Clip volumes block hulls but can only share independently physical support.
                const bool ClipOnly = (Contents.At<std::uint32_t>(Primitive & 0xFF) & PhysicalContents) == 0;
                AddBrush(GridData, Index, m_Map.m_Geometry, ClipOnly);
                ++m_Map.m_Statistics.BrushCount;
            }
            else if (Type == 0x40)
            {
                const auto Header = Headers.At<TricollHeader_t>(Index);
                if (Header.VertexCount < 0 || Header.TriangleCount < 0 || Header.FirstVertex < 0 || Header.FirstTriangle < 0 || Header.Texture < 0 ||
                    static_cast<std::size_t>(Header.Texture) >= m_Materials.size())
                    Headers.Fail("invalid tricoll header");
                Positions.Check(static_cast<std::size_t>(Header.FirstVertex) * 12, Header.VertexCount, 12);
                Triangles.Check(static_cast<std::size_t>(Header.FirstTriangle) * 4, Header.TriangleCount, 4);
                const bool Unwalkable =
                    (Header.TextureFlags & 0x6) != 0 || m_Materials[Header.Texture] == "world/thaw/foliage/thaw_eggplant_01";
                for (int Triangle = 0; Triangle < Header.TriangleCount; ++Triangle)
                {
                    const auto Packed = Triangles.At<std::uint32_t>(static_cast<std::size_t>(Header.FirstTriangle) + Triangle);
                    const unsigned A = Packed & 0x3FF;
                    const unsigned Local[3] = {A, A + ((Packed >> 10) & 0x7F), A + ((Packed >> 17) & 0x7F)};
                    int Output[3];
                    for (unsigned Corner = 0; Corner < 3; ++Corner)
                    {
                        if (Local[Corner] >= static_cast<unsigned>(Header.VertexCount))
                            Headers.Fail("tricoll vertex outside local hull");
                        const unsigned VertexIndex = Header.FirstVertex + Local[Corner];
                        auto Found = m_VertexMap.find(VertexIndex);
                        if (Found == m_VertexMap.end())
                            Found =
                                m_VertexMap
                                    .emplace(VertexIndex, AddVertex(m_Map.m_Geometry, Point(Positions.At<std::array<float, 3>>(VertexIndex).data())))
                                    .first;
                        Output[Corner] = Found->second;
                    }
                    AddTriangle(m_Map.m_Geometry, Output[0], Output[1], Output[2], Unwalkable);
                }
                ++m_Map.m_Statistics.TriangleCollisionCount;
            }
            else if (Type == 0x60)
            {
                if (!m_PropsLoaded)
                {
                    LoadStaticProps(m_Models, m_Props);
                    m_PropsLoaded = true;
                }
                if (Index >= m_Props.size())
                    PrimitiveData.Fail("collision primitive references missing static prop " + std::to_string(Index));
                const auto& Prop = m_Props[Index];
                if (Prop.Model >= m_Models.size())
                    PrimitiveData.Fail("static prop references missing model dictionary entry");
                auto Found = m_ModelCache.find(Prop.Model);
                if (Found == m_ModelCache.end())
                {
                    try
                    {
                        Found = m_ModelCache.emplace(Prop.Model, LoadModelCollision(m_Models[Prop.Model])).first;
                    }
                    catch (const std::exception& Error)
                    {
                        throw std::runtime_error(m_Models[Prop.Model] + ": " + Error.what());
                    }
                    if (!Found->second.empty())
                        ++m_Map.m_Statistics.StaticPropModelCount;
                }
                ++m_Map.m_Statistics.StaticPropCount;
                if (Found->second.empty())
                {
                    ++m_Map.m_Statistics.NonCollidingPropCount;
                    continue;
                }
                AddStaticProp(Prop, Found->second, m_Map.m_Geometry);
            }
            else
                PrimitiveData.Fail("unsupported collision primitive type " + std::to_string(Type));
        }
    }

    std::string m_Path;
    ReadFileFn m_ReadFile;
    std::string m_File;
    std::array<LumpHeader_t, 128> m_Headers{};
    std::array<std::string, 128> m_External;
    std::array<std::string, 128> m_LumpPaths;
    std::array<std::string_view, 128> m_LumpData{};
    std::array<bool, 128> m_Loaded{};
    CNavMeshMap& m_Map;
    std::vector<std::string> m_Materials;
    std::vector<std::string> m_Models;
    std::vector<StaticProp_t> m_Props;
    std::unordered_map<unsigned, ModelFaces_t> m_ModelCache;
    std::unordered_map<unsigned, int> m_VertexMap;
    bool m_PropsLoaded = false;
};

//-----------------------------------------------------------------------------
// Purpose: Build native Source-XYZ collision and authored input using mounted
//          BSP, entity partition and MDL assets. No extraction or staging files.
//-----------------------------------------------------------------------------
CNavMeshMap::CNavMeshMap(const char* MapName, bool TitanCollision, ReadFileFn ReadFile)
{
    if (!MapName || !*MapName || !ReadFile)
        throw std::invalid_argument("CNavMeshMap requires a map name and file reader");
    for (const char* Character = MapName; *Character; ++Character)
        if (!((*Character >= 'a' && *Character <= 'z') || (*Character >= 'A' && *Character <= 'Z') || (*Character >= '0' && *Character <= '9') ||
              *Character == '_' || *Character == '-'))
            throw std::invalid_argument("invalid BSP map name");
    const std::string MapPath = std::string("maps/") + MapName;
    try
    {
        CNavMeshBspReader Reader(MapPath + ".bsp", ReadFile, *this);
        Reader.Load(TitanCollision);
    }
    catch (const std::exception& Error)
    {
        throw std::runtime_error(MapPath + ".bsp: " + Error.what());
    }
}
