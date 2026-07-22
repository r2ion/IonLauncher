#include "pakfile.h"
#include "pakdecode.h"
#include "pakpatch.h"
#include "pakstate.h"

#include <array>
#include <filesystem>
#include <fstream>
struct AsyncHandleStatus_s
{
	enum Status_e: uint8_t
	{
		// the file is still pending, or being read at this moment
		FS_ASYNC_PENDING = 0,

		// the file is ready to be used
		FS_ASYNC_READY,

		// there was an error while reading the file
		FS_ASYNC_ERROR,

		// async read operations were canceled
		FS_ASYNC_CANCELLED
	};
};

static uint64_t rtechBaseAddr;
static constexpr size_t PAK_ZSTD_FRAME_HEADER_MAX_SIZE = 18;

#define ALIGN_VALUE( val, alignment ) ( ( val + alignment - 1 ) & ~( alignment - 1 ) ) 

#define CMD_INVALID -1

// only patch cmds 4,5,6 use this array to determine their data size
static const int s_patchCmdToBytesToProcess[] = { CMD_INVALID, CMD_INVALID, CMD_INVALID, CMD_INVALID, 3, 7, 6, 0 };
#undef CMD_INVALID

uint8_t (*Pak_PollAsyncRead)(uint8_t requestId, uint64_t* bytesRead, const char** errorText);
int (*Pak_QueueAsyncRead)(PakHandle_t fileHandle, uint64_t fileOffset, uint64_t size, void* destination, int queueIndex);
void (*Pak_ReleaseFileHandle)(PakHandle_t fileHandle);
PakHandle_t (*Pak_OpenFile)(const char* filename, uint64_t* fileSize);

void Pak_ReleaseZStdDecoder(RTechDecodeState_s* const decoder)
{
	if (!decoder || decoder->decodeMode != PakDecodeMode_e::MODE_ZSTD)
		return;

	if (decoder->zstreamContext)
		ZSTD_freeDStream(decoder->zstreamContext);

	decoder->zstreamContext = nullptr;
	decoder->decodeMode = PakDecodeMode_e::MODE_DISABLED;
}

static size_t Pak_ZStdDecoderInit(RTechDecodeState_s* const decoder,
	const size_t dataSize, const size_t headerSize, const size_t expectedDecompressedSize)
{
	if (dataSize <= headerSize)
		return 0;

	const size_t frameSize = dataSize - headerSize;
	const size_t headerBytes = std::min(frameSize, PAK_ZSTD_FRAME_HEADER_MAX_SIZE);
	std::array<uint8_t, PAK_ZSTD_FRAME_HEADER_MAX_SIZE> frameHeader{};

	for (size_t i = 0; i < headerBytes; ++i)
		frameHeader[i] = decoder->inputBuf[(decoder->inBufBytePos + i) & decoder->inputMask];

	unsigned long long frameContentSize = ZSTD_getFrameContentSize(frameHeader.data(), headerBytes);
	if (frameContentSize == ZSTD_CONTENTSIZE_ERROR)
	{
		NS::log::rpak->error("{}: invalid Zstd frame header", __FUNCTION__);
		return 0;
	}
	if (frameContentSize == ZSTD_CONTENTSIZE_UNKNOWN)
	{
		if (expectedDecompressedSize < headerSize)
			return 0;
		frameContentSize = expectedDecompressedSize - headerSize;
	}
	if (frameContentSize > SIZE_MAX - headerSize)
		return 0;

	decoder->zstreamContext = ZSTD_createDStream();
	if (!decoder->zstreamContext)
	{
		NS::log::rpak->error("{}: failed to allocate Zstd decoder", __FUNCTION__);
		return 0;
	}

	decoder->nextInputSize = ZSTD_initDStream(decoder->zstreamContext);
	if (ZSTD_isError(decoder->nextInputSize))
	{
		NS::log::rpak->error("{}: failed to initialize Zstd decoder: {}", __FUNCTION__, ZSTD_getErrorName(decoder->nextInputSize));
		Pak_ReleaseZStdDecoder(decoder);
		return 0;
	}

	decoder->allChunksStreamed = false;
	decoder->bufferSizeNeeded = decoder->inBufBytePos + decoder->nextInputSize;
	decoder->decompSize = static_cast<size_t>(frameContentSize) + headerSize;
	return decoder->decompSize;
}

