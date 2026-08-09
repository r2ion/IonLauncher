#pragma once

#include <cstddef>
#include <cstdint>
#include <d3d11.h>

struct TextureDesc_s
{
	std::uint64_t assetGuid;
	const char* debugName;
	std::uint16_t width;
	std::uint16_t height;
	std::uint16_t depth;
	std::uint16_t imageFormat;
};
static_assert(sizeof(TextureDesc_s) == 0x18);
static_assert(offsetof(TextureDesc_s, assetGuid) == 0x0);
static_assert(offsetof(TextureDesc_s, debugName) == 0x8);
static_assert(offsetof(TextureDesc_s, width) == 0x10);
static_assert(offsetof(TextureDesc_s, imageFormat) == 0x16);

struct TextureAsset_s : public TextureDesc_s
{
	std::uint32_t dataSize;
	std::uint8_t swizzleType;
	std::uint8_t optStreamedMipLevels;
	std::uint8_t arraySize;
	std::uint8_t layerCount;
	std::uint8_t usageFlags;
	std::uint8_t permanentMipLevels;
	std::uint8_t streamedMipLevels;
	std::uint8_t unkPerMip[13];
	std::uint64_t texelCount;
	std::int16_t streamedTextureIndex;
	std::uint8_t loadedStreamedMipLevelCount;
	std::uint8_t totalStreamedMipLevelCount;
	std::uint32_t lastUsedFrame;
	std::uint32_t lastFrame;
	std::uint32_t unknown44;
	float transform[16];
	std::uint64_t unknownQword88;
	std::uint64_t unknownQword90;
	std::uint32_t unknownArray98[16];
	std::uint32_t unknownArrayD8[16];
	ID3D11Resource* d3d11Resource;
	ID3D11ShaderResourceView* shaderResourceView;
	std::uint8_t unknownByte128;
	std::uint8_t padding[7];
};
static_assert(sizeof(TextureAsset_s) == 0x130);
static_assert(offsetof(TextureAsset_s, dataSize) == 0x18);
static_assert(offsetof(TextureAsset_s, transform) == 0x48);
static_assert(offsetof(TextureAsset_s, d3d11Resource) == 0x118);
static_assert(offsetof(TextureAsset_s, shaderResourceView) == 0x120);
