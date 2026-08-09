#pragma once

#include <cstddef>
#include <cstdint>

enum class MemoryPoolGrowType_t : int
{
	None = 0,
	Fast,
	Slow,
};

class CUtlMemoryPool
{
public:
	int m_BlockSize;
	int m_BlocksPerBlob;
	MemoryPoolGrowType_t m_GrowMode;
	int m_BlocksAllocated;
	int m_PeakAlloc;
	std::uint16_t m_Alignment;
	std::uint16_t m_NumBlobs;
	const char* m_AllocOwner;
	void* m_FreeBlocks;
	void* m_BlobList;
};

static_assert(sizeof(CUtlMemoryPool) == 0x30);
static_assert(offsetof(CUtlMemoryPool, m_BlocksAllocated) == 0xC);
static_assert(offsetof(CUtlMemoryPool, m_Alignment) == 0x14);
static_assert(offsetof(CUtlMemoryPool, m_AllocOwner) == 0x18);
static_assert(offsetof(CUtlMemoryPool, m_FreeBlocks) == 0x20);
static_assert(offsetof(CUtlMemoryPool, m_BlobList) == 0x28);
