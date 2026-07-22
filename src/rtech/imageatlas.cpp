#include "rtech/imageatlas.h"
#include "rtech/pakfilesystem.h"
#include "rtech/paktools.h"
#include "materialsystem/cmaterialglue.h"
#include "dedicated/dedicated.h"
#include "vscript/squirrel/squirrel.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include <immintrin.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
DECLARE_MODULE(ImageAtlas)

struct RuiImageAtlas;
struct RuiInstance;
struct RuiRenderContext;

struct RuiDrawQuad
{
	uint32_t vertexCount;
	uint32_t vertexCapacity;
	float positions[2][4];
};
static_assert(sizeof(RuiDrawQuad) == 0x28);

struct RuiImageAtlasEntry
{
	float bounds[4];
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
	float uvMax[2];
};
static_assert(sizeof(RuiImageAtlasGpuRecord) == 0x10);

struct RuiImageAtlasNineSlice
{
	float normalizedBounds[4];
	float edgeScale[2];
	float minimumEdgeSize[2];
};
static_assert(sizeof(RuiImageAtlasNineSlice) == 0x20);

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
	uint64_t* imageNameHashes;
	const char** imageNames;
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
static_assert(offsetof(RuiImageAtlas, images) == 0x10);
static_assert(offsetof(RuiImageAtlas, nineSliceData) == 0x20);
static_assert(offsetof(RuiImageAtlas, imageNameHashes) == 0x28);

struct RuiRenderJobState
{
	float fittedScale;
	uint8_t firstLine;
	uint8_t lineCount;
	uint8_t firstInlineImage;
	uint8_t inlineImageCount;
};
static_assert(sizeof(RuiRenderJobState) == 0x8);

struct RuiTransform
{
	__m128 rows[2];
};
static_assert(sizeof(RuiTransform) == 0x20);

struct RuiInlineImageSpan
{
	uint16_t descriptorIndex;
	uint16_t styleIndex;
	float boundsMin[2];
	float boundsMax[2];
};
static_assert(sizeof(RuiInlineImageSpan) == 0x14);

struct RuiRuntimeState
{
	uint8_t* unknown00;
	uint32_t unknown08;
	uint32_t unknown0C;
	uint32_t unknown10;
	uint32_t unknown14;
	RuiRenderContext* textContext;
	RuiRenderJobState renderJobStates[179];
	uint32_t textScratchUsed;
	uint8_t textScratch[0x2008];
	float textLineData[193];
	uint32_t textLineCount;
	uint32_t inlineImageCount;
	RuiInlineImageSpan inlineImages[64];
	__m128 transformSizes[203];
	RuiTransform transforms[1];
};
static_assert(sizeof(RuiRuntimeState) == 0x3AA0);
static_assert(offsetof(RuiRuntimeState, renderJobStates) == 0x20);
static_assert(offsetof(RuiRuntimeState, textScratchUsed) == 0x5B8);
static_assert(offsetof(RuiRuntimeState, textScratch) == 0x5BC);
static_assert(offsetof(RuiRuntimeState, textLineData) == 0x25C4);
static_assert(offsetof(RuiRuntimeState, inlineImages) == 0x28D0);
static_assert(offsetof(RuiRuntimeState, transformSizes) == 0x2DD0);
static_assert(offsetof(RuiRuntimeState, transforms) == 0x3A80);

struct RuiGlobalState
{
	uint8_t reserved00[60];
	float viewOrigin[3];
	float screenWidth;
	float screenHeight;
	uint8_t reserved50[64];
	uint64_t frameTime;
	float currentTime;
	uint8_t reserved9C[76];
};
static_assert(sizeof(RuiGlobalState) == 0xE8);

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

struct RuiStyleDescriptorOffsets
{
	uint16_t type;
	uint16_t colorRed;
	uint16_t colorGreen;
	uint16_t colorBlue;
	uint16_t colorAlpha;
	uint16_t unknown0A;
	uint16_t unknown0C;
	uint16_t unknown0E;
	uint16_t unknown10;
	uint16_t unknown12;
	uint16_t unknown14;
	uint16_t unknown16;
	uint16_t unknown18;
	uint16_t unknown1A;
	uint16_t unknown1C;
	uint16_t fontIndex;
	uint16_t unknown20;
	uint16_t unknown22;
	uint16_t unknown24;
	uint16_t unknown26;
	uint16_t textSize;
	uint16_t stretchX;
	uint16_t unknown2C;
	uint16_t unknown2E;
	uint16_t unknown30;
	uint16_t unknown32;
};
static_assert(sizeof(RuiStyleDescriptorOffsets) == 0x34);

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
	void* argumentClusters;
	void* arguments;
	uint16_t argumentCount;
	uint16_t unknown42;
	uint16_t instanceDataSize;
	uint16_t defaultValuesSize;
	uint16_t styleDescriptorCount;
	uint16_t unknown4A;
	uint16_t renderJobCount;
	uint16_t argumentClusterCount;
	RuiStyleDescriptorOffsets* styleDescriptors;
	uint8_t* renderJobs;
	void* mappingData;
	void(__fastcall* update)(void*, void*, RuiInstance*, uint8_t*);
	void(__fastcall* updateHidden)(void*, void*, RuiInstance*, uint8_t*);
};
static_assert(sizeof(RuiHeader) == 0x78);

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

struct RuiFontAtlas
{
	uint16_t fontCount;
	uint16_t wordBreakClassCount;
	uint16_t width;
	uint16_t height;
	float inverseWidth;
	float inverseHeight;
	void* fontGpuRecords;
	uint8_t* wordBreakTable;
	RuiImageAtlas* imageAtlas;
	uint32_t gpuRecordBuffer;
	uint32_t reserved2C;
};
static_assert(sizeof(RuiFontAtlas) == 0x30);

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
	float verticalMetrics[2];
	uint32_t atlasGlyphBase;
	uint16_t* unicodeChunks;
	uint16_t* unicodeChunkIndices;
	uint64_t* unicodeChunkMasks;
	RuiFontProportion* proportions;
	RuiFontGlyph* glyphs;
	RuiFontKerning* kerning;
};
static_assert(sizeof(RuiFont) == 0x60);

struct RHashMap
{
	uint32_t size;
	uint32_t bucketPairCount;
	void* entries;
	int32_t* buckets;
	uint32_t(__fastcall* hashKey)(uint32_t key);
	bool(__fastcall* keysEqual)(const void* entry, uint32_t key);
	uint32_t freeListHead;
	uint32_t nextUnusedIndex;
	uint32_t lastInsertIndex;
	uint32_t lastProbeIndex;
	uint64_t entryStride;
	RTL_SRWLOCK lock;
};
static_assert(sizeof(RHashMap) == 0x48);

struct RuiComputedStyle
{
	float commonValues[14];
	float typeValues[7];
	uint8_t reserved54[12];
};
static_assert(sizeof(RuiComputedStyle) == 0x60);

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

struct RuiImageAssetDescriptor
{
	uint32_t nameHash;
	int16_t imageIndex;
	uint8_t atlasIndex;
	uint8_t flags;
};
static_assert(sizeof(RuiImageAssetDescriptor) == 0x8);

using RuiDrawInfoHandlerFn = bool(__fastcall*)(RuiDrawInfo*, const RuiBaseUv*, RuiDrawQuad*, RuiDrawBatch*);
using BindImageAtlasFn = bool(__fastcall*)(RuiDrawBatch*, RuiImageAtlas*);
using ReadUnicodeCharacterFn = uint32_t(__fastcall*)(char**);
using GetFontGlyphIndexFn = uint64_t(__fastcall*)(RuiFont*, int32_t);
using ResolveTextEscapeFn = char*(__fastcall*)(RuiInstance*, RuiRenderContext*, char**, char*);
using BuildEdgeCorrectionFn = void(__fastcall*)(const RuiTransform*, const float*, __m128*);
using ApplyEdgeCorrectionFn = void(__fastcall*)(RuiGlobalState*, RuiInstance*, const __m128*, const __m128*, __m128*);
using RHashMapInsertFn = void*(__fastcall*)(RHashMap*, uint32_t, uint8_t*);
using RHashMapRemoveFn = uint32_t*(__fastcall*)(RHashMap*, uint32_t);

static RuiImageAtlas g_RuiImageAtlases[RUI_IMAGE_ATLAS_CAPACITY];
static bool s_ImageAtlasLoaderConfigured = false;

DECLARE_HOOK(
	Pak_RegisterAssetBindingType,
	rtech_game.DLL + 0x7BE0,
	[](auto& hook, PakAssetBinding_s* binding, uint8_t assetType, uint32_t affinity) -> uint8_t
	{
		if (std::memcmp(binding->type, "uimg", sizeof(binding->type)) == 0)
		{
			// Native atlases use the lower range. The upper range is reserved for
			// runtime wrappers around ordinary texture assets.
			binding->trackerCapacity = RUI_DYNAMIC_IMAGE_ATLAS_FIRST;
			binding->trackerStorage = g_RuiImageAtlases;
			s_ImageAtlasLoaderConfigured = true;
		}

		return hook.Original(binding, assetType, affinity);
	})

static RuiFontAtlas* g_RuiFontAtlases;
static RuiImageAssetDescriptor* g_RuiImageDescriptors;
static RuiFont** g_RuiFonts;
static uint8_t* g_RuiFontAtlasIndices;
static RHashMap* g_RuiImageDescriptorMap;
static __m128* g_RuiEllipseAxisMasks;
static __m128* g_RuiEdgeCorrectionMasks;
static RuiDrawInfoHandlerFn* g_RuiDrawInfoHandlers;

static BindImageAtlasFn s_BindImageAtlas;
static ReadUnicodeCharacterFn s_ReadUnicodeCharacter;
static GetFontGlyphIndexFn s_GetFontGlyphIndex;
static ResolveTextEscapeFn s_ResolveTextEscape;
static BuildEdgeCorrectionFn s_BuildEdgeCorrection;
static ApplyEdgeCorrectionFn s_ApplyEdgeCorrection;
static RHashMapInsertFn s_RHashMapInsert;
static RHashMapRemoveFn s_RHashMapRemove;

static __m128 g_RuiSignMaskAll;
static __m128 g_RuiSignMaskLowHalf;
static __m128 g_RuiSignMaskMiddleLanes;
static __m128 g_RuiSignMaskHighHalf;
static __m128 g_RuiSignMaskLane2;
static __m128 g_RuiFloatTwo;
static __m128 g_RuiFloatOne;
static __m128 g_RuiFloatHalf;
static __m128 g_RuiFloatFour;
static __m128 g_RuiFloatMinNormal;
static __m128 g_RuiFloatAbsMask;
static __m128 g_RuiUnitX;
static __m128 g_RuiUnitY;
static __m128 g_RuiHighHalfOne;
static __m128 g_RuiHighHalfSignedOne;
static __m128 g_RuiQuarterEndpoints;
static __m128 g_RuiIntOne;
static __m128 g_RuiIntTwo;

static __m128 g_RuiBlendMaskLowHalf;
static __m128 g_RuiBlendMaskLane0;
static __m128 g_RuiBlendMaskLane1;
static __m128 g_RuiBlendMaskHighHalf;

static __m128 g_RuiSinApproxCoeff0;
static __m128 g_RuiSinApproxCoeff1;
static __m128 g_RuiSinApproxCoeff2;
static __m128 g_RuiSinApproxCoeff3;
static __m128 g_RuiCosApproxCoeff0;
static __m128 g_RuiCosApproxCoeff1;
static __m128 g_RuiCosApproxCoeff2;
static __m128 g_RuiCosApproxCoeff3;