static size_t Pak_RTechDecoderInit(RTechDecodeState_s* const decoder, const uint8_t* const fileBuffer,
	const uint64_t inputMask, const size_t dataSize, const size_t dataOffset, const size_t headerSize)
{
	uint64_t frameHeader = *(_QWORD*)((inputMask & (dataOffset + headerSize)) + fileBuffer);
	const int decompressedSizeBits = frameHeader & 0x3F;

	frameHeader >>= 6;
	decoder->decompSize = (1i64 << decompressedSizeBits) | frameHeader & ((1i64 << decompressedSizeBits) - 1);

	const uint64_t bytePos = dataOffset + headerSize + 8;
	const int64_t currByteLow = *(_QWORD*)((inputMask & bytePos) + fileBuffer) << (64 - ((uint8_t)decompressedSizeBits + 6));

	decoder->inBufBytePos = bytePos + ((uint64_t)(uint32_t)(decompressedSizeBits + 6) >> 3);
	const uint32_t bitPosFinal = ((decompressedSizeBits + 6) & 7) + 13;

	const uint64_t currByte = (0xFFFFFFFFFFFFFFFFui64 >> ((decompressedSizeBits + 6) & 7)) & ((frameHeader >> decompressedSizeBits) | currByteLow);
	const uint32_t currbits = (((_BYTE)currByte - 1) & 0x3F) + 1;

	const uint64_t invMaskIn = 0xFFFFFFFFFFFFFFFFui64 >> (64 - (uint8_t)currbits);
	decoder->inputInvMask = invMaskIn;

	const uint64_t invMaskOut = 0xFFFFFFFFFFFFFFFFui64 >> (64 - ((((currByte >> 6) - 1) & 0x3F) + 1));
	decoder->outputInvMask = invMaskOut;

	const uint64_t finalByteFull = (currByte >> 13) | (*(_QWORD*)((inputMask & decoder->inBufBytePos) + fileBuffer) << (64 - (uint8_t)bitPosFinal));
	const uint32_t finalBitOffset = bitPosFinal & 7;

	decoder->inBufBytePos += bitPosFinal >> 3;
	const uint64_t finalByte = (0xFFFFFFFFFFFFFFFFui64 >> finalBitOffset) & finalByteFull;

	if (decoder->inputInvMask == 0xFFFFFFFFFFFFFFFFui64)
	{
		decoder->headerOffset = 0;
		decoder->bufferSizeNeeded = dataSize;
	}
	else
	{
		const uint64_t finalPos = inputMask & decoder->inBufBytePos;
		decoder->headerOffset = (currbits >> 3) + 1;
		decoder->inBufBytePos += (currbits >> 3) + 1;
		decoder->bufferSizeNeeded = *(_QWORD*)(finalPos + fileBuffer) & ((1i64 << (8 * ((uint8_t)(currbits >> 3) + 1))) - 1);
	}

	decoder->bufferSizeNeeded += dataOffset;
	decoder->currentByte = finalByte;
	decoder->currentBit = finalBitOffset;
	decoder->qword70 = decoder->inputInvMask + dataOffset - 6;
	decoder->dword6C = 0;
	decoder->compressedStreamSize = decoder->bufferSizeNeeded;
	decoder->decompressedStreamSize = decoder->decompSize;

	if ((((uint8_t)(currByte >> 6) - 1) & 0x3F) != -1i64 && decoder->decompSize - 1 > decoder->outputInvMask)
	{
		const uint64_t streamCompressedSize = decoder->bufferSizeNeeded - decoder->headerOffset;
		decoder->compressedStreamSize = streamCompressedSize;

		decoder->decompressedStreamSize = decoder->outputInvMask + 1;
	}

	return decoder->decompSize;
}


