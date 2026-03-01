//=============================================================================//
//
// Purpose: pak page patching
//
//=============================================================================//
#include "pakfile.h"
#include "pakpatch.h"

static bool PATCH_CMD_0(PakFile* const pak, size_t* const numAvailableBytes)
{
  size_t numPatchBytesToProcess; // r8
  size_t v3; // rdi
  size_t qword_548; // rcx
  size_t v6; // rax
  uint64_t processedPatchedDataSize; // rdx
  char *patchDstPtr; // rcx
  size_t patchSrcSize; // rsi
  uint64_t v11; // r14
  __int64 maxCopySize; // rax
  const void *decompressedBuffer; // rdx
  size_t v14; // r14
  size_t v15; // r8

  numPatchBytesToProcess = pak->numPatchBytesToProcess;
  v3 = *numAvailableBytes;
  qword_548 = pak->qword_548;
  v6 = *numAvailableBytes;
  if ( numPatchBytesToProcess < *numAvailableBytes )
    v6 = numPatchBytesToProcess;
  if ( qword_548 )
  {
    if ( v6 <= qword_548 )
    {
      pak->processedPatchedDataSize += v6;
      pak->qword_548 = qword_548 - v6;
      pak->numPatchBytesToProcess = numPatchBytesToProcess - v6;
      *numAvailableBytes = v3 - v6;
      return pak->numPatchBytesToProcess == 0LL;
    }
    pak->processedPatchedDataSize += qword_548;
    v6 -= qword_548;
    pak->qword_548 = 0LL;
    v3 -= qword_548;
    pak->numPatchBytesToProcess = numPatchBytesToProcess - qword_548;
  }
  processedPatchedDataSize = pak->processedPatchedDataSize;
  patchDstPtr = pak->patchDstPtr;
  patchSrcSize = pak->patchSrcSize;
  v11 = ~processedPatchedDataSize;
  if ( v6 < patchSrcSize )
    patchSrcSize = v6;
  maxCopySize = pak->maxCopySize;
  decompressedBuffer = (const void *)(pak->decompressedBuffer + (maxCopySize & processedPatchedDataSize));
  v14 = (maxCopySize & v11) + 1;
  if ( patchSrcSize > v14 )
  {
    memcpy(patchDstPtr, decompressedBuffer, v14);
    decompressedBuffer = (const void *)pak->decompressedBuffer;
    patchDstPtr = &pak->patchDstPtr[v14];
    v15 = patchSrcSize - v14;
  }
  else
  {
    v15 = patchSrcSize;
  }
  memcpy(patchDstPtr, decompressedBuffer, v15);
  pak->processedPatchedDataSize += patchSrcSize;
  pak->patchSrcSize -= patchSrcSize;
  pak->patchDstPtr += patchSrcSize;
  pak->numPatchBytesToProcess -= patchSrcSize;
  *numAvailableBytes = v3 - patchSrcSize;
  return pak->numPatchBytesToProcess == 0LL;
}

static bool PATCH_CMD_1(PakFile* const pak, size_t* const pNumBytesAvailable)
{
  unsigned __int64 numPatchBytesToProcess; // rax
  __int64 v3; // r8

  numPatchBytesToProcess = pak->numPatchBytesToProcess;
  v3 = *pNumBytesAvailable;
  if ( *pNumBytesAvailable > numPatchBytesToProcess )
  {
    pak->processedPatchedDataSize += numPatchBytesToProcess;
    pak->numPatchBytesToProcess = 0LL;
    *pNumBytesAvailable = v3 - numPatchBytesToProcess;
    return 1;
  }
  else
  {
    pak->processedPatchedDataSize += v3;
    pak->numPatchBytesToProcess = numPatchBytesToProcess - v3;
    *pNumBytesAvailable = 0LL;
    return 0;
  }
}