namespace
{
struct RuiDynamicAtlasImage_t
{
	std::string m_Path;
	uint32_t m_PosX = 0;
	uint32_t m_PosY = 0;
	uint32_t m_Width = 0;
	uint32_t m_Height = 0;
	uint8_t m_Flags = 0;
	bool m_UseFullTexture = false;
};

struct RuiDynamicAtlasDescriptor_t
{
	uint32_t m_NameHash;
	int16_t m_AssetIndex;
	RuiImageAssetDescriptor* m_Descriptor;
};

struct RuiDynamicAtlasDefinition_t
{
	PakHandle_t m_OwnerHandle = PAK_INVALID_HANDLE;
	fs::path m_SourcePath;
	std::string m_TexturePath;
	std::optional<uint64_t> m_TextureGuid;
	std::vector<RuiDynamicAtlasImage_t> m_Images;
	std::vector<RuiImageAtlasEntry> m_TextureRecords;
	std::vector<RuiImageDimensions> m_TextureDimensions;
	std::vector<RuiImageAtlasGpuRecord> m_GpuRecords;
	std::vector<uint64_t> m_TextureHashes;
	std::vector<RuiDynamicAtlasDescriptor_t> m_Descriptors;
	std::optional<uint8_t> m_AtlasIndex;
	bool m_Invalid = false;
};

using LoadImageAtlasFn = void(__fastcall*)(RuiImageAtlas* atlas, const void* gpuRecords);
using DestroyImageAtlasFn = void(__fastcall*)(RuiImageAtlas* atlas);
using GetAssetDescriptorFn = RuiImageAssetDescriptor*(__fastcall*)(RHashMap* hashMap, uint32_t nameHash);
using RpakHashFn = uint64_t(__fastcall*)(const char* path);

LoadImageAtlasFn s_LoadImageAtlas = nullptr;
DestroyImageAtlasFn s_DestroyImageAtlas = nullptr;
GetAssetDescriptorFn s_GetAssetDescriptor = nullptr;
RpakHashFn s_RpakHashAligned = nullptr;
RpakHashFn s_RpakHashUnaligned = nullptr;

std::mutex s_DynamicAtlasMutex;
std::array<bool, RUI_IMAGE_ATLAS_CAPACITY> s_DynamicAtlasSlots = {};
std::unordered_map<PakHandle_t, std::vector<std::unique_ptr<RuiDynamicAtlasDefinition_t>>> s_DynamicAtlasesByPak;
std::unordered_map<uint32_t, std::vector<RuiDynamicAtlasDefinition_t*>> s_DynamicAtlasesByImage;
std::unordered_map<int32_t, std::unique_ptr<RuiDynamicAtlasDefinition_t>> s_ScriptDynamicAtlases;
int32_t s_NextScriptDynamicAtlasHandle = 1;

uint64_t GetRuiAssetGuid(const char* path)
{
	if (!path)
		return 0;

	RpakHashFn hashFunction = (reinterpret_cast<uintptr_t>(path) & 3)
		? s_RpakHashUnaligned
		: s_RpakHashAligned;
	return hashFunction ? hashFunction(path) : Pak_StringToGuid(path);
}

uint32_t GetRuiImageHash(const char* path)
{
	const uint64_t guid = GetRuiAssetGuid(path);
	return static_cast<uint32_t>(guid) ^ static_cast<uint32_t>(guid >> 32);
}

uint32_t GetRuiImageHash(const std::string& path)
{
	return GetRuiImageHash(path.c_str());
}

bool ParseTextureGuid(const char* text, uint64_t& textureGuid)
{
	if (!text)
		return false;

	std::string_view value(text);
	if (value.starts_with("0x") || value.starts_with("0X"))
		value.remove_prefix(2);
	if (value.empty() || value.size() > 16)
		return false;

	const auto result = std::from_chars(value.data(), value.data() + value.size(), textureGuid, 16);
	return result.ec == std::errc() && result.ptr == value.data() + value.size();
}

RuiImageAssetDescriptor* FindRuiImageDescriptor(uint32_t nameHash)
{
	if (!g_RuiImageDescriptorMap || !s_GetAssetDescriptor)
		return nullptr;

	return s_GetAssetDescriptor(g_RuiImageDescriptorMap, nameHash);
}

bool ReadAtlasUint(
	const rapidjson_document::ValueType& object,
	const char* memberName,
	uint32_t& value,
	const fs::path& sourcePath,
	const char* imagePath)
{
	if (!object.HasMember(memberName) || !object[memberName].IsUint())
	{
		spdlog::error(
			"RUI atlas sidecar '{}': image '{}' requires unsigned integer member '{}'",
			sourcePath.string(),
			imagePath,
			memberName);
		return false;
	}

	value = object[memberName].GetUint();
	return true;
}

std::unique_ptr<RuiDynamicAtlasDefinition_t> ParseDynamicAtlasDefinition(
	const rapidjson_document::ValueType& value,
	const fs::path& sourcePath,
	PakHandle_t ownerHandle)
{
	if (!value.IsObject())
	{
		spdlog::error("RUI atlas sidecar '{}': each atlas must be a JSON object", sourcePath.string());
		return nullptr;
	}

	if (value.HasMember("$type") && (!value["$type"].IsString() || strcmp(value["$type"].GetString(), "uimg") != 0))
	{
		spdlog::error("RUI atlas sidecar '{}': '$type' must be 'uimg'", sourcePath.string());
		return nullptr;
	}

	if (!value.HasMember("atlas") || !value["atlas"].IsString() || value["atlas"].GetStringLength() == 0)
	{
		spdlog::error("RUI atlas sidecar '{}': each atlas requires a non-empty string member 'atlas'", sourcePath.string());
		return nullptr;
	}

	auto definition = std::make_unique<RuiDynamicAtlasDefinition_t>();
	definition->m_OwnerHandle = ownerHandle;
	definition->m_SourcePath = sourcePath;
	definition->m_TexturePath.assign(value["atlas"].GetString(), value["atlas"].GetStringLength());

	if (!value.HasMember("textures"))
	{
		RuiDynamicAtlasImage_t image;
		image.m_Path = definition->m_TexturePath;
		image.m_UseFullTexture = true;
		definition->m_Images.push_back(std::move(image));
		return definition;
	}

	if (!value["textures"].IsArray() || value["textures"].Empty())
	{
		spdlog::error("RUI atlas sidecar '{}': 'textures' must be a non-empty array", sourcePath.string());
		return nullptr;
	}

	if (value["textures"].Size() > static_cast<rapidjson::SizeType>(INT16_MAX))
	{
		spdlog::error(
			"RUI atlas sidecar '{}': atlas '{}' contains too many images (maximum {})",
			sourcePath.string(),
			definition->m_TexturePath,
			INT16_MAX);
		return nullptr;
	}

	std::unordered_set<uint32_t> imageHashes;
	for (const auto& imageValue : value["textures"].GetArray())
	{
		if (!imageValue.IsObject() || !imageValue.HasMember("path") || !imageValue["path"].IsString()
			|| imageValue["path"].GetStringLength() == 0)
		{
			spdlog::error("RUI atlas sidecar '{}': every image requires a non-empty string member 'path'", sourcePath.string());
			return nullptr;
		}

		RuiDynamicAtlasImage_t image;
		image.m_Path.assign(imageValue["path"].GetString(), imageValue["path"].GetStringLength());
		if (!ReadAtlasUint(imageValue, "posX", image.m_PosX, sourcePath, image.m_Path.c_str())
			|| !ReadAtlasUint(imageValue, "posY", image.m_PosY, sourcePath, image.m_Path.c_str())
			|| !ReadAtlasUint(imageValue, "width", image.m_Width, sourcePath, image.m_Path.c_str())
			|| !ReadAtlasUint(imageValue, "height", image.m_Height, sourcePath, image.m_Path.c_str()))
		{
			return nullptr;
		}

		if (image.m_Width == 0 || image.m_Height == 0 || image.m_Width > UINT16_MAX || image.m_Height > UINT16_MAX)
		{
			spdlog::error(
				"RUI atlas sidecar '{}': image '{}' width and height must be between 1 and {}",
				sourcePath.string(),
				image.m_Path,
				UINT16_MAX);
			return nullptr;
		}

		if (imageValue.HasMember("flags"))
		{
			if (!imageValue["flags"].IsUint() || imageValue["flags"].GetUint() > UINT8_MAX)
			{
				spdlog::error(
					"RUI atlas sidecar '{}': image '{}' flags must be between 0 and {}",
					sourcePath.string(),
					image.m_Path,
					UINT8_MAX);
				return nullptr;
			}
			image.m_Flags = static_cast<uint8_t>(imageValue["flags"].GetUint());
		}

		const uint32_t imageHash = GetRuiImageHash(image.m_Path);
		if (!imageHashes.insert(imageHash).second)
		{
			spdlog::error(
				"RUI atlas sidecar '{}': image '{}' collides with another image name in the same atlas",
				sourcePath.string(),
				image.m_Path);
			return nullptr;
		}

		definition->m_Images.push_back(std::move(image));
	}

	return definition;
}

std::vector<std::unique_ptr<RuiDynamicAtlasDefinition_t>> ParseDynamicAtlasSidecar(
	const fs::path& sourcePath,
	PakHandle_t ownerHandle)
{
	std::ifstream stream(sourcePath, std::ios::binary);
	if (!stream)
	{
		spdlog::error("Failed opening RUI atlas sidecar '{}'", sourcePath.string());
		return {};
	}

	std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	rapidjson_document document;
	document.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(contents.c_str(), contents.size());
	if (document.HasParseError())
	{
		spdlog::error(
			"Failed parsing RUI atlas sidecar '{}': {} at byte {}",
			sourcePath.string(),
			rapidjson::GetParseError_En(document.GetParseError()),
			document.GetErrorOffset());
		return {};
	}

	std::vector<std::unique_ptr<RuiDynamicAtlasDefinition_t>> definitions;
	auto parseArray = [&](const rapidjson_document::ValueType& array)
	{
		for (const auto& atlasValue : array.GetArray())
		{
			auto definition = ParseDynamicAtlasDefinition(atlasValue, sourcePath, ownerHandle);
			if (definition)
				definitions.push_back(std::move(definition));
		}
	};

	if (document.IsArray())
	{
		parseArray(document);
	}
	else if (document.IsObject() && document.HasMember("atlases"))
	{
		if (!document["atlases"].IsArray())
		{
			spdlog::error("RUI atlas sidecar '{}': 'atlases' must be an array", sourcePath.string());
			return {};
		}
		parseArray(document["atlases"]);
	}
	else if (document.IsObject())
	{
		auto definition = ParseDynamicAtlasDefinition(document, sourcePath, ownerHandle);
		if (definition)
			definitions.push_back(std::move(definition));
	}
	else
	{
		spdlog::error("RUI atlas sidecar '{}': root must be an atlas object or array", sourcePath.string());
	}

	return definitions;
}

std::unique_ptr<RuiDynamicAtlasDefinition_t> ParseScriptDynamicAtlas(
	uint64_t textureGuid,
	const char* jsonData,
	std::string& errorMessage)
{
	if (!jsonData || !*jsonData)
	{
		errorMessage = "jsonData cannot be empty";
		return nullptr;
	}

	rapidjson_document document;
	document.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(jsonData);
	if (document.HasParseError())
	{
		errorMessage = fmt::format(
			"invalid atlas JSON: {} at byte {}",
			rapidjson::GetParseError_En(document.GetParseError()),
			document.GetErrorOffset());
		return nullptr;
	}

	const rapidjson_document::ValueType* textures = nullptr;
	if (document.IsArray())
		textures = &document;
	else if (document.IsObject() && document.HasMember("textures"))
		textures = &document["textures"];

	if (!textures || !textures->IsArray() || textures->Empty())
	{
		errorMessage = "jsonData must be a non-empty image array or an object containing a non-empty 'textures' array";
		return nullptr;
	}
	if (textures->Size() > static_cast<rapidjson::SizeType>(INT16_MAX))
	{
		errorMessage = fmt::format("an atlas can contain at most {} images", INT16_MAX);
		return nullptr;
	}

	auto definition = std::make_unique<RuiDynamicAtlasDefinition_t>();
	definition->m_SourcePath = "NS_CreateImageAtlas";
	definition->m_TexturePath = fmt::format("GUID 0x{:016X}", textureGuid);
	definition->m_TextureGuid = textureGuid;

	std::unordered_set<uint32_t> imageHashes;
	for (const auto& imageValue : textures->GetArray())
	{
		if (!imageValue.IsObject() || !imageValue.HasMember("path") || !imageValue["path"].IsString()
			|| imageValue["path"].GetStringLength() == 0)
		{
			errorMessage = "every atlas image requires a non-empty string member 'path'";
			return nullptr;
		}

		RuiDynamicAtlasImage_t image;
		image.m_Path.assign(imageValue["path"].GetString(), imageValue["path"].GetStringLength());
		auto readUint = [&](const char* memberName, uint32_t& output)
		{
			if (!imageValue.HasMember(memberName) || !imageValue[memberName].IsUint())
			{
				errorMessage = fmt::format("image '{}' requires unsigned integer member '{}'", image.m_Path, memberName);
				return false;
			}
			output = imageValue[memberName].GetUint();
			return true;
		};

		if (!readUint("posX", image.m_PosX) || !readUint("posY", image.m_PosY)
			|| !readUint("width", image.m_Width) || !readUint("height", image.m_Height))
		{
			return nullptr;
		}
		if (image.m_Width == 0 || image.m_Height == 0 || image.m_Width > UINT16_MAX || image.m_Height > UINT16_MAX)
		{
			errorMessage = fmt::format("image '{}' width and height must be between 1 and {}", image.m_Path, UINT16_MAX);
			return nullptr;
		}

		if (imageValue.HasMember("flags"))
		{
			if (!imageValue["flags"].IsUint() || imageValue["flags"].GetUint() > UINT8_MAX)
			{
				errorMessage = fmt::format("image '{}' flags must be between 0 and {}", image.m_Path, UINT8_MAX);
				return nullptr;
			}
			image.m_Flags = static_cast<uint8_t>(imageValue["flags"].GetUint());
		}

		const uint32_t imageHash = GetRuiImageHash(image.m_Path);
		if (!imageHashes.insert(imageHash).second)
		{
			errorMessage = fmt::format("image '{}' collides with another image name in the atlas", image.m_Path);
			return nullptr;
		}
		definition->m_Images.push_back(std::move(image));
	}

	return definition;
}

std::optional<fs::path> FindDynamicAtlasSidecar(const fs::path& pakPath)
{
	std::error_code error;
	fs::path appendedPath = pakPath;
	appendedPath += ".atlas.json";
	if (fs::is_regular_file(appendedPath, error))
		return appendedPath;

	error.clear();
	fs::path replacedPath = pakPath;
	replacedPath.replace_extension(".atlas.json");
	if (fs::is_regular_file(replacedPath, error))
		return replacedPath;

	return std::nullopt;
}

std::optional<uint8_t> AllocateDynamicAtlasSlot()
{
	for (int atlasIndex = RUI_DYNAMIC_IMAGE_ATLAS_LAST; atlasIndex >= RUI_DYNAMIC_IMAGE_ATLAS_FIRST; --atlasIndex)
	{
		if (s_DynamicAtlasSlots[atlasIndex])
			continue;

		s_DynamicAtlasSlots[atlasIndex] = true;
		return static_cast<uint8_t>(atlasIndex);
	}

	return std::nullopt;
}

void ReleaseDynamicAtlasSlot(uint8_t atlasIndex)
{
	if (atlasIndex >= RUI_DYNAMIC_IMAGE_ATLAS_FIRST && atlasIndex <= RUI_DYNAMIC_IMAGE_ATLAS_LAST)
		s_DynamicAtlasSlots[atlasIndex] = false;
}

bool HasFreeRuiImageDescriptor()
{
	return g_RuiImageDescriptorMap->freeListHead != g_RuiImageDescriptorMap->nextUnusedIndex
		|| g_RuiImageDescriptorMap->nextUnusedIndex < g_RuiImageDescriptorMap->bucketPairCount;
}

bool DescriptorStillBelongsTo(
	const RuiDynamicAtlasDescriptor_t& insertedDescriptor,
	uint8_t atlasIndex)
{
	const RuiImageAssetDescriptor* descriptor = insertedDescriptor.m_Descriptor;
	return descriptor && descriptor->nameHash == insertedDescriptor.m_NameHash
		&& descriptor->imageIndex == insertedDescriptor.m_AssetIndex && descriptor->atlasIndex == atlasIndex;
}

void RemoveDynamicAtlasDescriptors(RuiDynamicAtlasDefinition_t& definition)
{
	if (!g_RuiImageDescriptorMap || !s_RHashMapRemove || !definition.m_AtlasIndex)
	{
		definition.m_Descriptors.clear();
		return;
	}

	AcquireSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
	for (const RuiDynamicAtlasDescriptor_t& insertedDescriptor : definition.m_Descriptors)
	{
		if (DescriptorStillBelongsTo(insertedDescriptor, *definition.m_AtlasIndex))
			s_RHashMapRemove(g_RuiImageDescriptorMap, insertedDescriptor.m_NameHash);
	}
	ReleaseSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
	definition.m_Descriptors.clear();
}

bool AddMissingDynamicAtlasDescriptors(RuiDynamicAtlasDefinition_t& definition)
{
	if (!g_RuiImageDescriptorMap || !s_RHashMapInsert || !s_RHashMapRemove || !s_GetAssetDescriptor || !definition.m_AtlasIndex)
		return false;

	bool addedDescriptor = false;
	AcquireSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
	for (size_t imageIndex = 0; imageIndex < definition.m_Images.size(); ++imageIndex)
	{
		const RuiDynamicAtlasImage_t& image = definition.m_Images[imageIndex];
		const uint32_t nameHash = GetRuiImageHash(image.m_Path);
		if (s_GetAssetDescriptor(g_RuiImageDescriptorMap, nameHash))
			continue;

		if (!HasFreeRuiImageDescriptor())
		{
			spdlog::error("RUI image descriptor table is full while loading '{}'", definition.m_SourcePath.string());
			break;
		}

		uint8_t inserted = 0;
		auto* descriptor = static_cast<RuiImageAssetDescriptor*>(s_RHashMapInsert(g_RuiImageDescriptorMap, nameHash, &inserted));
		if (!descriptor || !inserted)
			continue;

		descriptor->nameHash = nameHash;
		descriptor->imageIndex = static_cast<int16_t>(imageIndex);
		descriptor->atlasIndex = *definition.m_AtlasIndex;
		descriptor->flags = image.m_Flags;
		g_RuiImageDescriptorMap->buckets[g_RuiImageDescriptorMap->lastProbeIndex] = g_RuiImageDescriptorMap->lastInsertIndex;
		++g_RuiImageDescriptorMap->size;
		definition.m_Descriptors.push_back(
			{nameHash, static_cast<int16_t>(imageIndex), descriptor});
		if (s_GetAssetDescriptor(g_RuiImageDescriptorMap, nameHash) != descriptor)
		{
			spdlog::error(
				"RUI image '{}' descriptor insertion failed verification for hash 0x{:08X}",
				image.m_Path,
				nameHash);
			continue;
		}

		spdlog::info(
			"Registered RUI image '{}' with hash 0x{:08X} in runtime atlas {} at index {}",
			image.m_Path,
			nameHash,
			*definition.m_AtlasIndex,
			imageIndex);
		addedDescriptor = true;
	}
	ReleaseSRWLockExclusive(&g_RuiImageDescriptorMap->lock);

	return addedDescriptor;
}

void DestroyDynamicAtlas(RuiDynamicAtlasDefinition_t& definition)
{
	RemoveDynamicAtlasDescriptors(definition);
	if (!definition.m_AtlasIndex)
		return;

	RuiImageAtlas& atlas = g_RuiImageAtlases[*definition.m_AtlasIndex];
	if (s_DestroyImageAtlas && atlas.gpuRecordBuffer != UINT_MAX)
		s_DestroyImageAtlas(&atlas);

	atlas = {};
	ReleaseDynamicAtlasSlot(*definition.m_AtlasIndex);
	definition.m_AtlasIndex.reset();
	definition.m_TextureRecords.clear();
	definition.m_TextureDimensions.clear();
	definition.m_GpuRecords.clear();
	definition.m_TextureHashes.clear();
}

bool CreateDynamicAtlas(RuiDynamicAtlasDefinition_t& definition)
{
	if (definition.m_Invalid)
		return false;
	if (!s_ImageAtlasLoaderConfigured || !g_pakLoadApi || !s_LoadImageAtlas || !s_DestroyImageAtlas)
	{
		spdlog::error(
			"Cannot create runtime RUI atlas from '{}': the engine atlas loader is not ready",
			definition.m_SourcePath.string());
		return false;
	}

	const uint64_t textureGuid = definition.m_TextureGuid
		? *definition.m_TextureGuid
		: GetRuiAssetGuid(definition.m_TexturePath.c_str());
	auto* texture = reinterpret_cast<RpakTextureHeader*>(
		g_pakLoadApi->GetAssetBinding(textureGuid));
	if (!texture)
	{
		spdlog::error(
			"RUI atlas sidecar '{}': TXTR '{}' (GUID 0x{:016X}) is not loaded",
			definition.m_SourcePath.string(),
			definition.m_TexturePath,
			textureGuid);
		return false;
	}

	const uint32_t atlasWidth = texture->width;
	const uint32_t atlasHeight = texture->height;
	if (atlasWidth == 0 || atlasHeight == 0)
	{
		spdlog::error(
			"RUI atlas sidecar '{}': TXTR '{}' has invalid dimensions {}x{}",
			definition.m_SourcePath.string(),
			definition.m_TexturePath,
			atlasWidth,
			atlasHeight);
		return false;
	}

	std::vector<RuiDynamicAtlasImage_t> resolvedImages = definition.m_Images;
	for (RuiDynamicAtlasImage_t& image : resolvedImages)
	{
		if (image.m_UseFullTexture)
		{
			image.m_Width = atlasWidth;
			image.m_Height = atlasHeight;
		}

		const uint64_t maxX = static_cast<uint64_t>(image.m_PosX) + image.m_Width;
		const uint64_t maxY = static_cast<uint64_t>(image.m_PosY) + image.m_Height;
		if (image.m_Width == 0 || image.m_Height == 0 || maxX > atlasWidth || maxY > atlasHeight)
		{
			spdlog::error(
				"RUI atlas sidecar '{}': image '{}' bounds ({}, {}, {}, {}) exceed texture '{}' dimensions {}x{}",
				definition.m_SourcePath.string(),
				image.m_Path,
				image.m_PosX,
				image.m_PosY,
				image.m_Width,
				image.m_Height,
				definition.m_TexturePath,
				atlasWidth,
				atlasHeight);
			definition.m_Invalid = true;
			return false;
		}
	}

	const std::optional<uint8_t> atlasIndex = AllocateDynamicAtlasSlot();
	if (!atlasIndex)
	{
		spdlog::error(
			"No runtime RUI image atlas slots remain while loading '{}' ({} slots are reserved)",
			definition.m_SourcePath.string(),
			RUI_IMAGE_ATLAS_CAPACITY - RUI_DYNAMIC_IMAGE_ATLAS_FIRST);
		return false;
	}

	definition.m_Images = std::move(resolvedImages);
	definition.m_TextureRecords.resize(definition.m_Images.size());
	definition.m_TextureDimensions.resize(definition.m_Images.size());
	definition.m_GpuRecords.resize(definition.m_Images.size());
	definition.m_TextureHashes.resize(definition.m_Images.size());
	definition.m_AtlasIndex = atlasIndex;

	const float inverseWidth = 1.0f / static_cast<float>(atlasWidth);
	const float inverseHeight = 1.0f / static_cast<float>(atlasHeight);
	for (size_t imageIndex = 0; imageIndex < definition.m_Images.size(); ++imageIndex)
	{
		const RuiDynamicAtlasImage_t& image = definition.m_Images[imageIndex];
		RuiImageAtlasEntry& textureRecord = definition.m_TextureRecords[imageIndex];
		textureRecord.bounds[0] = 0.0f;
		textureRecord.bounds[1] = 0.0f;
		textureRecord.bounds[2] = static_cast<float>(image.m_Width);
		textureRecord.bounds[3] = static_cast<float>(image.m_Height);
		textureRecord.uvBase[0] = static_cast<float>(image.m_PosX) * inverseWidth;
		textureRecord.uvBase[1] = static_cast<float>(image.m_PosY) * inverseHeight;
		textureRecord.uvScale[0] = inverseWidth;
		textureRecord.uvScale[1] = inverseHeight;

		definition.m_TextureDimensions[imageIndex] = {
			static_cast<uint16_t>(image.m_Width),
			static_cast<uint16_t>(image.m_Height)};

		RuiImageAtlasGpuRecord& gpuRecord = definition.m_GpuRecords[imageIndex];
		gpuRecord.uvMin[0] = textureRecord.uvBase[0];
		gpuRecord.uvMin[1] = textureRecord.uvBase[1];
		gpuRecord.uvMax[0] = static_cast<float>(image.m_PosX + image.m_Width) * inverseWidth;
		gpuRecord.uvMax[1] = static_cast<float>(image.m_PosY + image.m_Height) * inverseHeight;

		definition.m_TextureHashes[imageIndex] = static_cast<uint64_t>(GetRuiImageHash(image.m_Path))
			| (static_cast<uint64_t>(image.m_Flags) << 32);
	}

	RuiImageAtlas& atlas = g_RuiImageAtlases[*atlasIndex];
	atlas = {};
	atlas.inverseWidth = inverseWidth;
	atlas.inverseHeight = inverseHeight;
	atlas.width = static_cast<uint16_t>(atlasWidth);
	atlas.height = static_cast<uint16_t>(atlasHeight);
	atlas.imageCount = static_cast<uint16_t>(definition.m_Images.size());
	atlas.nineSliceImageCount = 0;
	atlas.images = definition.m_TextureRecords.data();
	atlas.imageDimensions = definition.m_TextureDimensions.data();
	atlas.nineSliceData = nullptr;
	atlas.imageNameHashes = definition.m_TextureHashes.data();
	atlas.imageNames = nullptr;
	atlas.texture = texture;
	atlas.gpuRecordBuffer = UINT_MAX;

	s_LoadImageAtlas(&atlas, definition.m_GpuRecords.data());
	if (atlas.gpuRecordBuffer == UINT_MAX)
	{
		spdlog::error(
			"Failed creating the GPU bounds buffer for runtime RUI atlas '{}'",
			definition.m_TexturePath);
		DestroyDynamicAtlas(definition);
		return false;
	}

	if (!AddMissingDynamicAtlasDescriptors(definition))
	{
		DestroyDynamicAtlas(definition);
		return false;
	}

	spdlog::info(
		"Registered texture '{}' as runtime RUI atlas {} with {} image(s)",
		definition.m_TexturePath,
		*atlasIndex,
		definition.m_Images.size());
	return true;
}

bool EnsureDynamicAtlas(RuiDynamicAtlasDefinition_t& definition)
{
	if (!definition.m_AtlasIndex && !CreateDynamicAtlas(definition))
		return false;

	AddMissingDynamicAtlasDescriptors(definition);
	return true;
}

void IndexDynamicAtlasImages(RuiDynamicAtlasDefinition_t& definition)
{
	for (const RuiDynamicAtlasImage_t& image : definition.m_Images)
		s_DynamicAtlasesByImage[GetRuiImageHash(image.m_Path)].push_back(&definition);
}

void UnindexDynamicAtlasImages(RuiDynamicAtlasDefinition_t& definition)
{
	for (const RuiDynamicAtlasImage_t& image : definition.m_Images)
	{
		const uint32_t nameHash = GetRuiImageHash(image.m_Path);
		auto imageIt = s_DynamicAtlasesByImage.find(nameHash);
		if (imageIt == s_DynamicAtlasesByImage.end())
			continue;

		auto& definitions = imageIt->second;
		definitions.erase(std::remove(definitions.begin(), definitions.end(), &definition), definitions.end());
		if (definitions.empty())
			s_DynamicAtlasesByImage.erase(imageIt);
	}
}

std::optional<int32_t> AllocateScriptDynamicAtlasHandle()
{
	for (size_t attempt = 0; attempt <= s_ScriptDynamicAtlases.size(); ++attempt)
	{
		const int32_t candidate = s_NextScriptDynamicAtlasHandle;
		s_NextScriptDynamicAtlasHandle = candidate == INT32_MAX ? 1 : candidate + 1;
		if (!s_ScriptDynamicAtlases.contains(candidate))
			return candidate;
	}

	return std::nullopt;
}

void UnregisterDynamicAtlasesForPakLocked(PakHandle_t handle)
{
	auto ownerIt = s_DynamicAtlasesByPak.find(handle);
	if (ownerIt == s_DynamicAtlasesByPak.end())
		return;

	for (const auto& definition : ownerIt->second)
	{
		DestroyDynamicAtlas(*definition);
		UnindexDynamicAtlasImages(*definition);
	}

	spdlog::info("Unregistered {} runtime RUI atlas definition(s) for pak handle {}", ownerIt->second.size(), handle);
	s_DynamicAtlasesByPak.erase(ownerIt);
}

bool TryRegisterDynamicAtlasForImage(const char* imagePath)
{
	if (!imagePath || !*imagePath || !s_ImageAtlasLoaderConfigured)
		return false;

	const uint32_t nameHash = GetRuiImageHash(imagePath);
	if (FindRuiImageDescriptor(nameHash))
		return true;

	std::lock_guard<std::mutex> lock(s_DynamicAtlasMutex);
	auto imageIt = s_DynamicAtlasesByImage.find(nameHash);
	if (imageIt == s_DynamicAtlasesByImage.end())
		return false;

	spdlog::info(
		"Runtime RUI image lookup '{}' matched sidecar hash 0x{:08X}",
		imagePath,
		nameHash);

	for (auto definitionIt = imageIt->second.rbegin(); definitionIt != imageIt->second.rend(); ++definitionIt)
	{
		if (EnsureDynamicAtlas(**definitionIt) && FindRuiImageDescriptor(nameHash))
			return true;
	}

	spdlog::error(
		"Runtime RUI image '{}' matched a sidecar but descriptor registration failed for hash 0x{:08X}",
		imagePath,
		nameHash);
	return false;
}
}

