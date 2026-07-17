#include "pakfile.h"

#define IALIGN(a, b) (((a)+((b)-1)) & ~((b)-1))

bool PakFile::IsValid()
{
	if (header.memSlabCount > PAK_MAX_SEGMENTS || !sections.slabHeaders || !sections.pageHeaders)
		return false;

	size_t slabPageSizes[PAK_MAX_SEGMENTS] = {};
	size_t slabRequiredAlignmentPadding[PAK_MAX_SEGMENTS] = {};
	size_t slabNextPageOffsets[PAK_MAX_SEGMENTS] = {};

	for (size_t i = 0; i < header.memPageCount; ++i)
	{
		const RPakPageHeader_s& pageHeader = sections.pageHeaders[i];
		if (pageHeader.slabIndex < 0 || pageHeader.slabIndex >= header.memSlabCount ||
			pageHeader.alignment <= 0 || (pageHeader.alignment & (pageHeader.alignment - 1)) != 0 ||
			pageHeader.dataSize < 0)
		{
			return false;
		}

		const size_t slabIndex = static_cast<size_t>(pageHeader.slabIndex);
		slabPageSizes[slabIndex] += static_cast<size_t>(pageHeader.dataSize);

		const size_t pageOffsetAligned = IALIGN(slabNextPageOffsets[slabIndex], static_cast<size_t>(pageHeader.alignment));
		slabRequiredAlignmentPadding[slabIndex] += pageOffsetAligned - slabNextPageOffsets[slabIndex];
		slabNextPageOffsets[slabIndex] = pageOffsetAligned + static_cast<size_t>(pageHeader.dataSize);
	}

	for (size_t slabIndex = 0; slabIndex < header.memSlabCount; ++slabIndex)
	{
		const RPakSlabHeader_s& slabHeader = sections.slabHeaders[slabIndex];
		if (slabHeader.dataSize < slabPageSizes[slabIndex])
			return false;

		const size_t actualAlignmentPadding = slabHeader.dataSize - slabPageSizes[slabIndex];
		if (actualAlignmentPadding < slabRequiredAlignmentPadding[slabIndex])
			return false;
	}

	return true;
}