static bool Pak_RTechStreamDecode(RTechDecodeState_s* const decoder, const size_t inLen, const size_t outLen)
{
	bool result; // al
	uint64_t outBufBytePos; // r15
	uint8_t* outputBuf; // r11
	uint32_t currentBit; // ebp
	uint64_t currentByte; // rsi
	uint64_t inBufBytePos; // rdi
	size_t qword70; // r12
	const uint8_t* inputBuf; // r13
	uint32_t dword6C; // ecx
	uint64_t v13; // rsi
	unsigned __int64 i; // rax
	unsigned __int64 v15; // r8
	__int64 v16; // r9
	int v17; // ecx
	unsigned __int64 v18; // rax
	uint64_t v19; // rsi
	__int64 v20; // r14
	int v21; // ecx
	unsigned __int64 v22; // r11
	int v23; // edx
	uint64_t outputMask; // rax
	int v25; // r8d
	unsigned int v26; // r13d
	uint64_t v27; // r10
	uint8_t* v28; // rax
	uint8_t* v29; // r10
	size_t decompSize; // r9
	uint64_t inputInvMask; // r10
	uint64_t headerOffset; // r8
	uint64_t v33; // rax
	uint64_t v34; // rax
	uint64_t v35; // rax
	size_t v36; // rcx
	__int64 v37; // rdx
	size_t v38; // r14
	size_t v39; // r11
	uint64_t v40; // cl
	uint64_t v41; // rsi
	__int64 v42; // rcx
	uint64_t v43; // r8
	int v44; // r11d
	unsigned __int8 v45; // r9
	uint64_t v46; // rcx
	uint64_t v47; // rcx
	__int64 v48; // r9
	__int64 m; // r8
	__int64 v50; // r9d
	__int64 v51; // r8
	__int64 v52; // rdx
	__int64 k; // r8
	signed __int64 v54; // r10
	__int64 v55; // rdx
	unsigned int v56; // r14d
	const uint8_t* v57; // rdx
	uint8_t* v58; // r8
	uint64_t v59; // al
	uint64_t v60; // rsi
	__int64 v61; // rax
	uint64_t v62; // r9
	int v63; // r10d
	unsigned __int8 v64; // cl
	uint64_t v65; // rax
	unsigned int v66; // r14d
	unsigned int j; // ecx
	__int64 v68; // rax
	uint64_t v69; // rcx
	uint8_t* v70; // [rsp+0h] [rbp-58h]
	uint32_t v71; // [rsp+60h] [rbp+8h]
	const uint8_t* v74; // [rsp+78h] [rbp+20h]

	outBufBytePos = decoder->outBufBytePos;

	outputBuf = decoder->outputBuf;
	currentBit = decoder->currentBit;
	currentByte = decoder->currentByte;
	inBufBytePos = decoder->inBufBytePos;
	qword70 = decoder->qword70;
	inputBuf = decoder->inputBuf;

	if (decoder->compressedStreamSize < qword70)
		qword70 = decoder->compressedStreamSize;

	dword6C = decoder->dword6C;
	v74 = inputBuf;
	v70 = outputBuf;
	v71 = dword6C;
	if (!currentBit)
		goto LABEL_11;

	v13 = (*(_QWORD*)&inputBuf[inBufBytePos & decoder->inputMask] << (64 - (unsigned __int8)currentBit)) | currentByte;
	for (i = currentBit; ; i = currentBit)
	{
		currentBit &= 7u;
		inBufBytePos += i >> 3;
		dword6C = v71;
		currentByte = (0xFFFFFFFFFFFFFFFFui64 >> currentBit) & v13;
	LABEL_11:
		v15 = (unsigned __int64)dword6C << 8;
		v16 = dword6C;
		v17 = s_defaultDecoderLUT[(unsigned __int8)currentByte + 512 + v15];
		v18 = (unsigned __int8)currentByte + v15;
		currentBit += v17;
		v19 = currentByte >> v17;
		v20 = (unsigned int)(char)s_defaultDecoderLUT[v18];
		if ((s_defaultDecoderLUT[v18] & 0x80u) != 0)
		{
			v56 = -(int)v20;
			v57 = &inputBuf[inBufBytePos & decoder->inputMask];
			v71 = 1;
			v58 = &outputBuf[outBufBytePos & decoder->outputMask];
			if (v56 == s_defaultDecoderLUT[v16 + 1248])
			{
				if ((~inBufBytePos & decoder->inputInvMask) < 0xF || (decoder->outputInvMask & ~outBufBytePos) < 0xF || decoder->decompSize - outBufBytePos < 0x10)
					v56 = 1;
				v59 = v19;
				v60 = v19 >> 3;
				v61 = v59 & 7;
				v62 = v60;
				if (v61)
				{
					v63 = s_defaultDecoderLUT[v61 + 1232];
					v64 = s_defaultDecoderLUT[v61 + 1240];
				}
				else
				{
					v62 = v60 >> 4;
					v65 = v60 & 0xF;
					currentBit += 4;
					v63 = *(_DWORD*)&s_defaultDecoderLUT[4 * v65 + 1152];
					v64 = s_defaultDecoderLUT[v65 + 1216];
				}
				currentBit += v64 + 3;
				v19 = v62 >> v64;
				v66 = v63 + (v62 & ((1 << v64) - 1)) + v56;
				for (j = v66 >> 3; j; --j)
				{
					v68 = *(_QWORD*)v57;
					v57 += 8;
					*(_QWORD*)v58 = v68;
					v58 += 8;
				}
				if ((v66 & 4) != 0)
				{
					*(_DWORD*)v58 = *(_DWORD*)v57;
					v58 += 4;
					v57 += 4;
				}
				if ((v66 & 2) != 0)
				{
					*(_WORD*)v58 = *(_WORD*)v57;
					v58 += 2;
					v57 += 2;
				}
				if ((v66 & 1) != 0)
					*v58 = *v57;
				inBufBytePos += v66;
				outBufBytePos += v66;
			}
			else
			{
				*(_QWORD*)v58 = *(_QWORD*)v57;
				*((_QWORD*)v58 + 1) = *((_QWORD*)v57 + 1);
				inBufBytePos += v56;
				outBufBytePos += v56;
			}
		}
		else
		{
			v21 = v19 & 0xF;
			v71 = 0;
			v22 = ((unsigned __int64)(unsigned int)v19 >> (((unsigned int)(v21 - 31) >> 3) & 6)) & 0x3F;
			v23 = 1 << (v21 + ((v19 >> 4) & ((24 * (((unsigned int)(v21 - 31) >> 3) & 2)) >> 4)));
			currentBit += (((unsigned int)(v21 - 31) >> 3) & 6) + s_defaultDecoderLUT[v22 + 1088] + v21 + ((v19 >> 4) & ((24 * (((unsigned int)(v21 - 31) >> 3) & 2)) >> 4));
			outputMask = decoder->outputMask;
			v25 = 16 * (v23 + ((v23 - 1) & (v19 >> ((((unsigned int)(v21 - 31) >> 3) & 6) + s_defaultDecoderLUT[v22 + 1088]))));
			v19 >>= (((unsigned int)(v21 - 31) >> 3) & 6) + s_defaultDecoderLUT[v22 + 1088] + v21 + ((v19 >> 4) & ((24 * (((unsigned int)(v21 - 31) >> 3) & 2)) >> 4));
			v26 = v25 + s_defaultDecoderLUT[v22 + 1024] - 16;
			v27 = outputMask & (outBufBytePos - v26);
			v28 = &v70[outBufBytePos & outputMask];
			v29 = &v70[v27];
			if ((_DWORD)v20 == 17)
			{
				v40 = v19;
				v41 = v19 >> 3;
				v42 = v40 & 7;
				v43 = v41;
				if (v42)
				{
					v44 = s_defaultDecoderLUT[v42 + 1232];
					v45 = s_defaultDecoderLUT[v42 + 1240];
				}
				else
				{
					currentBit += 4;
					v46 = v41 & 0xF;
					v43 = v41 >> 4;
					v44 = *(_DWORD*)&s_defaultDecoderLUT[4 * v46 + 1152];
					v45 = s_defaultDecoderLUT[v46 + 1216];
					if (v74 && currentBit + v45 >= 61)
					{
						v47 = inBufBytePos++ & decoder->inputMask;
						v43 |= (unsigned __int64)v74[v47] << (61 - (unsigned __int8)currentBit);
						currentBit -= 8;
					}
				}
				currentBit += v45 + 3;
				v19 = v43 >> v45;
				v48 = ((unsigned int)v43 & ((1 << v45) - 1)) + v44 + 17;
				outBufBytePos += v48;
				if (v26 < 8)
				{
					v50 = v48 - 13;
					outBufBytePos -= 13i64;
					if (v26 == 1)
					{
						v51 = *v29;
						//++dword_14D40B2BC;
						v52 = 0i64;
						for (k = 0x101010101010101i64 * v51; (unsigned int)v52 < v50; v52 = (unsigned int)(v52 + 8))
							*(_QWORD*)&v28[v52] = k;
					}
					else
					{
						//++dword_14D40B2B8;
						if (v50)
						{
							v54 = v29 - v28;
							v55 = v50;
							do
							{
								*v28 = v28[v54];
								++v28;
								--v55;
							} while (v55);
						}
					}
				}
				else
				{
					//++dword_14D40B2AC;
					for (m = 0i64; (unsigned int)m < (unsigned int)v48; m = (unsigned int)(m + 8))
						*(_QWORD*)&v28[m] = *(_QWORD*)&v29[m];
				}
			}
			else
			{
				outBufBytePos += v20;
				*(_QWORD*)v28 = *(_QWORD*)v29;
				*((_QWORD*)v28 + 1) = *((_QWORD*)v29 + 1);
			}
			inputBuf = v74;
		}
		if (inBufBytePos >= qword70)
			break;
	LABEL_29:
		outputBuf = v70;
		v13 = (*(_QWORD*)&inputBuf[inBufBytePos & decoder->inputMask] << (64 - (unsigned __int8)currentBit)) | v19;
	}
	if (outBufBytePos != decoder->decompressedStreamSize)
		goto LABEL_25;
	decompSize = decoder->decompSize;
	if (outBufBytePos == decompSize)
	{
		result = true;
		goto LABEL_69;
	}
	inputInvMask = decoder->inputInvMask;
	headerOffset = decoder->headerOffset;
	v33 = inputInvMask & -(__int64)inBufBytePos;
	v19 >>= 1;
	++currentBit;
	if (headerOffset > v33)
	{
		inBufBytePos += v33;
		v34 = decoder->qword70;
		if (inBufBytePos > v34)
			decoder->qword70 = inputInvMask + v34 + 1;
	}
	v35 = inBufBytePos & decoder->inputMask;
	inBufBytePos += headerOffset;
	v36 = outBufBytePos + decoder->outputInvMask + 1;
	v37 = *(_QWORD*)&inputBuf[v35] & ((1i64 << (8 * (unsigned __int8)headerOffset)) - 1);
	v38 = v37 + decoder->bufferSizeNeeded;
	v39 = v37 + decoder->compressedStreamSize;
	decoder->bufferSizeNeeded = v38;
	decoder->compressedStreamSize = v39;
	if (v36 >= decompSize)
	{
		v36 = decompSize;
		decoder->compressedStreamSize = headerOffset + v39;
	}
	decoder->decompressedStreamSize = v36;
	if (inLen >= v38 && outLen >= v36)
	{
	LABEL_25:
		qword70 = decoder->qword70;
		if (inBufBytePos >= qword70)
		{
			inBufBytePos = ~decoder->inputInvMask & (inBufBytePos + 7);
			qword70 += decoder->inputInvMask + 1;
			decoder->qword70 = qword70;
		}
		if (decoder->compressedStreamSize < qword70)
			qword70 = decoder->compressedStreamSize;
		goto LABEL_29;
	}
	v69 = decoder->qword70;
	if (inBufBytePos >= v69)
	{
		inBufBytePos = ~inputInvMask & (inBufBytePos + 7);
		decoder->qword70 = v69 + inputInvMask + 1;
	}
	decoder->dword6C = v71;
	result = false;
	decoder->currentByte = v19;
	decoder->currentBit = currentBit;
LABEL_69:
	decoder->outBufBytePos = outBufBytePos;
	decoder->inBufBytePos = inBufBytePos;
	return result;
}

