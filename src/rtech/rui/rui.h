#pragma once

#include <emmintrin.h>
#include "mathlib/fltx4.h"
#include "mathlib/vector2d.h"
#include "mathlib/vector4d.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#define RUI_TEXT_LINE_CAPACITY 64
#define RUI_TEXT_ANTI_ALIAS_EDGE_MASK 0xE
#define RUI_STANDALONE_PERCENT_MASK 0x80005002u

#define RUI_INLINE_IMAGE_CAPACITY 64

#define RUI_RENDER_JOB_CAPACITY 179

#define RUI_TRANSFORM_CAPACITY 203

#define RUI_CODEPOINT_ICON_FIRST 0xF0000u
#define RUI_CODEPOINT_ICON_COUNT 0x2000u
#define RUI_CODEPOINT_IMAGE_BORDER_BEGIN (RUI_CODEPOINT_ICON_FIRST + RUI_CODEPOINT_ICON_COUNT)
#define RUI_CODEPOINT_IMAGE_BORDER_END (RUI_CODEPOINT_IMAGE_BORDER_BEGIN + 1u)
#define RUI_CODEPOINT_MISSING_GLYPH 0x25A1

struct RuiComputedStyle;
struct RuiDrawInfo;
struct RuiFunctionTable_t;
struct RuiGlobalState;
struct RuiInstance;
struct RuiRuntimeState;
struct RuiFontAtlas;
struct RuiImageAtlas;
struct RuiImageAtlasEntry;
struct RuiStyleDescriptorOffsets;

using RuiImageHandle = int32_t;

using RuiUpdateCallback_t = void (*)(const RuiFunctionTable_t* api, RuiGlobalState* globals, RuiInstance* rui, uint8_t* instanceData);

enum RuiArgumentType_e : uint8_t
{
    RUI_ARG_INVALID = 0,
    RUI_ARG_STRING = 1,
    RUI_ARG_ASSET = 2,
    RUI_ARG_BOOL = 3,
    RUI_ARG_INT = 4,
    RUI_ARG_FLOAT = 5,
    RUI_ARG_FLOAT2 = 6,
    RUI_ARG_FLOAT3 = 7,
    RUI_ARG_FLOAT4 = 8,
    RUI_ARG_GAMETIME = 9,
    RUI_ARG_WALLTIME = 10,
    RUI_ARG_IMAGE = 11,
    RUI_ARG_ARRAY = 12,
    RUI_ARG_COUNT = 13,
};

enum RuiVariableType_e : uint8_t
{
    RUI_VAR_ARGUMENT = 0,
    RUI_VAR_MEMORY = 1,
    RUI_VAR_COUNT = 2,
};

enum class RuiDrawInfoMode : uint32_t
{
    Direct = 0,
    Clipped = 1,
    Mesh = 2,
    Angular = 3,
};

enum class RuiTransformSign : uint32_t
{
    Positive = 0,
    Zero = 1,
    Negative = 2,
};


struct RuiInverseTransform
{
    float gradient[4];
    Vector2D origin;
    RuiTransformSign sign;
};

struct alignas(16) RuiGlobalState
{
    float localFromWorld[3][4];
    float cameraOriginLocal[3];
    float cameraOriginWorld[3];
    float viewportWidth;
    float viewportHeight;
    float viewProjection[4][4];
    uint64_t wallTime;
    float gameTime;
    float uiTime;
    int32_t isWatchingKillReplay;
    int32_t isUsingController;
    int32_t isAlive;
    int32_t isSpectator;
    int32_t hasOpenMenu;
    int32_t isPhaseShifted;
    float adsFraction;
    float friendlyTeamColor[3];
    float enemyTeamColor[3];
    float partyTeamColor[3];
    float announcementChangeTime;
    int32_t announcementIsActive;
    uint8_t reservedE8[8];
};

struct RuiTextLookupEntry
{
    const char* key;
    char* value;
};

struct RuiTextLookupTable
{
    uint32_t usePrimaryVariant;
    uint16_t minimumKeyLength;
    uint16_t maximumKeyLength;
    const uint16_t* lengthBuckets;
    const RuiTextLookupEntry* entries;
    char* fallback;
};

struct RuiRenderContext
{
    RuiGlobalState* globals;
    const RuiTextLookupTable* stringTable;
    uint16_t stage;
    uint16_t slotCount;
    uint16_t drawBatchCount;
    uint16_t instanceCount;
    RuiInstance* instances[1];
};

struct RuiArgument
{
    RuiArgumentType_e dataType;
    RuiVariableType_e variableType;
    uint16_t dataOffset;
    uint16_t nameOffset;
    uint16_t hashValidation;
};