ADD_SQFUNC(
	"int",
	NS_CreateImageAtlas,
	"string textureGUID, string jsonData",
	"Creates a runtime RUI image atlas for a loaded TXTR GUID and returns its handle.",
	ScriptContext::UI | ScriptContext::CLIENT)
{
	const char* textureGuidText = g_pSquirrel[context]->getstring(sqvm, 1);
	const char* jsonData = g_pSquirrel[context]->getstring(sqvm, 2);
	uint64_t textureGuid = 0;
	if (!ParseTextureGuid(textureGuidText, textureGuid))
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			"NS_CreateImageAtlas expected textureGUID to contain 1-16 hexadecimal digits, optionally prefixed with 0x");
		return SQRESULT_ERROR;
	}

	std::string parseError;
	auto definition = ParseScriptDynamicAtlas(textureGuid, jsonData, parseError);
	if (!definition)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			fmt::format("NS_CreateImageAtlas: {}", parseError).c_str());
		return SQRESULT_ERROR;
	}

	std::lock_guard<std::mutex> lock(s_DynamicAtlasMutex);
	for (const RuiDynamicAtlasImage_t& image : definition->m_Images)
	{
		const uint32_t nameHash = GetRuiImageHash(image.m_Path);
		const auto reservedIt = s_DynamicAtlasesByImage.find(nameHash);
		if (FindRuiImageDescriptor(nameHash) || reservedIt != s_DynamicAtlasesByImage.end())
		{
			g_pSquirrel[context]->raiseerror(
				sqvm,
				fmt::format(
					"NS_CreateImageAtlas cannot register RUI image '{}' because hash 0x{:08X} is already registered or reserved",
					image.m_Path,
					nameHash)
					.c_str());
			return SQRESULT_ERROR;
		}
	}

	if (!CreateDynamicAtlas(*definition))
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			fmt::format(
				"NS_CreateImageAtlas could not create an atlas for loaded TXTR GUID 0x{:016X}; check the log for the exact failure",
				textureGuid)
				.c_str());
		return SQRESULT_ERROR;
	}

	const std::optional<int32_t> handle = AllocateScriptDynamicAtlasHandle();
	if (!handle)
	{
		DestroyDynamicAtlas(*definition);
		g_pSquirrel[context]->raiseerror(sqvm, "NS_CreateImageAtlas could not allocate an atlas handle");
		return SQRESULT_ERROR;
	}

	IndexDynamicAtlasImages(*definition);
	s_ScriptDynamicAtlases.emplace(*handle, std::move(definition));
	spdlog::info("Created script runtime RUI atlas handle {} for TXTR GUID 0x{:016X}", *handle, textureGuid);
	g_pSquirrel[context]->pushinteger(sqvm, *handle);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC(
	"bool",
	NS_DestroyImageAtlas,
	"int atlasHandle",
	"Destroys a runtime RUI image atlas created by NS_CreateImageAtlas.",
	ScriptContext::UI | ScriptContext::CLIENT)
{
	const int32_t handle = g_pSquirrel[context]->getinteger(sqvm, 1);
	std::lock_guard<std::mutex> lock(s_DynamicAtlasMutex);
	auto atlasIt = s_ScriptDynamicAtlases.find(handle);
	if (atlasIt == s_ScriptDynamicAtlases.end())
	{
		g_pSquirrel[context]->pushbool(sqvm, false);
		return SQRESULT_NOTNULL;
	}

	DestroyDynamicAtlas(*atlasIt->second);
	UnindexDynamicAtlasImages(*atlasIt->second);
	s_ScriptDynamicAtlases.erase(atlasIt);
	spdlog::info("Destroyed script runtime RUI atlas handle {}", handle);
	g_pSquirrel[context]->pushbool(sqvm, true);
	return SQRESULT_NOTNULL;
}

//-----------------------------------------------------------------------------
// Purpose: Registers the atlas definition stored beside a pak before loading.
//-----------------------------------------------------------------------------
void RuiImageAtlas_OnPakLoaded(const fs::path& pakPath, PakHandle_t handle)
{
	if (handle == PAK_INVALID_HANDLE || IsDedicatedServer())
		return;

	const std::optional<fs::path> sidecarPath = FindDynamicAtlasSidecar(pakPath);
	if (!sidecarPath)
		return;

	auto definitions = ParseDynamicAtlasSidecar(*sidecarPath, handle);
	if (definitions.empty())
		return;

	std::lock_guard<std::mutex> lock(s_DynamicAtlasMutex);
	UnregisterDynamicAtlasesForPakLocked(handle);
	for (const auto& definition : definitions)
		IndexDynamicAtlasImages(*definition);

	spdlog::info(
		"Loaded {} runtime RUI atlas definition(s) from '{}'",
		definitions.size(),
		sidecarPath->string());
	s_DynamicAtlasesByPak.emplace(handle, std::move(definitions));
}

//-----------------------------------------------------------------------------
// Purpose: Creates atlas wrappers once the pak's texture bindings are live.
//-----------------------------------------------------------------------------
void RuiImageAtlas_OnPakLoadCompleted(PakHandle_t handle)
{
	if (handle == PAK_INVALID_HANDLE || IsDedicatedServer())
		return;

	std::lock_guard<std::mutex> lock(s_DynamicAtlasMutex);
	auto ownerIt = s_DynamicAtlasesByPak.find(handle);
	if (ownerIt == s_DynamicAtlasesByPak.end())
		return;

	for (const auto& definition : ownerIt->second)
		EnsureDynamicAtlas(*definition);
}

//-----------------------------------------------------------------------------
// Purpose: Removes descriptors and GPU buffers before their texture pak unloads.
//-----------------------------------------------------------------------------
void RuiImageAtlas_OnPakUnloading(PakHandle_t handle)
{
	if (handle == PAK_INVALID_HANDLE)
		return;

	std::lock_guard<std::mutex> lock(s_DynamicAtlasMutex);
	UnregisterDynamicAtlasesForPakLocked(handle);
}

DECLARE_HOOK(Rui_FindImageAsset, engine.dll + 0xF8000,
	[](auto& hook, RuiInstance* rui, const char* imagePath) -> __int64
	{
		TryRegisterDynamicAtlasForImage(imagePath);
		return hook.Original(rui, imagePath);
	})


DECLARE_HOOK(RuiImageAtlas_Replace, engine.dll + 0xFB960, [](auto& hook, RuiImageAtlas* destination, const RuiImageAtlas* source, const RuiImageAtlas* previous)
{
	if (previous)
	{
		for (uint16_t imageIndex = 0; imageIndex < previous->imageCount; ++imageIndex)
			s_RHashMapRemove(g_RuiImageDescriptorMap, static_cast<uint32_t>(previous->imageNameHashes[imageIndex]));
	}
	if (source)
	{
		*destination = *source;
		AcquireSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
		for (uint16_t imageIndex = 0; imageIndex < source->imageCount; ++imageIndex)
		{
			uint8_t inserted = 0;
			auto* descriptor = static_cast<RuiImageAssetDescriptor*>(s_RHashMapInsert(
				g_RuiImageDescriptorMap,
				static_cast<uint32_t>(source->imageNameHashes[imageIndex]),
				&inserted));
			descriptor->nameHash = static_cast<uint32_t>(source->imageNameHashes[imageIndex]);
			descriptor->imageIndex = static_cast<int16_t>(imageIndex);
			descriptor->atlasIndex = static_cast<uint8_t>(destination - g_RuiImageAtlases);
			descriptor->flags = static_cast<uint8_t>(source->imageNameHashes[imageIndex] >> 32);
			g_RuiImageDescriptorMap->buckets[g_RuiImageDescriptorMap->lastProbeIndex] = g_RuiImageDescriptorMap->lastInsertIndex;
			++g_RuiImageDescriptorMap->size;
		}
		ReleaseSRWLockExclusive(&g_RuiImageDescriptorMap->lock);
	}
});



//__m128i* g_RuiEdgeCorrectionMasks; // edge-clip table


#define RUI_SHUFFLE_PS(value, imm) _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(value), imm))

#define RUI_SHUFFLE_I32_AS_PS(value, imm) _mm_castsi128_ps(_mm_shuffle_epi32((value), imm))

