#pragma once

#include "rstdlib.h"
#include "pakfile.h"

#define PAK_MAX_TRACKED_TYPES 16
#define PAK_MAX_TRACKED_TYPES_MASK (PAK_MAX_TRACKED_TYPES - 1)

#define PAK_MAX_LOADED_PAKS 512
#define PAK_MAX_LOADED_PAKS_MASK (PAK_MAX_LOADED_PAKS - 1)

#define PAK_MAX_LOADED_ASSETS 0x20000
#define PAK_MAX_LOADED_ASSETS_MASK (PAK_MAX_LOADED_ASSETS - 1)

#define PAK_MAX_TRACKED_ASSETS (PAK_MAX_LOADED_ASSETS / 2)
#define PAK_MAX_TRACKED_ASSETS_MASK (PAK_MAX_TRACKED_ASSETS - 1)

#define MAX_PAK_STREAMING_HANDLES 12

#define PAK_SLAB_BUFFER_TYPES 4

#define PAK_MAX_DISPATCH_LOAD_JOBS 4

typedef uint8_t JobTypeID_t;

enum PakStatus_e : int
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

using PakHandle_t = int32_t;
inline constexpr PakHandle_t PAK_INVALID_HANDLE = -1;

typedef uint64_t PakGuid_t;

struct PakAssetShort_s
{
	PakGuid_t guid;
	uint32_t bindingIndex;
	uint32_t globalIndex;
};

struct PakAssetBinding_s
{
	char type[4];
	uint8_t padding04[4];
	char* description;
	void* loadAssetFunc;
	void* unloadAssetFunc;
	void* replaceAssetFunc;
	void* allocator;

	uint32_t N00009080;
	uint32_t N0000908D;
	uint32_t N00009081;
	uint32_t count;

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

struct PakAllocator_s
{
	void* (__fastcall* allocate)(void* allocator, uint64_t size, uint64_t alignment);
	void (__fastcall* release)(void* allocator, void* memory);
};

struct PakLoadedInfo_s
{
	struct PakStreamingInfo_s
	{
		PakHandle_t handles[MAX_PAK_STREAMING_HANDLES];
		uint32_t fileCount;
	};

	PakHandle_t handle;
	PakStatus_e status;
	int32_t loadJobId;
	int32_t assetCount;
	char* filename;
	char* rePakFilename;
	PakAllocator_s* allocator;
	void** loadedAssets;
	void* slabBuffers[PAK_SLAB_BUFFER_TYPES];
	void* guidDescriptors;
	FILETIME fileTime;
  	PakFile* pakFile;
	PakHandle_t pendingFileHandle;
	PakStreamingInfo_s streamingInfo;
	HMODULE hModule;
};

// i think this is wrong but honestly such a who cares area of the struct in general
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

	int64_t assetUnloadPlanReady;
	int assetUnloadPlanOverlay;
	bool updateSplitScreenAnims;

	int16_t numAssetLoadJobs;
	JobFifoLock_s fifoLock;
	int pakLoadJobId;

	int16_t loadedPakCount;
	int16_t requestedPakCount;

	int loadedPakHandles[PAK_MAX_LOADED_PAKS]; //0x0120

	JobTypeID_t assetBindJobTypes[PAK_MAX_TRACKED_TYPES];
	int unusedSlots[PAK_MAX_DISPATCH_LOAD_JOBS];

	int32_t N0000928A; //0x0940
	int32_t N0000A1C2; //0x0944
	int32_t N0000928B; //0x0948
	uint32_t N0000A1C6; //0x094C
	__int64 gap;
	RTL_SRWLOCK lock;
	char pad_395F90[48];

	int currentLoadedPaks;
	int numPatchedPaks;
	const char** patchedPakNames;
	uint8_t* patchNumbers;
};

PakGlobalState_s* Pak_GetGlobals();

static_assert(sizeof(PakGlobalState_s) == 3760088);
static_assert(sizeof(PakLoadedInfo_s) == 0xA8);
static_assert(sizeof(PakLoadedInfo_s::PakStreamingInfo_s) == 0x34);
static_assert(sizeof(PakAssetShort_s) == 0x10);
static_assert(sizeof(PakAssetBinding_s) == 0x60);
static_assert(sizeof(PakAssetTracker_s) == 0x18);
static_assert(sizeof(PakAllocator_s) == 0x10);
static_assert(sizeof(JobFifoLock_s) == 0x108);
static_assert(offsetof(PakLoadedInfo_s, pakFile) == 0x60);
static_assert(offsetof(PakLoadedInfo_s, streamingInfo) == 0x6C);
static_assert(offsetof(PakLoadedInfo_s, hModule) == 0xA0);
static_assert(offsetof(PakGlobalState_s, loadedPaks) == 0x380630);
static_assert(offsetof(PakGlobalState_s, fifoLock) == 0x395640);
static_assert(offsetof(PakGlobalState_s, assetBindJobTypes) == 0x395F50);
static_assert(offsetof(PakGlobalState_s, currentLoadedPaks) == 0x395FC0);

extern PakGlobalState_s* g_pakGlobalState;

extern std::vector<PakHandle_t> g_pBadPaks;
