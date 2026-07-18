#pragma once

#include "datacache/imdlcache.h"

#include <cstddef>

struct MDLCacheDictionaryEntry_t
{
    MDLHandle_t m_Left;
    MDLHandle_t m_Right;
    MDLHandle_t m_Parent;
    uint16_t m_Tag;
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
    uint16_t m_nAllocationCount;
    std::byte m_Unknown12[0xE];
    MDLHandle_t m_Root;
    uint16_t m_nElementCount;
    MDLHandle_t m_FirstFree;
    MDLHandle_t m_LastAllocation;
    void* m_pElements;
};

static_assert(sizeof(MDLCacheDictionary_t) == 0x30);
static_assert(offsetof(MDLCacheDictionary_t, m_pEntries) == 0x8);
static_assert(offsetof(MDLCacheDictionary_t, m_nAllocationCount) == 0x10);
static_assert(offsetof(MDLCacheDictionary_t, m_Root) == 0x20);

class CMDLCache : public IMDLCache
{
  public:
    MDLHandle_t FindExistingMDL(const char* pModelPath);
    bool FlushCacheByHandle(MDLHandle_t handle);

  private:
    volatile int32_t m_nFrameUnlockCounter;
    uint32_t m_Unknown0C;
    MDLCacheDictionary_t m_MDLDictionary;
    CRITICAL_SECTION m_MDLMutex;
    uint8_t m_nStateFlags;
    std::byte m_Reserved69[0x7];
};

static_assert(sizeof(CMDLCache) == 0x70);

extern CMDLCache* g_pMDLCache;