bool RuiDrawImageAtlasEntry(
	RuiGlobalState* globalState,
	RuiInstance* rui,
	RuiDrawBatch* batch,
	const RuiBaseUv* baseUv,
	const RuiTransform* transform,
	int orientation,
	const RuiImageAssetDescriptor* descriptor,
	const __m128i* atlasUv,
	const __m128* clipThreshold,
	const __m128* uvBias,
	const __m128* viewportScale)
{
	const __int64 instanceIndex = batch->materialBatchIndex;
	RuiDrawMaterialBatch* instances = batch->materialBatches;
	const uint8_t atlasIndex = descriptor->atlasIndex;
	RuiImageAtlas* imageAtlas = &g_RuiImageAtlases[atlasIndex];

	if (instances[instanceIndex].imageAtlas != imageAtlas)
	{
		RuiImageAtlas* currentAtlas = instances[instanceIndex].imageAtlas;

		if (!currentAtlas || instances[instanceIndex].firstIndex == batch->indexBufferSize)
		{
			instances[instanceIndex].imageAtlas = imageAtlas;
		}
		else
		{
			instances[instanceIndex].firstIndex = batch->indexBufferSize;

			if (++batch->materialBatchIndex == batch->materialBatchCapacity)
				return false;

			instances[instanceIndex + 1].firstIndex = batch->indexBufferSize;
			instances[instanceIndex + 1].fontAtlas = 0;
			instances[instanceIndex + 1].firstVertex = instances[instanceIndex].firstVertex;
			instances[instanceIndex + 1].imageAtlas = imageAtlas;
		}
	}

	RuiDrawQuad tri;
	RuiBaseUv drawUv;
	__m128 correctionData[5];

	tri.vertexCount = 4;
	tri.vertexCapacity = 4;

	const __int16 correctionMask = (~baseUv->flags >> 8) & 0xF;
	if (correctionMask)
		s_BuildEdgeCorrection(transform, &rui->header->elementWidth, correctionData);

	const uint16_t textureOffsetIndex = static_cast<uint16_t>(descriptor->imageIndex);
	const __m128 zero = _mm_setzero_ps();

	auto submitDraw = [&]() -> bool
	{
		RuiDrawInfo* drawInfo = rui->drawInfo;
		return g_RuiDrawInfoHandlers[static_cast<uint32_t>(drawInfo->mode)](
			drawInfo,
			&drawUv,
			&tri,
			batch);
	};

	auto setTriangleFromUv = [&](__m128 u, __m128 v, const __m128* correction, bool useAlternateOrientationShuffle, bool forceCorrection)
	{
		const __m128 row0 = transform->rows[0];
		const __m128 row1 = transform->rows[1];

		__m128 projected[2];
		projected[0] = _mm_add_ps(
			_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_PS(row0, 170), u), _mm_mul_ps(RUI_SHUFFLE_PS(row0, 0), v)), RUI_SHUFFLE_PS(row1, 0));
		projected[1] = _mm_add_ps(
			_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_PS(row0, 255), u), _mm_mul_ps(RUI_SHUFFLE_PS(row0, 85), v)), RUI_SHUFFLE_PS(row1, 85));

		if (correction && (forceCorrection || _mm_movemask_ps(_mm_cmpneq_ps(*correction, zero))))
			s_ApplyEdgeCorrection(globalState, rui, correctionData, correction, projected);

		__m128 quad0 = _mm_unpacklo_ps(projected[0], projected[1]);
		__m128 quad1 = _mm_unpackhi_ps(projected[0], projected[1]);

		if (orientation == 2)
		{
			if (useAlternateOrientationShuffle)
			{
				quad0 = RUI_SHUFFLE_PS(quad0, 78);
				quad1 = RUI_SHUFFLE_PS(quad1, 78);
			}
			else
			{
				quad0 = RUI_SHUFFLE_PS(quad0, _MM_SHUFFLE(1, 0, 3, 2));
				quad1 = RUI_SHUFFLE_PS(quad1, _MM_SHUFFLE(1, 0, 3, 2));
			}
		}

		_mm_storeu_ps(&tri.positions[0][0], quad0);
		_mm_storeu_ps(&tri.positions[1][0], quad1);
	};

	auto drawPiece =
		[&](__m128 u, __m128 v, const __m128* correction, bool useAlternateOrientationShuffle, __m128 base, __m128 xDir, __m128 yDir) -> bool
	{
		setTriangleFromUv(u, v, correction, useAlternateOrientationShuffle, false);

		drawUv.primaryOrigin = yDir;
		drawUv.primaryBasisX = base;
		drawUv.primaryBasisY = xDir;
		drawUv.imageIndex = baseUv->imageIndex;
		drawUv.maskImageIndex = baseUv->maskImageIndex;
		drawUv.computedStyleIndex = baseUv->computedStyleIndex;
		drawUv.flags = baseUv->flags;
		std::memset(&drawUv.secondaryBasisX, 0, sizeof(drawUv.secondaryBasisX) * 3);
		return submitDraw();
	};

	if (textureOffsetIndex >= imageAtlas->nineSliceImageCount)
	{
		const __m128 transformRow0 = transform->rows[0];
		const __m128 u = RUI_SHUFFLE_I32_AS_PS(*atlasUv, 125);
		const __m128 v = RUI_SHUFFLE_I32_AS_PS(*atlasUv, 160);

		__m128 projected[2];
		projected[0] = _mm_add_ps(
			_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 170), u),
					   _mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 0), v)),
			RUI_SHUFFLE_PS(transform->rows[1], 0));
		projected[1] = _mm_add_ps(
			_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 255), u), _mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 85), v)),
			RUI_SHUFFLE_PS(transform->rows[1], 85));

		if (correctionMask) {
			s_ApplyEdgeCorrection(globalState, rui, correctionData, &g_RuiEdgeCorrectionMasks[correctionMask], projected);
		}
		__m128 quad0 = _mm_unpacklo_ps(projected[0], projected[1]);
		__m128 quad1 = _mm_unpackhi_ps(projected[0], projected[1]);

		if (orientation == 2)
		{
			quad0 = RUI_SHUFFLE_PS(quad0, _MM_SHUFFLE(1, 0, 3, 2));
			quad1 = RUI_SHUFFLE_PS(quad1, _MM_SHUFFLE(1, 0, 3, 2));
		}
		_mm_storeu_ps(&tri.positions[0][0], quad0);
		_mm_storeu_ps(&tri.positions[1][0], quad1);

		RuiDrawInfo* drawInfo = rui->drawInfo;
		return g_RuiDrawInfoHandlers[static_cast<uint32_t>(drawInfo->mode)](
			drawInfo,
			baseUv,
			&tri,
			batch);
	}
	const RuiImageAtlasNineSlice& atlasRecord = imageAtlas->nineSliceData[textureOffsetIndex];
	const __m128 atlasRect = _mm_loadu_ps(atlasRecord.normalizedBounds);

	const __m128 reciprocalScale = _mm_rcp_ps(*viewportScale);
	const __m128 reciprocalScaleError = _mm_sub_ps(g_RuiFloatOne, _mm_mul_ps(reciprocalScale, *viewportScale));
	const __m128 refinedReciprocalScale = _mm_add_ps(
		_mm_mul_ps(_mm_add_ps(_mm_mul_ps(reciprocalScaleError, reciprocalScaleError), reciprocalScaleError), reciprocalScale),
		reciprocalScale);

	const __m128 canvasSize = _mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(&rui->canvasWidth)));

	const __m128 flippedAtlasRect = _mm_xor_ps(atlasRect, g_RuiSignMaskHighHalf);
	const __m128 atlasMin = _mm_and_ps(atlasRect, g_RuiBlendMaskLowHalf);
	const __m128 atlasExtent = _mm_add_ps(RUI_SHUFFLE_PS(atlasRect, 238), flippedAtlasRect);
	const __m128 inverseAtlasExtent = _mm_sub_ps(g_RuiFloatOne, atlasExtent);

	const __m128 projectedCanvas =
		_mm_mul_ps(_mm_mul_ps(_mm_unpacklo_ps(canvasSize, canvasSize), RUI_SHUFFLE_PS(transform->rows[0], 216)), refinedReciprocalScale);
	const __m128 projectedCanvasSq = _mm_mul_ps(projectedCanvas, projectedCanvas);
	const __m128 edgeSize = _mm_max_ps(
		_mm_mul_ps(
			_mm_sqrt_ps(_mm_add_ps(RUI_SHUFFLE_PS(projectedCanvasSq, 78), projectedCanvasSq)),
			_mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(atlasRecord.edgeScale)))),
		_mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(atlasRecord.minimumEdgeSize))));

	const __m128 availableExtent = _mm_sub_ps(edgeSize, atlasExtent);
	const __m128 availableExtentReciprocal = _mm_rcp_ps(availableExtent);
	const __m128 availableExtentError = _mm_sub_ps(g_RuiFloatOne, _mm_mul_ps(availableExtentReciprocal, availableExtent));
	const __m128 refinedAvailableExtentReciprocal = _mm_add_ps(
		_mm_mul_ps(_mm_add_ps(_mm_mul_ps(availableExtentError, availableExtentError), availableExtentError), availableExtentReciprocal),
		availableExtentReciprocal);

	const __m128 edgeScale = _mm_movelh_ps(edgeSize, g_RuiFloatOne);
	const __m128 outerYDir = _mm_mul_ps(edgeScale, baseUv->primaryOrigin);
	const __m128 outerXDir = _mm_mul_ps(edgeScale, baseUv->primaryBasisY);
	const __m128 edgeYDir = _mm_add_ps(_mm_sub_ps(g_RuiFloatOne, edgeScale), outerYDir);
	const __m128 outerBase = _mm_mul_ps(edgeScale, baseUv->primaryBasisX);

	const __m128 innerScale =
		_mm_movelh_ps(_mm_mul_ps(_mm_mul_ps(inverseAtlasExtent, edgeSize), refinedAvailableExtentReciprocal), g_RuiFloatOne);
	const __m128 innerBase = _mm_mul_ps(innerScale, baseUv->primaryBasisX);
	const __m128 innerXDir = _mm_mul_ps(innerScale, baseUv->primaryBasisY);
	const __m128 innerYDir = _mm_add_ps(
		_mm_sub_ps(atlasMin, _mm_mul_ps(_mm_mul_ps(atlasMin, inverseAtlasExtent), refinedAvailableExtentReciprocal)),
		_mm_mul_ps(innerScale, baseUv->primaryOrigin));

	const __m128 atlasStep = _mm_mul_ps(
		_mm_sub_ps(
			_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_PS(refinedAvailableExtentReciprocal, 238), flippedAtlasRect), g_RuiHighHalfOne), *uvBias),
		refinedReciprocalScale);
	const __m128 atlasStepShuffled = RUI_SHUFFLE_PS(atlasStep, 216);
	const __m128 atlasUvBase = RUI_SHUFFLE_I32_AS_PS(*atlasUv, 216);
	const __m128 uvHigh = _mm_unpackhi_ps(atlasUvBase, atlasStepShuffled);
	const __m128 uvLow = _mm_unpacklo_ps(atlasUvBase, atlasStepShuffled);

	int clipMaskX = _mm_movemask_ps(_mm_cmple_ps(*clipThreshold, _mm_xor_ps(atlasStep, g_RuiSignMaskLowHalf)));
	int clipMaskY = _mm_movemask_ps(_mm_cmple_ps(*clipThreshold, _mm_xor_ps(RUI_SHUFFLE_PS(atlasStep, 78), g_RuiSignMaskLowHalf)));

	auto blendByMask = [](__m128 keep, __m128 replace, __m128 mask)
	{ return _mm_or_ps(_mm_andnot_ps(mask, keep), _mm_and_ps(replace, mask)); };

	const int clipMaskY_5 = clipMaskY & 5;
	const int clipMaskY_A = clipMaskY & 0xA;

	if ((clipMaskX & 3) == 0 && !drawPiece(
									RUI_SHUFFLE_PS(uvHigh, 20),
									RUI_SHUFFLE_PS(uvLow, 80),
									&g_RuiEdgeCorrectionMasks[correctionMask & 5],
									false,
									outerBase,
									outerXDir,
									outerYDir))
	{
		return false;
	}

	if ((clipMaskY_5 | (clipMaskX & 2)) || drawPiece(
											   RUI_SHUFFLE_PS(uvHigh, 20),
											   RUI_SHUFFLE_PS(uvLow, 245),
											   &g_RuiEdgeCorrectionMasks[correctionMask & 4],
											   true,
											   blendByMask(outerBase, innerBase, g_RuiBlendMaskLane0),
											   blendByMask(outerXDir, innerXDir, g_RuiBlendMaskLane0),
											   blendByMask(outerYDir, innerYDir, g_RuiBlendMaskLane0)))
	{
		// The corresponding piece is visible only when neither clipped axis rejects it.
		if ((clipMaskX & 6) == 0)
		{
			if (!drawPiece(
					RUI_SHUFFLE_PS(uvHigh, 20),
					RUI_SHUFFLE_PS(uvLow, 175),
					&g_RuiEdgeCorrectionMasks[correctionMask & 6],
					false,
					outerBase,
					outerXDir,
					blendByMask(outerYDir, edgeYDir, g_RuiBlendMaskLane0)))
			{
				return false;
			}
		}

		if ((clipMaskY_A | (clipMaskX & 1)) || drawPiece(
												   RUI_SHUFFLE_PS(uvHigh, 125),
												   RUI_SHUFFLE_PS(uvLow, 80),
												   &g_RuiEdgeCorrectionMasks[correctionMask & 1],
												   false,
												   blendByMask(outerBase, innerBase, g_RuiBlendMaskLane1),
												   blendByMask(outerXDir, innerXDir, g_RuiBlendMaskLane1),
												   blendByMask(outerYDir, innerYDir, g_RuiBlendMaskLane1)))
		{
			if (clipMaskY ||
				drawPiece(RUI_SHUFFLE_PS(uvHigh, 125), RUI_SHUFFLE_PS(uvLow, 245), nullptr, true, innerBase, innerXDir, innerYDir))
			{
				if ((clipMaskY_A | (clipMaskX & 4)) || drawPiece(
														   RUI_SHUFFLE_PS(uvHigh, 125),
														   RUI_SHUFFLE_PS(uvLow, 175),
														   &g_RuiEdgeCorrectionMasks[correctionMask & 2],
														   false,
														   blendByMask(outerBase, innerBase, g_RuiBlendMaskLane1),
														   blendByMask(outerXDir, innerXDir, g_RuiBlendMaskLane1),
														   blendByMask(edgeYDir, innerYDir, g_RuiBlendMaskLane1)))
				{
					if ((clipMaskX & 9) == 0 && !drawPiece(
													RUI_SHUFFLE_PS(uvHigh, 235),
													RUI_SHUFFLE_PS(uvLow, 80),
													&g_RuiEdgeCorrectionMasks[correctionMask & 9],
													true,
													outerBase,
													outerXDir,
											blendByMask(outerYDir, edgeYDir, g_RuiBlendMaskLane1)))
					{
						return false;
					}

					if ((clipMaskY_5 | (clipMaskX & 8)) || drawPiece(
															   RUI_SHUFFLE_PS(uvHigh, 235),
															   RUI_SHUFFLE_PS(uvLow, 245),
															   &g_RuiEdgeCorrectionMasks[correctionMask & 8],
															   false,
															   blendByMask(outerBase, innerBase, g_RuiBlendMaskLane0),
															   blendByMask(outerXDir, innerXDir, g_RuiBlendMaskLane0),
															   blendByMask(edgeYDir, innerYDir, g_RuiBlendMaskLane0)))
					{
						if ((clipMaskX & 0xC) == 0)
						{
							return drawPiece(
								RUI_SHUFFLE_PS(uvHigh, 235),
								RUI_SHUFFLE_PS(uvLow, 175),
								&g_RuiEdgeCorrectionMasks[correctionMask & 0xA],
								true,
								outerBase,
								outerXDir,
								edgeYDir);
						}
					}
					else
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
	return true;
}



DECLARE_HOOK(RuiDrawImageAtlasEntry, engine.dll + 0xF9B80, [](auto& hook,
	RuiGlobalState* globalState,
	RuiInstance* rui,
	RuiDrawBatch* batch,
	const RuiBaseUv* baseUv,
	const RuiTransform* transform,
	int orientation,
	const RuiImageAssetDescriptor* descriptor,
	const __m128i* atlasUv,
	const __m128* clipThreshold,
	const __m128* uvBias,
	const __m128* viewportScale) -> bool
	{
		return RuiDrawImageAtlasEntry(
			globalState, rui, batch, baseUv, transform, orientation, descriptor,
			atlasUv, clipThreshold, uvBias, viewportScale);
	});




bool __fastcall RuiRenderEllipseJob(
	RuiRenderContext* context,
	RuiInstance* rui,
	const RuiEllipseRenderJob* job,
	RuiDrawBatch* batch)
{
	(void)context;

	auto dataFloat = [&](uint16_t offset) -> float
	{
		float value;
		std::memcpy(&value, &rui->data[offset], sizeof(value));
		return value;
	};

	auto dataInt = [&](uint16_t offset) -> int32_t
	{
		int32_t value;
		std::memcpy(&value, &rui->data[offset], sizeof(value));
		return value;
	};

	auto dataScalar = [&](uint16_t offset) -> __m128
	{
		return _mm_set_ss(dataFloat(offset));
	};

	const uint8_t styleIndex = job->styleIndex;
	const RuiStyleDescriptorOffsets& style = rui->header->styleDescriptors[styleIndex];
	if (dataFloat(style.colorAlpha) <= 0.0f)
		return true;

	const uint16_t transformIndex = job->transformIndex;
	const RuiTransform* transform = &rui->runtime->transforms[transformIndex];
	const __m128 transformRow0 = transform->rows[0];
	const __m128 transformRow1 = transform->rows[1];
	const __m128 determinant = _mm_sub_ps(
		_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 255), RUI_SHUFFLE_PS(transformRow0, 0)),
		_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 170), RUI_SHUFFLE_PS(transformRow0, 85)));
	if (_mm_movemask_ps(_mm_cmpeq_ps(determinant, _mm_setzero_ps())) != 0)
		return true;

	const int orientation = _mm_movemask_ps(determinant) & 2;
	const __m128 inverseBasis = _mm_div_ps(_mm_xor_ps(RUI_SHUFFLE_PS(transformRow0, 39), g_RuiSignMaskMiddleLanes), determinant);
	const __m128 transformedOrigin = _mm_mul_ps(_mm_xor_ps(inverseBasis, g_RuiSignMaskAll), RUI_SHUFFLE_PS(transformRow1, 216));
	const __m128 originSum = _mm_add_ps(RUI_SHUFFLE_PS(transformedOrigin, 78), transformedOrigin);

	const int32_t assetDescriptorIndex = dataInt(job->imageOffset);
	if (assetDescriptorIndex == -1)
		return true;

	const RuiImageAssetDescriptor& asset = g_RuiImageDescriptors[assetDescriptorIndex];
	const int16_t assetIndex = asset.imageIndex;
	const int16_t combinedFlags = static_cast<int16_t>(job->flags | asset.flags);

	const __m128 mins = _mm_unpacklo_ps(dataScalar(job->boundsMinOffsets.x), dataScalar(job->boundsMinOffsets.y));
	const __m128 maxs = _mm_unpacklo_ps(dataScalar(job->boundsMaxOffsets.x), dataScalar(job->boundsMaxOffsets.y));
	const float texMinX = dataFloat(job->uvMinOffsets.x);
	const float texMinY = dataFloat(job->uvMinOffsets.y);
	const float texMaxX = dataFloat(job->uvMaxOffsets.x);
	const float texMaxY = dataFloat(job->uvMaxOffsets.y);
	const __m128 texMins = _mm_setr_ps(texMinX, texMinY, texMinX, texMinY);
	const __m128 texMaxs = _mm_setr_ps(texMaxX, texMaxY, texMaxX, texMaxY);
	const float stretchX = dataFloat(style.stretchX);

	const __m128 transformSize = rui->runtime->transformSizes[transformIndex];
	const float transformWidth = transformSize.m128_f32[0];
	const float transformHeight = transformSize.m128_f32[2];
	const float minimumTransformExtent = _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(transformWidth), _mm_set_ss(transformHeight)));
	if (minimumTransformExtent <= 0.0f)
		return true;

	RuiImageAtlas* imageAtlas = &g_RuiImageAtlases[asset.atlasIndex];
	if (!s_BindImageAtlas(batch, imageAtlas))
		return false;

	const RuiImageAtlasEntry& textureRecord = imageAtlas->images[assetIndex];

	const __m128 textureExtent = _mm_max_ps(_mm_sub_ps(texMaxs, texMins), g_RuiFloatMinNormal);
	const unsigned int axisMaskIndex = (static_cast<uint16_t>(combinedFlags) >> 4) & 3;
	const __m128 axisMask = g_RuiEllipseAxisMasks[axisMaskIndex];
	const float stretchCorrectionX = ((transformHeight * stretchX) * (texMaxX - texMinX)) / transformWidth;
	const float stretchCorrectionY = (texMaxY - texMinY) * stretchX;
	const __m128 stretchCorrection = _mm_setr_ps(stretchCorrectionX, stretchCorrectionY, stretchCorrectionX, stretchCorrectionY);

	const __m128 textureBounds = _mm_loadu_ps(textureRecord.bounds);
	const __m128 normalizedTextureBounds = _mm_div_ps(
		_mm_add_ps(
			_mm_sub_ps(textureBounds, _mm_xor_ps(_mm_and_ps(_mm_min_ps(texMins, texMaxs), axisMask), g_RuiSignMaskLowHalf)),
			stretchCorrection),
		_mm_or_ps(
			_mm_and_ps(_mm_andnot_ps(g_RuiSignMaskAll, textureExtent), axisMask),
			_mm_andnot_ps(axisMask, g_RuiFloatOne)));
	if (_mm_movemask_ps(_mm_cmplt_ps(normalizedTextureBounds, g_RuiFloatAbsMask)) != 0)
		return true;

	const __m128 requestedBounds = _mm_movelh_ps(_mm_xor_ps(mins, g_RuiSignMaskAll), maxs);
	const __m128 clippedBounds = _mm_xor_ps(
		_mm_min_ps(
			requestedBounds,
			normalizedTextureBounds),
		g_RuiSignMaskLowHalf);
	if (_mm_movemask_ps(_mm_cmple_ps(RUI_SHUFFLE_PS(clippedBounds, 238), RUI_SHUFFLE_PS(clippedBounds, 68))) != 0)
		return true;

	RuiBaseUv uv;
	uv.imageIndex = assetIndex;
	uv.maskImageIndex = -1;
	uv.computedStyleIndex = static_cast<int16_t>(static_cast<uint16_t>(batch->computedStyleCount) + styleIndex);
	uv.flags = combinedFlags;
	std::memset(&uv.secondaryBasisX, 0, sizeof(uv.secondaryBasisX) * 3);

	const __m128 textureScale = _mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(textureRecord.uvScale)));
	const __m128 textureBasis = _mm_mul_ps(_mm_mul_ps(inverseBasis, textureExtent), textureScale);
	const __m128 ellipseBasis = _mm_mul_ps(inverseBasis, g_RuiFloatTwo);
	const __m128 textureBase = _mm_add_ps(
		_mm_mul_ps(_mm_add_ps(_mm_mul_ps(originSum, textureExtent), texMins), textureScale),
		_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(textureRecord.uvBase))));
	const __m128 ellipseBase = _mm_sub_ps(_mm_mul_ps(originSum, g_RuiFloatTwo), g_RuiFloatOne);
	uv.primaryBasisX = _mm_movelh_ps(textureBasis, ellipseBasis);
	uv.primaryBasisY = _mm_movehl_ps(ellipseBasis, textureBasis);
	uv.primaryOrigin = _mm_movelh_ps(textureBase, ellipseBase);

	const __m128 packedYBounds = RUI_SHUFFLE_PS(clippedBounds, 125);
	const __m128 packedXBounds = RUI_SHUFFLE_PS(clippedBounds, 160);
	const __m128 projectedX = _mm_add_ps(
		_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 170), packedYBounds), _mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 0), packedXBounds)),
		RUI_SHUFFLE_PS(transformRow1, 0));
	const __m128 projectedY = _mm_add_ps(
		_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 255), packedYBounds), _mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 85), packedXBounds)),
		RUI_SHUFFLE_PS(transformRow1, 85));

	__m128 vertices0 = _mm_unpacklo_ps(projectedX, projectedY);
	__m128 vertices1 = _mm_unpackhi_ps(projectedX, projectedY);
	if (orientation == 2)
	{
		vertices0 = RUI_SHUFFLE_PS(vertices0, 78);
		vertices1 = RUI_SHUFFLE_PS(vertices1, 78);
	}

	RuiDrawQuad triangle;
	triangle.vertexCount = 4;
	triangle.vertexCapacity = 4;
	_mm_storeu_ps(&triangle.positions[0][0], vertices0);
	_mm_storeu_ps(&triangle.positions[1][0], vertices1);

	RuiDrawInfo* drawInfo = rui->drawInfo;
	return g_RuiDrawInfoHandlers[static_cast<uint32_t>(drawInfo->mode)](
		drawInfo,
		&uv,
		&triangle,
		batch);
}


