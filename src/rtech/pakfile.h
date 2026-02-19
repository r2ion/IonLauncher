#pragma once

#define PAK_MAX_SEGMENTS 20

struct PakFileStream__Descriptor
{
	int64_t startPointerMaybe;
	int64_t endPointerMaybe;
	int64_t decompressedSize;
	bool isCompressed;
	int8_t gap_19[7];
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
  PakFileStream__Descriptor filesizeStruct[8];
  uint8_t *fileBuffer;
  int64_t bufferMask;
  int64_t bytesStreamed;
};


struct __declspec(align(8)) PakDecompState
{
	uint8_t* input_buf;
	uint64_t out;
	uint64_t mask;
	uint64_t out_mask;
	uint64_t file_len_total;
	uint64_t decompressed_size;
	uint64_t inv_mask_in;
	uint64_t inv_mask_out;
	uint32_t header_skip_bytes_bs;
	uint32_t dword44;
	uint64_t input_byte_pos;
	uint64_t decompressed_position;
	uint64_t len_needed;
	uint64_t byte;
	uint32_t byte_bit_offset;
	uint32_t dword6C;
	uint64_t qword70;
	uint64_t stream_compressed_size;
	uint64_t stream_decompressed_size;
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
	int16_t unknown2;
	uint32_t relationsStartIndex;
	uint32_t usesStartIndex;
	uint32_t relationsCount;
	uint16_t usesCount;
	uint16_t unknown;
	uint32_t subHeaderSize;
	uint32_t version;
	char magic[4];
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
	uint8_t flags;
	uint8_t IsCompressed;
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
};

struct PakFile
{
	bool IsValid();

	int dword_0;
	int assetsRead;
	int readPagesMaybe;
	int dword_C;
	int lastLoadedPatchIndex;
	int dword_14;
	PakFileStream fileStream;
	int64_t qword_1F0;
	char byte_1F8;
	BYTE gap_1F9[4];
	char byte_1FD;
	int16_t word_1FE;
	PakDecompState decomp_state;
	int64_t decompressedBuffer;
	int64_t qword_290;
	int64_t qword_298;
	uint64_t qword_2A0;
	char* puint8_2A8;
	char* qword_2B0;
	int32_t dword_2B8;
	uint8_t gap_2BC[4];
	int32_t dword_2C0;
	uint8_t gap_2C4[4];
	uint8_t buf_2C8[64];
	uint8_t buf_308[64];
	uint8_t gap_348[512];
	int64_t qword_548;
	int64_t startOfGuidDescriptorsRelativeToFileStart;
	char* qword_558;
	int64_t qword_560;
	bool(__fastcall* func_568)(void*, size_t*);
	int64_t qword_570;
	int dword_578;
	unsigned int jobId;
	int* pdword_580;
	int64_t* pageOffsets;
	PakFilePointer headerFields;
	int** pdword_5F8;
	int dword_600;
	int32_t dword_604;
	int64_t qword_608[16];
	const char* pakFileName;
	PakHeader header;
};
