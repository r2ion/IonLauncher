#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>

struct RFixedArray
{
	uint32_t freeHeadToken;
	uint32_t freeTailIndex;
	uint32_t elementStride;
	uint32_t capacity;
	void* storage;
};

static_assert(sizeof(RFixedArray) == 0x18);
static_assert(offsetof(RFixedArray, storage) == 0x10);

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
	uint32_t(*hashKey)(uint32_t key);
	bool(*keysEqual)(const void* entry, uint32_t key);
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

using RHashMapU32FindOrReserveUnlockedFn = void* (*)(
	RHashMapU32* map,
	uint32_t key,
	uint8_t* reservedNewEntry);

// The key must exist. The return value points at the free-list link changed by
// the erase operation; it is not the removed entry.
using RHashMapU32RemoveExistingFn = uint32_t* (*)(RHashMapU32* map, uint32_t key);

#pragma pack(push, 4)
struct RBitRead
{
	unsigned __int64 m_dataBuf;
	unsigned int m_bitsAvailable;

	RBitRead() : m_dataBuf(0), m_bitsAvailable(64) {};

	FORCEINLINE void ConsumeData(unsigned __int64 input, unsigned int numBits = 64)
	{
		if (numBits > m_bitsAvailable)
		{
			assert(false && "RBitRead::ConsumeData: numBits must be less than or equal to m_bitsAvailable.");
			return;
		}

		m_dataBuf |= input << (64 - numBits);
	}

	FORCEINLINE void ConsumeData(void* input, unsigned int numBits = 64)
	{
		if (numBits > m_bitsAvailable)
		{
			assert(false && "RBitRead::ConsumeData: numBits must be less than or equal to m_bitsAvailable.");
			return;
		}

		m_dataBuf |= *reinterpret_cast<unsigned __int64*>(input) << (64 - numBits);
	}

	FORCEINLINE int BitsAvailable() const { return m_bitsAvailable; };

	FORCEINLINE unsigned __int64 ReadBits(unsigned int numBits)
	{
		assert(numBits <= 64 && "RBitRead::ReadBits: numBits must be less than or equal to 64.");
		return m_dataBuf & ((1ull << numBits) - 1);
	}

	FORCEINLINE void DiscardBits(unsigned int numBits)
	{
		assert(numBits <= 64 && "RBitRead::DiscardBits: numBits must be less than or equal to 64.");
		this->m_dataBuf >>= numBits;
		this->m_bitsAvailable += numBits;
	}
};
#pragma pack(pop)
