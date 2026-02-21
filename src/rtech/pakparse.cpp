#include "pakfile.h"
#include "pakdecode.h"
#include <util/zstdutils.h>

static ZSTDDecoder_s s_zstdPakDecoder;
#define ALIGN_VALUE( val, alignment ) ( ( val + alignment - 1 ) & ~( alignment - 1 ) ) 

int64_t (*CheckAsyncRequest)(int64_t requestHandle, size_t* bytesProcessed, const char** statusMsg);
void (*RTechLog)(__int64 a1, const char *a2, ...);
void(*sub_9570)(PakFile* pak);
__int64 (*FS_ReadAsyncFile)(
        unsigned int handle,
        __int64 readOffset,
        unsigned __int64 readSize,
        __int64 buffer,
        int type);

size_t (*vsnprintf_)(char *a1, size_t a2, const char *a3, ...);

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

static size_t Pak_RTechDecoderInit(PakDecompState* const decoder, const uint8_t* const fileBuffer,
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

static bool Pak_RTechStreamDecode(PakDecompState* const decoder, const size_t inLen, const size_t outLen)
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

size_t Pak_InitDecoder(PakDecompState* decoder, const uint8_t* const inputBuf,  uint8_t* const outputBuf,
	const uint64_t inputMask, const uint64_t outputMask, const size_t dataSize, const size_t dataOffset,
	const size_t headerSize, const PakDecodeMode_e decodeMode)
{
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

		RTechLog(4,"%s: decode error: %s\n", __FUNCTION__, decodeError);
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

	if(decodeMode == PakDecodeMode_e::MODE_RTECH) {
	   return Pak_RTechStreamDecode(decoder,inLen,outLen);
    }

	assert(decoder->zstreamContext && decoder->inBufBytePos <= inLen);

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


bool __fastcall Pak_ProcessPakFile(PakFile *pak)
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
  char* v35; // rdx
  char* j; // rcx
  int v37; // edi
  __int64 v38; // r14
  unsigned int numDataChunks_2; // eax
  unsigned __int64 v40; // rdx
  char v42[260]; // [rsp+30h] [rbp-138h] BYREF
  char v43[12]; // [rsp+134h] [rbp-34h] BYREF
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
        if ( v8 == 2 )
          RTechLog(4LL, "Error reading pak file \"%s\" -- %s\n", pak->pakFileName, statusMsg);
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
            p_header = (PakHeader *)&fileStream->fileBuffer[v14];
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
		if(fileStreamDescriptior->compressionMode != MODE_DISABLED)
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
		if (fileStreamDescriptior->compressionMode == PakDecodeMode_e::MODE_ZSTD) {
              pak->pakDecoder.zstreamContext = s_zstdPakDecoder.dctx;
		}
		inited = Pak_InitDecoder(&pak->pakDecoder,
            fileStream->fileBuffer, pak->decompressedBuffer,
            PAK_DECODE_IN_RING_BUFFER_MASK, PAK_DECODE_OUT_RING_BUFFER_MASK,
            fileStreamDescriptior->compressedSize - (fileStreamDescriptior->dataOffset - sizeof(PakHeader)),
            fileStreamDescriptior->dataOffset - sizeof(PakHeader),
			sizeof(PakHeader),
			fileStreamDescriptior->compressionMode);

		if ( inited != fileStreamDescriptior->decompressedSize ) {
          RTechLog(
            4LL,
            "Error reading pak file \"%s\" -- decompressed size %u doesn't match expected value %u\n",
            pak->pakFileName,
            inited + 0x58,
            pak->header.decompressedSize);
		 }

      }
      if ( !pak->isCompressed )
        goto LABEL_24;
      if ( pak->pakDecoder.outBufBytePos != pak->pakDecoder.decompSize )
      {
		 if (fileStreamDescriptior->compressionMode == PakDecodeMode_e::MODE_ZSTD) {
			pak->pakDecoder.allChunksStreamed = fileStream->numDataChunksProcessed == fileStream->numDataChunks;
		}
		
		 bool didDecode = Pak_StreamToBufferDecode(&pak->pakDecoder, 
            fileStream->bytesStreamed, (pak->processedPatchedDataSize + PAK_DECODE_OUT_RING_BUFFER_SIZE), fileStreamDescriptior->compressionMode);
		pak->inputBytePos = pak->pakDecoder.inBufBytePos;
		if (didDecode)
            {
				if(fileStreamDescriptior->compressionMode == PakDecodeMode_e::MODE_ZSTD)
					NS::log::rpak->info("{}: pak '{}' decoded successfully with method {}", __FUNCTION__, pak->pakFileName, Pak_DecoderToString(p_header->GetCompressionMode()));
                pak->pakDecoder.zstreamContext = nullptr;
            }
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
    while ( pak->patchFunc(pak, (size_t *)&statusMsg) && pak->startOfGuidDescriptorsRelativeToFileStart + pak->qword_548 );
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
            v34 = v42[0];
            v35 = 0LL;
            for ( j = v42; v34; ++j )
            {
              if ( v34 == '.' )
              {
                v35 = j;
              }
              else if ( v34 == '\\' || v34 == '/' )
              {
                v35 = 0LL;
              }
              v34 = j[1];
            }
            if ( v35 )
              j = v35;
            vsnprintf_(j, v43 - j, "(%02u).rpak",pak->headerFields.patchFileIndexes[lastLoadedPatchIndex]);
          }
          v37 = FS_OpenAsyncFile(v42, (size_t *)&statusMsg);
          if ( v37 == -1 )
            RTechLog(4LL, "Couldn't open file \"%s\".\n", v42);
          v38 = lastLoadedPatchIndex_cpy;
          if ( (unsigned __int64)statusMsg < pak->headerFields.patchCompressPairs[v38].compressedSize )
            RTechLog(4LL, "File \"%s\" appears truncated.\n", v42);
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
using Pak_ProcessFile_t = bool(__fastcall*)(PakFile* pak);
Pak_ProcessFile_t pPak_ProcessPakFile = nullptr;
HOOK(v_Pak_ProcessPakFile, o_Pak_ProcessPakFile, bool, __fastcall, (PakFile* pak))
{
	return Pak_ProcessPakFile(pak);
}


ON_DLL_LOAD("rtech_game.DLL", PakParseRtech, [](CModule module)
{
	pPak_ProcessPakFile = module.Offset(0x8D10).RCast<Pak_ProcessFile_t>();
	v_Pak_ProcessPakFile.Dispatch(reinterpret_cast<LPVOID*>(pPak_ProcessPakFile));
	CheckAsyncRequest = module.Offset( 0x1AF0 ).RCast<int64_t (*)(int64_t, size_t*, const char**)>();
	sub_9570 = module.Offset( 0x9570 ).RCast<void (*)(PakFile*)>();
	FS_ReadAsyncFile = module.Offset( 0x1F00 ).RCast<int64_t (*)(unsigned int, __int64, unsigned __int64, __int64, int)>();
	FS_CloseAsyncFile = module.Offset( 0x2100 ).RCast<void (*)(unsigned int)>();
	FS_OpenAsyncFile = module.Offset( 0x1E20 ).RCast<int16_t (*)(const char*, size_t*)>();
	RTechLog = module.Offset( 0x10AB0 ).RCast<void (*)(int64_t, const char*, ...)>();
	vsnprintf_ = module.Offset( 0x31C0 ).RCast<size_t (*)(char*, size_t, const char*,...)>();
	// allow use of zstd flags
	module.Offset(0x0A30C).Patch({0xB8,0x00,0x81});
})