size_t Pak_InitDecoder(RTechDecodeState_s* decoder, const uint8_t* const inputBuf, uint8_t* const outputBuf,
	const uint64_t inputMask, const uint64_t outputMask, const size_t dataSize, const size_t dataOffset,
	const size_t headerSize, const size_t expectedDecompressedSize, const PakDecodeMode_e decodeMode)
{
	assert((inputMask & (inputMask + 1)) == 0);
	assert((outputMask & (outputMask + 1)) == 0);

	if (decoder->decodeMode == PakDecodeMode_e::MODE_ZSTD)
		Pak_ReleaseZStdDecoder(decoder);
	else if (decodeMode == PakDecodeMode_e::MODE_ZSTD)
		decoder->zstreamContext = nullptr;

	// the absolute start address of the input and output buffers


	// the actual file size, which consists of dataOffset (anything up to the
	// frame header, like the file header) and the actual encoded data itself
	decoder->fileSize = dataOffset + dataSize;
	decoder->decodeMode = decodeMode;

	decoder->inputBuf = inputBuf;
	decoder->outputBuf = outputBuf;

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

	if(decodeMode == PakDecodeMode_e::MODE_RTECH)
		return Pak_RTechDecoderInit(decoder, inputBuf,inputMask,dataSize,dataOffset,headerSize);

	decoder->outputInvMask = outputMask;

	return Pak_ZStdDecoderInit(decoder, dataSize, headerSize, expectedDecompressedSize);
}

