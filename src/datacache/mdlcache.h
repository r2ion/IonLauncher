#pragma once

#include "datacache/imdlcache.h"
#include "engine/vphysics.h"
#include "studio.h"
#include "tier1/refcount.h"

#include <cstddef>
#include <type_traits>

struct StudioAssetLoadRequest_t
{
    void* m_pOpaqueContext;
    void* m_pOutput;
    const void* m_pAssetDescriptor;
    void* m_pLoadedData;
};

static_assert(sizeof(StudioAssetLoadRequest_t) == 0x20);
static_assert(offsetof(StudioAssetLoadRequest_t, m_pOpaqueContext) == 0x0);
static_assert(offsetof(StudioAssetLoadRequest_t, m_pOutput) == 0x8);
static_assert(offsetof(StudioAssetLoadRequest_t, m_pAssetDescriptor) == 0x10);
static_assert(offsetof(StudioAssetLoadRequest_t, m_pLoadedData) == 0x18);

template <typename T>
class CStudioAssetRef : public CRefCounted<>
{
  public:
    ~CStudioAssetRef() override;

    T* m_pAsset;
    bool m_bResolveAllocationBase;
};

class CStudioAnim : public CStudioAssetRef<std::uint8_t>
{
};

class CStudioVertices : public CStudioAssetRef<vertexFileHeader_t>
{
};

class CStudioColors : public CStudioAssetRef<vertexColorFileHeader_t>
{
};

class CStudioHdrRef : public CStudioAssetRef<studiohdr_t>
{
};

class CStudioPhysicsGeoms : public CRefCounted<>
{
  public:
    void* m_pGeometryDataDescriptor;
    std::int32_t m_nUnknown18;
    std::int16_t m_nUnknown1C;
    std::int16_t m_nUnknown1E;
};

class CStudioVCollide : public CRefCounted<>
{
  public:
    ~CStudioVCollide() override;

    vcollide_t m_VCollide;
};

static_assert(sizeof(CStudioAssetRef<std::uint8_t>) == 0x20);
static_assert(offsetof(CStudioAssetRef<std::uint8_t>, m_pAsset) == 0x10);
static_assert(offsetof(CStudioAssetRef<std::uint8_t>, m_bResolveAllocationBase) == 0x18);
static_assert(sizeof(CStudioAnim) == 0x20);
static_assert(sizeof(CStudioVertices) == 0x20);
static_assert(sizeof(CStudioColors) == 0x20);
static_assert(sizeof(CStudioHdrRef) == 0x20);
static_assert(std::is_base_of_v<CRefCounted<>, CStudioAssetRef<std::uint8_t>>);
static_assert(std::is_base_of_v<CStudioAssetRef<std::uint8_t>, CStudioAnim>);
static_assert(std::is_base_of_v<CStudioAssetRef<vertexFileHeader_t>, CStudioVertices>);
static_assert(std::is_base_of_v<CStudioAssetRef<vertexColorFileHeader_t>, CStudioColors>);
static_assert(std::is_base_of_v<CStudioAssetRef<studiohdr_t>, CStudioHdrRef>);
static_assert(sizeof(CStudioPhysicsGeoms) == 0x20);
static_assert(std::is_base_of_v<CRefCounted<>, CStudioPhysicsGeoms>);
static_assert(offsetof(CStudioPhysicsGeoms, m_pGeometryDataDescriptor) == 0x10);
static_assert(offsetof(CStudioPhysicsGeoms, m_nUnknown18) == 0x18);
static_assert(offsetof(CStudioPhysicsGeoms, m_nUnknown1C) == 0x1C);
static_assert(offsetof(CStudioPhysicsGeoms, m_nUnknown1E) == 0x1E);
static_assert(sizeof(CStudioVCollide) == 0x30);
static_assert(std::is_base_of_v<CRefCounted<>, CStudioVCollide>);
static_assert(offsetof(CStudioVCollide, m_VCollide) == 0x10);