struct RuiArgumentCluster
{
    uint16_t argumentHashBegin;
    uint16_t argumentHashCount;
    uint8_t hashMultiplier;
    uint8_t hashBias;
    uint16_t defaultValuesOffset;
    uint16_t defaultValuesSize;
    uint16_t runtimeDataSize;
    uint16_t maxArrayElements;
    uint16_t renderJobOffset;
    uint16_t renderJobCount;
};

struct RuiMappingDescriptor
{
    uint32_t frameCount;
    uint16_t dimensionCount;
    uint16_t isCubic;
    const float* data;
};

struct RuiInterpolationWeights
{
    const float* values;
    float weights[4];
};

struct RuiHeader
{
    const char* name;
    const void* defaultValues;
    const uint8_t* transformData;
    float elementWidth;
    float elementHeight;
    float inverseElementWidth;
    float inverseElementHeight;
    const char* argumentNames;
    const RuiArgumentCluster* argumentClusters;
    const RuiArgument* arguments;
    uint16_t argumentCount;
    uint16_t mappingCount;
    uint16_t instanceDataSize;
    uint16_t defaultValuesSize;
    uint16_t styleDescriptorCount;
    uint16_t transformCount;
    uint16_t renderJobCount;
    uint16_t argumentClusterCount;
    const RuiStyleDescriptorOffsets* styleDescriptors;
    const uint8_t* renderJobs;
    const RuiMappingDescriptor* mappingData;
    RuiUpdateCallback_t update;
    RuiUpdateCallback_t updateHidden;
};

struct RuiInstance
{
    RuiHeader* header;
    float actualWidth;
    float actualHeight;
    float reciprocalActualWidth;
    float reciprocalActualHeight;
    RuiRuntimeState* runtime;
    uint64_t randomSeed;
    uint8_t hidden;
    uint8_t hasError;
    uint8_t reserved2A[14];
    RuiDrawInfo* drawInfo;
    uint8_t data[1];

    template <typename T> T GetValue(uint16_t offset) const
    {
        static_assert(std::is_trivially_copyable_v<T>);
        T value;
        std::memcpy(&value, data + offset, sizeof(value));
        return value;
    }
};

struct RuiFunctionTable_t
{
    void (*setHidden)(RuiInstance* rui);
    void (*setNoRender)(RuiInstance* rui);
    void (*setErrorWithReason)(RuiInstance* rui, const char* reason);
    fltx4* (*getTransformSizes)(RuiInstance* rui);
    fltx4 (*measureTextJob)(RuiInstance* rui, uint32_t renderJobOffset);
    fltx4 (*normalizeTransformRange)(RuiInstance* rui, uint32_t firstTransform, uint32_t endTransform);
    void (*executeTransform)(RuiInstance* rui, uint32_t endOffset);
    const char* (*format)(RuiInstance* rui, const char* format, ...);
    const char* (*localize)(RuiInstance* rui, const char* key, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);
    const char* (*toUppercase)(RuiInstance* rui, const char* text);
    fltx4 (*srgbToLinear)(const fltx4* srgb);
    fltx4 (*buildSinCosVector)(float angle);
    float (*randomFloat)(RuiInstance* rui);
    fltx4 (*unproject)(const RuiInstance* rui, const fltx4* screenToWorld, const fltx4* screenPoint);
    fltx4 (*computeAspectCompensationExtents)(const RuiInstance* rui);
    RuiImageHandle (*findImageAsset)(RuiInstance* rui, const char* imageName);
    const char* (*encodeCodepoint)(RuiInstance* rui, int32_t codepoint);
    float (*evaluateFloat)(RuiInstance* rui, uint32_t mappingIndex, float position);
    fltx4 (*evaluateFloat2)(RuiInstance* rui, uint32_t mappingIndex, float position);
    fltx4 (*evaluateFloat3)(RuiInstance* rui, uint32_t mappingIndex, float position);
    fltx4 (*evaluateFloat4)(RuiInstance* rui, uint32_t mappingIndex, float position);
};

struct RuiBounds
{
    float left;
    float top;
    float right;
    float bottom;
};

struct RuiColorOffsets
{
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    uint16_t alpha;
};

struct RuiStyleCommonOffsets
{
    RuiColorOffsets primaryColor;
    RuiColorOffsets secondaryColor;
    RuiColorOffsets tertiaryColor;
    uint16_t blend;
    uint16_t premultiply;
};

struct RuiStyleDescriptorOffsets
{
    uint16_t type;
    RuiStyleCommonOffsets common;
    uint16_t typeSpecificOffsets[11];
};

struct RuiEllipseStyleDescriptorOffsets
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
    uint16_t reserved2C[4];
};

struct RuiFloat2Offsets
{
    uint16_t x;
    uint16_t y;
};

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

struct alignas(16) RuiTransform
{
    float gradient[4];
    float origin[4];
};