bool __fastcall RuiRenderImageJob(
	RuiRenderContext* context,
	RuiInstance* rui,
	const RuiImageRenderJob* job,
	RuiDrawBatch* batch)
{
	const __int16 styleIndex = job->styleIndex;
	const RuiStyleDescriptorOffsets* styleOffsets = &rui->header->styleDescriptors[styleIndex];

	auto dataFloat = [&](int offset) -> float
	{
		return *reinterpret_cast<const float*>(&rui->data[offset]);
	};

	auto dataInt = [&](int offset) -> int
	{
		return *reinterpret_cast<const int*>(&rui->data[offset]);
	};

	auto dataScalar = [&](int offset) -> __m128
	{
		return _mm_set_ss(dataFloat(offset));
	};

	if (dataFloat(styleOffsets->colorAlpha) <= 0.0f)
		return true;

	const RuiTransform* transform = &rui->runtime->transforms[job->transformIndex];
	const __m128 transformRow0 = transform->rows[0];
	const __m128 transformRow1 = transform->rows[1];
	const __m128 determinant = _mm_sub_ps(
		_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(3, 3, 3, 3)), RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(0, 0, 0, 0))),
		_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(2, 2, 2, 2)), RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(1, 1, 1, 1))));
	if (_mm_movemask_ps(_mm_cmpeq_ps(_mm_setzero_ps(), determinant)) != 0)
		return true;

	const __m128 inverseBasis = _mm_div_ps(_mm_xor_ps(RUI_SHUFFLE_PS(transformRow0, 39), g_RuiSignMaskMiddleLanes), determinant);
	const int orientation = _mm_movemask_ps(determinant) & 2;
	const __m128 transformedOrigin = _mm_mul_ps(_mm_xor_ps(inverseBasis, g_RuiSignMaskAll), RUI_SHUFFLE_PS(transformRow1, 216));
	const __m128 originSum = _mm_add_ps(RUI_SHUFFLE_PS(transformedOrigin, 78), transformedOrigin);

	const int primaryAssetDescriptorIndex = dataInt(job->imageOffset);
	if (primaryAssetDescriptorIndex == -1)
		return true;

	const auto* primaryAsset = &g_RuiImageDescriptors[primaryAssetDescriptorIndex];
	const uint8_t atlasIndex = primaryAsset->atlasIndex;
	const __int16 assetIndex = primaryAsset->imageIndex;
	__int16 secondaryAssetIndex = -1;
	__int16 flags = job->flags | static_cast<uint8_t>(primaryAsset->flags);

	const int secondaryAssetDescriptorIndex = dataInt(job->maskImageOffset);
	if (secondaryAssetDescriptorIndex != -1)
	{
		const auto* secondaryAsset = &g_RuiImageDescriptors[secondaryAssetDescriptorIndex];
		if (atlasIndex != secondaryAsset->atlasIndex)
			return true;

		secondaryAssetIndex = secondaryAsset->imageIndex;
		flags |= static_cast<__int16>(4 * static_cast<uint8_t>(secondaryAsset->flags));
	}

	RuiImageAtlas* imageAtlas = &g_RuiImageAtlases[atlasIndex];
	const RuiImageAtlasEntry& primaryTextureRecord = imageAtlas->images[static_cast<uint16_t>(assetIndex)];

	const __m128 mins = _mm_unpacklo_ps(dataScalar(job->boundsMinOffsets.x), dataScalar(job->boundsMinOffsets.y));
	const __m128 maxs = _mm_unpacklo_ps(dataScalar(job->boundsMaxOffsets.x), dataScalar(job->boundsMaxOffsets.y));
	const __m128 texMinsLo = _mm_unpacklo_ps(dataScalar(job->uvMinOffsets.x), dataScalar(job->uvMinOffsets.y));
	__m128 texMins = _mm_movelh_ps(texMinsLo, texMinsLo);
	const __m128 texMaxsLo = _mm_unpacklo_ps(dataScalar(job->uvMaxOffsets.x), dataScalar(job->uvMaxOffsets.y));
	__m128 texMaxs = _mm_movelh_ps(texMaxsLo, texMaxsLo);
	__m128 geometryBounds = _mm_movelh_ps(_mm_xor_ps(g_RuiSignMaskAll, mins), maxs);
	__m128 textureExtent = _mm_sub_ps(texMaxs, texMins);

	const __m128 axisMask = g_RuiEllipseAxisMasks[((static_cast<__int64>(flags) >> 4) & 3)];
	const __m128 primaryTextureOffset = _mm_loadu_ps(primaryTextureRecord.bounds);
	const __m128 normalizedTextureOffset = _mm_div_ps(
		_mm_sub_ps(primaryTextureOffset, _mm_xor_ps(_mm_and_ps(_mm_min_ps(texMins, texMaxs), axisMask), g_RuiSignMaskLowHalf)),
		_mm_or_ps(
			_mm_and_ps(_mm_andnot_ps(g_RuiSignMaskAll, textureExtent), axisMask),
			_mm_andnot_ps(axisMask, g_RuiFloatOne)));
	if (_mm_movemask_ps(_mm_cmplt_ps(normalizedTextureOffset, g_RuiFloatAbsMask)) != 0)
		return true;

	__m128i atlasUv = _mm_castps_si128(_mm_xor_ps(_mm_min_ps(geometryBounds, normalizedTextureOffset), g_RuiSignMaskLowHalf));
	if (_mm_movemask_ps(_mm_cmple_ps(RUI_SHUFFLE_I32_AS_PS(atlasUv, 238), RUI_SHUFFLE_I32_AS_PS(atlasUv, 68))) != 0)
		return true;

	RuiBaseUv baseUv;
	baseUv.imageIndex = assetIndex;
	baseUv.maskImageIndex = secondaryAssetIndex;
	baseUv.flags = flags;
	baseUv.computedStyleIndex = static_cast<__int16>(styleIndex + batch->computedStyleCount);

	const __m128 texturePosition = _mm_add_ps(_mm_mul_ps(originSum, textureExtent), texMins);
	const __m128 textureScale = _mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(primaryTextureRecord.uvScale)));
	const __m128 basisExtent = _mm_mul_ps(inverseBasis, textureExtent);
	const __m128 primaryBase = _mm_add_ps(
		_mm_mul_ps(textureScale, texturePosition),
		_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(primaryTextureRecord.uvBase))));
	const __m128 primaryBasis = _mm_mul_ps(textureScale, basisExtent);

	__m128 maskBase = _mm_setzero_ps();
	__m128 maskBasis = _mm_setzero_ps();
	if (secondaryAssetIndex == -1)
	{
		const __m128 minXy = RUI_SHUFFLE_PS(mins, 68);
		const __m128 spanXy = _mm_max_ps(g_RuiFloatMinNormal, _mm_sub_ps(RUI_SHUFFLE_PS(maxs, 68), minXy));
		const __m128 spanReciprocal = _mm_rcp_ps(spanXy);
		const __m128 spanError = _mm_sub_ps(g_RuiFloatOne, _mm_mul_ps(spanReciprocal, spanXy));
		const __m128 refinedSpanReciprocal = _mm_add_ps(
			_mm_mul_ps(_mm_add_ps(_mm_mul_ps(spanError, spanError), spanError), spanReciprocal),
			spanReciprocal);

		maskBasis = _mm_mul_ps(inverseBasis, refinedSpanReciprocal);
		maskBase = _mm_mul_ps(_mm_sub_ps(originSum, minXy), refinedSpanReciprocal);
	}
	else
	{
		const RuiImageAtlasEntry& secondaryTextureRecord = imageAtlas->images[static_cast<uint16_t>(secondaryAssetIndex)];
		const __m128 maskRotation = dataScalar(job->maskRotationOffset);
		const __m128 maskCenter = _mm_unpacklo_ps(dataScalar(job->maskCenterOffsets.x), dataScalar(job->maskCenterOffsets.y));
		const __m128 maskSize = _mm_unpacklo_ps(dataScalar(job->maskScaleOffsets.x), dataScalar(job->maskScaleOffsets.y));
		const __m128 maskTranslate = _mm_unpacklo_ps(dataScalar(job->maskTranslationOffsets.x), dataScalar(job->maskTranslationOffsets.y));

		const __m128 rotationTurns = _mm_mul_ps(
			_mm_add_ps(_mm_xor_ps(RUI_SHUFFLE_PS(maskRotation, 0), g_RuiSignMaskLane2), g_RuiQuarterEndpoints),
			g_RuiFloatFour);
		const __m128i rotationQuadrant = _mm_cvtps_epi32(rotationTurns);
		const __m128 quadrantIsEven = _mm_castsi128_ps(_mm_cmpeq_epi32(
			_mm_and_si128(_mm_castps_si128(g_RuiIntOne), rotationQuadrant),
			_mm_setzero_si128()));
		const __m128 rotationFraction = _mm_sub_ps(rotationTurns, _mm_cvtepi32_ps(rotationQuadrant));
		const __m128 fractionSq = _mm_mul_ps(rotationFraction, rotationFraction);

		const __m128 cosApprox = _mm_sub_ps(
			g_RuiFloatOne,
			_mm_sub_ps(
				fractionSq,
				_mm_mul_ps(
					_mm_add_ps(
						_mm_mul_ps(
							_mm_add_ps(
								_mm_mul_ps(
									_mm_add_ps(_mm_mul_ps(g_RuiCosApproxCoeff3, fractionSq), g_RuiCosApproxCoeff2),
									fractionSq),
								g_RuiCosApproxCoeff1),
							fractionSq),
						g_RuiCosApproxCoeff0),
					fractionSq)));
		const __m128 sinApprox = _mm_add_ps(
			_mm_mul_ps(
				_mm_add_ps(
					_mm_mul_ps(
						_mm_add_ps(
							_mm_mul_ps(
								_mm_add_ps(_mm_mul_ps(g_RuiSinApproxCoeff3, fractionSq), g_RuiSinApproxCoeff2),
								fractionSq),
							g_RuiSinApproxCoeff1),
						fractionSq),
					g_RuiSinApproxCoeff0),
				rotationFraction),
			rotationFraction);
		const __m128 quadrantSign = _mm_castsi128_ps(_mm_slli_epi32(
			_mm_and_si128(_mm_castps_si128(g_RuiIntTwo), rotationQuadrant),
			0x1E));
		const __m128 rotationBasis = _mm_mul_ps(
			_mm_xor_ps(_mm_or_ps(_mm_andnot_ps(quadrantIsEven, cosApprox), _mm_and_ps(sinApprox, quadrantIsEven)), quadrantSign),
			_mm_movelh_ps(maskSize, maskSize));

		const __m128 maskTextureScale = _mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(secondaryTextureRecord.uvScale)));
		const __m128 maskTextureCenter = _mm_add_ps(_mm_mul_ps(_mm_movelh_ps(maskCenter, maskCenter), textureExtent), texMins);
		const __m128 rotatedPosition = _mm_mul_ps(RUI_SHUFFLE_PS(_mm_sub_ps(texturePosition, maskTextureCenter), 216), rotationBasis);
		const __m128 maskTexturePosition = _mm_mul_ps(
			_mm_add_ps(_mm_add_ps(maskTranslate, maskTextureCenter), _mm_add_ps(RUI_SHUFFLE_PS(rotatedPosition, 78), rotatedPosition)),
			maskTextureScale);
		maskBasis = _mm_mul_ps(
			_mm_add_ps(
				_mm_mul_ps(RUI_SHUFFLE_PS(rotationBasis, 78), RUI_SHUFFLE_PS(basisExtent, 165)),
				_mm_mul_ps(RUI_SHUFFLE_PS(basisExtent, 240), rotationBasis)),
			maskTextureScale);
		maskBase = _mm_add_ps(maskTexturePosition, _mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(secondaryTextureRecord.uvBase))));
	}

	const __m128 zero = _mm_setzero_ps();
	baseUv.primaryBasisX = _mm_movelh_ps(primaryBasis, maskBasis);
	baseUv.secondaryOrigin = zero;
	baseUv.primaryOrigin = _mm_movelh_ps(primaryBase, maskBase);
	baseUv.primaryBasisY = _mm_movehl_ps(maskBasis, primaryBasis);
	baseUv.secondaryBasisX = zero;
	baseUv.secondaryBasisY = zero;

	return RuiDrawImageAtlasEntry(
		context->globals,
		rui,
		batch,
		&baseUv,
		transform,
		orientation,
		primaryAsset,
		&atlasUv,
		&geometryBounds,
		&texMins,
		&textureExtent);
}

