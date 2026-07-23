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

struct PakAsyncReadBlock_s
{
	uint64_t logicalEndOffset;
	uint64_t sourceEndOffset;
	uint64_t expectedOutputEnd;
	PakDecodeMode_e decodeMode;
};

struct PakFileStream_s
{
	uint64_t currentFileBaseOffset;
	uint64_t currentFileEndOffset;
	int32_t currentFileHandle;
	int32_t asyncRequestIds[PAK_MAX_DATA_CHUNKS_PER_STREAM];
	uint8_t asyncRequestStates[PAK_MAX_DATA_CHUNKS_PER_STREAM];
	uint32_t asyncCompleteIndex;
	uint32_t asyncSubmitIndex;
	uint8_t nextReadMode;
	bool endOfInput;
	uint8_t reservedBE;
	uint8_t completedBlockWriteIndex;
	PakAsyncReadBlock_s completedBlocks[PAK_MAX_ASYNC_STREAMED_LOAD_REQUESTS];
	uint8_t* readRingBuffer;
	uint64_t readRingMask;
	uint64_t inputBytesReady;
};

static_assert(sizeof(PakAsyncReadBlock_s) == 0x20);
static_assert(sizeof(PakFileStream_s) == 0x1D8);
static_assert(offsetof(PakFileStream_s, completedBlocks) == 0xC0);
static_assert(offsetof(PakFileStream_s, readRingBuffer) == 0x1C0);
static_assert(offsetof(PakFileStream_s, readRingMask) == 0x1C8);

struct __declspec(align(8)) RTechDecodeState_s
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

		// The first/next input-size hint returned by the Zstd streaming API.
		size_t nextInputSize;
	};

	union
	{
		size_t decompressedStreamSize;

		// decompressedStreamSize isn't used on ZStd paks; use this space for
		// the decoder
		ZSTD_DStream* zstreamContext;
	};
};

static_assert(sizeof(RTechDecodeState_s) == 0x88);
static_assert(offsetof(RTechDecodeState_s, decodeMode) == 0x44);
static_assert(offsetof(RTechDecodeState_s, zstreamContext) == 0x80);

void Pak_ReleaseZStdDecoder(RTechDecodeState_s* decoder);

struct RPakPatchFileHeader_s
{
	uint64_t compressedSize;
	uint64_t decompressedSize;
};

struct RPakSlabHeader_s
{
	int32_t flags;
	int32_t alignment;
	uint64_t dataSize;
};

struct RPakPageHeader_s
{
	int32_t slabIndex;
	int32_t alignment;
	int32_t dataSize;
};

struct RPakPagePtr_s
{
	uint32_t pageIndex;
	uint32_t offset;
};

struct RPakAssetEntryV7_s
{
	uint64_t guid;
	uint8_t unknown08[8];
	RPakPagePtr_s headerPtr;
	RPakPagePtr_s cpuPtr;
	int64_t packedStarpakOffset;
	uint16_t pageEnd;
	int16_t internalDependencyCount;
	uint32_t dependentsIndex;
	uint32_t usesIndex;
	uint32_t dependentsCount;
	uint32_t usesCount;
	uint32_t headerSize;
	uint32_t version;
	uint32_t assetType;

	FORCEINLINE uint8_t HashTableIndexForAssetType() const
	{
		return (((0xFF0B020B * assetType) >> 24) & 0xF);
	}
};

static_assert(sizeof(RPakPatchFileHeader_s) == 0x10);
static_assert(sizeof(RPakSlabHeader_s) == 0x10);
static_assert(sizeof(RPakPageHeader_s) == 0xC);
static_assert(sizeof(RPakPagePtr_s) == 0x8);
static_assert(sizeof(RPakAssetEntryV7_s) == 0x48);
static_assert(offsetof(RPakAssetEntryV7_s, usesCount) == 0x38);

struct RPakPatchMetadata_s
{
	uint32_t pageDataSkipSize;
	uint32_t unknown04;
};

static_assert(sizeof(RPakPatchMetadata_s) == 0x8);

struct PakFileSectionPointers_s
{
	RPakPatchFileHeader_s* patchFileHeaders;
	uint16_t* patchFileIndices;
	char* starpakPaths;
	RPakSlabHeader_s* slabHeaders;
	RPakPageHeader_s* pageHeaders;
	RPakPagePtr_s* pointerDescriptors;
	RPakAssetEntryV7_s* assetEntries;
	RPakPagePtr_s* assetUses;
	uint32_t* assetDependents;
	uint32_t* unknownTable;
	uint8_t* unknownData;
	uint8_t* pageData;
	RPakPatchMetadata_s* patchMetadata;
};
static_assert(sizeof(PakFileSectionPointers_s) == 0x68);