struct alignas(16) RuiAntiAliasSetup
{
    Vector4D normGradX_x;
    Vector4D normGradX_y;
    Vector4D normGradY_x;
    Vector4D normGradY_y;
    Vector4D cornerExtraScaleSq;
};

struct RuiRenderJobState
{
    float fittedScale;
    uint8_t firstLine;
    uint8_t lineCount;
    uint8_t firstInlineImage;
    uint8_t inlineImageCount;
};

struct RuiInlineImageSpan
{
    uint16_t descriptorIndex;
    uint16_t styleIndex;
    Vector2D boundsMin;
    Vector2D boundsMax;
};

struct RuiTextLineRecord
{
    float terminalWidth;
    uint32_t breakGlyph;
    float wrappedWidth;
};

struct RuiRuntimeState
{
    uint8_t* transformDataCursor;
    uint32_t transformCount;
    uint32_t randomState[3];
    RuiRenderContext* textContext;
    RuiRenderJobState renderJobStates[RUI_RENDER_JOB_CAPACITY];
    uint32_t textScratchUsed;
    uint8_t textScratch[0x2008];
    RuiTextLineRecord textLines[RUI_TEXT_LINE_CAPACITY];
    float textLineTerminalWidth;
    uint32_t textLineCount;
    uint32_t inlineImageCount;
    RuiInlineImageSpan inlineImages[RUI_INLINE_IMAGE_CAPACITY];
    Vector4D transformSizes[RUI_TRANSFORM_CAPACITY];
    RuiTransform transforms[RUI_TRANSFORM_CAPACITY];

    float& GetTextLineTerminalWidth(uint32_t lineIndex)
    {
        return lineIndex < RUI_TEXT_LINE_CAPACITY ? textLines[lineIndex].terminalWidth : textLineTerminalWidth;
    }
};

struct RuiDrawInfo
{
    RuiDrawInfoMode mode;
};

struct RuiDrawQuad
{
    uint32_t vertexCount;
    uint32_t vertexCapacity;
    float positions[2][4];
};

struct alignas(16) RuiProjectedQuad
{
    float x[4];
    float y[4];
};

struct RuiDrawMaterialBatch
{
    uint32_t firstVertex;
    uint32_t firstIndex;
    RuiFontAtlas* fontAtlas;
    RuiImageAtlas* imageAtlas;
};

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

struct RuiBaseUv
{
    Vector4D primaryBasisX;
    Vector4D primaryBasisY;
    Vector4D primaryOrigin;
    Vector4D secondaryBasisX;
    Vector4D secondaryBasisY;
    Vector4D secondaryOrigin;
    int16_t imageIndex;
    int16_t maskImageIndex;
    int16_t computedStyleIndex;
    uint16_t flags;
};

bool RuiIsIconCodePoint(uint32_t codepoint);
bool RuiIsStringDiv(uint32_t codepoint);
bool RuiIsStandalonePercent(char marker);
bool RuiInvertTransform(const RuiTransform& transform, RuiInverseTransform& inverse);
bool RuiClipImageBounds(const RuiImageAtlasEntry& image, const RuiBounds& requested, const Vector2D& uvMin, const Vector2D& uvMax,
                        const uint32_t* clampMasks, uint16_t flags, const float* usefulPadding, RuiBounds& clipped);
void RuiProjectBounds(const RuiTransform& transform, const RuiBounds& bounds, RuiProjectedQuad& positions);
void RuiStoreQuad(RuiDrawQuad& quad, const RuiProjectedQuad& positions, RuiTransformSign transformSign);
void RuiBuildPrimaryMapping(RuiBaseUv& mapping, const RuiInverseTransform& inverse, const RuiImageAtlasEntry& image, const Vector2D& uvMin,
                            const Vector2D& uvMax);
void RuiBuildDefaultMaskMapping(RuiBaseUv& mapping, const RuiInverseTransform& inverse, const RuiBounds& bounds);
void RuiBuildImageMaskMapping(RuiBaseUv& mapping, const RuiInstance& rui, const RuiImageRenderJob& job, const RuiInverseTransform& inverse,
                              const RuiImageAtlasEntry& image, const Vector2D& uvMin, const Vector2D& uvMax);
void RuiBuildInlineMapping(RuiBaseUv& mapping, const RuiInverseTransform& inverse, const RuiImageAtlasEntry& image, const RuiBounds& bounds);
bool RuiDrawBatch_BindImageAtlas(RuiDrawBatch* batch, RuiImageAtlas* atlas);
void RuiBuildAntiAliasSetup(const RuiTransform* transform, const float* elementSize, RuiAntiAliasSetup* setup);
uint32_t RuiDecodeCodePoint(char** cursor);