DECLARE_HOOK(RuiRenderImageJob, engine.dll + 0xF72F0, [](auto& hook, RuiRenderContext* context, RuiInstance* rui, const RuiImageRenderJob* job, RuiDrawBatch* batch) -> bool
	{
		return RuiRenderImageJob(context, rui, job, batch);
	});

DECLARE_HOOK(
	RuiRenderEllipseJob,
	engine.dll + 0xF7A80,
	[](auto& hook, RuiRenderContext* context, RuiInstance* rui, const RuiEllipseRenderJob* job, RuiDrawBatch* batch) -> bool
	{
		return RuiRenderEllipseJob(context, rui, job, batch);
	});


bool __fastcall RuiRenderTextJob(
	RuiRenderContext* context,
	RuiInstance* rui,
	const RuiTextRenderJob* job,
	RuiDrawBatch* batch)
{
	RuiRuntimeState* runtime = rui->runtime;
	const __int64 transformIndex = job->transformIndex;
	const __m128 transformSize = runtime->transformSizes[transformIndex];
	if (_mm_movemask_ps(_mm_cmpeq_ps(_mm_setzero_ps(), transformSize)) != 0)
		return true;

	const RuiTransform* transform = &runtime->transforms[transformIndex];
	const __m128 transformRow0 = transform->rows[0];
	const __m128 transformRow1 = transform->rows[1];
	const __m128 determinant = _mm_sub_ps(
		_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(3, 3, 3, 3)), RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(0, 0, 0, 0))),
		_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(2, 2, 2, 2)), RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(1, 1, 1, 1))));
	if (_mm_movemask_ps(_mm_cmpeq_ps(determinant, _mm_setzero_ps())) != 0)
		return true;

	const __m128 inverseBasis = _mm_div_ps(_mm_xor_ps(RUI_SHUFFLE_PS(transformRow0, 39), g_RuiSignMaskMiddleLanes), determinant);
	const int orientation = _mm_movemask_ps(determinant) & 2;
	const __m128 transformedOrigin = _mm_mul_ps(_mm_xor_ps(inverseBasis, g_RuiSignMaskAll), RUI_SHUFFLE_PS(transformRow1, 216));
	const __m128 originSum = _mm_add_ps(RUI_SHUFFLE_PS(transformedOrigin, 78), transformedOrigin);

	RuiHeader* header = rui->header;
	RuiStyleDescriptorOffsets* descriptors = header->styleDescriptors;
	RuiStyleDescriptorOffsets* textStyles[4] = {
		&descriptors[job->styleIndices[0]],
		&descriptors[job->styleIndices[1]],
		&descriptors[job->styleIndices[2]],
		&descriptors[job->styleIndices[3]],
	};

	auto dataFloat = [&](uint16_t offset) -> float
	{
		float value;
		std::memcpy(&value, &rui->data[offset], sizeof(value));
		return value;
	};

	auto dataText = [&](uint16_t offset) -> char*
	{
		char* value;
		std::memcpy(&value, &rui->data[offset], sizeof(value));
		return value;
	};

	auto maxScalar = [] (float lhs, float rhs) -> float
	{
		return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(lhs), _mm_set_ss(rhs)));
	};

	auto minScalar = [] (float lhs, float rhs) -> float
	{
		return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(lhs), _mm_set_ss(rhs)));
	};

	auto refineReciprocal = [](__m128 value) -> __m128
	{
		const __m128 reciprocal = _mm_rcp_ps(value);
		const __m128 error = _mm_sub_ps(g_RuiFloatOne, _mm_mul_ps(reciprocal, value));
		return _mm_add_ps(
			_mm_mul_ps(_mm_add_ps(_mm_mul_ps(error, error), error), reciprocal),
			reciprocal);
	};

	RuiFont* fonts[4] = {
		g_RuiFonts[textStyles[0]->fontIndex],
		g_RuiFonts[textStyles[1]->fontIndex],
		g_RuiFonts[textStyles[2]->fontIndex],
		g_RuiFonts[textStyles[3]->fontIndex],
	};

	const __m128 refinedTransformSizeReciprocal = refineReciprocal(transformSize);

	auto styleTextWidth = [&](int styleIndex) -> float
	{
		return dataFloat(textStyles[styleIndex]->textSize) * fonts[styleIndex]->verticalMetrics[0]
			- dataFloat(textStyles[styleIndex]->unknown32);
	};

	const float maximumStyleWidth = maxScalar(
		maxScalar(styleTextWidth(0), styleTextWidth(1)),
		maxScalar(styleTextWidth(2), styleTextWidth(3)));
	float lineY = RUI_SHUFFLE_PS(refinedTransformSizeReciprocal, 255).m128_f32[0] * maximumStyleWidth;
	const float lineSpacing = RUI_SHUFFLE_PS(refinedTransformSizeReciprocal, 255).m128_f32[0] * dataFloat(job->lineSpacingOffset);
	const float horizontalAlignScale = refinedTransformSizeReciprocal.m128_f32[0] * dataFloat(job->horizontalAlignmentOffset);

	const size_t renderJobIndex =
		static_cast<size_t>(reinterpret_cast<const uint8_t*>(job) - header->renderJobs) >> 4;
	const RuiRenderJobState& runtimeJob = runtime->renderJobStates[renderJobIndex];

	// Inline image spans are stored in runtime->inlineImages and are rendered before the text glyph pass.
	const uint8_t inlineImageBegin = runtimeJob.firstInlineImage;
	const uint8_t inlineImageCount = runtimeJob.inlineImageCount;
	if (inlineImageCount)
	{
		const __m128 scaledTransformSize = RUI_SHUFFLE_PS(refinedTransformSizeReciprocal, 216);
		const __m128 lineOffsetVector = RUI_SHUFFLE_PS(_mm_set_ss(lineY), 17);
		__m128 clipUnit = g_RuiHighHalfOne;

		const uint32_t inlineImageEnd = static_cast<uint32_t>(inlineImageBegin) + inlineImageCount;
		for (uint32_t inlineImageIndex = inlineImageBegin; inlineImageIndex != inlineImageEnd; ++inlineImageIndex)
		{
			const RuiInlineImageSpan* inlineImage = &runtime->inlineImages[inlineImageIndex];
			const auto* assetDescriptor = &g_RuiImageDescriptors[inlineImage->descriptorIndex];
			const __int16 assetIndex = assetDescriptor->imageIndex;
			RuiImageAtlas* imageAtlas = &g_RuiImageAtlases[assetDescriptor->atlasIndex];

			const RuiImageAtlasEntry& textureRecord = imageAtlas->images[assetIndex];

			const __m128 imageMin = _mm_mul_ps(
				_mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(inlineImage->boundsMin))),
				scaledTransformSize);
			const __m128 imageExtent = _mm_sub_ps(
				_mm_mul_ps(
					_mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(inlineImage->boundsMax))),
					scaledTransformSize),
				imageMin);
			const __m128 imageBase = _mm_add_ps(lineOffsetVector, imageMin);
			const __m128 refinedImageExtentReciprocal = refineReciprocal(imageExtent);

			const __m128 textureOffset = _mm_loadu_ps(textureRecord.bounds);
			const __m128 atlasUv = _mm_add_ps(_mm_mul_ps(_mm_xor_ps(textureOffset, g_RuiSignMaskLowHalf), imageExtent), imageBase);
			const __m128 atlasScale = _mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(textureRecord.uvScale)));
			const __m128 inlineMaskBase = _mm_xor_ps(_mm_mul_ps(refinedImageExtentReciprocal, imageBase), g_RuiSignMaskAll);
			const __m128 inlineMaskTransform = _mm_mul_ps(_mm_mul_ps(_mm_sub_ps(originSum, imageBase), refinedImageExtentReciprocal), atlasScale);
			const __m128 inlineMaskBasis = _mm_mul_ps(_mm_mul_ps(inverseBasis, refinedImageExtentReciprocal), atlasScale);

			RuiBaseUv imageUv{};
			imageUv.imageIndex = assetIndex;
			imageUv.maskImageIndex = -1;
			imageUv.computedStyleIndex = static_cast<__int16>(
				batch->computedStyleCount + job->styleIndices[inlineImage->styleIndex]);
			imageUv.flags = 7936;
			imageUv.primaryOrigin = _mm_add_ps(inlineMaskTransform, _mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(textureRecord.uvBase))));
			imageUv.primaryBasisX = RUI_SHUFFLE_PS(_mm_castsi128_ps(_mm_castps_si128(inlineMaskBasis)), 68);
			imageUv.primaryBasisY = RUI_SHUFFLE_PS(_mm_castsi128_ps(_mm_castps_si128(inlineMaskBasis)), 238);
			std::memset(&imageUv.secondaryBasisX, 0, sizeof(imageUv.secondaryBasisX) * 3);

			if (!RuiDrawImageAtlasEntry(
					context->globals,
					rui,
					batch,
					&imageUv,
					transform,
					orientation,
					assetDescriptor,
					reinterpret_cast<const __m128i*>(&atlasUv),
					&clipUnit,
					&inlineMaskBase,
					&refinedImageExtentReciprocal))
			{
				return false;
			}
		}
	}

	{
		const __int64 defaultFontIndex = textStyles[0]->fontIndex;
		RuiDrawMaterialBatch* instances = batch->materialBatches;
		const uint8_t fontAtlasIndex = g_RuiFontAtlasIndices[defaultFontIndex];
		RuiFontAtlas* fontAtlas = &g_RuiFontAtlases[fontAtlasIndex];
		const __int64 instanceIndex = batch->materialBatchIndex;
		RuiFontAtlas* currentFontAtlas = instances[instanceIndex].fontAtlas;
		if (currentFontAtlas != fontAtlas)
		{
			if (!currentFontAtlas || instances[instanceIndex].firstIndex == batch->indexBufferSize)
			{
				instances[instanceIndex].fontAtlas = fontAtlas;
			}
			else
			{
				const unsigned int nextInstanceIndex = static_cast<unsigned int>(instanceIndex + 1);
				batch->materialBatchIndex = nextInstanceIndex;
				if (nextInstanceIndex != batch->materialBatchCapacity)
				{
					instances[instanceIndex].firstIndex = batch->indexBufferSize;
					instances[instanceIndex + 1].firstIndex = batch->indexBufferSize;
					instances[instanceIndex + 1].fontAtlas = fontAtlas;
					instances[instanceIndex + 1].firstVertex = instances[instanceIndex].firstVertex;
					instances[instanceIndex + 1].imageAtlas = nullptr;
				}
			}
		}
	}

	char* activeCursor = dataText(job->textOffset);
	uint32_t styleEscapeCount = 0;
	uint8_t activeStyle = 0;
	if (*activeCursor == '`')
	{
		do
		{
			activeStyle = static_cast<uint8_t>(activeCursor[1] - '0');
			if (activeStyle >= 4)
				break;

			activeCursor += 2;
			++styleEscapeCount;
		}
		while (*activeCursor == '`');
	}

	const uint8_t lineBegin = runtimeJob.firstLine;
	const uint32_t lineEnd = static_cast<uint32_t>(lineBegin) + runtimeJob.lineCount;
	const float lineHeightScale = runtimeJob.fittedScale;

	auto lineBreakGlyph = [&](uint32_t lineIndex) -> uint32_t
	{
		return *reinterpret_cast<const uint32_t*>(&runtime->textLineData[3 * lineIndex + 1]);
	};
	uint32_t nextLineGlyph = static_cast<uint32_t>(-1);
	uint32_t lineCursor = lineBegin;
	float currentAdvance = 0.0f;
	if (lineCursor < lineEnd)
	{
		nextLineGlyph = lineBreakGlyph(lineCursor);
		currentAdvance = (transformSize.m128_f32[0] - runtime->textLineData[3 * lineCursor + 2]) * horizontalAlignScale;
		++lineCursor;
	}
	float carryAdvance = 0.0f;
	__m128 correctionData[5];
		s_BuildEdgeCorrection(transform, &header->elementWidth, correctionData);

	float previousLineMax = 0.0f;
	const __m128 transformSizeXY = RUI_SHUFFLE_PS(transformSize, 216);
	char* includeStack[14] = {};
	uint32_t includeDepth = 0;

	for (;;)
	{
		RuiTextGlyphState firstGlyphState{};
		RuiTextGlyphState lastGlyphState{};
		RuiTextGlyphState currentGlyphState{};
		RuiFontGlyph* glyph = nullptr;
		RuiFont* font = fonts[activeStyle];
		auto* styleWords = reinterpret_cast<uint16_t*>(textStyles[activeStyle]);
		const uint16_t styleDescriptorIndex = static_cast<uint16_t>(
			batch->computedStyleCount + job->styleIndices[activeStyle]);

		RuiBaseUv glyphUv{};
		glyphUv.flags = 0;
		glyphUv.computedStyleIndex = styleDescriptorIndex;

		const float baselineOffset = maxScalar(dataFloat(styleWords[23]), previousLineMax);
		__m128 glyphScaleX = _mm_set_ss(lineHeightScale);
		__m128 glyphScaleY = _mm_set_ss(dataFloat(styleWords[20]));
		glyphScaleX.m128_f32[0] = (lineHeightScale * dataFloat(styleWords[21])) * glyphScaleY.m128_f32[0];
		const float lineExtra = dataFloat(styleWords[22]);
		const __m128 glyphScale = _mm_movelh_ps(_mm_unpacklo_ps(glyphScaleX, glyphScaleY), _mm_unpacklo_ps(glyphScaleX, glyphScaleY));
		const __m128 refinedGlyphScaleReciprocal = refineReciprocal(glyphScale);
		const bool styleRenderable =
			maxScalar(dataFloat(styleWords[4]), minScalar(maxScalar(dataFloat(styleWords[8]), dataFloat(styleWords[12])), lineExtra)) > 0.0f;
		const float glyphAdvanceScale = refinedTransformSizeReciprocal.m128_f32[0] * glyphScaleX.m128_f32[0];
		const __m128 styleOffset = _mm_unpacklo_ps(_mm_set_ss(dataFloat(styleWords[17])), _mm_set_ss(dataFloat(styleWords[18])));
		__m128 textHeight = _mm_set_ss(dataFloat(styleWords[19]));
		__m128 glyphScaleYScreen = RUI_SHUFFLE_PS(refinedTransformSizeReciprocal, 255);
		glyphScaleYScreen.m128_f32[0] *= glyphScaleY.m128_f32[0];
		const __m128 outlinePad = _mm_mul_ps(_mm_set1_ps(dataFloat(styleWords[24])), g_RuiFloatHalf);
		const __m128 textBoundsPad = _mm_max_ps(
			_mm_add_ps(_mm_mul_ps(_mm_set1_ps(textHeight.m128_f32[0]), g_RuiFloatHalf), _mm_xor_ps(_mm_movelh_ps(styleOffset, styleOffset), g_RuiSignMaskLowHalf)),
			outlinePad);
		textHeight.m128_f32[0] = lineExtra + baselineOffset;
		const __m128 glyphBoundsOffset = _mm_mul_ps(
			_mm_xor_ps(_mm_add_ps(textBoundsPad, _mm_set1_ps(textHeight.m128_f32[0])), g_RuiSignMaskLowHalf),
			RUI_SHUFFLE_PS(refinedTransformSizeReciprocal, 216));
		const __m128 fontAtlasScale = _mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(font->atlasScale)));
		const float glyphBoundsMaxY = glyphBoundsOffset.m128_f32[3];
		const float glyphBoundsMinY = glyphBoundsOffset.m128_f32[1];

		RuiDrawQuad tri{};
		tri.vertexCount = 4;
		tri.vertexCapacity = 4;

		uint32_t previousCodepoint = 0;
		constexpr uint32_t inlineAssetLeadCodepoint = 0xF2000;
		constexpr uint32_t inlineAssetTrailCodepoint = 0xF2001;
		uint32_t pendingGlyphCount = 0;
		float batchStartX = 0.0f;
		__m128 batchMinY = _mm_setzero_ps();
		__m128 batchMaxY = _mm_setzero_ps();

		const __m128 glyphUvScale = _mm_mul_ps(
			_mm_mul_ps(refinedGlyphScaleReciprocal, transformSizeXY),
			fontAtlasScale);
		const __m128 glyphBasis = _mm_mul_ps(inverseBasis, glyphUvScale);
		const __m128 glyphOrigin = _mm_mul_ps(originSum, glyphUvScale);
		__m128 correctionMask = g_RuiHighHalfSignedOne;

		for (;;)
		{
			uint32_t parsedCount = styleEscapeCount;
			int codepoint;
			bool haveCodepoint = false;
			for (;;)
			{
				for (;;)
				{
					codepoint = static_cast<int>(s_ReadUnicodeCharacter(&activeCursor));
					++parsedCount;
					styleEscapeCount = parsedCount;
					if (codepoint == '%')
						break;

					if (codepoint || !includeDepth)
					{
						haveCodepoint = true;
						break;
					}

					activeCursor = includeStack[--includeDepth];
				}

				if (haveCodepoint)
					break;

				const char includeMarker = *activeCursor;
				if (includeMarker <= 32 || (includeMarker <= 63 && ((1ULL << (includeMarker - 32)) & 0x80005002ULL) != 0))
				{
					haveCodepoint = true;
					break;
				}

				if (includeMarker == '%')
					break;

				char includeScratch[8];
				char* includeText = s_ResolveTextEscape(
					rui,
					context,
					&activeCursor,
					includeScratch);
				if (!includeText)
					return true;

				includeStack[includeDepth++] = activeCursor;
				activeCursor = includeText;
			}

			if (!haveCodepoint)
				++activeCursor;

			const bool controlCode = static_cast<unsigned int>(codepoint - 1) >= 0xEFFFF || codepoint == '`';
			float glyphAdvance = 0.0f;
			float currentGlyphX;
			const RuiFontGlyph* glyphMetrics;
			if (controlCode)
			{
				currentGlyphX = currentGlyphState.penX;
				glyphMetrics = currentGlyphState.glyph;
			}
			else
			{
				const uint32_t glyphIndex = static_cast<uint32_t>(s_GetFontGlyphIndex(font, codepoint));
				glyph = &font->glyphs[glyphIndex];
				int kernIndex = glyph->firstKerning;
				const int kernEnd = glyph[1].firstKerning;
				float kernOffset = 0.0f;
				if (kernIndex < kernEnd)
				{
					RuiFontKerning* kernTable = font->kerning;
					while (static_cast<uint16_t>(kernIndex) < kernEnd &&
						kernTable[static_cast<uint16_t>(kernIndex)].codepoint != previousCodepoint)
					{
						kernIndex = static_cast<uint16_t>(kernIndex + 1);
					}
					if (static_cast<uint16_t>(kernIndex) < kernEnd)
						kernOffset = kernTable[static_cast<uint16_t>(kernIndex)].offset;
				}
				currentGlyphState.glyphIndex = glyphIndex;
				glyphAdvance = glyphAdvanceScale * glyph->advance;
				glyphMetrics = glyph;
				currentGlyphState.glyph = glyph;
				currentAdvance += kernOffset * glyphAdvanceScale;
				currentGlyphX = currentAdvance;
				currentGlyphState.penX = currentAdvance;
			}

			const bool reachedLineBreak = parsedCount >= nextLineGlyph;
			auto submitGlyphBatch = [&](float drawCenterX, __m128 rightMinY, __m128 rightMaxY) -> bool
			{
				const auto* firstGlyph = firstGlyphState.glyph;
				const auto* lastGlyph = lastGlyphState.glyph;
				__m128 drawCenter = _mm_set_ss(drawCenterX);
				__m128 drawStart = _mm_set_ss(batchStartX);
				const __m128 batchUv = _mm_shuffle_ps(drawStart, drawCenter, 0);
				const __m128 proportionScale = _mm_shuffle_ps(
					_mm_set_ss(font->proportions[firstGlyph->proportionIndex].boundsScale),
					_mm_set_ss(font->proportions[lastGlyph->proportionIndex].boundsScale),
					0);

				__m128 glyphTextureBase = _mm_castsi128_ps(
					_mm_loadl_epi64(reinterpret_cast<const __m128i*>(firstGlyph->uvBase)));
				glyphTextureBase = _mm_castpd_ps(
					_mm_loadh_pd(_mm_castps_pd(glyphTextureBase), reinterpret_cast<const double*>(lastGlyph->uvBase)));
				const __m128 glyphXPair = _mm_unpacklo_ps(_mm_set_ss(firstGlyphState.penX), _mm_set_ss(lastGlyphState.penX));
				const __m128 lineOffsetPair = _mm_unpacklo_ps(_mm_set_ss(lineY), _mm_set_ss(lineY));
				glyphUv.primaryOrigin = _mm_add_ps(
					_mm_mul_ps(
						_mm_sub_ps(
							glyphOrigin,
							_mm_mul_ps(
								_mm_unpacklo_ps(glyphXPair, lineOffsetPair),
								glyphUvScale)),
						proportionScale),
					glyphTextureBase);
				glyphUv.primaryBasisX = _mm_mul_ps(RUI_SHUFFLE_PS(glyphBasis, 68), proportionScale);
				glyphUv.secondaryOrigin = RUI_SHUFFLE_PS(proportionScale, 216);
				glyphUv.primaryBasisY = _mm_mul_ps(RUI_SHUFFLE_PS(glyphBasis, 238), proportionScale);
				std::memset(&glyphUv.secondaryBasisX, 0, sizeof(glyphUv.secondaryBasisX) * 2);
				glyphUv.imageIndex = static_cast<__int16>(font->atlasGlyphBase + static_cast<__int16>(firstGlyphState.glyphIndex));
				glyphUv.maskImageIndex = static_cast<__int16>(font->atlasGlyphBase + static_cast<__int16>(lastGlyphState.glyphIndex));

				const __m128 bounds = _mm_add_ps(
					_mm_unpacklo_ps(_mm_unpacklo_ps(batchMinY, rightMaxY), _mm_unpacklo_ps(batchMaxY, rightMinY)),
					_mm_set1_ps(lineY));
				const __m128i transform0 = _mm_castps_si128(transform->rows[0]);
				alignas(16) __m128 projected[2];
				projected[0] = _mm_add_ps(
					_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_I32_AS_PS(transform0, 0), batchUv), _mm_mul_ps(RUI_SHUFFLE_I32_AS_PS(transform0, 170), bounds)),
					RUI_SHUFFLE_PS(transform->rows[1], 0));
				projected[1] = _mm_add_ps(
					_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_I32_AS_PS(transform0, 85), batchUv), _mm_mul_ps(RUI_SHUFFLE_I32_AS_PS(transform0, 255), bounds)),
					RUI_SHUFFLE_PS(transform->rows[1], 85));

				s_ApplyEdgeCorrection(context->globals, rui, correctionData, &correctionMask, projected);

				__m128 quad0 = _mm_unpacklo_ps(projected[0], projected[1]);
				__m128 quad1 = _mm_unpackhi_ps(projected[0], projected[1]);
				if (orientation == 2)
				{
					quad0 = RUI_SHUFFLE_PS(quad0, 78);
					quad1 = RUI_SHUFFLE_PS(quad1, 78);
				}

				_mm_storeu_ps(&tri.positions[0][0], quad0);
				_mm_storeu_ps(&tri.positions[1][0], quad1);
				RuiDrawInfo* drawInfo = rui->drawInfo;
				if (!g_RuiDrawInfoHandlers[static_cast<uint32_t>(drawInfo->mode)](drawInfo, &glyphUv, &tri, batch))
					return false;

				batchStartX = drawCenterX;
				batchMinY = rightMinY;
				batchMaxY = rightMaxY;
				correctionMask = _mm_and_ps(correctionMask, g_RuiBlendMaskHighHalf);
				return true;
			};

			if (styleRenderable)
			{
				if (reachedLineBreak || controlCode)
				{
					if (pendingGlyphCount)
					{
						if (pendingGlyphCount == 1)
						{
							correctionMask = _mm_add_ps(correctionMask, g_RuiUnitX);
							firstGlyphState = lastGlyphState;
						}

						const auto* firstGlyph = firstGlyphState.glyph;
						const auto* lastGlyph = lastGlyphState.glyph;
						pendingGlyphCount = 0;
						__m128 minY = _mm_set_ss(minScalar(firstGlyph->boundsMin[1], lastGlyph->boundsMin[1]));
						__m128 maxY = _mm_set_ss(maxScalar(firstGlyph->boundsMax[1], lastGlyph->boundsMax[1]));
						const float batchEndX = (glyphAdvanceScale * lastGlyph->boundsMax[0]) + lastGlyphState.penX;
						correctionMask = _mm_add_ps(correctionMask, g_RuiUnitY);
						const float drawCenterX = batchEndX + glyphBoundsOffset.m128_f32[2];
						minY.m128_f32[0] = (minY.m128_f32[0] * glyphScaleYScreen.m128_f32[0]) + glyphBoundsMinY;
						maxY.m128_f32[0] = (maxY.m128_f32[0] * glyphScaleYScreen.m128_f32[0]) + glyphBoundsMaxY;
						if (!submitGlyphBatch(drawCenterX, minY, maxY))
						return false;
					}
				}
				else
				{
					const float posMinX = glyph->boundsMin[0];
					if (posMinX == glyph->boundsMax[0])
					{
						currentAdvance += glyphAdvance;
						previousCodepoint = codepoint;
						glyph = nullptr;
						continue;
					}

					if (pendingGlyphCount <= 1)
					{
						__m128 minY = glyphScaleYScreen;
						__m128 maxY = glyphScaleYScreen;
						minY.m128_f32[0] = (glyphScaleYScreen.m128_f32[0] * glyph->boundsMin[1]) + glyphBoundsMinY;
						maxY.m128_f32[0] = (glyphScaleYScreen.m128_f32[0] * glyph->boundsMax[1]) + glyphBoundsMaxY;

						if (pendingGlyphCount)
						{
							batchMinY.m128_f32[0] = minScalar(batchMinY.m128_f32[0], minY.m128_f32[0]);
							batchMaxY.m128_f32[0] = maxScalar(batchMaxY.m128_f32[0], maxY.m128_f32[0]);
						}
						else
						{
							batchMinY = minY;
							batchMaxY = maxY;
							batchStartX = (glyphAdvanceScale * posMinX) + glyphBoundsOffset.m128_f32[0] + currentAdvance;
							correctionMask = _mm_sub_ps(correctionMask, g_RuiUnitX);
						}

						++pendingGlyphCount;
						currentAdvance += glyphAdvance;
						firstGlyphState = lastGlyphState;
						lastGlyphState = currentGlyphState;
						previousCodepoint = codepoint;
						glyph = nullptr;
						continue;
					}

					{
						const auto* firstGlyph = firstGlyphState.glyph;
						const auto* lastGlyph = lastGlyphState.glyph;
						__m128 rightMinY = _mm_set_ss(firstGlyph->boundsMin[1]);
						__m128 rightMaxY = _mm_set_ss(firstGlyph->boundsMax[1]);
						float drawCenterX = (((firstGlyph->boundsMax[0] + glyphMetrics->boundsMin[0]) * glyphAdvanceScale)
							+ (firstGlyphState.penX + currentGlyphX)) * 0.5f;
						rightMinY.m128_f32[0] =
							(minScalar(minScalar(rightMinY.m128_f32[0], lastGlyph->boundsMin[1]), glyphMetrics->boundsMin[1])
								* glyphScaleYScreen.m128_f32[0])
							+ glyphBoundsMinY;
						rightMaxY.m128_f32[0] =
							(maxScalar(maxScalar(rightMaxY.m128_f32[0], lastGlyph->boundsMax[1]), glyphMetrics->boundsMax[1])
								* glyphScaleYScreen.m128_f32[0])
							+ glyphBoundsMaxY;

						if (!submitGlyphBatch(drawCenterX, rightMinY, rightMaxY))
						return false;
					}
				}
			}

			if (reachedLineBreak)
			{
				lineY += glyphScaleYScreen.m128_f32[0] + lineSpacing;
				if (lineCursor >= lineEnd)
				{
					nextLineGlyph = static_cast<uint32_t>(-1);
					currentAdvance = transformSize.m128_f32[0] - runtime->textLineData[3 * lineEnd];
				}
				else
				{
					const __int64 lineRecord = 3LL * lineCursor;
					nextLineGlyph = lineBreakGlyph(lineCursor);
					++lineCursor;
					currentAdvance = transformSize.m128_f32[0] - runtime->textLineData[lineRecord + 2];
				}
				currentAdvance *= horizontalAlignScale;

				if (!glyph || glyph->boundsMin[0] == glyph->boundsMax[0])
				{
					pendingGlyphCount = 0;
				}
				else
				{
					currentGlyphState.penX = currentAdvance;
					pendingGlyphCount = 1;
					const float posMinX = glyph->boundsMin[0];
					const float xOffset = glyphAdvanceScale * posMinX;
					batchStartX = xOffset + glyphBoundsOffset.m128_f32[0] + currentAdvance;
					batchMaxY = glyphScaleYScreen;
					batchMaxY.m128_f32[0] = (glyphScaleYScreen.m128_f32[0] * glyph->boundsMax[1]) + glyphBoundsMaxY;
					batchMinY = glyphScaleYScreen;
					batchMinY.m128_f32[0] = (glyphScaleYScreen.m128_f32[0] * glyph->boundsMin[1]) + glyphBoundsMinY;
				}
			}

			currentAdvance += glyphAdvance;
			if (!controlCode)
			{
				firstGlyphState = lastGlyphState;
				lastGlyphState = currentGlyphState;
				previousCodepoint = codepoint;
				glyph = nullptr;
				continue;
			}

			if (!codepoint)
				return true;

			if (codepoint == '`')
				break;

			if (static_cast<unsigned int>(codepoint - 0xF0000) >= 0x2000)
			{
				if (codepoint != inlineAssetTrailCodepoint)
				{
					previousCodepoint = codepoint;
					glyph = nullptr;
					continue;
				}

				currentAdvance += carryAdvance;
				previousCodepoint = inlineAssetTrailCodepoint;
				glyph = nullptr;
				carryAdvance = 0.0f;
			}
			else
			{
				const auto* unicodeAssetTable = static_cast<const RuiImageAssetDescriptor*>(g_RuiImageDescriptorMap->entries);
				const uint16_t unicodeAssetIndex = static_cast<uint16_t>(codepoint);
				const RuiImageAssetDescriptor& unicodeAsset = unicodeAssetTable[unicodeAssetIndex];
				const __int16 unicodeTextureIndex = unicodeAsset.imageIndex;
				const uint8_t unicodeAtlasIndex = unicodeAsset.atlasIndex;
				RuiImageAtlas* unicodeAtlas = &g_RuiImageAtlases[unicodeAtlasIndex];
				const RuiImageDimensions& unicodeDimensions = unicodeAtlas->imageDimensions[unicodeTextureIndex];
				const float unicodeWidth = static_cast<float>(unicodeDimensions.width);

				if (previousCodepoint == inlineAssetLeadCodepoint)
				{
					if (static_cast<uint16_t>(unicodeTextureIndex) >= unicodeAtlas->nineSliceImageCount)
					{
						carryAdvance = 0.0f;
						previousCodepoint = codepoint;
						glyph = nullptr;
					}
					else
					{
						const RuiImageAtlasNineSlice& unicodeNineSlice = unicodeAtlas->nineSliceData[unicodeTextureIndex];
						const float scaledWidth = refinedTransformSizeReciprocal.m128_f32[0] * unicodeWidth;
						currentAdvance += scaledWidth * unicodeNineSlice.normalizedBounds[0];
						carryAdvance = scaledWidth * unicodeNineSlice.normalizedBounds[2];
						previousCodepoint = codepoint;
						glyph = nullptr;
					}
				}
				else
				{
					currentAdvance += (unicodeWidth / static_cast<float>(unicodeDimensions.height)) * glyphAdvanceScale;
					previousCodepoint = codepoint;
					glyph = nullptr;
				}
			}
		}

		activeStyle = static_cast<uint8_t>(*activeCursor - '0');
		if (activeStyle >= 4)
			return true;

		++activeCursor;
		previousLineMax = 0.0f;
	}
}

