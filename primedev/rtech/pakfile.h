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

#define PAK_MAX_DISPATCH_LOAD_JOBS 4

enum PakStatus_e
{
    PAK_STATUS_FREED             = 0x0,
    PAK_STATUS_LOAD_PENDING      = 0x1,
    PAK_STATUS_REPAK_RUNNING     = 0x2,
    PAK_STATUS_REPAK_DONE        = 0x3,
    PAK_STATUS_LOAD_STARTING     = 0x4,
    PAK_STATUS_LOAD_PATCH_INIT   = 0x5,
    PAK_STATUS_LOAD_ASSETS       = 0x6,
    PAK_STATUS_LOADED            = 0x7,
    PAK_STATUS_UNLOAD_PENDING    = 0x8,
    PAK_STATUS_FREE_PENDING      = 0x9,
    PAK_STATUS_CANCELING         = 0xA,
    PAK_STATUS_ERROR             = 0xB,
    PAK_STATUS_INVALID_PAKHANDLE = 0xC,
    PAK_STATUS_BUSY              = 0xD,
};

typedef int PakHandle_t;
typedef uint64_t PakGuid_t;

struct PakAssetShort_s
{
	PakGuid_t m_Guid;
	uint32_t bindingIndex;
	uint32_t globalIndex;
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

	RFixedArray trackers;
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

	RFixedArray trackers;

  	PakGuid_t guid;
  	void *pakFile;
	PakStreamingInfo_s streamingInfo;
	HMODULE hModule;
};

struct JobFifoLock_s
{
	int id;
	int depth;
	int tls[64];
};

struct PakGlobalState_s
{
	PakAssetBinding_s assetBindings[PAK_MAX_TRACKED_TYPES];
	PakAssetShort_s loadedAssets[PAK_MAX_LOADED_ASSETS];
	PakAssetTracker_s trackedAssets[PAK_MAX_TRACKED_ASSETS];

	RFixedArray trackedAssetMap;
	RFixedArray loadedPakMap;

	PakLoadedInfo_s loadedPaks[PAK_MAX_LOADED_PAKS];

	// b64
	__int64 hasPendingLoadJob;
	int lastAssetTrackerIndex;
	bool updateSplitScreenAnims;

	int16_t numAssetLoadJobs;
	JobFifoLock_s fifoLock;
	int pakLoadJobId;

	int16_t loadedPakCount;
	int16_t requestedPakCount;

	int loadedPakHandles[PAK_MAX_LOADED_PAKS]; //0x0120

	// these fields might be related to loading, int16s increment but haven't checked what they actually are
	int16_t N00009286; //0x0920
	int16_t N0000A1B5; //0x0923

	int32_t N0000A1AD; //0x0924
	int32_t N00009287; //0x0928

	int16_t N0000A1B0; //0x092C
	int16_t N0000A1BC; //0x092F

	int unusedSlots[PAK_MAX_DISPATCH_LOAD_JOBS]; //0x0930

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
// constexpr int guh = sizeof(PakGlobalState_s);
// constexpr int fish = 3760088;

extern PakGlobalState_s* g_pakGlobalState;