static bool Pak_HasEnoughDecodeBufferAvailable(RTechDecodeState_s* const decoder, const size_t outLen)
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
	ring.frameLen = 0;

	if (seekPos >= dataLen)
		return ring;

	// the total amount of bytes used and available in this frame
	const size_t bytesUsed = ring.bufIndex & bufMask;
	const size_t totalAvail = bufMask + 1 - bytesUsed;

	// the last part of the data might be smaller than the remainder of the ring
	// buffer; clamp it
	ring.frameLen = std::min(dataLen - seekPos, totalAvail);
	return ring;
}

static bool Pak_ZStdStreamDecode(RTechDecodeState_s* const decoder, const PakRingBufferFrame_s& outFrame, const PakRingBufferFrame_s& inFrame)
{
	if (!decoder->zstreamContext || !decoder->inputBuf || !decoder->outputBuf ||
		decoder->outputBuf == decoder->inputBuf)
	{
		NS::log::rpak->error("{}: invalid Zstd decoder state", __FUNCTION__);
		Pak_ReleaseZStdDecoder(decoder);
		return false;
	}

	ZSTD_outBuffer outBuffer = {
		&decoder->outputBuf[outFrame.bufIndex],
		outFrame.frameLen, 0
	};

	ZSTD_inBuffer inBuffer = {
		&decoder->inputBuf[inFrame.bufIndex],
		inFrame.frameLen, 0
	};

	ZSTD_DStream* const dctx = decoder->zstreamContext;
	
	const size_t ret = ZSTD_decompressStream(dctx, &outBuffer, &inBuffer);

	if (ZSTD_isError(ret))
	{
		const char* const decodeError = ZSTD_getErrorName(ret);
		NS::log::rpak->error("{}: decode error: {}", __FUNCTION__, decodeError);
		Pak_ReleaseZStdDecoder(decoder);
		return false;
	}

	// advance buffer io positions, required so the main parser could already
	// start parsing the headers while the rest is getting decoded still
	decoder->outBufBytePos += outBuffer.pos;
	decoder->inBufBytePos += inBuffer.pos;

	if (ret == 0)
	{
		if (decoder->inBufBytePos != decoder->fileSize || decoder->outBufBytePos != decoder->decompSize)
		{
			NS::log::rpak->error("{}: Zstd frame ended at unexpected input/output offsets", __FUNCTION__);
			Pak_ReleaseZStdDecoder(decoder);
			return false;
		}

		return true;
	}

	if (inBuffer.pos == 0 && outBuffer.pos == 0 && decoder->allChunksStreamed)
	{
		NS::log::rpak->error("{}: truncated Zstd frame made no decoding progress", __FUNCTION__);
		Pak_ReleaseZStdDecoder(decoder);
		return false;
	}

	// If all provided input was consumed, use Zstd's exact next-input hint.
	// Keeping the current target when input remains lets the caller first make
	// room in the output ring without unnecessarily waiting for another read.
	if (inBuffer.pos == inBuffer.size)
	{
		decoder->nextInputSize = ret;
		decoder->bufferSizeNeeded = ret > SIZE_MAX - decoder->inBufBytePos
			? SIZE_MAX
			: decoder->inBufBytePos + ret;
	}

	return false;
}

bool Pak_StreamToBufferDecode(RTechDecodeState_s* const decoder, const size_t inLen, const size_t outLen, const PakDecodeMode_e decodeMode)
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

	if(decodeMode == PakDecodeMode_e::MODE_RTECH) {
	   return Pak_RTechStreamDecode(decoder,inLen,outLen);
    }

	if (decoder->decodeMode != PakDecodeMode_e::MODE_ZSTD || !decoder->zstreamContext || decoder->inBufBytePos > inLen)
		return false;

	const PakRingBufferFrame_s inFrame = Pak_DetermineRingBufferFrame(decoder->inputMask, decoder->inBufBytePos, inLen);
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
	return "Unknown";
}

static bool Pak_HasCompleteZStdFrameHeader(const PakAsyncReadBlock_s& block, const uint64_t inputBytesReady)
{
	if (block.sourceEndOffset <= block.logicalEndOffset)
		return false;

	const uint64_t frameSize = block.sourceEndOffset - block.logicalEndOffset;
	const uint64_t headerSize = std::min<uint64_t>(frameSize, PAK_ZSTD_FRAME_HEADER_MAX_SIZE);
	return inputBytesReady >= block.logicalEndOffset + headerSize;
}

static void Pak_MarkLoadError(PakFile* const pak)
{
	PakGlobalState_s* const globals = Pak_GetGlobals();
	if (!globals)
		return;

	for (PakLoadedInfo_s& loadedPak : globals->loadedPaks)
	{
		if (loadedPak.pakFile == pak)
		{
			loadedPak.status = PAK_STATUS_ERROR;
			return;
		}
	}
}


