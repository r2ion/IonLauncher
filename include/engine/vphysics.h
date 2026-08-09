#pragma once

#include "appframework/IAppSystem.h"
#include "cmodel.h"
#include "gametrace.h"
#include "vphysics/vphysics_interface.h"
#include "mathlib/vector.h"

#include <cstddef>
#include <cstdint>

inline constexpr char VPHYSICS_INTERFACE_VERSION[] = "VPhysics031";
inline constexpr char VPHYSICS_COLLISION_INTERFACE_VERSION[] = "VPhysicsCollision007";
inline constexpr char VPHYSICS_SURFACEPROPS_INTERFACE_VERSION[] = "VPhysicsSurfaceProps001";

class CPhysCollide;
class CPhysConvex;
class CPhysPolysoup;
class CPolyhedron;
class ICollisionQuery;
class ISaveRestoreOps;
class IConvexInfo;
class IPhysicsCollision;
class IPhysicsCollisionSet;
class IPhysicsEnvironment;
class IPhysicsObjectPairHash;
class IVPhysicsKeyParser;
struct convertconvexparams_t;
struct surfacedata_t;
struct virtualmeshparams_t;
struct virtualmeshlist_t;

struct surfacephysicsparams_t
{
    float friction;
    float elasticity;
    float density;
    float thickness;
    float dampening;
};

static_assert(sizeof(surfacephysicsparams_t) == 20);
static_assert(offsetof(surfacephysicsparams_t, dampening) == 16);

struct vcollide_t
{
    std::uint16_t solidCount : 15;
    std::uint16_t isPacked : 1;
    std::uint16_t descSize;
    std::uint32_t reserved04;
    CPhysCollide** solids;
    char* pKeyValues;
    void* pUserData;
};

static_assert(sizeof(vcollide_t) == 32);
static_assert(offsetof(vcollide_t, descSize) == 2);
static_assert(offsetof(vcollide_t, solids) == 8);
static_assert(offsetof(vcollide_t, pKeyValues) == 16);
static_assert(offsetof(vcollide_t, pUserData) == 24);

struct truncatedcone_t
{
    Vector3 origin;
    Vector3 normal;
    float height;
    float theta;
};

static_assert(sizeof(truncatedcone_t) == 32);

class IPhysics : public IAppSystem
{
  public:
    virtual IPhysicsEnvironment* CreateEnvironment() = 0;                                              // 8
    virtual void DestroyEnvironment(IPhysicsEnvironment* environment) = 0;                             // 9
    virtual IPhysicsEnvironment* GetActiveEnvironmentByIndex(int index) = 0;                           // 10
    virtual IPhysicsObjectPairHash* CreateObjectPairHash() = 0;                                        // 11
    virtual void DestroyObjectPairHash(IPhysicsObjectPairHash* hash) = 0;                              // 12
    virtual IPhysicsCollisionSet* FindOrCreateCollisionSet(std::uint32_t id, int maxElementCount) = 0; // 13
    virtual IPhysicsCollisionSet* FindCollisionSet(std::uint32_t id) = 0;                              // 14
    virtual void DestroyAllCollisionSets() = 0;                                                        // 15
};

