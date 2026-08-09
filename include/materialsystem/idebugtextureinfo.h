#pragma once

#include <cstdint>

class KeyValues;

inline constexpr char DEBUG_TEXTURE_INFO_INTERFACE_VERSION[] = "DebugTextureInfo001";

enum TextureMemoryType_t : std::int32_t
{
	TEXTURE_MEMORY_RESERVED_MIN = 0,
	TEXTURE_MEMORY_BOUND_LAST_FRAME = 1,
	TEXTURE_MEMORY_TOTAL_LOADED = 2,
	TEXTURE_MEMORY_ESTIMATE_PICMIP_1 = 3,
	TEXTURE_MEMORY_ESTIMATE_PICMIP_2 = 4,
	TEXTURE_MEMORY_RESERVED_MAX = 5,
};

class IDebugTextureInfo
{
public:
	virtual KeyValues * GetDebugTextureList() = 0; // 0
	virtual std::uint64_t GetTextureMemoryUsed(TextureMemoryType_t memoryType) = 0; // 1
};
static_assert(sizeof(IDebugTextureInfo) == sizeof(void*));
