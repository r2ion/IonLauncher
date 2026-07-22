#pragma once

#include "pakasset.h"
#include "pakfile.h"
#include "tier0/jobthread.h"

#define MAX_PAK_STREAMING_HANDLES 12

#define PAK_SLAB_BUFFER_TYPES 4

#define PAK_MAX_DISPATCH_LOAD_JOBS 4

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

// RTech reserves 0x108 bytes for the Tier0 FIFO lock even though the public
// lock layout itself is 0x88 bytes. No rtech code accesses the reserved tail.
struct PakJobFifoLockStorage_s
{
	JobFifoLock_s lock;
	uint8_t reserved88[0x80];
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
	PakAssetUnloadPlan_s* assetUnloadPlan;
	PakJobFifoLockStorage_s fifoLock;
	int32_t pakLoadJobId;

	int16_t loadedPakCount;
	int16_t requestedPakCount;

	int32_t loadedPakHandles[PAK_MAX_LOADED_PAKS];

	JobTypeID_t assetBindJobTypes[PAK_MAX_TRACKED_TYPES];
	uint8_t reserved395F60[16];
	JobTypeID_t dispatchJobTypes[PAK_MAX_DISPATCH_LOAD_JOBS];
	uint8_t dispatchPriorities[PAK_MAX_DISPATCH_LOAD_JOBS];
	uint32_t dispatchAffinities[PAK_MAX_DISPATCH_LOAD_JOBS];
	RTL_SRWLOCK lock;
	uint8_t reserved395F90[48];

	// Northstar-owned state stored in the native reserved tail.
	int currentLoadedPaks;
	int numPatchedPaks;
	const char** patchedPakNames;
	uint8_t* patchNumbers;
};

PakGlobalState_s* Pak_GetGlobals();

static_assert(sizeof(PakGlobalState_s) == 0x395FD8);
static_assert(sizeof(PakLoadedInfo_s) == 0xA8);
static_assert(sizeof(PakLoadedInfo_s::PakStreamingInfo_s) == 0x34);
static_assert(sizeof(PakAllocator_s) == 0x10);
static_assert(sizeof(PakJobFifoLockStorage_s) == 0x108);
static_assert(offsetof(PakJobFifoLockStorage_s, lock) == 0x0);
static_assert(offsetof(PakJobFifoLockStorage_s, reserved88) == 0x88);
static_assert(offsetof(PakLoadedInfo_s, pakFile) == 0x60);
static_assert(offsetof(PakLoadedInfo_s, streamingInfo) == 0x6C);
static_assert(offsetof(PakLoadedInfo_s, hModule) == 0xA0);
static_assert(offsetof(PakGlobalState_s, loadedPaks) == 0x380630);
static_assert(offsetof(PakGlobalState_s, assetUnloadPlanReady) == 0x395630);
static_assert(offsetof(PakGlobalState_s, assetUnloadPlan) == 0x395638);
static_assert(offsetof(PakGlobalState_s, fifoLock) == 0x395640);
static_assert(offsetof(PakGlobalState_s, assetBindJobTypes) == 0x395F50);
static_assert(offsetof(PakGlobalState_s, dispatchJobTypes) == 0x395F70);
static_assert(offsetof(PakGlobalState_s, dispatchPriorities) == 0x395F74);
static_assert(offsetof(PakGlobalState_s, dispatchAffinities) == 0x395F78);
static_assert(offsetof(PakGlobalState_s, lock) == 0x395F88);
static_assert(offsetof(PakGlobalState_s, currentLoadedPaks) == 0x395FC0);

extern PakGlobalState_s* g_pakGlobalState;

// A pack detected only after its slab storage was populated may already have
// corrupted allocator state. Such packs remain resident and make targeted
// model/material teardown unsafe until the process is restarted.
bool Pak_IsUnsafeLoadedPak(PakHandle_t handle);
bool Pak_HasUnsafeLoadedPaks();
