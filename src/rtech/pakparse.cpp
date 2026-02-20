#include "pakfile.h"
#include <util/zstdutils.h>

static ZSTDDecoder_s s_zstdPakDecoder;
#define ALIGN_VALUE( val, alignment ) ( ( val + alignment - 1 ) & ~( alignment - 1 ) ) 

size_t (*Pak_RTechDecoderInit)(PakDecompState* const decoder, const uint8_t* const fileBuffer,
	const uint64_t inputMask, const size_t dataSize, const size_t dataOffset, const size_t headerSize);

bool (*Pak_RTechStreamDecode)(PakDecompState* const decoder, const size_t inLen, const size_t outLen);

int64_t (*CheckAsyncRequest)(int64_t requestHandle, size_t* bytesProcessed, const char** statusMsg);

void(*sub_9570)(PakFile* pak);
__int64 (*FS_ReadAsyncFile)(
        unsigned int handle,
        __int64 readOffset,
        unsigned __int64 readSize,
        __int64 buffer,
        int type);

void (*FS_CloseAsyncFile)(unsigned int handle);
int16_t (*FS_OpenAsyncFile)(const char*, size_t*);

static size_t Pak_ZStdDecoderInit(PakDecompState* const decoder, const uint8_t* frameHeader,
	const size_t dataSize, const size_t headerSize)
{
	ZSTD_DStream* dctx = decoder->zstreamContext;

	assert(dctx);
	
	if (ZSTD_getFrameHeader(&dctx->fParams, frameHeader, dataSize) != 0)
		return NULL; // content size error

	// ideally the frame header of the block gets parsed first, the length
	// thereof is returned by initDStream and thus being processed first
	// before moving on to actual data
	decoder->frameHeaderSize = ZSTD_initDStream(dctx);

	// we need at least this many bytes of streamed data to process the frame
	// header of the compressed block
	decoder->bufferSizeNeeded = decoder->inBufBytePos + decoder->frameHeaderSize;

	// must include header size
	decoder->decompSize = dctx->fParams.frameContentSize + headerSize;
	return decoder->decompSize;
}

size_t Pak_InitDecoder(PakDecompState* const decoder, const uint8_t* const inputBuf, uint8_t* const outputBuf,
	const uint64_t inputMask, const uint64_t outputMask, const size_t dataSize, const size_t dataOffset,
	const size_t headerSize, const PakDecodeMode_e decodeMode)
{
	
	// the absolute start address of the input and output buffers
	decoder->inputBuf = inputBuf;
	decoder->outputBuf = outputBuf;

	// the actual file size, which consists of dataOffset (anything up to the
	// frame header, like the file header) and the actual encoded data itself
	decoder->fileSize = dataOffset + dataSize;
	decoder->decodeMode = decodeMode;

	// buffer masks, which essentially gets used to index into the input and
	// output buffers, similar to 'idx % bufSize', where bufSize = bufMask+1
	decoder->inputMask = inputMask;
	decoder->outputMask = outputMask;

	// the current positions in the input and output buffers; if we deal with
	// paks that are patched, the input buffer position during the init and
	// decode call on subsequent patches may not be at the start of the buffer,
	// they will end where the previous 'to patch' pak had finished streaming
	// and decoding
	decoder->inBufBytePos = dataOffset + headerSize;
	decoder->outBufBytePos = headerSize;

	// if we use the default RTech decoder, return from here as the stuff below
	// is handled by the RTech decoder internally
	if (decodeMode == PakDecodeMode_e::MODE_RTECH)
		return Pak_RTechDecoderInit(decoder, inputBuf, inputMask, dataSize, dataOffset, headerSize);

	// NOTE: on RTech encoded paks this data is parsed out of the frame header,
	// but for ZStd encoded paks we are always limiting this to the ring buffer
	// size
	decoder->outputInvMask = PAK_DECODE_OUT_RING_BUFFER_MASK;

	// this points to the first byte of the frame header, takes dataOffset
	// into account which is the offset in the ring buffer to the patched
	// data as we parse it contiguously after the base pak data, which
	// might have ended somewhere in the middle of the ring buffer
	const uint8_t* const frameHeaderData = &inputBuf[inputMask & (dataOffset + headerSize)];

	const size_t decodeSize = Pak_ZStdDecoderInit(decoder, frameHeaderData, dataSize, headerSize);
	assert(decodeSize);

	return decodeSize;
}