DECLARE_HOOK(RuiRenderTextJob, engine.dll + 0xF5840, [](auto& hook, RuiRenderContext* context, RuiInstance* rui, const RuiTextRenderJob* job, RuiDrawBatch* batch) -> bool
{
	return RuiRenderTextJob(context, rui, job, batch);
});

__m128 RuiMeasureTextJob(RuiInstance* rui, uint32_t renderJobOffset)
{
	RuiRuntimeState* runtime = rui->runtime;
	const auto* job = reinterpret_cast<const RuiTextRenderJob*>(
		rui->header->renderJobs + renderJobOffset);
	RuiRenderContext* includeContext = runtime->textContext;
	RuiStyleDescriptorOffsets* descriptors = rui->header->styleDescriptors;
	RuiStyleDescriptorOffsets* textStyles[4] = {
		&descriptors[job->styleIndices[0]],
		&descriptors[job->styleIndices[1]],
		&descriptors[job->styleIndices[2]],
		&descriptors[job->styleIndices[3]],
	};

	auto dataFloat = [&](uint16_t offset) -> float
	{
		float value;
		std::memcpy(&value, &rui->data[offset], sizeof(value));
		return value;
	};

	auto dataText = [&](uint16_t offset) -> char*
	{
		char* value;
		std::memcpy(&value, &rui->data[offset], sizeof(value));
		return value;
	};

	auto maxScalar = [](float lhs, float rhs) -> float
	{
		return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(lhs), _mm_set_ss(rhs)));
	};

	auto minScalar = [](float lhs, float rhs) -> float
	{
		return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(lhs), _mm_set_ss(rhs)));
	};

	RuiFont* fonts[4] = {
		g_RuiFonts[textStyles[0]->fontIndex],
		g_RuiFonts[textStyles[1]->fontIndex],
		g_RuiFonts[textStyles[2]->fontIndex],
		g_RuiFonts[textStyles[3]->fontIndex],
	};
	float textSizes[4];
	float glyphAdvanceScales[4];
	float ascents[4];
	for (uint32_t styleIndex = 0; styleIndex < 4; ++styleIndex)
	{
		textSizes[styleIndex] = dataFloat(textStyles[styleIndex]->textSize);
		glyphAdvanceScales[styleIndex] =
			textSizes[styleIndex] * dataFloat(textStyles[styleIndex]->stretchX);
		ascents[styleIndex] =
			(textSizes[styleIndex] * fonts[styleIndex]->verticalMetrics[0])
			- dataFloat(textStyles[styleIndex]->unknown32);
	}

	const float maximumAscent = maxScalar(
		maxScalar(ascents[0], ascents[1]),
		maxScalar(ascents[2], ascents[3]));
	const float maximumDescent = maxScalar(
		maxScalar(textSizes[0] - ascents[0], textSizes[1] - ascents[1]),
		maxScalar(textSizes[2] - ascents[2], textSizes[3] - ascents[3]));
	const float lineHeight = maximumDescent + maximumAscent;
	const float lineAdvance = dataFloat(job->lineSpacingOffset) + lineHeight;
	const float wrapWidth = dataFloat(job->wrapWidthOffset);

	const uint32_t initialLineCount = runtime->textLineCount;
	const uint32_t initialInlineImageCount = runtime->inlineImageCount;
	uint32_t savedInlineImageCount = initialInlineImageCount;
	uint32_t savedBreakGlyph = 0;
	float savedLineWidth = 0.0f;
	float savedBreakX = 0.0f;
	float currentAdvance = 0.0f;
	float currentLineWidth = 0.0f;
	float maximumLineWidth = 0.0f;
	float verticalOffset = 0.0f;
	uint32_t parsedGlyphCount = 0;
	int32_t previousCodepoint = 0;
	uint32_t activeStyleMask = 0;
	uint8_t activeStyle = 0;
	uint8_t previousBreakClass = fonts[0]->glyphs[0].wordBreakClass;
	bool pendingSpace = false;

	auto appendLineBreak = [&](uint32_t breakGlyph, float width)
	{
		const uint32_t lineIndex = runtime->textLineCount++;
		if (lineIndex >= 64)
			return;

		std::memcpy(
			&runtime->textLineData[3 * lineIndex + 1],
			&breakGlyph,
			sizeof(breakGlyph));
		runtime->textLineData[3 * lineIndex + 2] = width;
		runtime->textLineData[3 * lineIndex + 3] = 0.0f;
	};

	RuiInlineImageSpan* inlineImages = runtime->inlineImages;
	const RuiFontAtlas* wordBreakAtlas =
		&g_RuiFontAtlases[g_RuiFontAtlasIndices[textStyles[0]->fontIndex]];
	RuiFont* font = fonts[0];
	RuiInlineImageSpan* pendingInlineImage = nullptr;
	char* cursor = dataText(job->textOffset);
	char* includeStack[29] = {};
	uint32_t includeDepth = 0;
	char includeScratch[8];

	for (;;)
	{
		const int32_t codepoint = static_cast<int32_t>(s_ReadUnicodeCharacter(&cursor));
		++parsedGlyphCount;

		const bool ordinaryCodepoint =
			(static_cast<uint32_t>(codepoint) - 1u) < 0xEFFFFu
			&& codepoint != '`';
		if (ordinaryCodepoint)
		{
			if (codepoint == '%')
			{
				const int marker = static_cast<int>(static_cast<int8_t>(*cursor));
				const bool literalPercent =
					marker <= ' '
					|| (marker <= '?'
						&& ((1u << (marker - ' ')) & 0x80005002u) != 0);
				if (!literalPercent)
				{
					if (marker == '%')
					{
						++cursor;
					}
					else
					{
						char* includeText = s_ResolveTextEscape(
							rui,
							includeContext,
							&cursor,
							includeScratch);
						if (!includeText)
							break;

						includeStack[includeDepth++] = cursor;
						cursor = includeText;
						continue;
					}
				}
			}

			const uint32_t glyphIndex = static_cast<uint32_t>(s_GetFontGlyphIndex(font, codepoint));
			const RuiFontGlyph* glyph = &font->glyphs[glyphIndex];
			uint16_t kernIndex = glyph->firstKerning;
			const uint16_t kernEnd = glyph[1].firstKerning;
			float kerning = 0.0f;
			while (kernIndex < kernEnd && font->kerning[kernIndex].codepoint != previousCodepoint)
				++kernIndex;
			if (kernIndex < kernEnd)
				kerning = font->kerning[kernIndex].offset;

			previousCodepoint = codepoint;
			const float beforeGlyph =
				(glyphAdvanceScales[activeStyle] * kerning) + currentAdvance;
			currentAdvance =
				(glyphAdvanceScales[activeStyle] * glyph->advance) + beforeGlyph;

			if (pendingInlineImage)
				continue;

			if (codepoint == ' ')
			{
				pendingSpace = true;
				continue;
			}

			if (codepoint == '\n')
			{
				appendLineBreak(parsedGlyphCount, currentLineWidth);
				currentAdvance = 0.0f;
				savedInlineImageCount = runtime->inlineImageCount;
				previousBreakClass = glyph->wordBreakClass;
				pendingSpace = false;
				maximumLineWidth = maxScalar(maximumLineWidth, currentLineWidth);
				verticalOffset += lineAdvance;
				currentLineWidth = 0.0f;
				continue;
			}

			const uint32_t breakBitIndex = static_cast<uint32_t>(pendingSpace)
				+ 2u * (static_cast<uint32_t>(glyph->wordBreakClass)
					+ static_cast<uint32_t>(previousBreakClass) * wordBreakAtlas->wordBreakClassCount);
			const uint8_t breakBit = static_cast<uint8_t>(1u << (breakBitIndex & 7));
			if ((wordBreakAtlas->wordBreakTable[breakBitIndex >> 3] & breakBit) != 0)
			{
				savedLineWidth = currentLineWidth;
				savedBreakGlyph = parsedGlyphCount;
				savedInlineImageCount = runtime->inlineImageCount;
				savedBreakX = beforeGlyph;
			}

			if (currentAdvance > wrapWidth)
			{
				for (uint32_t imageIndex = savedInlineImageCount;
					imageIndex < runtime->inlineImageCount;
					++imageIndex)
				{
					RuiInlineImageSpan& image = inlineImages[imageIndex];
					image.boundsMin[0] -= savedBreakX;
					image.boundsMin[1] += lineAdvance;
					image.boundsMax[0] -= savedBreakX;
					image.boundsMax[1] += lineAdvance;
				}

				appendLineBreak(savedBreakGlyph, savedLineWidth);
				currentAdvance -= savedBreakX;
				verticalOffset += lineAdvance;
				savedInlineImageCount = runtime->inlineImageCount;
				maximumLineWidth = maxScalar(maximumLineWidth, savedLineWidth);
			}

			pendingSpace = false;
			currentLineWidth = currentAdvance;
			previousBreakClass = glyph->wordBreakClass;
			continue;
		}

		if (codepoint == 0)
		{
			if (includeDepth)
			{
				cursor = includeStack[--includeDepth];
				continue;
			}
			break;
		}

		if (codepoint == '`')
		{
			const uint8_t nextStyle = static_cast<uint8_t>(*cursor - '0');
			if (nextStyle >= 4)
				break;

			activeStyle = nextStyle;
			++cursor;
			previousCodepoint = 0;
			font = fonts[activeStyle];
			activeStyleMask |= 1u << activeStyle;
			continue;
		}

		if ((static_cast<uint32_t>(codepoint) - 0xF0000u) < 0x2000u)
		{
			const uint32_t inlineImageIndex = runtime->inlineImageCount++;
			const uint32_t storageIndex = inlineImageIndex < 64 ? inlineImageIndex : 63;
			RuiInlineImageSpan* image = &inlineImages[storageIndex];
			image->descriptorIndex = static_cast<uint16_t>(codepoint);
			image->styleIndex = activeStyle;
			image->boundsMin[0] = currentAdvance;
			pendingInlineImage = image;
			activeStyleMask = 1u << activeStyle;

			const auto* unicodeAssetTable =
				static_cast<const RuiImageAssetDescriptor*>(g_RuiImageDescriptorMap->entries);
			const RuiImageAssetDescriptor& unicodeAsset =
				unicodeAssetTable[image->descriptorIndex];
			const int16_t textureIndex = unicodeAsset.imageIndex;
			RuiImageAtlas* imageAtlas = &g_RuiImageAtlases[unicodeAsset.atlasIndex];
			const RuiImageDimensions& dimensions = imageAtlas->imageDimensions[textureIndex];

			if (previousCodepoint == 0xF2000)
			{
				if (static_cast<uint16_t>(textureIndex) < imageAtlas->nineSliceImageCount)
				{
					const RuiImageAtlasNineSlice& trimRecord = imageAtlas->nineSliceData[textureIndex];
					currentAdvance +=
						(trimRecord.normalizedBounds[2] + trimRecord.normalizedBounds[0]) * static_cast<float>(dimensions.width);
				}
			}
			else
			{
				const float imageMinY = verticalOffset - ascents[activeStyle];
				const float imageWidth =
					(static_cast<float>(dimensions.width) / static_cast<float>(dimensions.height))
					* glyphAdvanceScales[activeStyle];
				image->boundsMin[1] = imageMinY;
				image->boundsMax[0] = imageWidth + image->boundsMin[0];
				image->boundsMax[1] = imageMinY + textSizes[activeStyle];
				currentAdvance += imageWidth;
				pendingInlineImage = nullptr;
			}

			previousBreakClass = 0;
			pendingSpace = false;
			currentLineWidth = currentAdvance;
			previousCodepoint = codepoint;
			continue;
		}

		if (codepoint == 0xF2001 && pendingInlineImage)
		{
			float imageMinY = verticalOffset;
			float imageMaxY = verticalOffset;
			while (activeStyleMask)
			{
				uint32_t styleIndex = 0;
				while ((activeStyleMask & (1u << styleIndex)) == 0)
					++styleIndex;
				activeStyleMask &= activeStyleMask - 1;

				const float styleMinY = verticalOffset - ascents[styleIndex];
				imageMinY = minScalar(imageMinY, styleMinY);
				imageMaxY = maxScalar(imageMaxY, styleMinY + textSizes[styleIndex]);
			}

			pendingInlineImage->boundsMin[1] = imageMinY;
			pendingInlineImage->boundsMax[0] = currentAdvance;
			pendingInlineImage->boundsMax[1] = imageMaxY;
			pendingInlineImage = nullptr;
			currentLineWidth = currentAdvance;
			previousBreakClass = 0;
			pendingSpace = false;
		}

		previousCodepoint = codepoint;
	}

	const uint32_t finalLineCount = runtime->textLineCount;
	if (finalLineCount != initialLineCount && finalLineCount <= 64)
		runtime->textLineData[3 * finalLineCount] = currentAdvance;

	const float measuredWidth = maxScalar(maximumLineWidth, currentAdvance);
	const float measuredHeight = verticalOffset + lineHeight;
	const float targetWidth = dataFloat(job->targetWidthOffset);
	const float horizontalScale = targetWidth / maxScalar(targetWidth, measuredWidth);
	const float fittedWidth = horizontalScale * measuredWidth;

	RuiRenderJobState& runtimeJob = runtime->renderJobStates[renderJobOffset >> 4];
	runtimeJob.fittedScale = horizontalScale;
	runtimeJob.firstLine = static_cast<uint8_t>(initialLineCount);
	runtimeJob.lineCount = static_cast<uint8_t>(runtime->textLineCount - initialLineCount);
	runtimeJob.firstInlineImage = static_cast<uint8_t>(initialInlineImageCount);
	runtimeJob.inlineImageCount = static_cast<uint8_t>(runtime->inlineImageCount - initialInlineImageCount);

	return _mm_shuffle_ps(_mm_set_ss(fittedWidth), _mm_set_ss(measuredHeight), 0);
}

