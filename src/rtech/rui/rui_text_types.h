#pragma once

#include "rtech/rui/rui_render_types.h"

#include <cstddef>
#include <cstdint>

struct RuiImageAtlas;
struct RuiInstance;
struct RuiRenderContext;

struct RuiTextStyleDescriptorOffsets
{
	uint16_t type;
	RuiStyleCommonOffsets common;
	uint16_t fontIndex;
	uint16_t shadowOpacity;
	uint16_t shadowOffsetX;
	uint16_t shadowOffsetY;
	uint16_t shadowFilterWidth;
	uint16_t textSize;
	uint16_t horizontalStretch;
	uint16_t strokeWidth;
	uint16_t distanceBias;
	uint16_t filterWidth;
	uint16_t ascentAdjustment;
};
static_assert(sizeof(RuiTextStyleDescriptorOffsets) == sizeof(RuiStyleDescriptorOffsets));
static_assert(offsetof(RuiTextStyleDescriptorOffsets, common) == 0x2);
static_assert(offsetof(RuiTextStyleDescriptorOffsets, fontIndex) == 0x1E);
static_assert(offsetof(RuiTextStyleDescriptorOffsets, shadowOffsetX) == 0x22);
static_assert(offsetof(RuiTextStyleDescriptorOffsets, textSize) == 0x28);
static_assert(offsetof(RuiTextStyleDescriptorOffsets, filterWidth) == 0x30);
static_assert(offsetof(RuiTextStyleDescriptorOffsets, ascentAdjustment) == 0x32);

struct RuiTextRenderJob
{
	uint16_t type;
	uint16_t transformIndex;
	uint8_t styleIndices[4];
	uint16_t textOffset;
	uint16_t targetWidthOffset;
	uint16_t wrapWidthOffset;
	uint16_t horizontalAlignmentOffset;
	uint16_t lineSpacingOffset;
};
static_assert(sizeof(RuiTextRenderJob) == 0x12);

struct RuiFont;

struct RuiFontAtlas
{
	uint16_t fontCount;
	uint16_t wordBreakClassCount;
	uint16_t width;
	uint16_t height;
	float inverseWidth;
	float inverseHeight;
	RuiFont* fonts;
	uint8_t* wordBreakTable;
	RuiImageAtlas* imageAtlas;
	uint32_t gpuRecordBuffer;
	uint32_t reserved2C;
};
static_assert(sizeof(RuiFontAtlas) == 0x30);
static_assert(offsetof(RuiFontAtlas, imageAtlas) == 0x20);
static_assert(offsetof(RuiFontAtlas, gpuRecordBuffer) == 0x28);

struct RuiFontGlyphGpuRecord
{
	float uvMin[2];
	float uvMax[2];
};
static_assert(sizeof(RuiFontGlyphGpuRecord) == 0x10);

struct RuiFontKerning
{
	int32_t codepoint;
	float offset;
};
static_assert(sizeof(RuiFontKerning) == 0x8);

struct RuiFontGlyph
{
	float advance;
	uint16_t firstKerning;
	uint8_t wordBreakClass;
	uint8_t proportionIndex;
	float uvBase[2];
	float boundsMin[2];
	float boundsMax[2];
};
static_assert(sizeof(RuiFontGlyph) == 0x20);

struct RuiTextGlyphState
{
	float penX;
	uint32_t glyphIndex;
	const RuiFontGlyph* glyph;
};
static_assert(sizeof(RuiTextGlyphState) == 0x10);

struct RuiFontProportion
{
	float boundsScale;
	float sizeScale;
};
static_assert(sizeof(RuiFontProportion) == 0x8);

struct RuiFont
{
	const char* name;
	uint16_t fontIndex;
	uint16_t proportionCount;
	uint16_t glyphChunkCount;
	uint16_t unicodeChunkCount;
	int32_t glyphChunkBase;
	int32_t unicodeChunkBase;
	uint32_t glyphCount;
	float atlasScale[2];
	float ascentFraction;
	float unknown28;
	uint32_t atlasGlyphBase;
	uint16_t* unicodeChunks;
	uint16_t* unicodeChunkIndices;
	uint64_t* unicodeChunkMasks;
	RuiFontProportion* proportions;
	RuiFontGlyph* glyphs;
	RuiFontKerning* kerning;
};
static_assert(sizeof(RuiFont) == 0x60);
static_assert(offsetof(RuiFont, ascentFraction) == 0x24);
static_assert(offsetof(RuiFont, unknown28) == 0x28);
static_assert(offsetof(RuiFont, atlasGlyphBase) == 0x2C);

using ReadUnicodeCharacterFn = uint32_t(__fastcall*)(char**);
using GetFontGlyphIndexFn = uint64_t(__fastcall*)(RuiFont*, int32_t);
using ResolveTextEscapeFn = char*(__fastcall*)(RuiInstance*, RuiRenderContext*, char**, char*);
