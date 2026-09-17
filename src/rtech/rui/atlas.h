#pragma once

#include "rtech/rui/rui.h"

#include <cstddef>
#include <cstdint>
#include <shared_mutex>
#include <span>

using RuiImageAtlasHandle = uint32_t;

#define RUI_INVALID_IMAGE_ATLAS UINT32_MAX
#define RUI_IMAGE_ATLAS_INDEX_DYNAMIC UINT8_MAX
#define RUI_NATIVE_IMAGE_ATLAS_CAPACITY 20
#define RUI_IMAGE_DESCRIPTOR_CAPACITY 8192
#define RUI_PAK_IMAGE_ATLAS_CAPACITY RUI_IMAGE_DESCRIPTOR_CAPACITY
#define RUI_RUNTIME_IMAGE_ATLAS_HANDLE_BASE RUI_PAK_IMAGE_ATLAS_CAPACITY

struct alignas(16) RuiImageAtlasEntry
{
    float usefulBounds[4];
    Vector2D uvOrigin;
    Vector2D uvGradient;
};

struct RuiImageDimensions
{
    uint16_t width;
    uint16_t height;
};

struct RuiImageAtlasGpuRecord
{
    Vector2D uvMin;
    Vector2D uvSize;
};

struct RuiImageAtlasNineSlice
{
    float normalizedBounds[4];
    Vector2D edgeScale;
    Vector2D minimumEdgeSize;
};

struct RuiImageAtlasNameRecord
{
    uint32_t nameHash;
    uint16_t flags;
    uint16_t nameOffset;
};

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

struct RuiImageAssetDescriptor
{
    uint32_t nameHash;
    int16_t imageIndex;
    uint8_t atlasIndex;
    uint8_t flags;
};

struct RuiResolvedImageAsset
{
    RuiImageAtlas* atlas;
    RuiImageAtlasHandle atlasHandle;
    int16_t imageIndex;
    uint8_t flags;
};

extern std::shared_mutex g_RuiImageAtlasMutex;
RuiImageAtlasHandle RuiRegisterImageAtlas(const RuiImageAtlas& atlas, std::span<const RuiImageAtlasGpuRecord> records);
void RuiUnregisterImageAtlas(RuiImageAtlasHandle atlasHandle);
RuiImageAtlasHandle RuiGetImageAtlasHandle(RuiImageHandle imageHandle);
RuiImageAtlas* RuiGetImageAtlas(RuiImageAtlasHandle atlasHandle);
bool RuiResolveImageAsset(RuiImageHandle imageHandle, RuiResolvedImageAsset& asset);
bool RuiIsDynamicImageAsset(RuiImageHandle imageHandle);