DECLARE_HOOK(RuiMeasureTextJob, engine.dll + 0xF6980, [](auto& hook, RuiInstance* rui, uint32_t renderJobOffset) -> __m128
{
	return RuiMeasureTextJob(rui, renderJobOffset);
});

ON_DLL_LOAD("rtech_game.DLL", ImageAtlasRpak, [](CModule module)
{
	DISPATCH_MODULE(ImageAtlas);
});

ON_DLL_LOAD("engine.dll", ImageAtlas, [](CModule module)
{
	s_RpakHashAligned = module.Offset(0x4305D0).RCast<RpakHashFn>();
	s_RpakHashUnaligned = module.Offset(0x4305E0).RCast<RpakHashFn>();
	s_LoadImageAtlas = module.Offset(0xFBF60).RCast<LoadImageAtlasFn>();
	s_DestroyImageAtlas = module.Offset(0xFC4F0).RCast<DestroyImageAtlasFn>();
	s_GetAssetDescriptor = module.Offset(0xF3C60).RCast<GetAssetDescriptorFn>();

	DISPATCH_MODULE(ImageAtlas);
	g_RuiDrawInfoHandlers = module.Offset(0x5F4560).RCast<RuiDrawInfoHandlerFn*>();
	g_RuiSignMaskAll = *module.Offset(0x5F3DD0).RCast<__m128*>();
	g_RuiSignMaskLowHalf = *module.Offset(0x5F3E20).RCast<__m128*>();
	g_RuiSignMaskMiddleLanes = *module.Offset(0x5F3E50).RCast<__m128*>();
	g_RuiSignMaskHighHalf = *module.Offset(0x5F3E70).RCast<__m128*>();
	g_RuiFloatTwo = *module.Offset(0x5F3E80).RCast<__m128*>();
	g_RuiFloatOne = *module.Offset(0x5F3E90).RCast<__m128*>();
	g_RuiFloatHalf = *module.Offset(0x5F3EB0).RCast<__m128*>();
	g_RuiUnitX = *module.Offset(0x5F3EE0).RCast<__m128*>();
	g_RuiUnitY = *module.Offset(0x5F3EF0).RCast<__m128*>();
	g_RuiFloatMinNormal = *module.Offset(0x5F3F30).RCast<__m128*>();
	g_RuiFloatAbsMask = *module.Offset(0x5F3F60).RCast<__m128*>();
	g_RuiHighHalfOne = *module.Offset(0x5F4600).RCast<__m128*>();
	g_RuiHighHalfSignedOne = *module.Offset(0x5F4610).RCast<__m128*>();

	g_RuiIntTwo = *module.Offset(0x5CB2A0).RCast<__m128*>();
	g_RuiSinApproxCoeff3 = *module.Offset(0x5F34E0).RCast<__m128*>();
	g_RuiSinApproxCoeff0 = *module.Offset(0x5F34B0).RCast<__m128*>();
	g_RuiSinApproxCoeff1 = *module.Offset(0x5F3510).RCast<__m128*>();
	g_RuiSinApproxCoeff2 = *module.Offset(0x5F3490).RCast<__m128*>();
	g_RuiCosApproxCoeff0 = *module.Offset(0x5F3500).RCast<__m128*>();
	g_RuiCosApproxCoeff1 = *module.Offset(0x5F34A0).RCast<__m128*>();
	g_RuiCosApproxCoeff3 = *module.Offset(0x5F3470).RCast<__m128*>();
	g_RuiCosApproxCoeff2 = *module.Offset(0x5F34F0).RCast<__m128*>();
	g_RuiIntOne = *module.Offset(0x5F3460).RCast<__m128*>();
	g_RuiSignMaskLane2 = *module.Offset(0x5F3E00).RCast<__m128*>();
	g_RuiFloatFour = *module.Offset(0x5F34C0).RCast<__m128*>();
	g_RuiQuarterEndpoints = *module.Offset(0x5F45D0).RCast<__m128*>();

	g_RuiBlendMaskLowHalf = *module.Offset(0x12A14650).RCast<__m128*>();
	g_RuiBlendMaskLane1 = *module.Offset(0x12A146A0).RCast<__m128*>();
	g_RuiBlendMaskHighHalf = *module.Offset(0x12A146B0).RCast<__m128*>();
	g_RuiBlendMaskLane0 = *module.Offset(0x12A146D0).RCast<__m128*>();
	g_RuiImageDescriptorMap = module.Offset(0x12A4E508).RCast<RHashMap*>();

	g_RuiFontAtlases = module.Offset(0x12A26080).RCast<RuiFontAtlas*>();
	g_RuiImageDescriptors = module.Offset(0x12A2E508).RCast<RuiImageAssetDescriptor*>();

	g_RuiEllipseAxisMasks = module.Offset(0x12A4E830).RCast<__m128*>();
	s_BindImageAtlas = module.Offset(0xFC0C0).RCast<BindImageAtlasFn>();
	s_ReadUnicodeCharacter = module.Offset(0xF2C40).RCast<ReadUnicodeCharacterFn>();
	g_RuiFonts = module.Offset(0x12A4E550).RCast<RuiFont**>();

	s_GetFontGlyphIndex = module.Offset(0xFAE80).RCast<GetFontGlyphIndexFn>();
	s_ResolveTextEscape = module.Offset(0xF98F0).RCast<ResolveTextEscapeFn>();
	s_BuildEdgeCorrection = module.Offset(0xFFAE0).RCast<BuildEdgeCorrectionFn>();
	s_ApplyEdgeCorrection = module.Offset(0xFEF30).RCast<ApplyEdgeCorrectionFn>();
	s_RHashMapInsert = module.Offset(0xF3BB0).RCast<RHashMapInsertFn>();
	s_RHashMapRemove = module.Offset(0xF3E30).RCast<RHashMapRemoveFn>();
	g_RuiEdgeCorrectionMasks = module.Offset(0x5F4740).RCast<__m128*>();
	g_RuiFontAtlasIndices = module.Offset(0x12A4E650).RCast<BYTE*>();
});