struct StudioData_t
{
    CStudioHdrRef* m_pStudioHdrRef;
    std::uint16_t m_nRefCount;
    std::uint16_t m_nFlags;
    MDLHandle_t m_Handle;
    std::uint16_t m_Reserved0E;
    void* m_pUserData;
    CStudioVCollide* m_pVCollideRef;
    CStudioHWDataRef* m_pHardwareRef;
    virtualmodel_t* m_pVirtualModel;
    CStudioAnim* m_pAnimRef;
    CStudioVertices* m_pVerticesRef;
    CStudioColors* m_pColorsRef;
    std::uint16_t* m_pAutoplayList;
    std::byte m_AutoplayAllocatorState[0x10];
    std::int32_t m_nAutoplayCount;
    std::uint32_t m_Reserved64;
    std::byte m_PerEntryMutex[0x10];
    void* m_pPendingLoad;
};

static_assert(sizeof(StudioData_t) == 0x80);
static_assert(offsetof(StudioData_t, m_nRefCount) == 0x8);
static_assert(offsetof(StudioData_t, m_nFlags) == 0xA);
static_assert(offsetof(StudioData_t, m_Handle) == 0xC);
static_assert(offsetof(StudioData_t, m_pUserData) == 0x10);
static_assert(offsetof(StudioData_t, m_pVCollideRef) == 0x18);
static_assert(offsetof(StudioData_t, m_pHardwareRef) == 0x20);
static_assert(offsetof(StudioData_t, m_pVirtualModel) == 0x28);
static_assert(offsetof(StudioData_t, m_pAnimRef) == 0x30);
static_assert(offsetof(StudioData_t, m_pVerticesRef) == 0x38);
static_assert(offsetof(StudioData_t, m_pColorsRef) == 0x40);
static_assert(offsetof(StudioData_t, m_pAutoplayList) == 0x48);
static_assert(offsetof(StudioData_t, m_nAutoplayCount) == 0x60);
static_assert(offsetof(StudioData_t, m_PerEntryMutex) == 0x68);
static_assert(offsetof(StudioData_t, m_pPendingLoad) == 0x78);

struct MDLCacheDictionaryEntry_t
{
    MDLHandle_t m_Left;
    MDLHandle_t m_Right;
    MDLHandle_t m_Parent;
    std::uint16_t m_Tag;
    const char* m_pName;
    StudioData_t* m_pStudioData;
};

static_assert(sizeof(MDLCacheDictionaryEntry_t) == 0x18);
static_assert(offsetof(MDLCacheDictionaryEntry_t, m_Left) == 0x0);
static_assert(offsetof(MDLCacheDictionaryEntry_t, m_pName) == 0x8);
static_assert(offsetof(MDLCacheDictionaryEntry_t, m_pStudioData) == 0x10);

struct MDLCacheDictionary_t
{
    void* m_pCompareFunction;
    MDLCacheDictionaryEntry_t* m_pEntries;
    std::uint16_t m_nAllocationCount;
    std::byte m_Reserved12[0xE];
    MDLHandle_t m_Root;
    std::uint16_t m_nElementCount;
    MDLHandle_t m_FirstFree;
    MDLHandle_t m_LastAllocation;
    void* m_pElements;
};

static_assert(sizeof(MDLCacheDictionary_t) == 0x30);
static_assert(offsetof(MDLCacheDictionary_t, m_pEntries) == 0x8);
static_assert(offsetof(MDLCacheDictionary_t, m_nAllocationCount) == 0x10);
static_assert(offsetof(MDLCacheDictionary_t, m_Root) == 0x20);
static_assert(offsetof(MDLCacheDictionary_t, m_nElementCount) == 0x22);
static_assert(offsetof(MDLCacheDictionary_t, m_pElements) == 0x28);

class CMDLCache : public IMDLCache
{
  public:
    MDLHandle_t FindExistingMDL(const char* pModelPath);
    bool FlushCacheByHandle(MDLHandle_t handle);

  private:
    volatile std::int32_t m_nFrameUnlockCounter;
    std::uint32_t m_Reserved0C;
    MDLCacheDictionary_t m_MDLDictionary;
    CRITICAL_SECTION m_MDLMutex;
    std::uint8_t m_nStateFlags;
    std::byte m_Reserved69[0x7];
};

static_assert(sizeof(CMDLCache) == 0x70);

extern CMDLCache* g_pMDLCache;
