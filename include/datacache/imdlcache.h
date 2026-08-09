#pragma once

#include "appframework/IAppSystem.h"
#include "studio.h"

#include <cstddef>
#include <cstdint>

using MDLHandle_t = std::uint16_t;
inline constexpr MDLHandle_t InvalidMDLHandle = static_cast<MDLHandle_t>(~0u);
inline constexpr std::size_t MDLCacheVTableSlotCount = 43;

enum MDLCacheDataType_t
{
    MDLCACHE_NONE = -1,
    MDLCACHE_STUDIOHDR = 0,
    MDLCACHE_STUDIOHWDATA,
    MDLCACHE_VCOLLIDE,
    MDLCACHE_ANIMBLOCK,
    MDLCACHE_VIRTUALMODEL,
    MDLCACHE_VERTEXES,
    MDLCACHE_DECODEDANIMBLOCK,
};

class IMDLCacheNotify
{
  public:
    virtual ~IMDLCacheNotify() = default;
    virtual void OnDataLoaded(MDLCacheDataType_t type, MDLHandle_t handle) = 0;
    virtual void OnDataUnloaded(MDLCacheDataType_t type, MDLHandle_t handle) = 0;
};

struct vcollide_t;
struct StudioAssetLoadRequest_t;

class CStudioAnim;
class CStudioColors;
class CStudioPhysicsGeoms;
class CStudioVCollide;
class CStudioVertices;

inline constexpr char MDLCacheInterfaceVersion[] = "MDLCache004";

//-----------------------------------------------------------------------------
class IMDLCache : public IAppSystem
{
  public:
    virtual void SetCacheNotify(IMDLCacheNotify* pNotify) = 0;                                     // 8
    virtual MDLHandle_t FindMDL(const char* pMdlRelativePath) = 0;                                 // 9
    virtual int AddRef(MDLHandle_t handle) = 0;                                                    // 10
    virtual int Release(MDLHandle_t handle) = 0;                                                   // 11
    virtual int GetRef(MDLHandle_t handle) = 0;                                                    // 12
    virtual studiohdr_t* GetStudioHdr(MDLHandle_t handle) = 0;                                     // 13
    virtual studiohwdata_t* GetHardwareData(MDLHandle_t handle) = 0;                               // 14
    virtual vcollide_t* GetVCollide(MDLHandle_t handle) = 0;                                       // 15
    virtual std::uint8_t* GetAnimBlock(MDLHandle_t handle, int block) = 0;                          // 16
    virtual bool ReturnTrue17() = 0;                                                               // 17
    virtual virtualmodel_t* GetVirtualModel(MDLHandle_t handle) = 0;                               // 18
    virtual int GetAutoplayList(MDLHandle_t handle, std::uint16_t** ppOut) = 0;                     // 19
    virtual vertexFileHeader_t* GetVertexData(MDLHandle_t handle) = 0;                             // 20
    virtual vertexColorFileHeader_t* GetColorData(MDLHandle_t handle) = 0;                         // 21
    virtual void TouchAllData(MDLHandle_t handle) = 0;                                             // 22
    virtual void SetUserData(MDLHandle_t handle, void* pUserData) = 0;                             // 23
    virtual void* GetUserData(MDLHandle_t handle) = 0;                                             // 24
    virtual bool IsErrorModel(MDLHandle_t handle) = 0;                                             // 25
    virtual void FlushCache(std::uint32_t flags) = 0;                                              // 26
    virtual void FlushCacheByHandle(MDLHandle_t handle, std::uint32_t flags) = 0;                  // 27
    virtual const char* GetModelName(MDLHandle_t handle) = 0;                                      // 28
    virtual virtualmodel_t* GetVirtualModelFast(const studiohdr_t* pStudioHeader,
                                                MDLHandle_t handle) = 0;                            // 29
    virtual void LoadVertexDataAsset(StudioAssetLoadRequest_t* pRequest) = 0;                       // 30
    virtual void ReleaseVertexDataAsset(CStudioVertices** ppAsset) = 0;                             // 31
    virtual void LoadAnimDataAsset(StudioAssetLoadRequest_t* pRequest) = 0;                         // 32
    virtual void ReleaseAnimDataAsset(CStudioAnim* pAsset) = 0;                                    // 33
    virtual CStudioVCollide* LoadVCollideAsset(StudioAssetLoadRequest_t* pRequest) = 0;             // 34
    virtual void ReleaseVCollideAsset(CStudioVCollide* pAsset) = 0;                                // 35
    virtual void ReleasePhysicsGeometryAsset(CStudioPhysicsGeoms* pAsset) = 0;                     // 36
    virtual void LoadColorDataAsset(StudioAssetLoadRequest_t* pRequest) = 0;                        // 37
    virtual void ReleaseColorDataAsset(CStudioColors** ppAsset) = 0;                               // 38
    virtual void CompleteStudioAssetLoad(StudioAssetLoadRequest_t* pRequest) = 0;                   // 39
    virtual void ReleaseStudioAsset(void* pAssetStorage) = 0;                                      // 40
    virtual void DumpCache() = 0;                                                                  // 41
    virtual void CancelPendingAssetLoads() = 0;                                                    // 42
};

static_assert(sizeof(MDLHandle_t) == 2);
static_assert(sizeof(IMDLCacheNotify) == sizeof(void*));
static_assert(sizeof(IMDLCache) == sizeof(void*));