class IPhysicsCollision
{
  public:
    virtual ~IPhysicsCollision() = default;                                             // 0
    virtual CPhysConvex* ConvexFromVerts(Vector3** verts, int vertCount) = 0;           // 1
    virtual CPhysConvex* ConvexFromPlanes(float* planes, int planeCount) = 0;           // 2
    virtual float ConvexVolume(CPhysConvex* convex) = 0;                                // 3
    virtual float ConvexSurfaceArea(CPhysConvex* convex) = 0;                           // 4
    virtual void SetConvexGameData(CPhysConvex* convex, std::uint32_t gameData) = 0;    // 5
    virtual void ConvexFree(CPhysConvex* convex) = 0;                                   // 6
    virtual CPhysConvex* BBoxToConvex(const Vector3& mins, const Vector3& maxs) = 0;    // 7
    virtual CPhysConvex* ConvexFromConvexPolyhedron(const CPolyhedron& polyhedron) = 0; // 8
    virtual void ConvexesFromConvexPolygon(const Vector3& normal, const Vector3* points, int pointCount,
                                           CPhysConvex** output) = 0;                                                                        // 9
    virtual CPhysPolysoup* PolysoupCreate() = 0;                                                                                             // 10
    virtual void PolysoupDestroy(CPhysPolysoup* soup) = 0;                                                                                   // 11
    virtual void PolysoupAddTriangle(CPhysPolysoup* soup, const Vector3& a, const Vector3& b, const Vector3& c, int materialIndex7Bits) = 0; // 12
    virtual CPhysCollide* ConvertPolysoupToCollide(CPhysPolysoup* soup) = 0;                                                                 // 13
    virtual std::uint32_t CountConvexesWithinVertexLimit(CPhysConvex* const* convexes, int convexCount) = 0;                               // 14
    virtual CPhysCollide* ConvertConvexToCollide(CPhysConvex** convexes, int convexCount) = 0;                                               // 15
    virtual CPhysCollide* ConvertConvexToCollideParams(CPhysConvex** convexes, int convexCount,
                                                       const convertconvexparams_t& params) = 0;                                              // 16
    virtual void DestroyCollide(CPhysCollide* collide) = 0;                                                                                   // 17
    virtual int CollideSize(CPhysCollide* collide) = 0;                                                                                       // 18
    virtual int CollideWrite(char* destination, CPhysCollide* collide, bool swap) = 0;                                                        // 19
    virtual CPhysCollide* UnserializeCollide(char* buffer, int size, int index) = 0;                                                          // 20
    virtual float CollideVolume(CPhysCollide* collide) = 0;                                                                                   // 21
    virtual float CollideSurfaceArea(CPhysCollide* collide) = 0;                                                                              // 22
    virtual Vector3 CollideGetExtent(const CPhysCollide* collide, const Vector3& origin, const QAngle& angles, const Vector3& direction) = 0; // 23
    virtual void CollideGetAABB(Vector3* mins, Vector3* maxs, const CPhysCollide* collide, const Vector3& origin, const QAngle& angles) = 0;  // 24
    virtual void CollideGetMassCenter(CPhysCollide* collide, Vector3* massCenter) = 0;                                                        // 25
    virtual void CollideSetMassCenter(CPhysCollide* collide, const Vector3& massCenter) = 0;                                                  // 26
    virtual Vector3 CollideGetOrthographicAreas(const CPhysCollide* collide) = 0;                                                             // 27
    virtual void CollideSetOrthographicAreas(CPhysCollide* collide, const Vector3& areas) = 0;                                                // 28
    virtual int CollideIndex(const CPhysCollide* collide) = 0;                                                                                // 29
    virtual CPhysCollide* BBoxToCollide(const Vector3& mins, const Vector3& maxs) = 0;                                                        // 30
    virtual int GetConvexesUsedInCollideable(const CPhysCollide* collide, CPhysConvex** output,
                                             int outputLimit) = 0;      // 31
    virtual std::uint32_t GetConvexCount(const CPhysCollide* collide) = 0; // 32
    virtual void TraceBox(const Ray_t& ray, const CPhysCollide* collide, const Vector3& collideOrigin, const QAngle& collideAngles,
                          trace_t* trace) = 0; // 33
    virtual void TraceBox(const Vector3& start, const Vector3& end, const Vector3& mins, const Vector3& maxs, const CPhysCollide* collide,
                          const Vector3& collideOrigin, const QAngle& collideAngles, trace_t* trace) = 0; // 34
    virtual void TraceBox(const Ray_t& ray, std::uint32_t contentsMask, IConvexInfo* convexInfo, const CPhysCollide* collide,
                          const Vector3& collideOrigin, const QAngle& collideAngles,
                          trace_t* trace) = 0; // 35
    virtual void TraceBox(const Vector3& start, const Vector3& end, const Vector3& mins, const Vector3& maxs, const CPhysCollide* collide,
                          const Vector3& collideOrigin, const QAngle& collideAngles, float scale,
                          trace_t* trace) = 0; // 36
    virtual void TraceBox(const Ray_t& ray, const CPhysCollide* collide, const Vector3& collideOrigin, const QAngle& collideAngles,
                          float scale, trace_t* trace) = 0; // 37
    virtual void TraceBox(const Ray_t& ray, std::uint32_t contentsMask, IConvexInfo* convexInfo, const CPhysCollide* collide,
                          const Vector3& collideOrigin, const QAngle& collideAngles, float scale,
                          trace_t* trace) = 0; // 38
    virtual void TraceBox(const Ray_t& ray, std::uint32_t contentsMask, IConvexInfo* convexInfo, const CPhysCollide* collide,
                          const matrix3x4_t& collideTransform, float scale,
                          trace_t* trace) = 0; // 39
    virtual void TraceCollide(const Vector3& start, const Vector3& end, const CPhysCollide* sweepCollide, const QAngle& sweepAngles,
                              const CPhysCollide* collide, const Vector3& collideOrigin, const QAngle& collideAngles,
                              trace_t* trace) = 0; // 40
    virtual bool IsBoxIntersectingCone(const Vector3& boxMins, const Vector3& boxMaxs,
                                       const truncatedcone_t& cone) = 0;                           // 41
    virtual void VCollideLoad(vcollide_t* output, int solidCount, const char* buffer) = 0;         // 42
    virtual void VCollideUnload(vcollide_t* collide) = 0;                                          // 43
    virtual IVPhysicsKeyParser* VPhysicsKeyParserCreate(const vcollide_t* collide) = 0;            // 44
    virtual IVPhysicsKeyParser* VPhysicsKeyParserCreate(const char* keyData) = 0;                  // 45
    virtual void VPhysicsKeyParserDestroy(IVPhysicsKeyParser* parser) = 0;                         // 46
    virtual int CreateDebugMesh(const CPhysCollide* collisionModel, Vector3** outputVertices) = 0; // 47
    virtual void DestroyDebugMesh(int vertexCount, Vector3* vertices) = 0;                         // 48
    virtual std::uint32_t GetCollideVertices(const CPhysCollide* collide, Vector3* output,
                                             int outputLimit) = 0;                              // 49
    virtual ICollisionQuery* CreateQueryModel(CPhysCollide* collide) = 0;                       // 50
    virtual void DestroyQueryModel(ICollisionQuery* query) = 0;                                 // 51
    virtual IPhysicsCollision* ThreadContextCreate() = 0;                                       // 52
    virtual void ThreadContextDestroy(IPhysicsCollision* context) = 0;                          // 53
    virtual int BuildVirtualMeshHull(const virtualmeshlist_t& mesh, std::uint32_t* outputHull) = 0;   // 54
    virtual CPhysCollide* CreateVirtualMesh(const virtualmeshparams_t& params) = 0;                   // 55
    virtual bool GetBBoxCacheSize(int* cachedSize, int* cachedCount) = 0;                       // 56
    virtual CPolyhedron* PolyhedronFromConvex(CPhysConvex* convex, bool useTempPolyhedron) = 0; // 57
    virtual void OutputDebugInfo(const CPhysCollide* collide) = 0;                              // 58
    virtual std::uint32_t ReadStat(int statId) = 0;                                             // 59
    virtual float CollideGetBoundingRadius(const CPhysCollide* collide) = 0;                         // 60
    virtual void* AllocateVCollideUserData(vcollide_t* collide, std::size_t size) = 0;          // 61
    virtual void FreeVCollideUserData(vcollide_t* collide) = 0;                                 // 62
    virtual int WarmVCollideCaches(vcollide_t* collide) = 0;                                        // 63
    virtual bool TraceBox(const Ray_t& ray, const CPhysCollide* collide, trace_t* trace) = 0;         // 64
};

