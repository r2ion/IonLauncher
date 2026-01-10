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

// these statuses might not be the same in r2, need to verify
enum PakStatus_e
{
	PAK_STATUS_FREED = 0,
	PAK_STATUS_LOAD_PENDING = 1,
	PAK_STATUS_REPAK_RUNNING = 2,
	PAK_STATUS_REPAK_DONE = 3,
	PAK_STATUS_LOAD_STARTING = 4,
	PAK_STATUS_LOAD_PAKHDR = 5,
	PAK_STATUS_LOAD_PATCH_INIT = 6,
	PAK_STATUS_LOAD_PATCH_EDIT_STREAM = 7,
	PAK_STATUS_LOAD_ASSETS = 8,
	PAK_STATUS_LOADED = 9,
	PAK_STATUS_UNLOAD_PENDING = 10,
	PAK_STATUS_FREE_PENDING = 11,
	PAK_STATUS_CANCELING = 12,
	PAK_STATUS_ERROR = 13,
	PAK_STATUS_INVALID_PAKHANDLE = 14,
	PAK_STATUS_BUSY = 15
};

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
		// not sure about this, maybe the first handle is something else? it's always -1
		int handles[MAX_PAK_STREAMING_HANDLES];
		int fileCount;
		// just a guess, not actually sure if this exists
		bool disabled;
	};

	PakHandle_t handle; // 0x0000
	PakStatus_e status; // 0x0004
	int loadJobId; // 0x0008
	int assetCount; // 0x000C

	char* filename; // 0x0010

	char pad_0018[8]; // 0x0018

	// TODO: need to reverse these pointers (obviously excluding the allocator smh, pakfile could be interesting)
	void* allocator; // 0x0020
	void* assetGuids; // 0x0028
	void* slabBuffers; // 0x0030
	void* guidDescriptors; // 0x0038

	char pad_0040[16]; // 0x0040

	void* pakFile; // 0x0050

	char pad_0058[16]; // 0x0058

	PakStreamingInfo_s streamingInfo;
};

struct PakGlobalState_s
{
	PakAssetBinding_s assetBindings[PAK_MAX_TRACKED_TYPES];
	PakAssetShort_s loadedAssets[PAK_MAX_LOADED_ASSETS];
	PakAssetTracker_s trackedAssets[PAK_MAX_TRACKED_ASSETS];

	RFixedArray trackedAssetMap;
	RFixedArray loadedPakMap;

	PakLoadedInfo_s loadedPaks[PAK_MAX_LOADED_PAKS];

	int lastAssetTrackerIndex;
	bool updateSplitScreenAnims;

	int unk[2];
	int mystery;
	int unk2;

	// arrays of handles to what?
	int unkArray1[32]; //0x0018
	int unkArray2[32]; //0x0098

	int32_t pakLoadJobId; //0x0118

	int16_t loadedPakCount; //0x011C
	int16_t requestedPakCount; //0x011E

	int loadedPakHandles[PAK_MAX_LOADED_PAKS]; //0x0120

	// these fields might be related to loading, int16s increment but haven't checked what they actually are
	int16_t N00009286; //0x0920
	int16_t N0000A1B5; //0x0923

	int32_t N0000A1AD; //0x0924
	int32_t N00009287; //0x0928

	int16_t N0000A1B0; //0x092C
	int16_t N0000A1BC; //0x092F

	char pad_0930[16]; //0x0930
	int32_t N0000928A; //0x0940
	int32_t N0000A1C2; //0x0944
	int32_t N0000928B; //0x0948
	uint32_t N0000A1C6; //0x094C
	__int64 gap;
	RTL_SRWLOCK lock;
	char pad_0950[48]; //0x0950

	int currentLoadedPaks; // haven't checked exactly what this offset is
	int numPatchedPaks;
	const char** patchedPakNames;
	uint8_t* patchNumbers;
};

static_assert(sizeof(PakGlobalState_s) == 3760088);

extern PakGlobalState_s* g_pakGlobalState;
