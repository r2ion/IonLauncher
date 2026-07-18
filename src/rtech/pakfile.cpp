#include "pakfile.h"

#include <limits>

#define IALIGN(a, b) (((a) + ((b) - 1)) & ~((b) - 1))

static constexpr size_t SlabBufferTypeCount = 4;

// Compatibility repair is intentionally bounded. Ordinary compiler alignment
// mistakes are small; large discrepancies are more likely to be corrupt
// metadata and must not be converted into an attacker-controlled allocation.
static constexpr uint64_t MaxSlabRepairBytes = 1ull << 20;
static constexpr uint64_t MaxTotalSlabRepairBytes = 1ull << 20;
static constexpr uint64_t MaxUnconditionalSlabRepairBytes = 64ull << 10;

static bool IsPositivePowerOfTwo(const int32_t value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

static bool AlignAndAdvance(size_t& cursor, const size_t alignment, const uint64_t dataSize)
{
    constexpr size_t maxSize = std::numeric_limits<size_t>::max();
    if (cursor > maxSize - (alignment - 1) || dataSize > maxSize)
        return false;

    const size_t alignedCursor = IALIGN(cursor, alignment);
    if (static_cast<size_t>(dataSize) > maxSize - alignedCursor)
        return false;

    cursor = alignedCursor + static_cast<size_t>(dataSize);
    return true;
}

static bool CanRepairSlab(const uint64_t oldDataSize, const uint64_t newDataSize, const PakSlabRepairReport_s& report)
{
    const uint64_t addedBytes = newDataSize - oldDataSize;
    if (addedBytes > MaxSlabRepairBytes || report.addedBytes > MaxTotalSlabRepairBytes - addedBytes)
    {
        return false;
    }

    // Permit small alignment-only mistakes regardless of the original slab
    // size. Larger repairs must also be no more than 25% growth.
    return addedBytes <= MaxUnconditionalSlabRepairBytes || (oldDataSize != 0 && addedBytes <= oldDataSize / 4);
}

static bool ValidateSlabMetadata(const PakFile& pakFile, PakSlabRepairReport_s* const proposedRepairs)
{
    if (proposedRepairs)
        *proposedRepairs = {};

    if (pakFile.header.memSlabCount > PAK_MAX_SEGMENTS || !pakFile.sections.slabHeaders || !pakFile.sections.pageHeaders)
    {
        return false;
    }

    size_t slabNextPageOffsets[PAK_MAX_SEGMENTS] = {};
    size_t originalSlabBufferSizes[SlabBufferTypeCount] = {};
    size_t repairedSlabBufferSizes[SlabBufferTypeCount] = {};

    for (size_t slabIndex = 0; slabIndex < pakFile.header.memSlabCount; ++slabIndex)
    {
        if (!IsPositivePowerOfTwo(pakFile.sections.slabHeaders[slabIndex].alignment))
            return false;
    }

    for (size_t i = 0; i < pakFile.header.memPageCount; ++i)
    {
        const RPakPageHeader_s& pageHeader = pakFile.sections.pageHeaders[i];
        if (pageHeader.slabIndex < 0 || pageHeader.slabIndex >= pakFile.header.memSlabCount || !IsPositivePowerOfTwo(pageHeader.alignment) ||
            pageHeader.dataSize < 0)
        {
            return false;
        }

        const size_t slabIndex = static_cast<size_t>(pageHeader.slabIndex);
        const RPakSlabHeader_s& slabHeader = pakFile.sections.slabHeaders[slabIndex];
        const size_t bufferType = static_cast<uint32_t>(slabHeader.flags) & (SlabBufferTypeCount - 1);
        if (bufferType != 0 && pageHeader.alignment > slabHeader.alignment)
            return false;

        if (!AlignAndAdvance(slabNextPageOffsets[slabIndex], static_cast<size_t>(pageHeader.alignment), static_cast<uint64_t>(pageHeader.dataSize)))
        {
            return false;
        }
    }

    for (size_t slabIndex = 0; slabIndex < pakFile.header.memSlabCount; ++slabIndex)
    {
        const RPakSlabHeader_s& slabHeader = pakFile.sections.slabHeaders[slabIndex];
        const size_t bufferType = static_cast<uint32_t>(slabHeader.flags) & (SlabBufferTypeCount - 1);
        if (!AlignAndAdvance(originalSlabBufferSizes[bufferType], static_cast<size_t>(slabHeader.alignment), slabHeader.dataSize))
        {
            return false;
        }

        uint64_t effectiveDataSize = slabHeader.dataSize;
        const uint64_t requiredDataSize = slabNextPageOffsets[slabIndex];

        if (effectiveDataSize < requiredDataSize)
        {
            // Type zero is populated from asset header sizes rather than this slab
            // declaration. Enlarging dataSize would not affect its allocation.
            if (!proposedRepairs || bufferType == 0 || !CanRepairSlab(effectiveDataSize, requiredDataSize, *proposedRepairs))
            {
                return false;
            }

            PakSlabRepair_s& repair = proposedRepairs->repairs[proposedRepairs->repairCount++];
            repair.slabIndex = slabIndex;
            repair.bufferType = static_cast<uint32_t>(bufferType);
            repair.oldDataSize = effectiveDataSize;
            repair.newDataSize = requiredDataSize;
            proposedRepairs->addedBytes += requiredDataSize - effectiveDataSize;
            effectiveDataSize = requiredDataSize;
        }

        if (!AlignAndAdvance(repairedSlabBufferSizes[bufferType], static_cast<size_t>(slabHeader.alignment), effectiveDataSize))
        {
            return false;
        }
    }

    if (proposedRepairs)
    {
        for (size_t bufferType = 0; bufferType < SlabBufferTypeCount; ++bufferType)
        {
            if (repairedSlabBufferSizes[bufferType] < originalSlabBufferSizes[bufferType])
                return false;

            const uint64_t allocationGrowth = repairedSlabBufferSizes[bufferType] - originalSlabBufferSizes[bufferType];
            if (allocationGrowth > MaxTotalSlabRepairBytes || proposedRepairs->allocationGrowthBytes > MaxTotalSlabRepairBytes - allocationGrowth)
            {
                return false;
            }

            proposedRepairs->allocationGrowthBytes += allocationGrowth;
        }
    }

    return true;
}

bool PakFile::IsValid() const
{
    return ValidateSlabMetadata(*this, nullptr);
}

bool PakFile::ValidateAndRepairSlabMetadata(PakSlabRepairReport_s& report)
{
    PakSlabRepairReport_s proposedRepairs = {};
    if (!ValidateSlabMetadata(*this, &proposedRepairs))
        return false;

    for (size_t i = 0; i < proposedRepairs.repairCount; ++i)
    {
        const PakSlabRepair_s& repair = proposedRepairs.repairs[i];
        sections.slabHeaders[repair.slabIndex].dataSize = repair.newDataSize;
    }

    // Keep this transactional even though the proposal pass used the same
    // checks. If a future validation rule is added, never leave a partial repair.
    if (!IsValid())
    {
        for (size_t i = 0; i < proposedRepairs.repairCount; ++i)
        {
            const PakSlabRepair_s& repair = proposedRepairs.repairs[i];
            sections.slabHeaders[repair.slabIndex].dataSize = repair.oldDataSize;
        }
        return false;
    }

    report = proposedRepairs;
    return true;
}
