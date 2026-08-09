#pragma once

#include <cstddef>
#include <cstdint>

struct PackedEntity
{
	std::uint32_t m_nSnapshotCreationTick : 31;
	std::uint32_t m_nShouldCheckCreationTick : 1;
	std::uint32_t m_Padding0004;
	std::uint64_t m_SerializedEntity;
};

static_assert(sizeof(PackedEntity) == 0x10);
static_assert(offsetof(PackedEntity, m_SerializedEntity) == 0x8);