struct RPakHeaderV7_s
{
	uint32_t magic;
	uint16_t version;
	uint16_t flags;
	FILETIME fileTime;
	uint64_t unknown10;
	uint64_t compressedSize;
	uint64_t unknown20;
	uint64_t decompressedSize;
	uint64_t unknown30;
	uint16_t starpakPathsSize;
	uint16_t memSlabCount;
	uint16_t memPageCount;
	uint16_t patchIndex;
	uint32_t pointerCount;
	uint32_t assetCount;
	uint32_t assetUsesCount;
	uint32_t assetDependentsCount;
	uint32_t unknownTableCount;
	uint32_t unknownDataSize;

	inline PakDecodeMode_e GetCompressionMode() const
	{
		if (flags & PAK_HEADER_FLAGS_RTECH_ENCODED)
			return PakDecodeMode_e::MODE_RTECH;
		if (flags & PAK_HEADER_FLAGS_ZSTD_ENCODED)
			return PakDecodeMode_e::MODE_ZSTD;

		return PakDecodeMode_e::MODE_DISABLED;
	}
};

static_assert(sizeof(RPakHeaderV7_s) == 0x58);

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
	bool IsValid() const;
	bool ValidateAndRepairSlabMetadata(size_t& repairCount, uint64_t& addedBytes);
	inline uint16_t GetPageCount() const
	{
		return header.memPageCount;
	}
	inline bool IsPageOffsetValid(uint32_t index, uint32_t offset) const
	{
		if (index == UINT32_MAX || index >= GetPageCount() || !sections.pageHeaders ||
			sections.pageHeaders[index].dataSize < 0)
			return false;

		return offset <= static_cast<uint32_t>(sections.pageHeaders[index].dataSize);
	}
	
	inline void* GetPointerForPageOffset(const RPakPagePtr_s* ptr) const
	{
		assert(IsPageOffsetValid(ptr->pageIndex, ptr->offset));
		return pageDataPointers[ptr->pageIndex] + ptr->offset;
	}
	uint32_t pointerFixupIndex;
	uint32_t assetLoadIndex;
	uint32_t loadedPageCount;
	uint32_t pageIndexBase;
	uint32_t currentPatchFileIndex;
	uint32_t hasPatchData;
	PakFileStream_s fileStream;
	uint64_t decodedCursor;
	uint8_t completedBlockReadIndex;
	uint8_t padding1F9[4];
	uint8_t loadNextBlock;
	bool directBlockActive;
	bool encodedBlockActive;
	RTechDecodeState_s codec;
	uint8_t* decoderRingBuffer;
	uint64_t decoderRingMask;
	uint64_t sourceOffset;
	uint64_t decodeCursor;
	char* bitstreamCursor;
	char* literalCursor;
	RBitRead bitBuf;
	uint32_t padding2C4;
	uint8_t patchCommands[64];
	uint8_t patchCodeLengths[64];
	uint8_t PATCH_unk2[256];
	uint8_t PATCH_unk3[256];
	uint64_t skipBytesRemaining;
	uint64_t copyBytesRemaining;
	char* copyDestination;
	uint64_t streamBytesRemaining;
	PakPatchFuncs_s::PatchFunc_t decodeStep;
	uint64_t metadataEndOffset;
	int32_t ownerPakHandle;
	uint32_t loadJobGroupId;
	uint32_t* assetJobIds;
	uint8_t** pageDataPointers;
	PakFileSectionPointers_s sections;
	RPakAssetEntryV7_s** sortedAssetEntries;
	uint32_t nextSortedAssetIndex;
	uint32_t nextPointerFixupIndex;
	uint64_t assetTypeWriteOffsets[16];
	const char* filename;
	RPakHeaderV7_s header;

private:
	static constexpr size_t SLAB_BUFFER_TYPE_COUNT = 4;
	static constexpr uint64_t MAX_SLAB_REPAIR_BYTES = 1ull << 20;
	static constexpr uint64_t MAX_TOTAL_SLAB_REPAIR_BYTES = 1ull << 20;
	static constexpr uint64_t MAX_UNCONDITIONAL_SLAB_REPAIR_BYTES = 64ull << 10;

	static bool IsPositivePowerOfTwo(int32_t value);
	static bool AlignAndAdvance(size_t& cursor, size_t alignment, uint64_t dataSize);
	static bool CanRepairSlab(uint64_t oldDataSize, uint64_t newDataSize, uint64_t addedBytes);
	bool ValidateSlabMetadata(
		uint64_t* repairedDataSizes,
		size_t& repairCount,
		uint64_t& addedBytes) const;
};

static_assert(sizeof(PakFile) == 0x6E8);
static_assert(offsetof(PakFile, fileStream) == 0x18);
static_assert(offsetof(PakFile, codec) == 0x200);
static_assert(offsetof(PakFile, decoderRingBuffer) == 0x288);
static_assert(offsetof(PakFile, decoderRingMask) == 0x290);
static_assert(offsetof(PakFile, decodeCursor) == 0x2A0);
static_assert(offsetof(PakFile, skipBytesRemaining) == 0x548);
static_assert(offsetof(PakFile, copyBytesRemaining) == 0x550);
static_assert(offsetof(PakFile, metadataEndOffset) == 0x570);
static_assert(offsetof(PakFile, sections) == 0x590);
static_assert(offsetof(PakFile, header) == 0x690);
