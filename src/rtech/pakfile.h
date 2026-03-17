#pragma once
#include <zstd.h>
#include "rstdlib.h"

#define PAK_MAX_SEGMENTS 20
#define PAK_READ_DATA_CHUNK_SIZE (1ull << 19)

#define PAK_MAX_DATA_CHUNKS_PER_STREAM 32
#define PAK_MAX_DATA_CHUNKS_PER_STREAM_MASK (PAK_MAX_DATA_CHUNKS_PER_STREAM-1)

#define PAK_MAX_ASYNC_STREAMED_LOAD_REQUESTS 8
#define PAK_MAX_ASYNC_STREAMED_LOAD_REQUESTS_MASK (PAK_MAX_ASYNC_STREAMED_LOAD_REQUESTS-1)

#define PAK_DECODE_IN_RING_BUFFER_SIZE (PAK_READ_DATA_CHUNK_SIZE * PAK_MAX_DATA_CHUNKS_PER_STREAM)
#define PAK_DECODE_IN_RING_BUFFER_MASK (PAK_DECODE_IN_RING_BUFFER_SIZE-1)

#define PAK_DECODE_OUT_RING_BUFFER_SIZE 0x400000
#define PAK_DECODE_OUT_RING_BUFFER_MASK (PAK_DECODE_OUT_RING_BUFFER_SIZE-1)

#define PAK_HEADER_FLAGS_RTECH_ENCODED (1<<8)
#define PAK_HEADER_FLAGS_ZSTD_ENCODED (1<<15)

struct PakFile;

enum PakDecodeMode_e
{
	MODE_DISABLED = -1,

	// the default decoder
	MODE_RTECH,
	MODE_ZSTD
};

struct PakRingBufferFrame_s
{
	size_t bufIndex;
	size_t frameLen;
};

struct PakFileStream__Descriptor
{
  int64_t dataOffset;
  int64_t compressedSize;
  int64_t decompressedSize;
  PakDecodeMode_e compressionMode;
};

struct PakFileStream
{
  int64_t readOffset;
  int64_t compressedSize;
  int32_t fileHandle;
  int fileReadJobs[32];
  _BYTE dataChunkStatuses[32];
  unsigned int numDataChunksProcessed;
  unsigned int numDataChunks;
  _BYTE fileReadStatus;
  bool finishedLoadingPatches;
  _BYTE gap_BE;
  _BYTE numLoadedFiles;
  PakFileStream__Descriptor descriptors[8];
  uint8_t *fileBuffer;
  int64_t bufferMask;
  int64_t bytesStreamed;
};


struct __declspec(align(8)) PakDecompState
{
	const uint8_t* inputBuf;
	uint8_t* outputBuf;

	uint64_t inputMask;
	uint64_t outputMask;

	size_t fileSize;
	size_t decompSize;

	uint64_t inputInvMask;
	uint64_t outputInvMask;

	uint32_t headerOffset;

	// this field was unused, it now contains the decoder mode
	PakDecodeMode_e decodeMode;

	uint64_t inBufBytePos;
	uint64_t outBufBytePos;

	size_t bufferSizeNeeded;

	// current byte and current bit of byte
	uint64_t currentByte;
	uint32_t currentBit;

	uint32_t dword6C;
	union
	{
		uint64_t qword70;

		// set when all chunks have been streamed in. for ZStd compressed
		// paks, we might end up with less data than requested in which
		// case we must just process what we got currently. ZStd has a
		// different stream size requirement than the RTech decoder; it
		// has a fixed 'recommended' to-stream input size (using the API
		// `ZSTD_DStreamInSize`. in the RTech decoder, the recommended
		// to-stream input for the next decode round appears baked into
		// the frame header of the encoded block data in the RPak files.
		// So for ZStd, we need to handle corner-case desyncs to avoid a
		// dead-lock in the runtime.
		bool allChunksStreamed;
	};

	union
	{
		size_t compressedStreamSize;

		// compressedStreamSize isn't used on ZStd paks, instead, we need to
		// store the frame header size
		size_t frameHeaderSize;
	};

	union
	{
		size_t decompressedStreamSize;

		// decompressedStreamSize isn't used on ZStd paks; use this space for
		// the decoder
		ZSTD_DStream* zstreamContext;
	};
};

struct PakPatchCompressPair
{
	uint64_t compressedSize;
	uint64_t decompressedSize;
};

struct PakVirtualSegment
{
	uint32_t flags;
	uint32_t align;
	uint64_t size;
};

struct PakPageInfo
{
	uint32_t segIdx;
	uint32_t align;
	uint32_t dataSize;
};

struct PakDescriptor
{
	uint32_t index;
	uint32_t offset;
};

struct PakPtr
{
	uint32_t index;
	uint32_t offset;
};

