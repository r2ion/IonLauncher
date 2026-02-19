#include "pakfile.h"
#include <util/zstdutils.h>

DECLARE_MODULE(PakParseHooks)

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
	decoder->bufferSizeNeeded = decoder->input_byte_pos + decoder->frameHeaderSize;

	// must include header size
	decoder->decompressed_size = dctx->fParams.frameContentSize + headerSize;
	return decoder->decompressed_size;
}

size_t Pak_InitDecoder(PakDecompState* const decoder, const uint8_t* const inputBuf, uint8_t* const outputBuf,
	const uint64_t inputMask, const uint64_t outputMask, const size_t dataSize, const size_t dataOffset,
	const size_t headerSize, const PakDecodeMode_e decodeMode)
{
	// buffer size must be power of two as we index into buffers using a bit
	// mask rather than a modulo, the mask provided must be bufferSize-1
	assert(IsPowerOfTwo(inputMask + 1));
	assert(IsPowerOfTwo(outputMask + 1));

	// the absolute start address of the input and output buffers
	decoder->input_buf = inputBuf;
	decoder->out = outputBuf;

	// the actual file size, which consists of dataOffset (anything up to the
	// frame header, like the file header) and the actual encoded data itself
	decoder->fileSize = dataOffset + dataSize;
	decoder->decodeMode = decodeMode;

	// buffer masks, which essentially gets used to index into the input and
	// output buffers, similar to 'idx % bufSize', where bufSize = bufMask+1
	decoder->inv_mask_in = inputMask;
	decoder->inv_mask_out = outputMask;

	// the current positions in the input and output buffers; if we deal with
	// paks that are patched, the input buffer position during the init and
	// decode call on subsequent patches may not be at the start of the buffer,
	// they will end where the previous 'to patch' pak had finished streaming
	// and decoding
	decoder->input_byte_pos = dataOffset + headerSize;
	decoder->outBufBytePos = headerSize;

	// if we use the default RTech decoder, return from here as the stuff below
	// is handled by the RTech decoder internally
	if (decodeMode == 0)
		return Pak_RTechDecoderInit(decoder, inputBuf, inputMask, dataSize, dataOffset, headerSize);

	// NOTE: on RTech encoded paks this data is parsed out of the frame header,
	// but for ZStd encoded paks we are always limiting this to the ring buffer
	// size
	decoder->out_mask = PAK_DECODE_OUT_RING_BUFFER_MASK;

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
	const uint64_t bufPosAligned = (decoder->outBufBytePos & ~decoder->out_mask);
	const uint64_t threshold = decoder->out_mask + bufPosAligned + 1;

	// make sure caller has copied all data out the ring buffer first before
	// overwriting it with new decoded data
	return (outLen >= threshold || outLen >= decoder->decompressed_size);
}

static PakRingBufferFrame_s Pak_DetermineRingBufferFrame(const uint64_t bufMask, const size_t seekPos, const size_t dataLen)
{
	PakRingBufferFrame_s ring;
	ring.bufIndex = seekPos & bufMask;

	// the total amount of bytes used and available in this frame
	const size_t bytesUsed = ring.bufIndex & bufMask;
	const size_t totalAvail = bufMask +1 - bytesUsed;

	// the last part of the data might be smaller than the remainder of the ring
	// buffer; clamp it
	ring.frameLen = std::min(dataLen - seekPos, totalAvail);
	return ring;
}

