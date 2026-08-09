#pragma once

#include "appframework/IAppSystem.h"
#include "studio.h"

#include <cstddef>
#include <cstdint>

using MDLHandle_t = std::uint16_t;
inline constexpr MDLHandle_t InvalidMDLHandle = static_cast<MDLHandle_t>(~0u);
inline constexpr std::size_t MDLCacheVTableSlotCount = 43;

struct vcollide_t;

inline constexpr char MDLCacheInterfaceVersion[] = "MDLCache004";

//-----------------------------------------------------------------------------
class IMDLCache : public IAppSystem
{
  public:
    virtual MDLHandle_t FindMDL(const char* pMdlRelativePath) = 0;                                                // 8
    virtual void AddRef(MDLHandle_t handle) = 0;                                                                  // 9
    virtual int Release(MDLHandle_t handle) = 0;                                                                  // 10
    virtual studiohdr_t* GetStudioHdr(MDLHandle_t handle) = 0;                                                    // 11
    virtual studiohwdata_t* GetHardwareData(MDLHandle_t handle) = 0;                                              // 12
    virtual vcollide_t* GetVCollide(MDLHandle_t handle) = 0;                                                      // 13
    virtual studiohdr_t* GetRawStudioHdr(MDLHandle_t handle) = 0;                                                 // 14
    virtual void* GetPhysicsGeometry(MDLHandle_t handle) = 0;                                                     // 15
    virtual void* CreatePhysicsGeometry(vcollide_t* pVCollide, bool useDefaults, const void* pCollisionData) = 0; // 16
    virtual void DestroyPhysicsGeometry(void* pGeometry) = 0;                                                     // 17
    virtual virtualmodel_t* GetVirtualModel(MDLHandle_t handle) = 0;                                              // 18
    virtual int GetAutoplayList(MDLHandle_t handle, std::uint16_t** ppOut) = 0;                                   // 19
    virtual vertexFileHeader_t* GetVertexData(MDLHandle_t handle) = 0;                                            // 20
    virtual vertexColorFileHeader_t* GetColorData(MDLHandle_t handle) = 0;                                        // 21
    virtual void TouchAllData(MDLHandle_t handle) = 0;                                                            // 22
    virtual void UnloadAuxiliaryData(MDLHandle_t handle) = 0;                                                     // 23
    virtual void UnloadRawStudioHdr(MDLHandle_t handle) = 0;                                                      // 24
    virtual void SetUserData(MDLHandle_t handle, void* pUserData) = 0;                                            // 25
    virtual void* GetUserData(MDLHandle_t handle) = 0;                                                            // 26
    virtual bool IsErrorModel(MDLHandle_t handle) = 0;                                                            // 27
    virtual void Extension28() = 0;                                                                               // 28
    virtual void Extension29() = 0;                                                                               // 29
    virtual void FlushCache() = 0;                                                                                // 30
    virtual void FlushCacheAsync() = 0;                                                                           // 31
    virtual void FlushCacheByName(const char* pNameSubstring) = 0;                                                // 32
    virtual void Extension33() = 0;                                                                               // 33
    virtual const char* GetModelName(MDLHandle_t handle) = 0;                                                     // 34
    virtual virtualmodel_t* GetVirtualModelFast(const studiohdr_t* pStudioHeader, MDLHandle_t handle) = 0;        // 35
    virtual void ReleaseStudioAsset(void* pAsset) = 0;                                                            // 36
    virtual void StudioRenderLoadMaterials(MDLHandle_t handle) = 0;                                               // 37
    virtual void StudioRenderUnloadMaterials(MDLHandle_t handle) = 0;                                             // 38
    virtual bool GetVCollideSize(MDLHandle_t handle, int* pSize) = 0;                                             // 39
    virtual int* GetFrameUnlockCounterPtr() = 0;                                                                  // 40
    virtual vertexFileHeader_t* GetVertexDataForStudioHdr(const studiohdr_t* pStudioHeader) = 0;                  // 41
    virtual vertexColorFileHeader_t* GetColorDataForStudioHdr(const studiohdr_t* pStudioHeader) = 0;              // 42
};

static_assert(sizeof(MDLHandle_t) == 2);
static_assert(sizeof(IMDLCache) == sizeof(void*));
