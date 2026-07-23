#pragma once

#include <cstddef>
#include <cstdint>
#include <immintrin.h>

struct RuiDrawInfo;
struct RuiFunctionTable_t;
struct RuiGlobalState;
struct RuiInstance;
struct RuiRuntimeState;
struct RuiStyleDescriptorOffsets;

using RuiImageHandle = int32_t;

using RuiUpdateCallback_t = void(*)(
	const RuiFunctionTable_t* api,
	RuiGlobalState* globals,
	RuiInstance* rui,
	uint8_t* instanceData);

struct RuiGlobalState
{
	float localToWorld[3][4];
	float cameraOriginLocal[3];
	float localPlayerPosition[3];
	float screenWidth;
	float screenHeight;
	uint8_t reserved50[64];
	uint64_t frameTime;
	float currentTime;
	float uiTime;
	int32_t isKillReplay;
	int32_t isUsingController;
	int32_t isAlive;
	int32_t isSpectating;
	int32_t isMenuOpen;
	int32_t isPhaseShifted;
	float globalAdsFraction;
	float friendlyTeamColor[3];
	float enemyTeamColor[3];
	float partyTeamColor[3];
	float announcementChangeTime;
	int32_t announcementIsActive;
};
static_assert(sizeof(RuiGlobalState) == 0xE8);
static_assert(offsetof(RuiGlobalState, localPlayerPosition) == 0x3C);
static_assert(offsetof(RuiGlobalState, currentTime) == 0x98);
static_assert(offsetof(RuiGlobalState, isMenuOpen) == 0xB0);
static_assert(offsetof(RuiGlobalState, friendlyTeamColor) == 0xBC);
static_assert(offsetof(RuiGlobalState, announcementIsActive) == 0xE4);

struct RuiRenderContext
{
	RuiGlobalState* globals;
	uint64_t reserved08;
	uint16_t rendererIndex;
	uint16_t recordCount;
	uint16_t materialBatchCount;
	uint16_t ruiCount;
	RuiInstance* instances[1];
};
static_assert(sizeof(RuiRenderContext) == 0x20);
static_assert(offsetof(RuiRenderContext, instances) == 0x18);

enum RuiArgumentType_e : uint8_t
{
	RUI_ARG_STRING = 1,
	RUI_ARG_ASSET = 2,
	RUI_ARG_BOOL = 3,
	RUI_ARG_INT = 4,
	RUI_ARG_FLOAT = 5,
	RUI_ARG_FLOAT2 = 6,
	RUI_ARG_FLOAT3 = 7,
	RUI_ARG_COLOR_ALPHA = 8,
	RUI_ARG_GAMETIME = 9,
	RUI_ARG_WALLTIME = 10,
	RUI_ARG_UIHANDLE = 11,
	RUI_ARG_IMAGE = 12,
	RUI_ARG_FONT_FACE = 13,
	RUI_ARG_FONT_HASH = 14,
	RUI_ARG_ARRAY = 15,
};

struct RuiArgument
{
	RuiArgumentType_e type;
	uint8_t flags;
	int16_t valueOffset;
	int16_t nameOffset;
	int16_t nameHash;
};
static_assert(sizeof(RuiArgument) == 0x8);

struct RuiArgumentCluster
{
	uint16_t firstArgument;
	uint16_t argumentCount;
	uint8_t hashMultiplier;
	uint8_t hashAddend;
	uint16_t reserved06;
	uint16_t reserved08;
	uint16_t instanceDataSize;
	uint16_t reserved0C;
	uint16_t reserved0E;
	uint16_t renderJobCount;
};
static_assert(sizeof(RuiArgumentCluster) == 0x12);
static_assert(offsetof(RuiArgumentCluster, hashMultiplier) == 0x4);
static_assert(offsetof(RuiArgumentCluster, instanceDataSize) == 0xA);
static_assert(offsetof(RuiArgumentCluster, renderJobCount) == 0x10);

