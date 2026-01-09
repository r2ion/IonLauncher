#pragma once

#include "rstdlib.h"

#define PAK_MAX_TRACKED_TYPES 16
#define PAK_MAX_TRACKED_TYPES_MASK (PAK_MAX_TRACKED_TYPES - 1)

#define PAK_MAX_LOADED_PAKS 512
#define PAK_MAX_LOADED_PAKS_MASK (PAK_MAX_LOADED_PAKS - 1)

#define PAK_MAX_LOADED_ASSETS 0x20000
#define PAK_MAX_LOADED_ASSETS_MASK (PAK_MAX_LOADED_ASSETS - 1)

#define PAK_MAX_TRACKED_ASSETS (PAK_MAX_LOADED_ASSETS / 2)
#define PAK_MAX_TRACKED_ASSETS_MASK (PAK_MAX_TRACKED_ASSETS - 1)

#define MAX_PAK_STREAMING_HANDLES 13

typedef int PakHandle_t;
typedef uint64_t PakGuid_t;

struct PakAssetShort_s
{
	PakGuid_t m_Guid;
	uint32_t m_nTrackerIndex;
	uint32_t unk;
};

struct PakAssetBinding_s
{
	char type[4];
	char* description;
	void* loadAssetFunc;
	void* unloadAssetFunc;
	void* replaceAssetFunc;
	void* allocator;

	uint32_t N00009080;
	uint32_t N0000908D;
	uint32_t N00009081;
	uint32_t N0000908F;
	uint32_t N00009082;
	uint32_t N00009098;
	uint32_t N00009090;
	uint32_t N0000909A;

	void* allocator_duplicate;
	void* page;
};

struct PakAssetTracker_s
{
	void* page;
	int32_t trackerIndex;
	int32_t loadedPakIndex;
	int8_t assetTypeHashIdx;
};

struct PakLoadedInfo_s
{
	struct PakStreamingInfo_s
	{
		// not sure about this, maybe the first one is something else?
		int handles[MAX_PAK_STREAMING_HANDLES];
		int fileCount;
		// just a guess, not actually sure if this exists
		bool disabled;
	};

	PakHandle_t handle; // 0x0000
	int status; // 0x0004
	int loadJobId; // 0x0008
	int assetCount; // 0x000C

	char* filename; // 0x0010

	char pad_0018[8]; // 0x0018

	void* allocator; // 0x0020
	void* assetGuids; // 0x0028
	void* slabBuffers; // 0x0030
	void* guidDescriptors; // 0x0038

	char pad_0040[16]; // 0x0040

	void* pakFile_probably; // 0x0050

	char pad_0058[16]; // 0x0058

	PakStreamingInfo_s streamingInfo;
};

constexpr int guh = sizeof(PakLoadedInfo_s);

struct PakGlobalState_s
{
	PakAssetBinding_s assetBindings[PAK_MAX_TRACKED_TYPES];
	PakAssetShort_s loadedAssets[PAK_MAX_LOADED_ASSETS];
	PakAssetTracker_s trackedAssets[PAK_MAX_TRACKED_ASSETS];

	RFixedArray trackedAssetMap;
	RFixedArray loadedPakMap;

	PakLoadedInfo_s loadedPaks[PAK_MAX_LOADED_PAKS];
};

extern PakGlobalState_s* g_pakGlobalState;