class IPhysicsSurfaceProps
{
  public:
    virtual ~IPhysicsSurfaceProps() = default;                                                                                    // 0
    virtual int ParseSurfaceData(const char* filename, const char* text) = 0;                                                     // 1
    virtual int SurfacePropCount() const = 0;                                                                                     // 2
    virtual int GetSurfaceIndex(const char* surfaceName) const = 0;                                                               // 3
    virtual void GetPhysicsProperties(int index, float* density, float* thickness, float* friction, float* elasticity) const = 0; // 4
    virtual surfacedata_t* GetSurfaceData(int index) = 0;                                                                         // 5
    virtual const char* GetString(std::uint16_t stringIndex) const = 0;                                                           // 6
    virtual const char* GetPropName(int index) const = 0;                                                                         // 7
    virtual void SetWorldMaterialIndexTable(int* map, int size) = 0;                                                              // 8
    virtual void GetPhysicsParameters(int index, surfacephysicsparams_t* output) const = 0;                                       // 9
    virtual ISaveRestoreOps* GetSurfaceIndexSaveRestoreOps() = 0;                                                      // 10
    virtual surfacedata_t* GetSurfaceDataFromMaterialIndex(int materialIndex) = 0;                                     // 11
    virtual int GetSurfaceIndex(const surfacedata_t* surfaceData) const = 0;                                           // 12
    virtual void* GetSurfaceNameTable() = 0;                                                                           // 13
    virtual int GetSurfaceIndexFromMaterialIndex(int materialIndex) const = 0;                                         // 14
    virtual const char* GetMaterialIndexName(int materialIndex) const = 0;                                             // 15
};

static_assert(sizeof(IPhysics) == sizeof(void*));
static_assert(sizeof(IPhysicsCollision) == sizeof(void*));
static_assert(sizeof(IPhysicsSurfaceProps) == sizeof(void*));