static bool Pak_HasEnoughDecodeBufferAvailable(PakDecompState* const decoder, const size_t outLen)
{
	// align to nearest multiple of buffer size
	const uint64_t bufPosAligned = (decoder->outBufBytePos & ~decoder->outputInvMask);
	const uint64_t threshold = decoder->outputInvMask + bufPosAligned + 1;

	// make sure caller has copied all data out the ring buffer first before
	// overwriting it with new decoded data
	return (outLen >= threshold || outLen >= decoder->decompSize);
}

static PakRingBufferFrame_s Pak_DetermineRingBufferFrame(const uint64_t bufMask, const size_t seekPos, const size_t dataLen)
{
	PakRingBufferFrame_s ring;
	ring.bufIndex = seekPos & bufMask;

	// the total amount of bytes used and available in this frame
	const size_t bytesUsed = ring.bufIndex & bufMask;
	const size_t totalAvail = bufMask + 1 - bytesUsed;

	// the last part of the data might be smaller than the remainder of the ring
	// buffer; clamp it
	ring.frameLen = std::min(dataLen - seekPos, totalAvail);
	return ring;
}

static bool Pak_ZStdStreamDecode(PakDecompState* const decoder, const PakRingBufferFrame_s& outFrame, const PakRingBufferFrame_s& inFrame)
{
	ZSTD_outBuffer outBuffer = {
		&decoder->outputBuf[outFrame.bufIndex],
		outFrame.frameLen, NULL
	};

	ZSTD_inBuffer inBuffer = {
		&decoder->inputBuf[inFrame.bufIndex],
		inFrame.frameLen, NULL
	};

	ZSTD_DStream* const dctx = decoder->zstreamContext;
	
	const size_t ret = ZSTD_decompressStream(dctx, &outBuffer, &inBuffer);

	if (ZSTD_isError(ret))
	{
		// NOTE: obtained here and not in the error formatter as we could check
		// the error string during the assertion
		const char* const decodeError = ZSTD_getErrorName(ret);
		assert(0);

		//Error(eDLL_T::RTECH, EXIT_FAILURE, "%s: decode error: %s\n", __FUNCTION__, decodeError);
		return false;
	}

	// advance buffer io positions, required so the main parser could already
	// start parsing the headers while the rest is getting decoded still
	decoder->outBufBytePos += outBuffer.pos;
	decoder->inBufBytePos += inBuffer.pos;

	// NOTE: if inBuffer.pos < inBuffer.size, we made full use of the output
	// buffer and couldn't decode any more data into it. the decoded data needs
	// to be copied out to the destination so we can reuse the ring buffer and
	// process the remainder of this frame. in these cases we do not update the
	// bufferSizeNeeded objective below as we still have data left to process.
	if (inBuffer.pos == inBuffer.size)
		decoder->bufferSizeNeeded = decoder->inBufBytePos + ZSTD_DStreamInSize();

	return ret == NULL;
}

bool Pak_StreamToBufferDecode(PakDecompState* const decoder, const size_t inLen, const size_t outLen, const PakDecodeMode_e decodeMode)
{
	if (!(inLen >= decoder->bufferSizeNeeded))
	{
		if (decodeMode != PakDecodeMode_e::MODE_ZSTD)
			return false;

		if (!decoder->allChunksStreamed)
			return false; // This only applies to ZStd!
	}

	if (!Pak_HasEnoughDecodeBufferAvailable(decoder, outLen))
		return false;

	if (decodeMode == PakDecodeMode_e::MODE_RTECH)
		return Pak_RTechStreamDecode(decoder, inLen, outLen);

	//// must have a decoder at this point
	////
	//// also, input seek pos may not exceed inLen as we can't read past
	//// currently streamed data; this should've been checked before reaching
	//// this position in code
	assert(decoder->zstreamContext && decoder->inBufBytePos <= inLen);

	const PakRingBufferFrame_s inFrame = Pak_DetermineRingBufferFrame(decoder->inputInvMask, decoder->inBufBytePos, inLen);
	//// if the file size is smaller than the provided output length, clamp it.
	//// this happens when the buffer is smaller than the default buffer size
	//// defined by 'PAK_DECODE_OUT_RING_BUFFER_SIZE'. just like how the rtech
	//// decoder clamps it internally, we should do it here to avoid an overflow.
	const PakRingBufferFrame_s outFrame = Pak_DetermineRingBufferFrame(decoder->outputMask, decoder->outBufBytePos, std::min(decoder->decompSize, outLen));

	return Pak_ZStdStreamDecode(decoder, outFrame, inFrame);
}