static bool PATCH_CMD_2(PakFile* const pak, size_t* const pNumBytesAvailable)
{
    NOTE_UNUSED(pNumBytesAvailable);

    size_t numBytesToProcess = pak->numPatchBytesToProcess;
    const size_t v3 = pak->qword_548;

    if (v3)
    {
        if (numBytesToProcess <= v3)
        {
            pak->numPatchBytesToProcess = 0ull;
            pak->patchDataPtr += numBytesToProcess;
            pak->qword_548 = v3 - numBytesToProcess;

            return true;
        }

        pak->qword_548 = 0i64;
        numBytesToProcess -= v3;
        pak->patchDataPtr += v3;
        pak->numPatchBytesToProcess = numBytesToProcess;
    }

    const size_t patchSrcSize = std::min<size_t>(numBytesToProcess, pak->patchSrcSize);

    memcpy(pak->patchDstPtr, pak->patchDataPtr, patchSrcSize);

    pak->patchDataPtr += patchSrcSize;
    pak->patchSrcSize -= patchSrcSize;
    pak->patchDstPtr += patchSrcSize;
    pak->numPatchBytesToProcess -= patchSrcSize;

    return pak->numPatchBytesToProcess == 0;
}

static bool PATCH_CMD_3(PakFile* const pak, size_t* const pNumBytesAvailable)
{
    const size_t numBytesLeft = std::min<size_t>(*pNumBytesAvailable, pak->numPatchBytesToProcess);
    const size_t patchSrcSize = std::min<size_t>(numBytesLeft, pak->patchSrcSize);

    memcpy(pak->patchDstPtr, pak->patchDataPtr, patchSrcSize);
    pak->patchDataPtr += patchSrcSize;
    pak->processedPatchedDataSize += patchSrcSize;
    pak->patchSrcSize -= patchSrcSize;
    pak->patchDstPtr += patchSrcSize;
    pak->numPatchBytesToProcess -= patchSrcSize;
    *pNumBytesAvailable = *pNumBytesAvailable - patchSrcSize;

    return pak->numPatchBytesToProcess == 0;
}

static bool PATCH_CMD_4_5(PakFile* const pak, size_t* const pNumBytesAvailable)
{
    const size_t numBytesAvailable = *pNumBytesAvailable;

    if (!numBytesAvailable)
        return false;

    *pak->patchDstPtr = *(_BYTE*)pak->patchDataPtr++;
    ++pak->processedPatchedDataSize;
    --pak->patchSrcSize;
    ++pak->patchDstPtr;
    pak->patchFunc = PATCH_CMD_0;
    *pNumBytesAvailable = numBytesAvailable - 1;

    return PATCH_CMD_0(pak, pNumBytesAvailable);
}

static bool PATCH_CMD_6(PakFile* const pak, size_t* const pNumBytesAvailable)
{
    const size_t numBytesAvailable = *pNumBytesAvailable;
    size_t numBytesToSkip = 2;

    if (*pNumBytesAvailable < 2)
    {
        if (!*pNumBytesAvailable)
            return false;

        numBytesToSkip = *pNumBytesAvailable;
    }

    const void* const patchDataPtr = (const void*)pak->patchDataPtr;
    const size_t patchSrcSize = pak->patchSrcSize;
    char* const patchDstPtr = pak->patchDstPtr;

    if (numBytesToSkip > patchSrcSize)
    {
        memcpy(patchDstPtr, patchDataPtr, patchSrcSize);
        pak->patchDataPtr += patchSrcSize;
        pak->processedPatchedDataSize += patchSrcSize;
        pak->patchSrcSize -= patchSrcSize;
        pak->patchDstPtr += patchSrcSize;
        pak->patchFunc = PATCH_CMD_4_5;
        *pNumBytesAvailable = numBytesAvailable - patchSrcSize;
    }
    else
    {
        memcpy(patchDstPtr, patchDataPtr, numBytesToSkip);
        pak->patchDataPtr += numBytesToSkip;
        pak->processedPatchedDataSize += numBytesToSkip;
        pak->patchSrcSize -= numBytesToSkip;
        pak->patchDstPtr += numBytesToSkip;

        if (numBytesAvailable >= 2)
        {
            pak->patchFunc = PATCH_CMD_0;
            *pNumBytesAvailable = numBytesAvailable - numBytesToSkip;

            return PATCH_CMD_0(pak, pNumBytesAvailable);
        }

        pak->patchFunc = PATCH_CMD_4_5;
        *pNumBytesAvailable = NULL;
    }

    return false;
}

const PakPatchFuncs_s g_pakPatchApi
{
    PATCH_CMD_0,
    PATCH_CMD_1,
    PATCH_CMD_2,
    PATCH_CMD_3,
    PATCH_CMD_4_5,
    PATCH_CMD_4_5,
    PATCH_CMD_6,
};