static bool Pak_ReadFile_Custom(PakFile* const pak)
{
    PakFileStream_s* const fileStream = &pak->fileStream;

    // first request is always just the header.
    size_t readStart = sizeof(RPakHeaderV7_s);

    if (fileStream->asyncSubmitIndex > 0)
        readStart = fileStream->asyncSubmitIndex * PAK_READ_DATA_CHUNK_SIZE;

    for (; fileStream->asyncCompleteIndex != fileStream->asyncSubmitIndex; fileStream->asyncCompleteIndex++)
    {
        const int currentDataChunkIndex = fileStream->asyncCompleteIndex & PAK_MAX_DATA_CHUNKS_PER_STREAM_MASK;
        const uint8_t dataChunkStatus = fileStream->asyncRequestStates[currentDataChunkIndex];

        if (dataChunkStatus != 1)
        {
            size_t bytesProcessed = 0;
            const char* statusMsg = "(no reason)";

            const uint8_t currentStatus = Pak_PollAsyncRead(
				static_cast<uint8_t>(fileStream->asyncRequestIds[currentDataChunkIndex]),
				reinterpret_cast<uint64_t*>(&bytesProcessed), &statusMsg);

            if (currentStatus == AsyncHandleStatus_s::FS_ASYNC_ERROR ||
				currentStatus == AsyncHandleStatus_s::FS_ASYNC_CANCELLED)
            {
				NS::log::rpak->error("Error reading pak file \"{}\" -- {}", pak->filename, statusMsg);
                Pak_MarkLoadError(pak);
                Pak_ReleaseZStdDecoder(&pak->codec);
                return false;
            }
            else if (currentStatus == AsyncHandleStatus_s::FS_ASYNC_PENDING)
                break;

            fileStream->inputBytesReady += bytesProcessed;
            if (dataChunkStatus)
            {
                const RPakHeaderV7_s* pakHeader = &pak->header;
                const uint64_t totalDataChunkSizeProcessed = fileStream->asyncCompleteIndex * PAK_READ_DATA_CHUNK_SIZE;

                if (dataChunkStatus == 2)
                {
                    fileStream->inputBytesReady = bytesProcessed + totalDataChunkSizeProcessed;
                    pakHeader = reinterpret_cast<RPakHeaderV7_s*>(&fileStream->readRingBuffer[totalDataChunkSizeProcessed & fileStream->readRingMask]);
                }

                const uint8_t fileIndex = fileStream->completedBlockWriteIndex++ & PAK_MAX_ASYNC_STREAMED_LOAD_REQUESTS_MASK;

                fileStream->completedBlocks[fileIndex].logicalEndOffset = totalDataChunkSizeProcessed + sizeof(RPakHeaderV7_s);
                fileStream->completedBlocks[fileIndex].sourceEndOffset = totalDataChunkSizeProcessed + pakHeader->compressedSize;
                fileStream->completedBlocks[fileIndex].expectedOutputEnd = pakHeader->decompressedSize;
                fileStream->completedBlocks[fileIndex].decodeMode = pakHeader->GetCompressionMode();
            }
        }
    }

    size_t currentOutBytePos = pak->decodeCursor;

    for (; pak->completedBlockReadIndex != fileStream->completedBlockWriteIndex; pak->completedBlockReadIndex++)
    {
        PakAsyncReadBlock_s* const streamDesc = &fileStream->completedBlocks[pak->completedBlockReadIndex & PAK_MAX_ASYNC_STREAMED_LOAD_REQUESTS_MASK];

        if (pak->loadNextBlock)
        {
			if (streamDesc->decodeMode == PakDecodeMode_e::MODE_ZSTD &&
				!Pak_HasCompleteZStdFrameHeader(*streamDesc, fileStream->inputBytesReady))
			{
				break;
			}

            pak->loadNextBlock = false;
            pak->decodedCursor = streamDesc->logicalEndOffset;

            if (streamDesc->decodeMode != PakDecodeMode_e::MODE_DISABLED)
            {
                pak->directBlockActive = false;
                pak->encodedBlockActive = true;

                pak->decodeCursor = sizeof(RPakHeaderV7_s);
            }
            else
            {
                pak->directBlockActive = true;
                pak->encodedBlockActive = false;

                pak->decodeCursor = streamDesc->logicalEndOffset;
            }

            if (pak->encodedBlockActive)
            {
                const size_t decompressedSize = Pak_InitDecoder(&pak->codec,
                    fileStream->readRingBuffer, pak->decoderRingBuffer,
					PAK_DECODE_IN_RING_BUFFER_MASK, PAK_DECODE_OUT_RING_BUFFER_MASK,
                    streamDesc->sourceEndOffset - (streamDesc->logicalEndOffset - sizeof(RPakHeaderV7_s)),
                    streamDesc->logicalEndOffset - sizeof(RPakHeaderV7_s), sizeof(RPakHeaderV7_s),
					static_cast<size_t>(streamDesc->expectedOutputEnd), streamDesc->decodeMode);

                if (decompressedSize == 0 || decompressedSize != streamDesc->expectedOutputEnd)
				{
					NS::log::rpak->error(
						"Error reading pak file \"{}\" with decoder \"{}\" -- decompressed size {} doesn't match expected value {}",
                        pak->filename,
                        Pak_DecoderToString(streamDesc->decodeMode),
                        decompressedSize,
						static_cast<size_t>(streamDesc->expectedOutputEnd));
					Pak_MarkLoadError(pak);
					Pak_ReleaseZStdDecoder(&pak->codec);
					return false;
				}
            }
        }

        if (pak->encodedBlockActive)
        {
            currentOutBytePos = pak->codec.outBufBytePos;

            if (currentOutBytePos != pak->codec.decompSize)
            {
                if (streamDesc->decodeMode == PakDecodeMode_e::MODE_ZSTD)
                    pak->codec.allChunksStreamed = fileStream->asyncCompleteIndex == fileStream->asyncSubmitIndex;

                const bool didDecode = Pak_StreamToBufferDecode(&pak->codec,
					fileStream->inputBytesReady,
					pak->decodeCursor + PAK_DECODE_OUT_RING_BUFFER_SIZE,
					streamDesc->decodeMode);

                currentOutBytePos = pak->codec.outBufBytePos;
                pak->decodedCursor = pak->codec.inBufBytePos;

				if (streamDesc->decodeMode == PakDecodeMode_e::MODE_ZSTD &&
					pak->codec.decodeMode == PakDecodeMode_e::MODE_DISABLED)
				{
					Pak_MarkLoadError(pak);
					return false;
				}

                if (didDecode)
                {
                    NS::log::rpak->info("Pak: {}, decoded with method {}", pak->filename, Pak_DecoderToString(streamDesc->decodeMode));
					Pak_ReleaseZStdDecoder(&pak->codec);
				}
			}
        }
        else
        {
            currentOutBytePos = std::min(streamDesc->sourceEndOffset, fileStream->inputBytesReady);
        }
		
        if (pak->decodedCursor != streamDesc->sourceEndOffset || pak->decodeCursor != currentOutBytePos)
            break;

        pak->loadNextBlock = true;
        currentOutBytePos = pak->decodeCursor;
    }

    size_t numBytesToProcess = currentOutBytePos - pak->decodeCursor;

    while (pak->copyBytesRemaining + pak->skipBytesRemaining)
    {
        // if there are no bytes left to process in this patch operation
		if (!pak->streamBytesRemaining) {
			RBitRead& bitbuf = pak->bitBuf;
            bitbuf.ConsumeData(pak->bitstreamCursor, bitbuf.BitsAvailable());

            // advance patch data buffer by the number of bytes that have just been fetched
            pak->bitstreamCursor = &pak->bitstreamCursor[bitbuf.BitsAvailable() >> 3];

            // store the number of bits remaining to complete the data read
            bitbuf.m_bitsAvailable = bitbuf.BitsAvailable() & 7; // number of bits above a whole byte

            const __int8 cmd = pak->patchCommands[bitbuf.ReadBits(6)];

            bitbuf.DiscardBits(pak->patchCodeLengths[bitbuf.ReadBits(6)]);

            // get the next patch function to execute
            pak->decodeStep = g_pakPatchApi[cmd];

            if (cmd <= 3u)
            {
                const uint8_t bitExponent = pak->PATCH_unk2[bitbuf.ReadBits(8)]; // number of stored bits for the data size

                bitbuf.DiscardBits(pak->PATCH_unk3[bitbuf.ReadBits(8)]);

                pak->streamBytesRemaining = (1ull << bitExponent) + bitbuf.ReadBits(bitExponent);

                bitbuf.DiscardBits(bitExponent);
            }
            else
            {
                pak->streamBytesRemaining = s_patchCmdToBytesToProcess[cmd];
            }

		}

        if (!pak->decodeStep(pak, &numBytesToProcess))
            break;
    }

    if (pak->directBlockActive)
        pak->decodedCursor = pak->decodeCursor;

    if (!fileStream->endOfInput)
    {
        const size_t numDataChunksProcessed = std::min<size_t>(fileStream->asyncCompleteIndex, pak->decodedCursor >> 19);

        while (fileStream->asyncSubmitIndex != numDataChunksProcessed + 32)
        {
            const int8_t requestIdx = fileStream->asyncSubmitIndex & PAK_MAX_DATA_CHUNKS_PER_STREAM_MASK;
            const size_t readOffsetEnd = (fileStream->asyncSubmitIndex + 1ull) * PAK_READ_DATA_CHUNK_SIZE;

            if (fileStream->nextReadMode == 1)
            {
                fileStream->asyncRequestIds[requestIdx] = -2;
                fileStream->asyncRequestStates[requestIdx] = 1;

                if (((requestIdx + 1) & PAK_MAX_ASYNC_STREAMED_LOAD_REQUESTS_MASK) == 0)
                    fileStream->nextReadMode = 2;

                ++fileStream->asyncSubmitIndex;
                readStart = readOffsetEnd;
            }
            else
            {
                if (readStart < fileStream->currentFileEndOffset)
                {
                    const size_t lenToRead = std::min<size_t>(fileStream->currentFileEndOffset, readOffsetEnd);

                    const size_t readOffset = readStart - fileStream->currentFileBaseOffset;
                    const size_t readSize = lenToRead - readStart;

                    fileStream->asyncRequestIds[requestIdx] = Pak_QueueAsyncRead(
                        fileStream->currentFileHandle,
                        readOffset,
                        readSize,
                        &fileStream->readRingBuffer[readStart & fileStream->readRingMask],
                       2);

                    fileStream->asyncRequestStates[requestIdx] = fileStream->nextReadMode;
                    fileStream->nextReadMode = 0;

                    ++fileStream->asyncSubmitIndex;
                    readStart = readOffsetEnd;
                }
                else
                {
                    if (pak->currentPatchFileIndex >= pak->header.patchIndex)
                    {
                        Pak_ReleaseFileHandle(fileStream->currentFileHandle);
                        fileStream->currentFileHandle = PAK_INVALID_HANDLE;
                        fileStream->currentFileBaseOffset = 0;
                        fileStream->endOfInput = true;

                        return pak->copyBytesRemaining == 0;
                    }

                    if (!pak->hasPatchData)
                        return pak->copyBytesRemaining == 0;

                    char pakPatchPath[MAX_PATH] = {};
                    sprintf(pakPatchPath, "r2\\paks\\Win64\\%s", pak->filename);

                    // get path of next patch rpak to load
                    if (pak->sections.patchFileIndices[pak->currentPatchFileIndex])
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
                            "(%02u).rpak", pak->sections.patchFileIndices[pak->currentPatchFileIndex]);
                    }

                    uint64_t patchFileSize = 0;
                    const PakHandle_t patchFileHandle = Pak_OpenFile(pakPatchPath, &patchFileSize);

                    if (patchFileHandle == PAK_INVALID_HANDLE)
					{
						NS::log::rpak->error("Couldn't open file \"{}\"", pakPatchPath);
						Pak_MarkLoadError(pak);
						Pak_ReleaseZStdDecoder(&pak->codec);
						return false;
					}

                    if (patchFileSize < pak->sections.patchFileHeaders[pak->currentPatchFileIndex].compressedSize)
					{
						NS::log::rpak->error("File \"{}\" appears truncated", pakPatchPath);
						Pak_ReleaseFileHandle(patchFileHandle);
						Pak_MarkLoadError(pak);
						Pak_ReleaseZStdDecoder(&pak->codec);
						return false;
					}

                    Pak_ReleaseFileHandle(fileStream->currentFileHandle);

                    fileStream->currentFileHandle = patchFileHandle;

                    const size_t readOffset = ALIGN_VALUE(fileStream->asyncSubmitIndex, 8ull) * PAK_READ_DATA_CHUNK_SIZE;
                    fileStream->nextReadMode = (fileStream->asyncSubmitIndex == ALIGN_VALUE(fileStream->asyncSubmitIndex, 8ull)) + 1;

                    fileStream->currentFileBaseOffset = readOffset;
                    fileStream->currentFileEndOffset = readOffset + pak->sections.patchFileHeaders[pak->currentPatchFileIndex].compressedSize;

                    pak->currentPatchFileIndex++;
                }
            }
        }
    }



    return pak->copyBytesRemaining == 0;
}



