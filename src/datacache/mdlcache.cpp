#include "datacache/mdlcache.h"

#include "tier0/callbacks.h"
#include "tier0/module.h"

#include <cctype>

CMDLCache* g_pMDLCache = nullptr;

using FlushStudioDataFn = void (*)(CMDLCache* pMDLCache, StudioData_t* pStudioData);
static FlushStudioDataFn s_FlushStudioData = nullptr;

using FirstInorderFn = MDLHandle_t (*)(MDLCacheDictionary_t* pDictionary);
using NextInorderFn = MDLHandle_t (*)(MDLCacheDictionary_t* pDictionary, MDLHandle_t handle);
static FirstInorderFn s_FirstInorder = nullptr;
static NextInorderFn s_NextInorder = nullptr;

static bool ModelPathsEqual(const char* lhs, const char* rhs)
{
    if (!lhs || !rhs)
        return false;

    while (*lhs && *rhs)
    {
        const unsigned char lhsChar = static_cast<unsigned char>(*lhs++);
        const unsigned char rhsChar = static_cast<unsigned char>(*rhs++);
        const int lhsNormalised = lhsChar == '\\' ? '/' : std::tolower(lhsChar);
        const int rhsNormalised = rhsChar == '\\' ? '/' : std::tolower(rhsChar);
        if (lhsNormalised != rhsNormalised)
            return false;
    }

    return *lhs == *rhs;
}

MDLHandle_t CMDLCache::FindExistingMDL(const char* pModelPath)
{
    if (!pModelPath || !s_FirstInorder || !s_NextInorder)
        return InvalidMDLHandle;

    EnterCriticalSection(&m_MDLMutex);
    MDLHandle_t foundHandle = InvalidMDLHandle;
    if (m_MDLDictionary.m_pEntries)
    {
        MDLHandle_t handle = s_FirstInorder(&m_MDLDictionary);
        for (uint32_t visited = 0; handle != InvalidMDLHandle && visited < m_MDLDictionary.m_nElementCount;
             ++visited, handle = s_NextInorder(&m_MDLDictionary, handle))
        {
            if (handle >= m_MDLDictionary.m_nAllocationCount)
                break;

            const MDLCacheDictionaryEntry_t& entry = m_MDLDictionary.m_pEntries[handle];
            if (!ModelPathsEqual(entry.m_pName, pModelPath))
                continue;

            AddRef(handle);
            foundHandle = handle;
            break;
        }
    }
    LeaveCriticalSection(&m_MDLMutex);
    return foundHandle;
}

bool CMDLCache::FlushCacheByHandle(const MDLHandle_t handle)
{
    if (handle == InvalidMDLHandle || !s_FlushStudioData)
        return false;

    EnterCriticalSection(&m_MDLMutex);

    if (!m_MDLDictionary.m_pEntries || handle >= m_MDLDictionary.m_nAllocationCount)
    {
        LeaveCriticalSection(&m_MDLMutex);
        return false;
    }

    MDLCacheDictionaryEntry_t& entry = m_MDLDictionary.m_pEntries[handle];
    if (entry.m_Left == handle || !entry.m_pStudioData)
    {
        LeaveCriticalSection(&m_MDLMutex);
        return false;
    }

    AddRef(handle);
    StudioData_t* const pStudioData = entry.m_pStudioData;
    LeaveCriticalSection(&m_MDLMutex);

    s_FlushStudioData(this, pStudioData);
    Release(handle);
    return true;
}

ON_DLL_LOAD("datacache.dll", MDLCache, [](CModule module)
{
    s_FirstInorder = module.Offset(0x5A800).RCast<FirstInorderFn>();
    s_NextInorder = module.Offset(0x5D3C0).RCast<NextInorderFn>();
    s_FlushStudioData = module.Offset(0x5AAF0).RCast<FlushStudioDataFn>();
    assert(s_FirstInorder && s_NextInorder);
    assert(s_FlushStudioData);

    CreateInterfaceFn pCreateInterface = module.GetExportedFunction(CREATEINTERFACE_PROCNAME).RCast<CreateInterfaceFn>();
    assert(pCreateInterface);

    g_pMDLCache = static_cast<CMDLCache*>(pCreateInterface(MDLCacheInterfaceVersion, nullptr));
    assert(g_pMDLCache);
})
