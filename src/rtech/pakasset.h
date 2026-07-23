#pragma once

#include "rstdlib.h"

#include <cstddef>
#include <cstdint>

#define PAK_MAX_TRACKED_TYPES 16
#define PAK_MAX_TRACKED_TYPES_MASK (PAK_MAX_TRACKED_TYPES - 1)

#define PAK_MAX_LOADED_PAKS 512
#define PAK_MAX_LOADED_PAKS_MASK (PAK_MAX_LOADED_PAKS - 1)

#define PAK_MAX_LOADED_ASSETS 0x20000
#define PAK_MAX_LOADED_ASSETS_MASK (PAK_MAX_LOADED_ASSETS - 1)

#define PAK_MAX_TRACKED_ASSETS (PAK_MAX_LOADED_ASSETS / 2)
#define PAK_MAX_TRACKED_ASSETS_MASK (PAK_MAX_TRACKED_ASSETS - 1)

using PakHandle_t = int32_t;
inline constexpr PakHandle_t PAK_INVALID_HANDLE = -1;

using PakGuid_t = uint64_t;

struct PakFile;

struct PakAssetShort_s
{
	PakGuid_t guid;
	uint32_t bindingIndex;
	uint32_t trackerIndex;
};

// Low 32 bits carry the owning PakHandle_t. RTech packs the low 12 bits of
// packedStarpakOffset into the upper half before invoking the load callback.
using PakAssetLoadContext_t = uint64_t;

using PakAssetLoadFn_t = void(*)(
	void* header,
	void* cpuData,
	uint64_t starpakOffset,
	PakAssetLoadContext_t loadContext);
using PakAssetUnloadFn_t = void(*)(void* header);
using PakAssetReplaceFn_t = void(*)(
	void* boundAsset,
	const void* newHeader,
	const void* previousHeader);
using PakAssetBindingChangeFn_t = void(*)(
	void* boundAsset,
	const void* newHeader,
	const void* previousHeader,
	void* userData);

struct PakAssetBindingLink_s
{
	void* userData;
	PakAssetBindingChangeFn_t callback;
	PakAssetBindingLink_s* next;
};

struct PakAssetBindingSlot_s
{
	PakAssetBindingLink_s* listeners;
	uint32_t loadedAssetIndex;
	uint32_t reserved0C;
};

struct PakAssetBinding_s
{
	union
	{
		uint32_t extension;
		char type[4];
	};
	uint32_t version;
	const char* description;
	PakAssetLoadFn_t loadAssetFunc;
	PakAssetUnloadFn_t unloadAssetFunc;
	PakAssetReplaceFn_t replaceAssetFunc;
	void* assetStorage;

	uint32_t headerSize;
	uint32_t assetStride;
	uint32_t headerAlignment;
	uint32_t assetCapacity;

	RFixedArray assetPool;
	PakAssetBindingSlot_s* assetSlots;
};

struct PakAssetTracker_s
{
	void* page;
	int32_t nextTrackerIndex;
	int32_t ownerPakIndex;
	uint8_t bindingIndex;
	uint8_t reserved11[7];
};

inline constexpr uint32_t PAK_MAX_UNLOAD_REFERENCES = 40000;

struct PakAssetUnloadRef_s
{
	PakGuid_t assetKey;
	int64_t referenceCount;
};

struct PakAssetUnloadPlan_s
{
	uint32_t queuedAssetCount;
	uint32_t pakFileCount;
	uint32_t refEntryCount;
	uint32_t unloadMode;
	PakFile* pakFiles[PAK_MAX_LOADED_PAKS];
	PakAssetUnloadRef_s assetRefs[PAK_MAX_UNLOAD_REFERENCES];
	uint32_t assetIndices[PAK_MAX_TRACKED_ASSETS];
};

static_assert(sizeof(PakAssetShort_s) == 0x10);
static_assert(sizeof(PakAssetBinding_s) == 0x60);
static_assert(offsetof(PakAssetBinding_s, loadAssetFunc) == 0x10);
static_assert(offsetof(PakAssetBinding_s, assetStorage) == 0x28);
static_assert(offsetof(PakAssetBinding_s, assetCapacity) == 0x3C);
static_assert(offsetof(PakAssetBinding_s, assetPool) == 0x40);
static_assert(offsetof(PakAssetBinding_s, assetSlots) == 0x58);
static_assert(sizeof(PakAssetBindingLink_s) == 0x18);
static_assert(sizeof(PakAssetBindingSlot_s) == 0x10);
static_assert(offsetof(PakAssetBindingLink_s, callback) == 0x8);
static_assert(offsetof(PakAssetBindingSlot_s, loadedAssetIndex) == 0x8);
static_assert(sizeof(PakAssetTracker_s) == 0x18);
static_assert(sizeof(PakAssetUnloadRef_s) == 0x10);
static_assert(sizeof(PakAssetUnloadPlan_s) == 0xDD410);
static_assert(offsetof(PakAssetUnloadPlan_s, pakFiles) == 0x10);
static_assert(offsetof(PakAssetUnloadPlan_s, assetRefs) == 0x1010);
static_assert(offsetof(PakAssetUnloadPlan_s, assetIndices) == 0x9D410);