using Pak_ReadFile_t = bool(__fastcall*)(PakFile* pak);
Pak_ReadFile_t pPak_ReadFile = nullptr;
HOOK(v_Pak_ReadFile, o_Pak_ReadFile, bool, __fastcall, (PakFile* pak))
{
	// Pak_LoadPendingPak seeds the first patch/decode step at this call site.
	if (reinterpret_cast<uint64_t>(_ReturnAddress()) == rtechBaseAddr + 0xA618)
	{
		pak->decodeStep = g_pakPatchApi[0];
	}
	return Pak_ReadFile_Custom(pak);
}

using Pak_FixupPointersAndQueueAssets_t = void(__fastcall*)(PakFile* pak);
Pak_FixupPointersAndQueueAssets_t pPak_FixupPointersAndQueueAssets = nullptr;
HOOK(v_Pak_FixupPointersAndQueueAssets, o_Pak_FixupPointersAndQueueAssets, void, __fastcall, (PakFile* pakFile))
{
	o_Pak_FixupPointersAndQueueAssets(pakFile);
}

using Pak_LoadPak_t = bool(__fastcall*)(PakLoadedInfo_s* pak);
Pak_LoadPak_t pPak_LoadPak = nullptr;
HOOK(v_Pak_LoadPak, o_Pak_LoadPak, bool, __fastcall, (PakLoadedInfo_s* pak))
{
	return o_Pak_LoadPak(pak);
}

