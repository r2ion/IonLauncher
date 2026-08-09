#pragma once

#include "materialsystem/itexture.h"

#include <cstdint>
#include <type_traits>

// Retail Titanfall 2 materialsystem_dx11.dll concrete texture extension.
// ITextureInternal RTTI derives directly from the 38-slot public ITexture surface.
class ITextureInternal : public ITexture
{
public:
	virtual void Extension038() = 0; // 38
	virtual void Extension039() = 0; // 39
	virtual void Extension040() = 0; // 40
	virtual void Extension041() = 0; // 41
	virtual void Extension042() = 0; // 42
	virtual void Extension043() = 0; // 43
	virtual void Extension044() = 0; // 44
	virtual void Extension045() = 0; // 45
	virtual std::uint64_t GetTextureHandle(std::uint32_t frame) = 0; // 46
	virtual std::uint64_t GetDepthTextureHandle() = 0; // 47
	virtual void Extension048() = 0; // 48
	virtual void Extension049() = 0; // 49
	virtual void Extension050() = 0; // 50
};
static_assert(std::is_base_of_v<ITexture, ITextureInternal>);
static_assert(sizeof(ITextureInternal) == sizeof(void*));
