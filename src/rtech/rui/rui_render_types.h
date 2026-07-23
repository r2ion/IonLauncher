#pragma once

#include <cstddef>
#include <cstdint>
#include <immintrin.h>

struct RuiFontAtlas;
struct RuiGlobalState;
struct RuiImageAtlas;
struct RuiInstance;
struct RuiTransform;

struct RuiDrawQuad
{
	uint32_t vertexCount;
	uint32_t vertexCapacity;
	float positions[2][4];
};
static_assert(sizeof(RuiDrawQuad) == 0x28);

struct RuiColorOffsets
{
	uint16_t red;
	uint16_t green;
	uint16_t blue;
	uint16_t alpha;
};
static_assert(sizeof(RuiColorOffsets) == 0x8);

struct RuiStyleCommonOffsets
{
	RuiColorOffsets primaryColor;
	RuiColorOffsets secondaryColor;
	RuiColorOffsets tertiaryColor;
	uint16_t blend;
	uint16_t premultiply;
};
static_assert(sizeof(RuiStyleCommonOffsets) == 0x1C);
static_assert(offsetof(RuiStyleCommonOffsets, secondaryColor) == 0x8);
static_assert(offsetof(RuiStyleCommonOffsets, tertiaryColor) == 0x10);
static_assert(offsetof(RuiStyleCommonOffsets, blend) == 0x18);
static_assert(offsetof(RuiStyleCommonOffsets, premultiply) == 0x1A);

struct RuiStyleDescriptorOffsets
{
	uint16_t type;
	RuiStyleCommonOffsets common;
	uint16_t typeSpecificOffsets[11];
};
static_assert(sizeof(RuiStyleDescriptorOffsets) == 0x34);
static_assert(offsetof(RuiStyleDescriptorOffsets, common) == 0x2);
static_assert(offsetof(RuiStyleDescriptorOffsets, typeSpecificOffsets) == 0x1E);

struct RuiStyleType2DescriptorOffsets
{
	uint16_t type;
	RuiStyleCommonOffsets common;
	uint16_t innerSliceBlend;
	uint16_t sliceBeginTurns;
	uint16_t sliceEndTurns;
	uint16_t ellipseAxis0;
	uint16_t ellipseAxis1;
	uint16_t innerMaskScale;
	uint16_t edgeSoftness;
	uint16_t unused2C[4];
};
static_assert(sizeof(RuiStyleType2DescriptorOffsets) == sizeof(RuiStyleDescriptorOffsets));
static_assert(offsetof(RuiStyleType2DescriptorOffsets, innerSliceBlend) == 0x1E);
static_assert(offsetof(RuiStyleType2DescriptorOffsets, edgeSoftness) == 0x2A);
static_assert(offsetof(RuiStyleType2DescriptorOffsets, unused2C) == 0x2C);

struct RuiFloat2Offsets
{
	uint16_t x;
	uint16_t y;
};
static_assert(sizeof(RuiFloat2Offsets) == 0x4);

struct RuiImageRenderJob
{
	uint16_t type;
	uint16_t transformIndex;
	uint16_t imageOffset;
	uint16_t maskImageOffset;
	RuiFloat2Offsets boundsMinOffsets;
	RuiFloat2Offsets boundsMaxOffsets;
	RuiFloat2Offsets uvMinOffsets;
	RuiFloat2Offsets uvMaxOffsets;
	RuiFloat2Offsets maskCenterOffsets;
	uint16_t maskRotationOffset;
	RuiFloat2Offsets maskTranslationOffsets;
	RuiFloat2Offsets maskScaleOffsets;
	uint16_t flags;
	uint8_t styleIndex;
	uint8_t reserved;
};
static_assert(sizeof(RuiImageRenderJob) == 0x2A);

struct RuiEllipseRenderJob
{
	uint16_t type;
	uint16_t transformIndex;
	uint16_t imageOffset;
	RuiFloat2Offsets boundsMinOffsets;
	RuiFloat2Offsets boundsMaxOffsets;
	RuiFloat2Offsets uvMinOffsets;
	RuiFloat2Offsets uvMaxOffsets;
	uint16_t flags;
	uint8_t styleIndex;
	uint8_t reserved;
};
static_assert(sizeof(RuiEllipseRenderJob) == 0x1A);

struct RuiDrawMaterialBatch
{
	uint32_t firstVertex;
	uint32_t firstIndex;
	RuiFontAtlas* fontAtlas;
	RuiImageAtlas* imageAtlas;
};
static_assert(sizeof(RuiDrawMaterialBatch) == 0x18);

enum class RuiDrawInfoMode : uint32_t
{
	Direct = 0,
	Clipped = 1,
	Mesh = 2,
	Angular = 3,
};