const char* Pak_DecoderToString(const PakDecodeMode_e mode)
{
	switch (mode)
	{
	case PakDecodeMode_e::MODE_RTECH: return "RTech";
	case PakDecodeMode_e::MODE_ZSTD: return "ZStd";
	case PakDecodeMode_e::MODE_DISABLED: return "Disabled";
	}
}
// #STR: "Error reading pak file \"%s\" -- %s\n", "Error reading pak file \"%s\" -- decompressed size %u does, "r2\\paks\\Win64\\%s", "(%02u).rpak", "Couldn't open file \"%s\".\n", "File \"%s\" appears truncated.\n", "(no reason)"
bool __fastcall Pak_ProcessPakFile_8D10(PakFile *pak)
{
  PakFileStream *fileStream; // rsi
  __int64 numDataChunks; // rax
  unsigned __int64 readStart; // r15
  __int64 currentDataChunkIndex; // rax
  char dataChunkStatus; // di
  int v7; // ecx
  char v8; // al
  __int64 bytesProcessed_1; // rdx
  char numLoadedFiles; // al
  PakHeader *p_header; // r8
  unsigned __int64 totalDataChunkSizeProcessed; // r9
  unsigned __int8 v13; // cl
  unsigned __int64 v14; // r8
  __int64 fileIdx; // rdx
  PakFileStream__Descriptor *fileStreamDescriptior; // rdi
  __int64 inited; // rax
  uint8_t *decompressedBuffer; // rax
  uint64_t compressedSize; // rcx
  char v20; // al
  __int64 qword_548; // rax
  unsigned int numDataChunksProcessed; // ecx
  __int64 numDataChunks_1; // rdx
  __int64 v24; // rax
  __int64 i; // r12
  char fileReadStatus; // r14
  __int64 v27; // rdx
  unsigned __int64 v28; // rbp
  unsigned __int64 v29; // r8
  __int64 v30; // rdi
  int AsyncFile; // eax
  __int64 lastLoadedPatchIndex; // rcx
  __int64 lastLoadedPatchIndex_cpy; // r14
  char v34; // al
  __int64 v35; // rdx
  __int64 j; // rcx
  int v37; // edi
  __int64 v38; // r14
  unsigned int numDataChunks_2; // eax
  unsigned __int64 v40; // rdx
  char v42[260]; // [rsp+30h] [rbp-138h] BYREF
  _BYTE v43[12]; // [rsp+134h] [rbp-34h] BYREF
  const char *statusMsg; // [rsp+170h] [rbp+8h] BYREF
  size_t bytesProcessed; // [rsp+178h] [rbp+10h] BYREF

  fileStream = &pak->fileStream;
  numDataChunks = pak->fileStream.numDataChunks;
  if ( (_DWORD)numDataChunks )
    readStart = numDataChunks << 19;
  else
    readStart = 0x58LL;
  if ( pak->fileStream.numDataChunksProcessed != (_DWORD)numDataChunks )
  {
    do
    {
      currentDataChunkIndex = fileStream->numDataChunksProcessed & 0x1F;
      dataChunkStatus = fileStream->dataChunkStatuses[currentDataChunkIndex];
      if ( dataChunkStatus != 1 )
      {
        v7 = fileStream->fileReadJobs[currentDataChunkIndex];
        statusMsg = "(no reason)";
        v8 = CheckAsyncRequest(v7, &bytesProcessed, &statusMsg);
        if ( !v8 )
          break;
        //if ( v8 == 2 )
        //  RTechLog_10AB0(4LL, "Error reading pak file \"%s\" -- %s\n", pak->pakFileName, statusMsg);
        bytesProcessed_1 = bytesProcessed;
        fileStream->bytesStreamed += bytesProcessed;
        if ( dataChunkStatus )
        {
          numLoadedFiles = fileStream->numLoadedFiles;
          p_header = &pak->header;
          totalDataChunkSizeProcessed = (unsigned __int64)fileStream->numDataChunksProcessed << 19;
          v13 = numLoadedFiles & 7;
          fileStream->numLoadedFiles = numLoadedFiles + 1;
          if ( dataChunkStatus == 2 )
          {
            v14 = totalDataChunkSizeProcessed & fileStream->bufferMask;
            fileStream->bytesStreamed = totalDataChunkSizeProcessed + bytesProcessed_1;
            //p_header = (RpakHeader_v7 *)&fileStream->fileBuffer[v14];
          }
          fileIdx = v13;
          fileStream->descriptors[v13].dataOffset = totalDataChunkSizeProcessed + 88;
          fileStream->descriptors[fileIdx].compressedSize = totalDataChunkSizeProcessed + p_header->compressedSize;
          fileStream->descriptors[fileIdx].decompressedSize = p_header->decompressedSize;
          fileStream->descriptors[fileIdx].compressionMode = p_header->GetCompressionMode();
        }
      }
      ++fileStream->numDataChunksProcessed;
    }
    while ( fileStream->numDataChunksProcessed != fileStream->numDataChunks );
  }
  if ( pak->processedStreamCount != fileStream->numLoadedFiles )
  {
    do
    {
      fileStreamDescriptior = &fileStream->descriptors[pak->processedStreamCount & 7];
      if ( pak->resetInBytePos )
      {
        pak->resetInBytePos = 0;
        pak->inputBytePos = fileStreamDescriptior->dataOffset;
        if ( LOBYTE(fileStreamDescriptior->compressionMode) )
        {
          *(_WORD *)&pak->updateBytePosPostProcess = 256;
          pak->processedPatchedDataSize = 0x58LL;
        }
        else
        {
          *(_WORD *)&pak->updateBytePosPostProcess = 1;
          pak->processedPatchedDataSize = fileStreamDescriptior->dataOffset;
        }
        if ( !pak->isCompressed )
        {
LABEL_24:
          compressedSize = fileStreamDescriptior->compressedSize;
          if ( fileStream->bytesStreamed < compressedSize )
            compressedSize = fileStream->bytesStreamed;
          goto LABEL_30;
        }
        inited = Pak_RTechDecoderInit(
                   &pak->pakDecoder,
                   fileStream->fileBuffer,
                   0xFFFFFFuLL,
                   fileStreamDescriptior->compressedSize - (fileStreamDescriptior->dataOffset - 0x58LL),
                   fileStreamDescriptior->dataOffset - 0x58LL,
                   0x58uLL);
        //if ( inited != fileStreamDescriptior->decompressedSize )
        //  RTechLog_10AB0(
        //    4LL,
        //    "Error reading pak file \"%s\" -- decompressed size %u doesn't match expected value %u\n",
        //    pak->pakFileName,
        //    inited + 0x58,
        //    pak->header.decompressedSize);
        decompressedBuffer = (uint8_t *)pak->decompressedBuffer;
        pak->pakDecoder.outputMask = 0x3FFFFFLL;
        pak->pakDecoder.outputBuf = decompressedBuffer;
      }
      if ( !pak->isCompressed )
        goto LABEL_24;
      if ( pak->pakDecoder.outBufBytePos != pak->pakDecoder.decompSize )
      {
        Pak_RTechStreamDecode(
          &pak->pakDecoder,
          fileStream->bytesStreamed,
          pak->processedPatchedDataSize + 0x400000);
        pak->inputBytePos = pak->pakDecoder.inBufBytePos;
      }
      compressedSize = pak->pakDecoder.outBufBytePos;
LABEL_30:
      if ( pak->inputBytePos != fileStreamDescriptior->compressedSize || pak->processedPatchedDataSize != compressedSize )
        goto LABEL_34;
      v20 = ++pak->processedStreamCount;
      pak->resetInBytePos = 1;
    }
    while ( v20 != fileStream->numLoadedFiles );
  }
  compressedSize = pak->processedPatchedDataSize;
LABEL_34:
  qword_548 = pak->qword_548;
  statusMsg = (const char *)(compressedSize - pak->processedPatchedDataSize);
  if ( pak->startOfGuidDescriptorsRelativeToFileStart + qword_548 )
  {
    do
    {
      if ( !pak->qword_560 )
        sub_9570(pak);
    }
    while ( pak->func_568(pak, (size_t *)&statusMsg) && pak->startOfGuidDescriptorsRelativeToFileStart + pak->qword_548 );
  }
  if ( pak->updateBytePosPostProcess )
    pak->inputBytePos = pak->processedPatchedDataSize;
  if ( !fileStream->finishedLoadingPatches )
  {
    numDataChunksProcessed = fileStream->numDataChunksProcessed;
    numDataChunks_1 = fileStream->numDataChunks;
    if ( (unsigned int)(pak->inputBytePos >> 19) < numDataChunksProcessed )
      numDataChunksProcessed = pak->inputBytePos >> 19;
    v24 = (unsigned int)numDataChunks_1;
    for ( i = numDataChunksProcessed + 32; numDataChunks_1 != i; v24 = numDataChunks_1 )
    {
      fileReadStatus = fileStream->fileReadStatus;
      v27 = numDataChunks_1 & 0x1F;
      v28 = (v24 + 1) << 19;
      if ( fileReadStatus == 1 )
      {
        fileStream->fileReadJobs[v27] = -2;
        fileStream->dataChunkStatuses[v27] = 1;
        if ( (((_BYTE)v27 + 1) & 7) == 0 )
          fileStream->fileReadStatus = 2;
        ++fileStream->numDataChunks;
        readStart = (v24 + 1) << 19;
      }
      else
      {
        v29 = fileStream->compressedSize;
        if ( readStart >= v29 )
        {
          if ( pak->lastLoadedPatchIndex >= (unsigned int)pak->header.patchIndex )
          {
            FS_CloseAsyncFile(fileStream->fileHandle);
            fileStream->fileHandle = -1;
            fileStream->readOffset = 0LL;
            fileStream->finishedLoadingPatches = 1;
            return pak->startOfGuidDescriptorsRelativeToFileStart == 0LL;
          }
          if ( !pak->dword_14 )
            return pak->startOfGuidDescriptorsRelativeToFileStart == 0LL;

          sprintf(v42, "r2\\paks\\Win64\\%s", pak->pakFileName);
          lastLoadedPatchIndex = (unsigned int)pak->lastLoadedPatchIndex;
          lastLoadedPatchIndex_cpy = lastLoadedPatchIndex;
          pak->lastLoadedPatchIndex = lastLoadedPatchIndex + 1;
          if ( pak->headerFields.patchFileIndexes[lastLoadedPatchIndex] )
          {
            char* pExtension = nullptr;

            char* it = v42;
            while (*it)
            {
                if (*it == '.')
                    pExtension = it;
                else if (*it == '\\' || *it == '/')
                    pExtension = nullptr;

                ++it;
            }

            if (pExtension)
                it = pExtension;
			snprintf(it, &v42[sizeof(lastLoadedPatchIndex)] - it,
                            "(%02u).rpak", pak->headerFields.patchFileIndexes[pak->lastLoadedPatchIndex]);
          }
          v37 = FS_OpenAsyncFile(v42, (size_t*)&statusMsg);
          //if ( v37 == -1 )
          //  RTechLog_10AB0(4LL, "Couldn't open file \"%s\".\n", v42);
          v38 = lastLoadedPatchIndex_cpy;
          //if ( (unsigned __int64)statusMsg < pak->headerFields.patchCompressPairs[v38].compressedSize )
          //  RTechLog_10AB0(4LL, "File \"%s\" appears truncated.\n", v42);
          FS_CloseAsyncFile(fileStream->fileHandle);
          numDataChunks_2 = fileStream->numDataChunks;
          fileStream->fileHandle = v37;
          v40 = (unsigned __int64)((numDataChunks_2 + 7) & 0xFFFFFFF8) << 19;
          fileStream->readOffset = v40;
          fileStream->fileReadStatus = (numDataChunks_2 == ((numDataChunks_2 + 7) & 0xFFFFFFF8)) + 1;
          fileStream->compressedSize = v40 + pak->headerFields.patchCompressPairs[v38].compressedSize;
        }
        else
        {
          v30 = (unsigned int)v27;
          if ( v28 < v29 )
            v29 = (v24 + 1) << 19;
          AsyncFile = FS_ReadAsyncFile(
                        fileStream->fileHandle,
                        readStart - fileStream->readOffset,
                        v29 - readStart,
                        (__int64)&fileStream->fileBuffer[readStart & fileStream->bufferMask],
                        2);
          readStart = v28;
          fileStream->fileReadJobs[v30] = AsyncFile;
          fileStream->dataChunkStatuses[v30] = fileReadStatus;
          ++fileStream->numDataChunks;
          fileStream->fileReadStatus = 0;
        }
      }
      numDataChunks_1 = fileStream->numDataChunks;
    }
  }
  return pak->startOfGuidDescriptorsRelativeToFileStart == 0LL;
}