ON_DLL_LOAD("rtech_game.DLL", PakParseRtech, [](CModule module)
{
	pPak_ReadFile = module.Offset(0x8D10).RCast<Pak_ReadFile_t>();
	pPak_LoadPak = module.Offset(0x9C60).RCast<Pak_LoadPak_t>();
	pPak_FixupPointersAndQueueAssets = module.Offset(0x9AD0).RCast<Pak_FixupPointersAndQueueAssets_t>();
	Pak_PollAsyncRead = module.Offset(0x1AF0).RCast<decltype(Pak_PollAsyncRead)>();
	Pak_QueueAsyncRead = module.Offset(0x1F00).RCast<decltype(Pak_QueueAsyncRead)>();
	Pak_ReleaseFileHandle = module.Offset(0x2100).RCast<decltype(Pak_ReleaseFileHandle)>();
	Pak_OpenFile = module.Offset(0x1E20).RCast<decltype(Pak_OpenFile)>();
	// Treat both RTech and Zstd paks as encoded when allocating the decode
	// buffer and when selecting the active copy ring/mask.
	module.Offset(0x0A30C).Patch({0xB8, 0x00, 0x81});
	module.Offset(0x0A4F9).Patch({0xB8, 0x00, 0x81});
	rtechBaseAddr = module.Offset(0);
	v_Pak_ReadFile.Dispatch(reinterpret_cast<LPVOID*>(pPak_ReadFile));
	v_Pak_LoadPak.Dispatch(reinterpret_cast<LPVOID*>(pPak_LoadPak));
	v_Pak_FixupPointersAndQueueAssets.Dispatch(reinterpret_cast<LPVOID*>(pPak_FixupPointersAndQueueAssets));
})
