#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>

inline constexpr int32_t RHASHMAP_BUCKET_EMPTY = -1;
inline constexpr int32_t RHASHMAP_BUCKET_TOMBSTONE = -2;

// RTech's open-addressed map specialization for 32-bit keys. Reserving a new
// entry and publishing it are deliberately separate operations.
struct RHashMapU32
{
	uint32_t liveEntryCount;
	uint32_t bucketPairCount;
	void* entryStorage;
	int32_t* bucketEntryIndices;
	uint32_t(__fastcall* hashKey)(uint32_t key);
	bool(__fastcall* keysEqual)(const void* entry, uint32_t key);
	uint32_t freeListHead;
	uint32_t nextUnusedIndex;
	uint32_t pendingEntryIndex;
	uint32_t pendingBucketIndex;
	uint64_t entryStride;
	RTL_SRWLOCK lock;
};
static_assert(sizeof(RHashMapU32) == 0x48);
static_assert(offsetof(RHashMapU32, entryStorage) == 0x8);
static_assert(offsetof(RHashMapU32, freeListHead) == 0x28);
static_assert(offsetof(RHashMapU32, pendingEntryIndex) == 0x30);
static_assert(offsetof(RHashMapU32, pendingBucketIndex) == 0x34);
static_assert(offsetof(RHashMapU32, entryStride) == 0x38);
static_assert(offsetof(RHashMapU32, lock) == 0x40);

using RHashMapU32FindOrReserveUnlockedFn = void*(__fastcall*)(
	RHashMapU32* map,
	uint32_t key,
	uint8_t* reservedNewEntry);
// The key must exist. The return value points at the free-list link changed by
// the erase operation; it is not the removed entry.
using RHashMapU32RemoveExistingFn = uint32_t*(__fastcall*)(RHashMapU32* map, uint32_t key);
