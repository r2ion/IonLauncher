#include "pakfile.h"

#include <limits>

bool PakFile::IsPositivePowerOfTwo(const int32_t value)
{
	return value > 0 && (value & (value - 1)) == 0;
}

bool PakFile::AlignAndAdvance(size_t& cursor, const size_t alignment, const uint64_t dataSize)
{
	constexpr size_t maxSize = std::numeric_limits<size_t>::max();
	if (alignment == 0 || cursor > maxSize - (alignment - 1) || dataSize > maxSize)
		return false;

	const size_t alignedCursor = (cursor + alignment - 1) & ~(alignment - 1);
	if (static_cast<size_t>(dataSize) > maxSize - alignedCursor)
		return false;

	cursor = alignedCursor + static_cast<size_t>(dataSize);
	return true;
}

bool PakFile::CanRepairSlab(
	const uint64_t oldDataSize,
	const uint64_t newDataSize,
	const uint64_t addedBytes)
{
	const uint64_t repairBytes = newDataSize - oldDataSize;
	if (repairBytes > MAX_SLAB_REPAIR_BYTES
		|| addedBytes > MAX_TOTAL_SLAB_REPAIR_BYTES - repairBytes)
	{
		return false;
	}

	// Permit small alignment-only mistakes regardless of the original slab
	// size. Larger repairs must also be no more than 25% growth.
	return repairBytes <= MAX_UNCONDITIONAL_SLAB_REPAIR_BYTES
		|| (oldDataSize != 0 && repairBytes <= oldDataSize / 4);
}

bool PakFile::ValidateSlabMetadata(
	uint64_t* const repairedDataSizes,
	size_t& repairCount,
	uint64_t& addedBytes) const
{
	repairCount = 0;
	addedBytes = 0;
	if (header.memSlabCount > PAK_MAX_SEGMENTS || !sections.slabHeaders || !sections.pageHeaders)
		return false;

	size_t slabNextPageOffsets[PAK_MAX_SEGMENTS] = {};
	size_t originalSlabBufferSizes[SLAB_BUFFER_TYPE_COUNT] = {};
	size_t repairedSlabBufferSizes[SLAB_BUFFER_TYPE_COUNT] = {};

	for (size_t slabIndex = 0; slabIndex < header.memSlabCount; ++slabIndex)
	{
		const RPakSlabHeader_s& slabHeader = sections.slabHeaders[slabIndex];
		if (!IsPositivePowerOfTwo(slabHeader.alignment))
			return false;
		if (repairedDataSizes)
			repairedDataSizes[slabIndex] = slabHeader.dataSize;
	}

	for (size_t pageIndex = 0; pageIndex < header.memPageCount; ++pageIndex)
	{
		const RPakPageHeader_s& pageHeader = sections.pageHeaders[pageIndex];
		if (pageHeader.slabIndex < 0 || pageHeader.slabIndex >= header.memSlabCount
			|| !IsPositivePowerOfTwo(pageHeader.alignment) || pageHeader.dataSize < 0)
		{
			return false;
		}

		const size_t slabIndex = static_cast<size_t>(pageHeader.slabIndex);
		const RPakSlabHeader_s& slabHeader = sections.slabHeaders[slabIndex];
		const size_t bufferType = static_cast<uint32_t>(slabHeader.flags) & (SLAB_BUFFER_TYPE_COUNT - 1);
		if (bufferType != 0 && pageHeader.alignment > slabHeader.alignment)
			return false;

		if (!AlignAndAdvance(
			slabNextPageOffsets[slabIndex],
			static_cast<size_t>(pageHeader.alignment),
			static_cast<uint64_t>(pageHeader.dataSize)))
		{
			return false;
		}
	}

	for (size_t slabIndex = 0; slabIndex < header.memSlabCount; ++slabIndex)
	{
		const RPakSlabHeader_s& slabHeader = sections.slabHeaders[slabIndex];
		const size_t bufferType = static_cast<uint32_t>(slabHeader.flags) & (SLAB_BUFFER_TYPE_COUNT - 1);
		if (!AlignAndAdvance(
			originalSlabBufferSizes[bufferType],
			static_cast<size_t>(slabHeader.alignment),
			slabHeader.dataSize))
		{
			return false;
		}

		uint64_t effectiveDataSize = slabHeader.dataSize;
		const uint64_t requiredDataSize = slabNextPageOffsets[slabIndex];
		if (effectiveDataSize < requiredDataSize)
		{
			// Type zero is populated from asset header sizes rather than this slab
			// declaration. Enlarging dataSize would not affect its allocation.
			if (!repairedDataSizes || bufferType == 0
				|| !CanRepairSlab(effectiveDataSize, requiredDataSize, addedBytes))
			{
				return false;
			}

			repairedDataSizes[slabIndex] = requiredDataSize;
			++repairCount;
			addedBytes += requiredDataSize - effectiveDataSize;
			effectiveDataSize = requiredDataSize;
		}

		if (!AlignAndAdvance(
			repairedSlabBufferSizes[bufferType],
			static_cast<size_t>(slabHeader.alignment),
			effectiveDataSize))
		{
			return false;
		}
	}

	uint64_t allocationGrowthBytes = 0;
	for (size_t bufferType = 0; bufferType < SLAB_BUFFER_TYPE_COUNT; ++bufferType)
	{
		if (repairedSlabBufferSizes[bufferType] < originalSlabBufferSizes[bufferType])
			return false;

		const uint64_t allocationGrowth = repairedSlabBufferSizes[bufferType]
			- originalSlabBufferSizes[bufferType];
		if (allocationGrowth > MAX_TOTAL_SLAB_REPAIR_BYTES
			|| allocationGrowthBytes > MAX_TOTAL_SLAB_REPAIR_BYTES - allocationGrowth)
		{
			return false;
		}
		allocationGrowthBytes += allocationGrowth;
	}

	return true;
}

bool PakFile::IsValid() const
{
	size_t repairCount = 0;
	uint64_t addedBytes = 0;
	return ValidateSlabMetadata(nullptr, repairCount, addedBytes);
}

bool PakFile::ValidateAndRepairSlabMetadata(size_t& repairCount, uint64_t& addedBytes)
{
	repairCount = 0;
	addedBytes = 0;
	uint64_t repairedDataSizes[PAK_MAX_SEGMENTS] = {};
	size_t proposedRepairCount = 0;
	uint64_t proposedAddedBytes = 0;
	if (!ValidateSlabMetadata(repairedDataSizes, proposedRepairCount, proposedAddedBytes))
		return false;

	uint64_t originalDataSizes[PAK_MAX_SEGMENTS] = {};
	for (size_t slabIndex = 0; slabIndex < header.memSlabCount; ++slabIndex)
	{
		originalDataSizes[slabIndex] = sections.slabHeaders[slabIndex].dataSize;
		sections.slabHeaders[slabIndex].dataSize = repairedDataSizes[slabIndex];
	}

	// Keep this transactional even though the proposal pass used the same
	// checks. If a future validation rule is added, never leave a partial repair.
	if (!IsValid())
	{
		for (size_t slabIndex = 0; slabIndex < header.memSlabCount; ++slabIndex)
			sections.slabHeaders[slabIndex].dataSize = originalDataSizes[slabIndex];
		return false;
	}

	repairCount = proposedRepairCount;
	addedBytes = proposedAddedBytes;
	return true;
}
