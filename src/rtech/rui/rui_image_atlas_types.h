#pragma once

#include <cstddef>
#include <cstdint>

struct RuiDrawBatch;

struct RuiImageAtlasEntry
{
	float pixelBounds[4];
	float uvBase[2];
	float uvScale[2];
};
static_assert(sizeof(RuiImageAtlasEntry) == 0x20);

struct RuiImageDimensions
{
	uint16_t width;
	uint16_t height;
};
static_assert(sizeof(RuiImageDimensions) == 0x4);

struct RuiImageAtlasGpuRecord
{
	float uvMin[2];
	float uvSize[2];
};
static_assert(sizeof(RuiImageAtlasGpuRecord) == 0x10);

struct RuiImageAtlasNineSlice
{
	float normalizedBounds[4];
	float edgeScale[2];
	float minimumEdgeSize[2];
};
static_assert(sizeof(RuiImageAtlasNineSlice) == 0x20);

struct RuiImageAtlasNameRecord
{
	uint32_t nameHash;
	uint16_t flags;
	uint16_t nameOffset;
};
static_assert(sizeof(RuiImageAtlasNameRecord) == 0x8);
static_assert(offsetof(RuiImageAtlasNameRecord, flags) == 0x4);
static_assert(offsetof(RuiImageAtlasNameRecord, nameOffset) == 0x6);

struct RuiImageAtlas
{
	float inverseWidth;
	float inverseHeight;
	uint16_t width;
	uint16_t height;
	uint16_t imageCount;
	uint16_t nineSliceImageCount;
	RuiImageAtlasEntry* images;
	RuiImageDimensions* imageDimensions;
	RuiImageAtlasNineSlice* nineSliceData;
	RuiImageAtlasNameRecord* imageNameRecords;
	const char* imageNames;
	void* texture;
	uint32_t gpuRecordBuffer;
	uint32_t reserved44;
};
static constexpr size_t RUI_IMAGE_ATLAS_CAPACITY = 255;
static constexpr uint8_t RUI_DYNAMIC_IMAGE_ATLAS_FIRST = 192;
static constexpr uint8_t RUI_DYNAMIC_IMAGE_ATLAS_LAST = RUI_IMAGE_ATLAS_CAPACITY - 1;

static_assert(RUI_DYNAMIC_IMAGE_ATLAS_FIRST < RUI_IMAGE_ATLAS_CAPACITY);
static_assert(RUI_IMAGE_ATLAS_CAPACITY <= UINT8_MAX);
static_assert(sizeof(RuiImageAtlas) == 0x48);
static_assert(offsetof(RuiImageAtlas, imageCount) == 0xC);
static_assert(offsetof(RuiImageAtlas, images) == 0x10);
static_assert(offsetof(RuiImageAtlas, nineSliceData) == 0x20);
static_assert(offsetof(RuiImageAtlas, imageNameRecords) == 0x28);
static_assert(offsetof(RuiImageAtlas, texture) == 0x38);
static_assert(offsetof(RuiImageAtlas, gpuRecordBuffer) == 0x40);
static_assert(offsetof(RuiImageAtlas, reserved44) == 0x44);

struct RuiImageAssetDescriptor
{
	uint32_t nameHash;
	int16_t imageIndex;
	uint8_t atlasIndex;
	uint8_t flags;
};
static_assert(sizeof(RuiImageAssetDescriptor) == 0x8);

using BindImageAtlasFn = bool(*)(RuiDrawBatch*, RuiImageAtlas*);