struct RuiMappingDescriptor
{
	uint32_t keyframeCount;
	uint16_t componentCount;
	uint16_t hasTangents;
	float* keyframeData;
};
static_assert(sizeof(RuiMappingDescriptor) == 0x10);

struct RuiInterpolationWeights
{
	const float* values;
	float weights[4];
};
static_assert(sizeof(RuiInterpolationWeights) == 0x18);

struct RuiHeader
{
	const char* name;
	void* defaultValues;
	uint8_t* transformData;
	float elementWidth;
	float elementHeight;
	float inverseElementWidth;
	float inverseElementHeight;
	const char* argumentNames;
	RuiArgumentCluster* argumentClusters;
	RuiArgument* arguments;
	uint16_t argumentCount;
	uint16_t reserved42;
	uint16_t instanceDataSize;
	uint16_t defaultValuesSize;
	uint16_t styleDescriptorCount;
	uint16_t reserved4A;
	uint16_t renderJobCount;
	uint16_t argumentClusterCount;
	RuiStyleDescriptorOffsets* styleDescriptors;
	uint8_t* renderJobs;
	RuiMappingDescriptor* mappingData;
	RuiUpdateCallback_t update;
	RuiUpdateCallback_t updateHidden;
};
static_assert(sizeof(RuiHeader) == 0x78);
static_assert(offsetof(RuiHeader, argumentClusters) == 0x30);
static_assert(offsetof(RuiHeader, arguments) == 0x38);
static_assert(offsetof(RuiHeader, mappingData) == 0x60);
static_assert(offsetof(RuiHeader, update) == 0x68);

struct RuiInstance
{
	RuiHeader* header;
	float canvasWidth;
	float canvasHeight;
	float inverseCanvasWidth;
	float inverseCanvasHeight;
	RuiRuntimeState* runtime;
	int64_t createTimestamp;
	uint8_t hidden;
	uint8_t hasError;
	uint8_t reserved2A[14];
	RuiDrawInfo* drawInfo;
	uint8_t data[1];
};
static_assert(sizeof(RuiInstance) == 0x48);
static_assert(offsetof(RuiInstance, drawInfo) == 0x38);

struct RuiFunctionTable_t
{
	void(*setHidden)(RuiInstance* rui);
	void(*setNoRender)(RuiInstance* rui);
	void(*setErrorWithReason)(RuiInstance* rui, const char* reason);
	__m128* (*getTransformSizes)(RuiInstance* rui);
	__m128(*measureTextJob)(RuiInstance* rui, uint32_t renderJobOffset);
	__m128(*normalizeTransformRange)(RuiInstance* rui, uint32_t firstTransform, uint32_t endTransform);
	void(*executeTransform)(RuiInstance* rui, uint32_t endOffset);
	const char* (*format)(RuiInstance* rui, const char* format, ...);
	const char* (*localize)(
		RuiInstance* rui,
		const char* key,
		uint64_t arg0,
		uint64_t arg1,
		uint64_t arg2,
		uint64_t arg3,
		uint64_t arg4);
	const char* (*toUppercase)(RuiInstance* rui, const char* text);
	__m128(*srgbToLinear)(const __m128* srgb);
	__m128(*buildSinCosVector)(float angle);
	float(*randomFloat)(RuiInstance* rui);
	__m128(*unproject)(const RuiInstance* rui, const __m128* screenToWorld, const __m128* screenPoint);
	__m128(*computeAspectCompensationExtents)(const RuiInstance* rui);
	RuiImageHandle(*findImageAsset)(RuiInstance* rui, const char* imageName);
	const char* (*encodeCodepoint)(RuiInstance* rui, int32_t codepoint);
	float(*evaluateFloat)(RuiInstance* rui, uint32_t mappingIndex, float position);
	__m128(*evaluateFloat2)(RuiInstance* rui, uint32_t mappingIndex, float position);
	__m128(*evaluateFloat3)(RuiInstance* rui, uint32_t mappingIndex, float position);
	__m128(*evaluateFloat4)(RuiInstance* rui, uint32_t mappingIndex, float position);
};
static_assert(sizeof(RuiFunctionTable_t) == 0xA8);
