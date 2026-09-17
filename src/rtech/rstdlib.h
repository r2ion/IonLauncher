#pragma once

#include <Windows.h>

#include <algorithm>
#include <cassert>

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

#define RHASHMAP_BUCKET_EMPTY = -1;
#define RHASHMAP_BUCKET_TOMBSTONE = -2;

struct RHashMapU32
{
	uint32_t liveEntryCount;
	uint32_t entryCapacity;
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

	uint32_t BucketCount() const
	{
		return entryCapacity * 2;
	}

	bool HasFreeEntry() const
	{
		return freeListHead != nextUnusedIndex || nextUnusedIndex < entryCapacity;
	}

	void* Find(uint32_t key) const
	{
		uint32_t hashOrBucket = hashKey(key);
		return FindWithHash(hashOrBucket, key);
	}

	void* FindOrReserveUnlocked(uint32_t key, bool& reserved)
	{
		uint32_t hashOrBucket = hashKey(key);
		if (void* entry = FindWithHash(hashOrBucket, key))
		{
			reserved = false;
			return entry;
		}

		if (!HasFreeEntry())
		{
			reserved = false;
			return nullptr;
		}

		const uint32_t entryIndex = freeListHead;
		void* entry = EntryAt(entryIndex);
		pendingEntryIndex = entryIndex;
		pendingBucketIndex = hashOrBucket;

		if (entryIndex == nextUnusedIndex)
		{
			++freeListHead;
			++nextUnusedIndex;
		}
		else
		{
			freeListHead = *static_cast<uint32_t*>(entry);
		}

		reserved = true;
		return entry;
	}

	void PublishReserved()
	{
		assert(pendingEntryIndex < entryCapacity);
		assert(bucketEntryIndices[pendingBucketIndex] < 0);
		bucketEntryIndices[pendingBucketIndex] = static_cast<int32_t>(pendingEntryIndex);
		++liveEntryCount;
	}

	uint32_t* RemoveExisting(uint32_t key)
	{
		void* entry = Find(key);
		return entry ? RemoveEntry(entry, key) : nullptr;
	}

	void Clear()
	{
		liveEntryCount = 0;
		std::fill_n(bucketEntryIndices, BucketCount(), RHASHMAP_BUCKET_EMPTY);
		freeListHead = 0;
		nextUnusedIndex = 0;
		pendingEntryIndex = 0;
		pendingBucketIndex = 0;
	}

	void Initialize(uint64_t stride)
	{
		entryStride = stride;
		InitializeSRWLock(&lock);
		Clear();
	}

private:
	void* EntryAt(uint32_t entryIndex) const
	{
		return static_cast<std::byte*>(entryStorage) + entryIndex * entryStride;
	}

	void* FindWithHash(uint32_t& hashOrBucket, uint32_t key) const
	{
		const uint32_t bucketMask = BucketCount() - 1;
		uint32_t bucketIndex = hashOrBucket & bucketMask;
		uint32_t firstTombstoneBucket = 0;
		bool foundTombstone = false;

		for (;;)
		{
			const int32_t entryIndex = bucketEntryIndices[bucketIndex];
			if (entryIndex >= 0)
			{
				void* entry = EntryAt(static_cast<uint32_t>(entryIndex));
				if (keysEqual(entry, key))
				{
					hashOrBucket = bucketIndex;
					return entry;
				}
			}
			else if (entryIndex == RHASHMAP_BUCKET_EMPTY)
			{
				hashOrBucket = foundTombstone ? firstTombstoneBucket : bucketIndex;
				return nullptr;
			}
			else if (!foundTombstone)
			{
				firstTombstoneBucket = bucketIndex;
				foundTombstone = true;
			}

			bucketIndex = (bucketIndex + 1) & bucketMask;
		}
	}

	uint32_t* RemoveEntry(void* entry, uint32_t key)
	{
		const uint32_t bucketMask = BucketCount() - 1;
		uint32_t bucketIndex = hashKey(key) & bucketMask;
		uint32_t tombstoneRunLength = 0;
		int32_t entryIndex;

		for (;;)
		{
			entryIndex = bucketEntryIndices[bucketIndex];
			if (entryIndex == RHASHMAP_BUCKET_EMPTY)
				return nullptr;

			if (entryIndex == RHASHMAP_BUCKET_TOMBSTONE)
			{
				++tombstoneRunLength;
			}
			else
			{
				if (EntryAt(static_cast<uint32_t>(entryIndex)) == entry)
					break;
				tombstoneRunLength = 0;
			}

			bucketIndex = (bucketIndex + 1) & bucketMask;
		}

		if (bucketEntryIndices[(bucketIndex + 1) & bucketMask] == RHASHMAP_BUCKET_EMPTY)
		{
			bucketEntryIndices[bucketIndex] = RHASHMAP_BUCKET_EMPTY;
			while (tombstoneRunLength != 0)
			{
				bucketIndex = (bucketIndex - 1) & bucketMask;
				bucketEntryIndices[bucketIndex] = RHASHMAP_BUCKET_EMPTY;
				--tombstoneRunLength;
			}
		}
		else
		{
			bucketEntryIndices[bucketIndex] = RHASHMAP_BUCKET_TOMBSTONE;
		}

		const uint32_t removedEntryIndex = static_cast<uint32_t>(entryIndex);
		uint32_t nextFreeEntryIndex = freeListHead;
		uint32_t* freeListLink = &freeListHead;
		while (nextFreeEntryIndex <= removedEntryIndex)
		{
			freeListLink = static_cast<uint32_t*>(EntryAt(nextFreeEntryIndex));
			nextFreeEntryIndex = *freeListLink;
		}

		*static_cast<uint32_t*>(entry) = nextFreeEntryIndex;
		*freeListLink = removedEntryIndex;
		--liveEntryCount;
		return freeListLink;
	}
};

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