static bool Pak_ZStdStreamDecode(PakDecompState* const decoder, const PakRingBufferFrame_s& outFrame, const PakRingBufferFrame_s& inFrame)
{
	ZSTD_outBuffer outBuffer = {
		&decoder->out[outFrame.bufIndex],
		outFrame.frameLen, NULL
	};

	ZSTD_inBuffer inBuffer = {
		&decoder->input_buf[inFrame.bufIndex],
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
	decoder->input_byte_pos += inBuffer.pos;

	// NOTE: if inBuffer.pos < inBuffer.size, we made full use of the output
	// buffer and couldn't decode any more data into it. the decoded data needs
	// to be copied out to the destination so we can reuse the ring buffer and
	// process the remainder of this frame. in these cases we do not update the
	// bufferSizeNeeded objective below as we still have data left to process.
	if (inBuffer.pos == inBuffer.size)
		decoder->bufferSizeNeeded = decoder->input_byte_pos + ZSTD_DStreamInSize();

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

	const PakRingBufferFrame_s inFrame = Pak_DetermineRingBufferFrame(decoder->inv_mask_in, decoder->input_byte_pos, inLen);
	//// if the file size is smaller than the provided output length, clamp it.
	//// this happens when the buffer is smaller than the default buffer size
	//// defined by 'PAK_DECODE_OUT_RING_BUFFER_SIZE'. just like how the rtech
	//// decoder clamps it internally, we should do it here to avoid an overflow.
	const PakRingBufferFrame_s outFrame = Pak_DetermineRingBufferFrame(decoder->out_mask, decoder->outBufBytePos, std::min(decoder->decompressed_size, outLen));

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
                    streamDesc->dataOffset - sizeof(PakHeader), sizeof(PakHeader), streamDesc->compressionMode);

                //if (decompressedSize != streamDesc->decompressedSize)
                //    Error(eDLL_T::RTECH, EXIT_FAILURE,
                //        "Error reading pak file \"%s\" with decoder \"%s\" -- decompressed size %zu doesn't match expected value %zu\n",
                //        pak->pakFileName,
                //        Pak_DecoderToString(streamDesc->compressionMode),
                //        decompressedSize,
                //        pak->header.decompressedSize);
            }
        }

        if (pak->isCompressed)
        {
            currentOutBytePos = pak->pakDecoder.outBufBytePos;

            if (currentOutBytePos != pak->pakDecoder.decompressed_size)
            {
                if (streamDesc->compressionMode == PakDecodeMode_e::MODE_ZSTD)
                    pak->pakDecoder.allChunksStreamed = fileStream->numDataChunksProcessed == fileStream->numDataChunks;

                const bool didDecode = Pak_StreamToBufferDecode(&pak->pakDecoder, 
                    fileStream->bytesStreamed, (pak->processedPatchedDataSize + PAK_DECODE_OUT_RING_BUFFER_SIZE), streamDesc->compressionMode);

                currentOutBytePos = pak->pakDecoder.outBufBytePos;
                pak->inputBytePos = pak->pakDecoder.input_byte_pos;

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

        if (pak->inputBytePos != streamDesc->compressedSize || pak->processedPatchedDataSize != currentOutBytePos)
            break;

        pak->resetInBytePos = true;
        currentOutBytePos = pak->processedStreamCount;
    }

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

                    //if (patchFileHandle == -1)
                    //    Error(eDLL_T::RTECH, EXIT_FAILURE, "Couldn't open file \"%s\".\n", pakPatchPath);

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

DECLARE_HOOK(Pak_ProcessPakFile_h, rtech_game.DLL + 0x8D10, [](auto& hook, PakFile* pak) -> bool {
	return Pak_ProcessPakFile(pak);
});


ON_DLL_LOAD("rtech_game.DLL", PakParse, [](CModule module)
{
	Pak_RTechDecoderInit = module.Offset( 0x4B80 ).RCast<size_t (*)(PakDecompState* const, const uint8_t* const, const uint64_t, const size_t, const size_t, const size_t)>();
	Pak_RTechStreamDecode = module.Offset( 0x4C20 ).RCast<bool (*)(PakDecompState* const, const size_t, const size_t)>();
	CheckAsyncRequest = module.Offset( 0x1AF0 ).RCast<int64_t (*)(int64_t, size_t*, const char**)>();
	sub_9570 = module.Offset( 0x9570 ).RCast<void (*)(PakFile*)>();
	FS_ReadAsyncFile = module.Offset( 0x1F00 ).RCast<int64_t (*)(unsigned int, __int64, unsigned __int64, __int64, int)>();
	FS_CloseAsyncFile = module.Offset( 0x2100 ).RCast<void (*)(unsigned int)>();
	FS_OpenAsyncFile = module.Offset( 0x1E20 ).RCast<int16_t (*)(const char*, size_t*)>();
	DISPATCH_MODULE(PakParseHooks)
})
