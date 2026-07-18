#pragma once

#include "core/appsystem.h"

#include <cstdint>

using MDLHandle_t = uint16_t;
inline constexpr MDLHandle_t InvalidMDLHandle = static_cast<MDLHandle_t>(~0u);

struct StudioData_t;
struct StudioHardwareData_t;
struct VCollide_t;
struct studiohdr_t;
struct vertexColorFileHeader_t;
struct vertexFileHeader_t;
struct virtualmodel_t;

inline constexpr char MDLCacheInterfaceVersion[] = "MDLCache004";

//-----------------------------------------------------------------------------
class IMDLCache : public IAppSystem
{
  public:
    virtual MDLHandle_t FindMDL(const char* pMdlRelativePath) = 0;                                                // 8
    virtual void AddRef(MDLHandle_t handle) = 0;                                                                  // 9
    virtual int Release(MDLHandle_t handle) = 0;                                                                  // 10
    virtual studiohdr_t* GetStudioHdr(MDLHandle_t handle) = 0;                                                    // 11
    virtual StudioHardwareData_t* GetHardwareData(MDLHandle_t handle) = 0;                                        // 12
    virtual VCollide_t* GetVCollide(MDLHandle_t handle) = 0;                                                      // 13
    virtual studiohdr_t* GetRawStudioHdr(MDLHandle_t handle) = 0;                                                 // 14
    virtual void* GetPhysicsGeometry(MDLHandle_t handle) = 0;                                                     // 15
    virtual void* CreatePhysicsGeometry(VCollide_t* pVCollide, bool useDefaults, const void* pCollisionData) = 0; // 16
    virtual void DestroyPhysicsGeometry(void* pGeometry) = 0;                                                     // 17
    virtual virtualmodel_t* GetVirtualModel(MDLHandle_t handle) = 0;                                              // 18
    virtual int GetAutoplayList(MDLHandle_t handle, uint16_t** ppOut) = 0;                                        // 19
    virtual vertexFileHeader_t* GetVertexData(MDLHandle_t handle) = 0;                                            // 20
    virtual vertexColorFileHeader_t* GetColorData(MDLHandle_t handle) = 0;                                        // 21
    virtual void TouchAllData(MDLHandle_t handle) = 0;                                                            // 22
    virtual void UnloadAuxiliaryData(MDLHandle_t handle) = 0;                                                     // 23
    virtual void UnloadRawStudioHdr(MDLHandle_t handle) = 0;                                                      // 24
    virtual void SetUserData(MDLHandle_t handle, void* pUserData) = 0;                                            // 25
    virtual void* GetUserData(MDLHandle_t handle) = 0;                                                            // 26
    virtual bool IsErrorModel(MDLHandle_t handle) = 0;                                                            // 27

    virtual void Unknown28() = 0; // 28
    virtual void Unknown29() = 0; // 29

    virtual void FlushCache() = 0;                                                                         // 30
    virtual void FlushCacheAsync() = 0;                                                                    // 31
    virtual void FlushCacheByName(const char* pNameSubstring) = 0;                                         // 32
    virtual void Unknown33() = 0;                                                                          // 33, nullsub
    virtual const char* GetModelName(MDLHandle_t handle) = 0;                                              // 34
    virtual virtualmodel_t* GetVirtualModelFast(const studiohdr_t* pStudioHeader, MDLHandle_t handle) = 0; // 35
    virtual void ReleaseStudioAsset(void* pAsset) = 0;                                                     // 36
    virtual void StudioRenderLoadMaterials(MDLHandle_t handle) = 0;                                       // 37
    virtual void StudioRenderUnloadMaterials(MDLHandle_t handle) = 0;                                     // 38
    virtual bool GetVCollideSize(MDLHandle_t handle, int* pSize) = 0;                                      // 39
    virtual int* GetFrameUnlockCounterPtr() = 0;                                                           // 40
    virtual vertexFileHeader_t* GetVertexDataForStudioHdr(const studiohdr_t* pStudioHeader) = 0;           // 41
    virtual vertexColorFileHeader_t* GetColorDataForStudioHdr(const studiohdr_t* pStudioHeader) = 0;       // 42
};