static bool Pak_ProcessPakFile(PakFile* const pak)
{
	PakFileStream* const fileStream = &pak->fileStream;
	    // first request is always just the header.
    size_t readStart = sizeof(PakHeader);

    if (fileStream->numDataChunks > 0)
        readStart = fileStream->numDataChunks * PAK_READ_DATA_CHUNK_SIZE;

    for (; fileStream->numDataChunksProcessed != fileStream->numDataChunks; fileStream->numDataChunksProcessed++)
    {
        const int currentDataChunkIndex = fileStream->numDataChunksProcessed & PAK_MAX_DATA_CHUNKS_PER_STREAM_MASK;
        const uint8_t dataChunkStatus = fileStream->dataChunkStatuses[currentDataChunkIndex];

        if (dataChunkStatus != 1)
        {
            size_t bytesProcessed = 0;
            const char* statusMsg = "(no reason)";

            const uint8_t currentStatus = CheckAsyncRequest(fileStream->fileReadJobs[currentDataChunkIndex], &bytesProcessed, &statusMsg);
			NS::log::rpak->error("Status reading pak file \"{}\" -- {}\n", pak->pakFileName, statusMsg);
            if (currentStatus == 2)
                NS::log::rpak->error("Error reading pak file \"{}\" -- {}\n", pak->pakFileName, statusMsg);
            else if (currentStatus == 0)
                break;

            fileStream->bytesStreamed += bytesProcessed;
            if (dataChunkStatus)
            {
                const PakHeader* pakHeader = &pak->header;
                const uint64_t totalDataChunkSizeProcessed = fileStream->numDataChunksProcessed * PAK_READ_DATA_CHUNK_SIZE;

                if (dataChunkStatus == 2)
                {
                    fileStream->bytesStreamed = bytesProcessed + totalDataChunkSizeProcessed;
                    pakHeader = (PakHeader*)&fileStream->fileBuffer[totalDataChunkSizeProcessed & fileStream->bufferMask];
                }

                const uint8_t fileIndex = fileStream->numLoadedFiles++ & PAK_MAX_ASYNC_STREAMED_LOAD_REQUESTS_MASK;

                fileStream->descriptors[fileIndex].dataOffset = totalDataChunkSizeProcessed + sizeof(PakHeader);
                fileStream->descriptors[fileIndex].compressedSize = totalDataChunkSizeProcessed + pakHeader->compressedSize;
                fileStream->descriptors[fileIndex].decompressedSize = pakHeader->decompressedSize;
                fileStream->descriptors[fileIndex].compressionMode = pakHeader->GetCompressionMode();
            }
        }
    }

    size_t currentOutBytePos = pak->processedPatchedDataSize;
	bool loopCompletedFully = true;

    for (; pak->processedStreamCount != fileStream->numLoadedFiles; pak->processedStreamCount++)
    {
        PakFileStream__Descriptor* const streamDesc = &fileStream->descriptors[pak->processedStreamCount & PAK_MAX_ASYNC_STREAMED_LOAD_REQUESTS_MASK];

        if (pak->resetInBytePos)
        {
            pak->resetInBytePos = false;
            pak->inputBytePos = streamDesc->dataOffset;

            if (streamDesc->compressionMode != PakDecodeMode_e::MODE_DISABLED)
            {
                pak->updateBytePosPostProcess = false;
                pak->isCompressed = true;

                pak->processedPatchedDataSize = sizeof(PakHeader);
            }
            else
            {
                pak->updateBytePosPostProcess = true;
                pak->isCompressed = false;

                pak->processedPatchedDataSize = streamDesc->dataOffset;
            }

            if (pak->isCompressed)
            {
                if (streamDesc->compressionMode == PakDecodeMode_e::MODE_ZSTD)
                    pak->pakDecoder.zstreamContext = s_zstdPakDecoder.dctx;

                const size_t decompressedSize = Pak_InitDecoder(&pak->pakDecoder,
                    fileStream->fileBuffer, pak->decompressedBuffer,
                    PAK_DECODE_IN_RING_BUFFER_MASK, PAK_DECODE_OUT_RING_BUFFER_MASK,
                    streamDesc->compressedSize - (streamDesc->dataOffset - sizeof(PakHeader)),
                    streamDesc->dataOffset - sizeof(PakHeader),
					sizeof(PakHeader),
					streamDesc->compressionMode);

                if (decompressedSize != streamDesc->decompressedSize)
					NS::log::rpak->error("Error reading pak file \"{}\" with decoder \"{}\" -- decompressed size {} doesn't match expected value {}\n", pak->pakFileName,
                        Pak_DecoderToString(streamDesc->compressionMode),
                        decompressedSize,
                        pak->header.decompressedSize);
				if (streamDesc->compressionMode == PakDecodeMode_e::MODE_RTECH) {
					pak->pakDecoder.outputMask = 0x3FFFFFLL;
					pak->pakDecoder.outputBuf = pak->decompressedBuffer;
				}
            }
        }
        if (pak->isCompressed)
        {
            currentOutBytePos = pak->pakDecoder.outBufBytePos;

            if (currentOutBytePos != pak->pakDecoder.decompSize)
            {
                if (streamDesc->compressionMode == PakDecodeMode_e::MODE_ZSTD)
                    pak->pakDecoder.allChunksStreamed = fileStream->numDataChunksProcessed == fileStream->numDataChunks;

                const bool didDecode = Pak_StreamToBufferDecode(&pak->pakDecoder, 
                    fileStream->bytesStreamed, (pak->processedPatchedDataSize + PAK_DECODE_OUT_RING_BUFFER_SIZE), streamDesc->compressionMode);

                currentOutBytePos = pak->pakDecoder.outBufBytePos;
                pak->inputBytePos = pak->pakDecoder.inBufBytePos;

                if (didDecode)
                {
					NS::log::rpak->info("{}: pak '{}' decoded successfully with decoder '{}'\n", __FUNCTION__, pak->pakFileName, Pak_DecoderToString(streamDesc->compressionMode));
                    pak->pakDecoder.zstreamContext = nullptr;
                }
            }
        }
        else
        {
            currentOutBytePos = std::min(streamDesc->compressedSize, fileStream->bytesStreamed);
        }

        if (pak->inputBytePos != streamDesc->compressedSize || pak->processedPatchedDataSize != currentOutBytePos) {
			loopCompletedFully = false;
			break;
		}

        pak->resetInBytePos = true;
        currentOutBytePos = pak->processedStreamCount;
    }

	if (loopCompletedFully)
		currentOutBytePos = pak->processedPatchedDataSize;

	size_t numBytesToProcess = currentOutBytePos - pak->processedPatchedDataSize;

	  if ( pak->startOfGuidDescriptorsRelativeToFileStart + pak->qword_548 )
	  {
		do
		{
		  if ( !pak->qword_560 )
			sub_9570(pak);
		}
		while ( pak->func_568(pak, (size_t *)&numBytesToProcess) && pak->startOfGuidDescriptorsRelativeToFileStart + pak->qword_548 );
	  }

    if (pak->updateBytePosPostProcess)
        pak->inputBytePos = pak->processedPatchedDataSize;

    if (!fileStream->finishedLoadingPatches)
    {
        const size_t numDataChunksProcessed = std::min<size_t>(fileStream->numDataChunksProcessed, pak->inputBytePos >> 19);

        while (fileStream->numDataChunks != numDataChunksProcessed + 32)
        {
            const int8_t requestIdx = fileStream->numDataChunks & PAK_MAX_DATA_CHUNKS_PER_STREAM_MASK;
            const int64_t readOffsetEnd = (fileStream->numDataChunks + 1ull) * PAK_READ_DATA_CHUNK_SIZE;

            if (fileStream->fileReadStatus == 1)
            {
                fileStream->fileReadJobs[requestIdx] = -2;
                fileStream->dataChunkStatuses[requestIdx] = 1;

                if (((requestIdx + 1) & PAK_MAX_ASYNC_STREAMED_LOAD_REQUESTS_MASK) == 0)
                    fileStream->fileReadStatus = 2;

                ++fileStream->numDataChunks;
                readStart = readOffsetEnd;
            }
            else
            {
                if (readStart < fileStream->compressedSize)
                {
                    const size_t lenToRead = std::min(fileStream->compressedSize, readOffsetEnd);

                    const size_t readOffset = readStart - fileStream->readOffset;
                    const size_t readSize = lenToRead - readStart;

                    fileStream->fileReadJobs[requestIdx] = FS_ReadAsyncFile(
                             fileStream->fileHandle,
                             readStart - fileStream->readOffset,
                             fileStream->compressedSize - readStart,
                             (__int64)&fileStream->fileBuffer[readStart & fileStream->bufferMask],
                             2);

                    fileStream->dataChunkStatuses[requestIdx] = fileStream->fileReadStatus;
                    fileStream->fileReadStatus = 0;

                    ++fileStream->numDataChunks;
                    readStart = readOffsetEnd;
                }
                else
                {
                    if (pak->lastLoadedPatchIndex >= pak->header.patchIndex)
                    {
                        FS_CloseAsyncFile(fileStream->fileHandle);
                        fileStream->fileHandle = -1;
                        fileStream->readOffset = 0;
                        fileStream->finishedLoadingPatches = true;

                        return pak->startOfGuidDescriptorsRelativeToFileStart == 0;
                    }

                    if (!pak->dword_14)
                        return pak->startOfGuidDescriptorsRelativeToFileStart == 0;

                    char pakPatchPath[MAX_PATH] = {};
                    sprintf(pakPatchPath, "%s%s", "r2\\Win64\\", pak->pakFileName);

                    // get path of next patch rpak to load
                    if (pak->headerFields.patchFileIndexes[pak->lastLoadedPatchIndex])
                    {
                        char* pExtension = nullptr;

                        char* it = pakPatchPath;
                        while (*it)
                        {
                            if (*it == '.')
                                pExtension = it;
                            else if (*it == '\\' || *it == '/')
                                pExtension = nullptr;

                            ++it;
                        }

                        if (pExtension)
                            it = pExtension;

                        // replace extension '.rpak' with '(xx).rpak'
                        snprintf(it, &pakPatchPath[sizeof(pakPatchPath)] - it,
                            "(%02u).rpak",pak->headerFields.patchFileIndexes[pak->lastLoadedPatchIndex]);
                    }

                    const int patchFileHandle = FS_OpenAsyncFile(pakPatchPath, &numBytesToProcess);

                    if (patchFileHandle == -1)
                        NS::log::rpak->error("Couldn't open file \"{}\".\n", pakPatchPath);

                    //if (numBytesToProcess < pak->headerFields.patchCompressPairs[pak->lastLoadedPatchIndex].compressedSize)
                    //    Error(eDLL_T::RTECH, EXIT_FAILURE, "File \"%s\" appears truncated; read size: %zu < expected size: %zu.\n",
                    //        pakPatchPath, numBytesToProcess, pak->headerFields.patchCompressPairs[pak->lastLoadedPatchIndex].compressedSize);

                    FS_CloseAsyncFile(fileStream->fileHandle);

                    fileStream->fileHandle = patchFileHandle;

                    const size_t readOffset = ALIGN_VALUE(fileStream->numDataChunks, 8ull) * PAK_READ_DATA_CHUNK_SIZE;
                    fileStream->fileReadStatus = (fileStream->numDataChunks == ALIGN_VALUE(fileStream->numDataChunks, 8ull)) + 1;

                    fileStream->readOffset = readOffset;
					fileStream->compressedSize = readOffset + pak->headerFields.patchCompressPairs[pak->lastLoadedPatchIndex].compressedSize;
                    pak->lastLoadedPatchIndex++;
                }
            }
        }
    }

    return pak->startOfGuidDescriptorsRelativeToFileStart == 0;
};