struct RuiDrawInfo
{
	RuiDrawInfoMode mode;
};
static_assert(sizeof(RuiDrawInfo) == 0x4);

struct RuiProjectionBasis
{
	__m128 positionOrigin;
	__m128 positionBasisX;
	__m128 positionBasisY;
	__m128 secondaryOrigin;
	__m128 secondaryBasisY;
	__m128 secondaryBasisX;
};
static_assert(sizeof(RuiProjectionBasis) == 0x60);

struct RuiMeshHeader
{
	uint32_t boneCount;
	uint32_t vertexCount;
	uint32_t faceCount;
	uint32_t boneIndicesOffset;
	uint32_t verticesOffset;
	uint32_t faceIndicesOffset;
	uint32_t faceBoundsOffset;
	uint32_t windingBits;
};
static_assert(sizeof(RuiMeshHeader) == 0x20);

struct RuiDrawInfoMesh
{
	RuiDrawInfoMode mode;
	uint32_t reserved04;
	uint64_t reserved08;
	RuiMeshHeader* mesh;
	RuiProjectionBasis* faceBases;
	float boneMatrices[2][3][4];
	const char* debugName;
};
static_assert(sizeof(RuiDrawInfoMesh) == 0x88);

struct RuiComputedStyleCommon
{
	float primaryColor[4];
	float secondaryColor[4];
	float tertiaryColor[4];
	float blend;
	float premultiply;
};
static_assert(sizeof(RuiComputedStyleCommon) == 0x38);

struct RuiComputedStyleType2
{
	float innerSliceBlend;
	float sliceBeginRadians;
	float sliceEndRadians;
	float ellipseAxis0;
	float ellipseAxis1;
	float innerMaskScale;
	float inverseEdgeSoftness;
	float reserved54[3];
};
static_assert(sizeof(RuiComputedStyleType2) == 0x28);

struct RuiComputedTextStyle
{
	float shadowOffsetX;
	float shadowOffsetY;
	float shadowOpacity;
	float shadowFilterWidth;
	float strokeWidth;
	float distanceBias;
	float maximumFilterWidth;
	float reserved54[3];
};
static_assert(sizeof(RuiComputedTextStyle) == 0x28);

union RuiComputedStylePayload
{
	float values[10];
	RuiComputedStyleType2 type2;
	RuiComputedTextStyle text;
};
static_assert(sizeof(RuiComputedStylePayload) == 0x28);

struct RuiComputedStyle
{
	RuiComputedStyleCommon common;
	RuiComputedStylePayload payload;
};
static_assert(sizeof(RuiComputedStyle) == 0x60);
static_assert(offsetof(RuiComputedStyle, payload) == 0x38);

struct RuiDrawBatch
{
	RuiDrawMaterialBatch* materialBatches;
	uint32_t materialBatchIndex;
	uint32_t materialBatchCapacity;
	uint8_t reserved10[8];
	void* vertexBuffer;
	uint32_t vertexCount;
	uint32_t vertexBufferSize;
	RuiComputedStyle* computedStyles;
	uint32_t computedStyleCount;
	uint32_t computedStyleCapacity;
	void* indexBuffer;
	uint32_t indexBufferSize;
	uint32_t indexBufferCapacity;
	uint64_t rendererData[6];
	uint64_t rendererFlags;
	uint64_t drawIndex;
};
static_assert(sizeof(RuiDrawBatch) == 0x88);

struct RuiVector4
{
	float values[4];

	RuiVector4& operator=(__m128 value)
	{
		_mm_storeu_ps(values, value);
		return *this;
	}

	operator __m128() const
	{
		return _mm_loadu_ps(values);
	}
};
static_assert(sizeof(RuiVector4) == 0x10);

struct RuiBaseUv
{
	RuiVector4 primaryBasisX;
	RuiVector4 primaryBasisY;
	RuiVector4 primaryOrigin;
	RuiVector4 secondaryBasisX;
	RuiVector4 secondaryBasisY;
	RuiVector4 secondaryOrigin;
	int16_t imageIndex;
	int16_t maskImageIndex;
	int16_t computedStyleIndex;
	uint16_t flags;
};
static_assert(sizeof(RuiBaseUv) == 0x68);

using RuiDrawInfoHandlerFn = bool(*)(RuiDrawInfo*, const RuiBaseUv*, RuiDrawQuad*, RuiDrawBatch*);
using BuildEdgeCorrectionFn = void(*)(const RuiTransform*, const float*, __m128*);
using ApplyEdgeCorrectionFn = void(*)(RuiGlobalState*, RuiInstance*, const __m128*, const __m128*, __m128*);
