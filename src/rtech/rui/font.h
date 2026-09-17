#pragma once

#include "rtech/rui/rui.h"

#include <cstdint>

struct RuiFont;

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

struct RuiFontKerning
{
    int32_t codepoint;
    float offset;
};

struct RuiFontGlyph
{
    float advance;
    uint16_t firstKerning;
    uint8_t wordBreakClass;
    uint8_t proportionIndex;
    Vector2D uvOrigin;
    Vector2D boundsMin;
    Vector2D boundsMax;
};

struct RuiFontProportion
{
    float boundsScale;
    float sizeScale;
};

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
    Vector2D atlasScale;
    float ascentFraction;
    float reserved28;
    uint32_t atlasGlyphBase;
    uint16_t* unicodeChunks;
    uint16_t* unicodeChunkIndices;
    uint64_t* unicodeChunkMasks;
    RuiFontProportion* proportions;
    RuiFontGlyph* glyphs;
    RuiFontKerning* kerning;
};

struct RuiTextDrawStyle
{
    RuiFont* font;
    uint8_t fontAtlasIndex;
    float textSize;
    float glyphScaleX;
    float advanceScale;
    float ascent;
    float boundsOffset[4];
    bool renderable;
};

uint64_t RuiFont_GetGlyphIndex(RuiFont* font, int32_t codepoint);