using Pak_ProcessFile_t = bool(__fastcall*)(PakFile* pak);
Pak_ProcessFile_t pPak_ProcessPakFile = nullptr;
HOOK(v_Pak_ProcessPakFile, o_Pak_ProcessPakFile, bool, __fastcall, (PakFile* pak))
{
	//return o_Pak_ProcessPakFile(pak);
	return Pak_ProcessPakFile_8D10(pak);
	//return Pak_ProcessPakFile(pak);
}
ON_DLL_LOAD("engine.dll", PakParse, [](CModule module)
{
	CModule rtechModule(GetModuleHandleA("rtech_game.dll"));
	pPak_ProcessPakFile = rtechModule.Offset(0x8D10).RCast<Pak_ProcessFile_t>();
	v_Pak_ProcessPakFile.Dispatch(reinterpret_cast<LPVOID*>(pPak_ProcessPakFile));
});

ON_DLL_LOAD("rtech_game.DLL", PakParseRtech, [](CModule module)
{
	Pak_RTechDecoderInit = module.Offset( 0x4B80 ).RCast<size_t (*)(PakDecompState* const, const uint8_t* const, const uint64_t, const size_t, const size_t, const size_t)>();
	Pak_RTechStreamDecode = module.Offset( 0x4C20 ).RCast<bool (*)(PakDecompState* const, const size_t, const size_t)>();
	CheckAsyncRequest = module.Offset( 0x1AF0 ).RCast<int64_t (*)(int64_t, size_t*, const char**)>();
	sub_9570 = module.Offset( 0x9570 ).RCast<void (*)(PakFile*)>();
	FS_ReadAsyncFile = module.Offset( 0x1F00 ).RCast<int64_t (*)(unsigned int, __int64, unsigned __int64, __int64, int)>();
	FS_CloseAsyncFile = module.Offset( 0x2100 ).RCast<void (*)(unsigned int)>();
	FS_OpenAsyncFile = module.Offset( 0x1E20 ).RCast<int16_t (*)(const char*, size_t*)>();
})
