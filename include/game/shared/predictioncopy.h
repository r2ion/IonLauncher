#pragma once

#include <cstddef>

#include "datamap.h"
#include "tier1/utlvector.h"

extern thread_local int g_nPredictionErrorCommand;

struct datacopy_t
{
    int startIndex;
    int endIndex;
    int offset[TD_OFFSET_COUNT];
    int length;
};

struct datamapinfo_t
{
    CUtlVector<typedescription_t> m_Flat;
    int m_nPackedSize;
    int m_nPackedStartOffset;
    CUtlVector<datacopy_t> m_CopyRuns;
};

struct optimized_datamap_t
{
    datamapinfo_t m_Info[2];
};

struct prediction_datamap_t
{
    datamap_t map;
    std::byte reserved30[8];
    optimized_datamap_t* optimizedMap;
};

static_assert(sizeof(datacopy_t) == 0x14);
static_assert(sizeof(datamapinfo_t) == 0x48);
static_assert(offsetof(datamapinfo_t, m_CopyRuns) == 0x28);
static_assert(offsetof(prediction_datamap_t, optimizedMap) == 0x38);

class CPredictionCopy
{
  public:
    int m_OpType;
    int m_nType;
    void* m_pDest;
    const void* m_pSrc;
    int m_nDestOffsetIndex;
    int m_nSrcOffsetIndex;
    int m_nErrorCount;
    int m_nEntIndex;
    void* m_FieldCompareFunc;
};

static_assert(offsetof(CPredictionCopy, m_nErrorCount) == 0x20);
static_assert(offsetof(CPredictionCopy, m_nEntIndex) == 0x24);
static_assert(offsetof(CPredictionCopy, m_FieldCompareFunc) == 0x28);
static_assert(sizeof(CPredictionCopy) == 0x30);