/* 89 */
struct PakAssetEntry
{
	uint64_t nameHash;
	uint64_t padding;
	PakPtr subHeader;
	PakPtr rawData;
	int64_t starpakOffset;
	uint16_t highestPageNum;
	int16_t numRemainingDependencies;
	uint32_t relationsStartIndex;
	uint32_t usesStartIndex;
	uint32_t relationsCount;
	uint16_t usesCount;
	uint16_t unknown;
	uint32_t subHeaderSize;
	uint32_t version;
	uint32_t magic;

	FORCEINLINE uint8_t HashTableIndexForAssetType() const
	{
		return (((0xFF0B020B * magic) >> 24) & 0xF);
	}
};


struct PakFilePointer
{
	PakPatchCompressPair* patchCompressPairs;
	__int16* patchFileIndexes;
	const char* starpakPath;
	PakVirtualSegment* virtualSegments;
	PakPageInfo* pageInfo;
	PakDescriptor* descriptors;
	PakAssetEntry* assetEntrys;
	uint64_t* guidDescriptors;
	uint64_t fileRelations;
	int* externalAssetOffsets;
	char* externalAssetStrings;
	uint64_t pages;
	uint64_t patchHeader;
};
struct PakHeader
{
	char magic[4];
	uint16_t version;
	uint16_t flags;
	uint64_t timeCreated;
	uint64_t unknown_0;
	uint64_t compressedSize;
	uint64_t starpakFileOffsetMaybe;
	uint64_t decompressedSize;
	uint64_t unknown2;
	uint16_t lenStarpakPaths;
	uint16_t virtualSegmentCount;
	uint16_t pageCount;
	uint16_t patchIndex;
	uint32_t descriptorCount;
	uint32_t assetEntryCount;
	uint32_t guidDescriptorCount;
	uint32_t fileRelationCount;
	uint32_t externalAssetCount;
	uint32_t externalAssetSize;

	inline PakDecodeMode_e GetCompressionMode() const
	{
		if (flags & PAK_HEADER_FLAGS_RTECH_ENCODED)
			return PakDecodeMode_e::MODE_RTECH;
		if (flags & PAK_HEADER_FLAGS_ZSTD_ENCODED)
			return PakDecodeMode_e::MODE_ZSTD;

		return PakDecodeMode_e::MODE_DISABLED;
	}
};


struct PakPatchFuncs_s
{
	typedef bool (*PatchFunc_t)(PakFile* const pak, size_t* const numAvailableBytes);

	enum PatchCommands_e
	{
		PATCH_CMD0,
		PATCH_CMD1,
		PATCH_CMD2,
		PATCH_CMD3,
		PATCH_CMD4,
		PATCH_CMD5, // Same as cmd4.
		PATCH_CMD6,

		// !!! NOT A CMD !!!
		PATCH_CMD_COUNT
	};

	inline PatchFunc_t operator[](ssize_t i) const
	{
		return patchFuncs[i];
	}

	PatchFunc_t patchFuncs[PATCH_CMD_COUNT];

};


struct PakFile
{
	bool IsValid();
	inline uint16_t GetPageCount() const
	{
		return header.pageCount;
	}
	inline bool IsPageOffsetValid(uint32_t index, uint32_t offset) const
	{
		// validate page index
		if (index == UINT32_MAX || index > GetPageCount())
			return false;

		return true;
	}
	
	inline void* GetPointerForPageOffset(const PakDescriptor* ptr) const
	{
		assert(IsPageOffsetValid(ptr->index, ptr->offset));
		return memPageBuffers[ptr->index] + ptr->offset;
	}
	int numProcessedPointers;
	int assetsRead;
	int processedPageCount;
	int firstPageIdx;
	int lastLoadedPatchIndex;
	int dword_14;
	PakFileStream fileStream;
	int64_t inputBytePos;
	char processedStreamCount;
	BYTE gap_1F9[4];
    char resetInBytePos;
	bool updateBytePosPostProcess;
	bool isCompressed;
	PakDecompState pakDecoder;
	uint8_t* decompressedBuffer;
	int64_t maxCopySize;
	int64_t headerSize;
	// Start of PakMemory Data
	uint64_t processedPatchedDataSize;
	char* patchData;
	char* patchDataPtr;
	RBitRead bitBuf;
	uint32_t patchDataOffset;
	uint8_t patchCommands[64];
	uint8_t buf_308[64];
	uint8_t PATCH_unk2[256];
	uint8_t PATCH_unk3[256];
	int64_t numBytesToSkip;
	int64_t patchSrcSize;
	char* patchDstPtr;
	int64_t numPatchBytesToProcess;
	PakPatchFuncs_s::PatchFunc_t patchFunc;
	int64_t fileSize;
	int pakId;
	unsigned int jobId;
	int* loadedAssetIndices;
	uint8_t** memPageBuffers;
	PakFilePointer headerFields;
	int** patchIndices;
	int dword_600;
	int32_t dword_604;
	int64_t qword_608[16];
	const char* pakFileName;
	PakHeader header;
};

