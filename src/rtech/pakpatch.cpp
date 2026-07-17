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
  size_t numBytesToSkip; // rcx
  size_t v6; // rax
  uint64_t processedPatchedDataSize; // rdx
  char *patchDstPtr; // rcx
  size_t patchSrcSize; // rsi
  uint64_t v11; // r14
  size_t ringMask;
  const void *decompressedBuffer; // rdx
  size_t v14; // r14
  size_t v15; // r8

  numPatchBytesToProcess = pak->streamBytesRemaining;
  v3 = *numAvailableBytes;
  numBytesToSkip = pak->skipBytesRemaining;
  v6 = *numAvailableBytes;
  if ( numPatchBytesToProcess < *numAvailableBytes )
    v6 = numPatchBytesToProcess;
  if ( numBytesToSkip )
  {
    if ( v6 <= numBytesToSkip )
    {
      pak->decodeCursor += v6;
      pak->skipBytesRemaining = numBytesToSkip - v6;
      pak->streamBytesRemaining = numPatchBytesToProcess - v6;
      *numAvailableBytes = v3 - v6;
      return pak->streamBytesRemaining == 0LL;
    }
    pak->decodeCursor += numBytesToSkip;
    v6 -= numBytesToSkip;
    pak->skipBytesRemaining = 0LL;
    v3 -= numBytesToSkip;
    pak->streamBytesRemaining = numPatchBytesToProcess - numBytesToSkip;
  }
  processedPatchedDataSize = pak->decodeCursor;
  patchDstPtr = pak->copyDestination;
  patchSrcSize = pak->copyBytesRemaining;
  v11 = ~processedPatchedDataSize;
  if ( v6 < patchSrcSize )
    patchSrcSize = v6;
  ringMask = pak->decoderRingMask;
  decompressedBuffer = (const void *)(pak->decoderRingBuffer + (ringMask & processedPatchedDataSize));
  v14 = (ringMask & v11) + 1;
  if ( patchSrcSize > v14 )
  {
    memcpy(patchDstPtr, decompressedBuffer, v14);
    decompressedBuffer = (const void *)pak->decoderRingBuffer;
    patchDstPtr = &pak->copyDestination[v14];
    v15 = patchSrcSize - v14;
  }
  else
  {
    v15 = patchSrcSize;
  }
  memcpy(patchDstPtr, decompressedBuffer, v15);
  pak->decodeCursor += patchSrcSize;
  pak->copyBytesRemaining -= patchSrcSize;
  pak->copyDestination += patchSrcSize;
  pak->streamBytesRemaining -= patchSrcSize;
  *numAvailableBytes = v3 - patchSrcSize;
  return pak->streamBytesRemaining == 0LL;
}

static bool PATCH_CMD_1(PakFile* const pak, size_t* const pNumBytesAvailable)
{
  unsigned __int64 numPatchBytesToProcess; // rax
  __int64 v3; // r8

  numPatchBytesToProcess = pak->streamBytesRemaining;
  v3 = *pNumBytesAvailable;
  if ( *pNumBytesAvailable > numPatchBytesToProcess )
  {
    pak->decodeCursor += numPatchBytesToProcess;
    pak->streamBytesRemaining = 0LL;
    *pNumBytesAvailable = v3 - numPatchBytesToProcess;
    return 1;
  }
  else
  {
    pak->decodeCursor += v3;
    pak->streamBytesRemaining = numPatchBytesToProcess - v3;
    *pNumBytesAvailable = 0LL;
    return 0;
  }
}

static bool PATCH_CMD_2(PakFile* const pak, size_t* const pNumBytesAvailable)
{
    NOTE_UNUSED(pNumBytesAvailable);

    size_t numBytesToProcess = pak->streamBytesRemaining;
    const size_t v3 = pak->skipBytesRemaining;

    if (v3)
    {
        if (numBytesToProcess <= v3)
        {
            pak->streamBytesRemaining = 0ull;
            pak->literalCursor += numBytesToProcess;
            pak->skipBytesRemaining = v3 - numBytesToProcess;

            return true;
        }

        pak->skipBytesRemaining = 0i64;
        numBytesToProcess -= v3;
        pak->literalCursor += v3;
        pak->streamBytesRemaining = numBytesToProcess;
    }

    const size_t patchSrcSize = std::min<size_t>(numBytesToProcess, pak->copyBytesRemaining);

    memcpy(pak->copyDestination, pak->literalCursor, patchSrcSize);

    pak->literalCursor += patchSrcSize;
    pak->copyBytesRemaining -= patchSrcSize;
    pak->copyDestination += patchSrcSize;
    pak->streamBytesRemaining -= patchSrcSize;

    return pak->streamBytesRemaining == 0;
}

static bool PATCH_CMD_3(PakFile* const pak, size_t* const pNumBytesAvailable)
{
    const size_t numBytesLeft = std::min<size_t>(*pNumBytesAvailable, pak->streamBytesRemaining);
    const size_t patchSrcSize = std::min<size_t>(numBytesLeft, pak->copyBytesRemaining);

    memcpy(pak->copyDestination, pak->literalCursor, patchSrcSize);
    pak->literalCursor += patchSrcSize;
    pak->decodeCursor += patchSrcSize;
    pak->copyBytesRemaining -= patchSrcSize;
    pak->copyDestination += patchSrcSize;
    pak->streamBytesRemaining -= patchSrcSize;
    *pNumBytesAvailable = *pNumBytesAvailable - patchSrcSize;

    return pak->streamBytesRemaining == 0;
}

static bool PATCH_CMD_4_5(PakFile* const pak, size_t* const pNumBytesAvailable)
{
    const size_t numBytesAvailable = *pNumBytesAvailable;

    if (!numBytesAvailable)
        return false;

    *pak->copyDestination = *(_BYTE*)pak->literalCursor++;
    ++pak->decodeCursor;
    --pak->copyBytesRemaining;
    ++pak->copyDestination;
    pak->decodeStep = PATCH_CMD_0;
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

    const void* const patchDataPtr = (const void*)pak->literalCursor;
    const size_t patchSrcSize = pak->copyBytesRemaining;
    char* const patchDstPtr = pak->copyDestination;

    if (numBytesToSkip > patchSrcSize)
    {
        memcpy(patchDstPtr, patchDataPtr, patchSrcSize);
        pak->literalCursor += patchSrcSize;
        pak->decodeCursor += patchSrcSize;
        pak->copyBytesRemaining -= patchSrcSize;
        pak->copyDestination += patchSrcSize;
        pak->decodeStep = PATCH_CMD_4_5;
        *pNumBytesAvailable = numBytesAvailable - patchSrcSize;
    }
    else
    {
        memcpy(patchDstPtr, patchDataPtr, numBytesToSkip);
        pak->literalCursor += numBytesToSkip;
        pak->decodeCursor += numBytesToSkip;
        pak->copyBytesRemaining -= numBytesToSkip;
        pak->copyDestination += numBytesToSkip;

        if (numBytesAvailable >= 2)
        {
            pak->decodeStep = PATCH_CMD_0;
            *pNumBytesAvailable = numBytesAvailable - numBytesToSkip;

            return PATCH_CMD_0(pak, pNumBytesAvailable);
        }

        pak->decodeStep = PATCH_CMD_4_5;
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
