#include <immintrin.h>
#include <cstdint>
DECLARE_MODULE(AtlasTest)

#define LODWORD(x) *(uint32_t*)&x

typedef uint8_t _BYTE;
typedef uint16_t _WORD;
typedef uint32_t _DWORD;
typedef uint64_t _QWORD;

typedef uint64_t ulonglong;
typedef uint32_t uint;
typedef uint16_t ushort;
typedef _BYTE byte;
typedef _BYTE uchar;

struct ListController
{
	uint32_t headPointer_fromGPT;
	uint32_t tailPointer_fromGPT;
	int elementSize;
	int elementAmount;
	void* storagePointer;
};
struct ruiDrawTriangle
{
	_DWORD size;
	_DWORD size_;
	float vert[2][4];
};
struct assetLoader
{
	__int64 hash;
	const char* name;
	__int64 qword_10;
	__int64 qword_18;
	void* pointer_20;
	void* listPointer;
	int dword_30;
	unsigned int listElementSize;
	int dword_38;
	int listElementAmount;
	ListController list_40;
	__int64 qword_58;
};

struct mat_3x4
{
	float m[3][4];
};

struct ruiWeaponMeshHeader
{
	_DWORD numParents;
	_DWORD numVerts;
	_DWORD numFaces;
	_DWORD parentOffset;
	_DWORD vertOffset;
	_DWORD indiceOffset;
	_DWORD bonundsOffset;
	_DWORD faceBitField;
};


struct weaponSubStruct
{
	__m128 f1;
	__m128 f2;
	__m128 f3;
	__m128 f4;
	__m128 f5;
	__m128 f6;
};

struct ruiDrawInfoDataWeapon
{
	int type;
	int pad4;
	_QWORD pad8;
	ruiWeaponMeshHeader* weaponMeshHeader;
	weaponSubStruct* output;
	mat_3x4 bone[2];
	const char* rui_name;
};


struct uiImageAtlas
{
	float widthRatio;
	float heightRatio;
	uint16_t width; 
	uint16_t height; 
	uint16_t TextureCount;
	uint16_t textureOffsetsCount;
	void* textureOffsets;
	void* textureDimensions;
	void* pointer_20;
	uint64_t* textureHashes;
	void* textureNames;
	_QWORD textureMaybe;
	unsigned int bufferStructIndex;
	_DWORD dword_44;
};
static_assert(offsetof(uiImageAtlas, widthRatio) == 0);
static_assert(offsetof(uiImageAtlas, heightRatio) == 4);
static_assert(offsetof(uiImageAtlas, width) == 8);
static_assert(offsetof(uiImageAtlas, height) == 0xa);
static_assert(offsetof(uiImageAtlas, TextureCount) == 0xc);
static_assert(offsetof(uiImageAtlas, textureOffsetsCount) == 0xe);
static_assert(offsetof(uiImageAtlas, textureOffsets) == 0x10);
static_assert(offsetof(uiImageAtlas, pointer_20) == 0x20);
static_assert(offsetof(uiImageAtlas, textureHashes) == 0x28);
struct unknown2
{
	float float_0;
	_BYTE byte_4;
	_BYTE byte_5;
	_BYTE byte_6;
	_BYTE byte_7;
};

struct testStruct
{
	__m128i m128_0;
	__m128i m128_10;
};

struct struct_v1
{
	uint8_t* pByte_0;
	uint32_t uint_8;
	_DWORD dword_C;
	_DWORD dword_10;
	_DWORD dword_14;
	_QWORD qword_18;
	unknown2 unk2[1];
	_BYTE gap_28[1424];
	unsigned int dword_5B8;
	_BYTE gap_5BC[8200];
	float float_25C4[192];
	_BYTE gap_28C4[4];
	uint32_t dword_28C8;
	uint32_t dword_28CC;
	_BYTE gap_28D0[1280];
	__m128 transformSizes[1];
	_BYTE gap_2DE0[3232];
	testStruct m128_3A80[3];
};

struct ruiArgCluster
{
	uint16_t argIndex;
	uint16_t argCount;
	uint8_t byte_4;
	uint8_t byte_5;
	_BYTE gap_6[4];
	uint16_t word_A;
	_BYTE gap_C[6];
};

struct ruiArg
{
	uint8_t argType;
	uint8_t unk1;
	uint16_t offset;
	uint16_t nameOffset;
	uint16_t shortHash;
};

struct globals
{
	_BYTE gap_0[60];
	float localPlayerPos[3];
	float screenWidth;
	float screenHeight;
	_BYTE gap_50[64];
	_QWORD frameTime;
	float currentTime;
	_BYTE gap_9C[4];
	int dword_A0;
	int isConsole;
	int dword_A8;
	int dword_AC;
	int dword_B0;
	int dword_B4;
	float globalAdsFrac;
	float float_BC;
	float float_C0;
	int dword_C4;
	float float_C8;
	float float_CC;
	_DWORD dword_D0;
	_DWORD dword_D4;
	_DWORD dword_D8;
	_DWORD dword_DC;
	float float_E0;
	_DWORD dword_E4;
};


struct styleDescriptorsStruct
{
	uint16_t type;
	uint16_t color_red;
	uint16_t color_green;
	uint16_t color_blue;
	uint16_t color_alpha;
	uint16_t word_A;
	uint16_t word_C;
	uint16_t word_E;
	uint16_t word_10;
	uint16_t word_12;
	uint16_t word_14;
	uint16_t word_16;
	uint16_t word_18;
	uint16_t word_1A;
	uint16_t word_1C;
	uint16_t fontIndex;
	uint16_t word_20;
	uint16_t word_22;
	uint16_t word_24;
	uint16_t word_26;
	uint16_t textSize;
	uint16_t stretchXOffset;
	uint16_t word_2C;
	uint16_t word_2E;
	uint16_t word_30;
	uint16_t uint16_32;
};

struct ruiUnknown10
{
	uint32_t dataCount;
	uint16_t unk1;
	uint16_t unk2;
	float* data;
};

struct Float2Offsets
{
	uint16_t x;
	uint16_t y;
};

struct Float4Offsets
{
	uint16_t x;
	uint16_t y;
	uint16_t z;
	uint16_t w;
};

struct AssetRenderOffsets
{
  uint16_t type;
  uint16_t transformIndex;
  uint16_t assetIndex_0;
  uint16_t assetIndex_1;
  Float2Offsets mins;
  Float2Offsets maxs;
  Float2Offsets texMins;
  Float2Offsets texMaxs;
  Float2Offsets maskCenter;
  uint16_t maskRotation;
  Float2Offsets maskTranslate;
  Float2Offsets maskSize;
  uint16_t flags;
  uint8_t styleIndex;
  char pad_29;
};


struct renderJobsStruct_2
{
	uint16_t type;
	uint16_t transformIndex;
	uint16_t uint16_4;
	uint16_t uint16_6;
	uint16_t uint16_8;
	uint16_t uint16_A;
	uint16_t uint16_C;
	uint16_t uint16_E;
	uint16_t uint16_10;
	uint16_t uint16_12;
	uint16_t uint16_14;
	_WORD word_16;
	uint8_t uint8_18;
};

struct ruiHeader
{
	const char* name;
	void* defaultValues;
	uint8_t* transformData;
	float elementWidth;
	float elementHeight;
	float elementWidthRatio;
	float elementHeightRatio;
	const char* argNames;
	ruiArgCluster* argClusters;
	ruiArg* args;
	__int16 argCount;
	__int16 unk10Count;
	uint16_t ruiDataStructSize;
	uint16_t defaultValuesSize;
	uint16_t unknown8Count;
	uint16_t unk_A4;
	uint16_t renderJobCount;
	uint16_t argClusterCount;
	styleDescriptorsStruct* styleDescriptors;
	uint8_t* renderJobs;
	ruiUnknown10* unknown_10;
	void(__fastcall* dllFunc)(void* a1, void*, void*, char*);
	void(__fastcall* dllFuncHidden)(void*, void*, void*, void*);
};

struct renderJobsStruct_0
{
	uint16_t type;
	uint16_t transformIndex;
	uint8_t styleDescriptorsIndices[4];
	uint16_t textOffset;
	uint16_t uint16_A;
	uint16_t uint16_C;
	uint16_t uint16_E;
	uint16_t uint16_10;
};

struct uiFontAtlas
{
	uint16_t fontCount;
	uint16_t uint16_2;
	uint16_t Width;
	uint16_t Hight;
	float float_8;
	float float_C;
	void* qword_10;
	char* qword_18;
	uiImageAtlas* atlas;
	_QWORD qword_28;
};

struct unknownRuiListElement
{
	_DWORD dword_0;
	_DWORD dword_4;
	uiFontAtlas* uiFontAtlas_8;
	uiImageAtlas* uiImageAtlas_10;
};

struct ruiDataStruct
{
	ruiHeader* header;
	float canvasWidth;
	float canvasHeight;
	float canvasWidthRatio;
	float canvasHeightRatio;
	struct_v1* v1;
	__int64 createTimeStamp;
	_BYTE byte_28;
	_BYTE error;
	_BYTE gap_2A[14];
	ruiDrawInfoDataWeapon* pvoid_38;
	char dataValues[1];
};

struct unknownFontStruct
{
	int dword_0;
	float float_4;
};

struct rpakFontGlyph
{
 float unk_0;
  uint16_t unk_4;
  uint8_t unk_6;
  uint8_t proportionIndex;
  float posBaseX;
  float posBaseY;
  float posMinX;
  float posMinY;
  float posMaxX;
  float posMaxY;
};

struct UIFont_UNK_t
{
  int unk_0;
  float unk_4;
};

struct UIFontProportion_v7_t
{
  float scaleBounds;
  float scaleSize;
};


struct rpakFont
{
char *name;
  uint16_t fontIndex;
  uint16_t numProportions;
  uint16_t numGlyphChunks;
  uint16_t numUnicodeChunks;
  int glyphIndex;
  int unicodeIndex;
  uint32_t numTextures;
  float proportionScaleX;
  float proportionScaleY;
  float unk_24[2];
  uint32_t textureIndex;
  uint16_t *unicodeChunks;
  uint16_t *unicodeChunksIndex;
  uint64_t *unicodeChunksMask;
  UIFontProportion_v7_t *proportions;
  rpakFontGlyph *textures;
  UIFont_UNK_t *unk_58;
};

struct struct_a1_2
{
	_DWORD dword_0;
	unsigned int pointer_10_ByteSize;
	_QWORD pointer_8;
	int* pointer_10;
	uint32_t(__fastcall* pfunc_18)(uint32_t);
	bool(__fastcall* pfunc_20)(_DWORD*, int);
	unsigned int dword_28;
	_DWORD dword_2C;
	_DWORD dword_30;
	_DWORD dword_34;
	_QWORD pointer8_elementByteSize;
	_RTL_SRWLOCK lock;
};

struct struct_v3
{
	unknownRuiListElement* ruiInstance;
	unsigned int unsigned_int_8;
	_DWORD dword_C;
	_BYTE gap_10[8];
	_QWORD vertexBuffer;
	uint16_t vertexBufferFlags;
	uint16_t vertexBufferElementCount;
	_DWORD vertexBufferSize;
	_QWORD qword_28;
	_DWORD styleDescriptorIndex;
	_DWORD dword_34;
	_QWORD indexBuffer;
	_DWORD indexBufferSize;
	_DWORD indexBufferCapacity;
	_QWORD unkDXData1;
	_QWORD unkDXData2;
	_QWORD unkDXData3;
	_QWORD unkDXData4;
	_QWORD unkDXData5;
	_QWORD unkDXData6;
	__int64 unkFlag;
	__int64 index;
};


struct ruiBaseUvStruct
{
	__m128 base;
	__m128 xDir;
	__m128 yDir;
	__m128 base2;
	__m128 xDir2;
	__m128 yDir2;
	__int16 assetIndex;
	__int16 assetIndex2;
	__int16 styleDescriptorIndex;
	__int16 flags;
	float vert[2][4];
};


typedef unsigned int (*sub_FC0C0Type)(struct_v3* a1, uiImageAtlas* a2);

typedef uint64_t (*getFontGlyphIndexType)(rpakFont* a1, int c);
typedef uint64_t (*getUnicodeCharacter_GPTType)(char** a1);
typedef char* (*sub_F98F0Type)(ruiDataStruct* a1, __int64 a2, char** a3, __int64 a4);
typedef void (*sub_FFAE0Type)(__m128* a1, const __m128i* a2, __m128* a3);
typedef void (*sub_FEF30Type)(globals*, ruiDataStruct*, __m128*, __m128, __m128*);
typedef void (*sub_FEF30_2Type)(globals*, ruiDataStruct*, __m128*, const __m128*, __m128*);
typedef _DWORD* (*sub_F3BB0Type)(struct_a1_2* a1, __int64 a2, _BYTE* a3);
typedef unsigned int* (*sub_F3E30Type)(struct_a1_2* a1, __int64 a2);
typedef int64_t (*sub_F9B80Type)(
	__int64 a1,
	__int64 a2,
	_QWORD* a3,
	__m128* a4,
	const __m128i* a5,
	int a6,
	__int64 a7,
	__m128i* a8,
	__m128* a9,
	 __m128 *a10, __m128 *a11);

typedef __int64 (*readUnicodeCharacter_F2C40Type)(char** a1);
readUnicodeCharacter_F2C40Type readUnicodeCharacter_F2C40;
typedef __int64 (*funcs5F4560Type)(__m128* a1, __m128* a2, ruiDrawTriangle* a3, struct_v3* a4);

 //uiImageAtlas rpakUIMGAtlases[50];
uiImageAtlas* rpakUIMGAtlases;

/*
DECLARE_HOOK(CBaseFileSystem_OpenEx, filesystem_stdio.dll + 0x15F50, [](auto& hook,
	IFileSystem* filesystem,
	const char* pPath,
	const char* pOptions,
	uint32_t flags,
	const char* pPathID,
	char** ppszResolvedFilename) -> FileHandle_t
*/

DECLARE_HOOK(
	addAssetLoader,
	rtech_game.DLL + 0x7BE0,
	[](auto& hook, assetLoader* a1, unsigned int a2, unsigned int a3) -> __int64
	{
	if (a1->hash == 0xA676D6975)
	{
		a1->listElementAmount = 20; // sizeof(rpakUIMGAtlases)/sizeof(uiImageAtlas);
		a1->listPointer = rpakUIMGAtlases;
	}
	return hook.Original(a1, a2, a3);
})

short* word_12A2E50C;
uint8_t* byte_12A2E50E;
uint8_t* byte_12A2E50F;
uiFontAtlas* uiFontAtlases;
struct assetIndexData
{
  _DWORD nameHash;
  uint16_t assetIndex;
  uint8_t atlasIndex;
  _BYTE flags;
};
assetIndexData* unk_12A2E508;

__m128* xmmword_12A4E830;
funcs5F4560Type* funcs_5F4560;
unsigned int (*sub_FC0C0)(struct_v3* a1, uiImageAtlas* a2);
rpakFont** rpakFontPointers;
struct_a1_2* assetIndexList;

uint64_t (*getFontGlyphIndex)(rpakFont* a1, int c);
uint64_t (*getUnicodeCharacter_GPT)(char** a1);
char* (*sub_F98F0)(ruiDataStruct* a1, __int64 a2, char** a3, __int64 a4);
// void (*sub_F9B80)(__int64 a1, __int64 a2, _QWORD *a3, __m128 *a4, const __m128i *a5, int a6, __int64 a7, __m128i *a8, __m128 *a9, __m128
// *a10, __m128 *a11);
void (*sub_FFAE0)(__m128* a1, const __m128i* a2, __m128* a3);
void (*sub_FEF30)(globals*, ruiDataStruct*, __m128*, __m128, __m128*);
void (*sub_FEF30_2)(globals*, ruiDataStruct*, __m128*, const __m128*, __m128*);
_DWORD* (*sub_F3BB0)(struct_a1_2* a1, __int64 a2, _BYTE* a3);
unsigned int* (*sub_F3E30)(struct_a1_2* a1, __int64 a2);

int64_t (*sub_F9B80)(
	__int64 a1,
	__int64 a2,
	_QWORD* a3,
	__m128* a4,
	const __m128i* a5,
	int a6,
	__int64 a7,
	__m128i* a8,
	__m128* a9,
	__m128* a10,
	__m128* a11);

__m128* stru_5F4740;

__m128 xmmword_5F3DD0;
__m128 xmmword_5F3E20 = {};
__m128 xmmword_5F3E50;
__m128 xmmword_5F3E70;
__m128 xmmword_5F3E80;
__m128 xmmword_5F3E90;
__m128 xmmword_5F3EB0;
__m128 xmmword_5F3EE0;
__m128 xmmword_5F3EF0;
__m128 xmmword_5F3F30;
__m128 xmmword_5F3F60;
__m128 xmmword_5F4600;
__m128 xmmword_5F4610;

__m128 xmmword_12A14650;
__m128 xmmword_12A146A0;
__m128 xmmword_12A146B0;
__m128 xmmword_12A146D0;

__m128 xmmword_5CB2A0;
__m128 xmmword_5F34E0;
__m128 xmmword_5F34B0;
__m128 xmmword_5F3510;
__m128 xmmword_5F3490;
__m128 xmmword_5F3500;
__m128 xmmword_5F34A0;
__m128 xmmword_5F3470;
__m128 xmmword_5F34F0;
__m128 xmmword_5F3460;
__m128 xmmword_5F3E00;
__m128 xmmword_5F34C0;
__m128 xmmword_5F45D0;

BYTE* fontIndices;
assetIndexData* assetIndexData_12A4E510;

//DECLARE_HOOK(ruiUnknown9Func_2, engine.dll + 0xF7A80, [] (auto& hook, __int64 a1, ruiDataStruct* a2, renderJobsStruct_2* a3, struct_v3* a4)

struct ruiRenderList
{
	globals* globals;
	_QWORD qword_8;
	uint16_t word_10;
	uint16_t word_12;
	_WORD word_14;
	uint16_t ruiCount;
	ruiDataStruct* ruiInstances[1];
};
static const __m128* xmmword_5F4740;


DECLARE_HOOK(sub_FB960, engine.dll + 0xFB960, [](auto& hook,uiImageAtlas * a1, __int64 a2, __int64 a3)
{
	__int64 v3; // rdi
	unsigned int i; // ebx
	_DWORD* v8; // rax
	int v9; // edx
	char a3a; // [rsp+48h] [rbp+10h] BYREF

	v3 = 0i64;
	if (a3)
	{
		for (i = 0; i < *(unsigned __int16*)(a3 + 12); sub_F3E30(assetIndexList, *(unsigned int*)(*(_QWORD*)(a3 + 40) + 8i64 * i++)))
		{
			;
		}
	}
	if (a2)
	{
		*a1 = *(uiImageAtlas*)a2;
		AcquireSRWLockExclusive(&assetIndexList->lock);
		if (*(_WORD*)(a2 + 12))
		{
			do
			{
				v8 = sub_F3BB0(assetIndexList, *(unsigned int*)(8 * v3 + *(_QWORD*)(a2 + 40)), (_BYTE*)&a3a);
				v9 = *(_DWORD*)(8 * v3 + *(_QWORD*)(a2 + 40));
				*((_WORD*)v8 + 2) = v3;
				*((_BYTE*)v8 + 6) = a1 - rpakUIMGAtlases;
				*v8 = v9;
				*((_BYTE*)v8 + 7) = *(_BYTE*)(*(_QWORD*)(a2 + 40) + 8 * v3 + 4);
				assetIndexList->pointer_10[assetIndexList->dword_34] = assetIndexList->dword_30;
				++assetIndexList->dword_0;
				v3 = (unsigned int)(v3 + 1);
			} while ((unsigned int)v3 < *(unsigned __int16*)(a2 + 12));
		}
		ReleaseSRWLockExclusive(&assetIndexList->lock);
	}
});



//__m128i* xmmword_5F4740; // edge-clip table


typedef uint32_t(__fastcall* ruiDrawInfoFunc)(ruiDrawInfoDataWeapon*, ruiBaseUvStruct*, ruiDrawTriangle*, struct_v3*); 

// External callees
ruiDrawInfoFunc* ruiDrawInfo_5f4560;

#define RUI_SHUFFLE_PS(value, imm) _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(value), imm))

#define RUI_SHUFFLE_I32_AS_PS(value, imm) _mm_castsi128_ps(_mm_shuffle_epi32((value), imm))

void __fastcall sub_F9B80_rebuild(
	globals* globals,
	ruiDataStruct* ruiData,
	struct_v3* batch,
	ruiBaseUvStruct* baseUv,
	__m128* transform,
	int orientation,
	__int64 nameHash,
	__m128i* atlasUv,
	__m128* clipThreshold,
	__m128* uvBias,
	__m128* viewportScale)
{
	const __int64 instanceIndex = batch->unsigned_int_8;
	unknownRuiListElement* instances = batch->ruiInstance;
	const uint8_t atlasIndex = *reinterpret_cast<const uint8_t*>(nameHash + 6);
	uiImageAtlas* imageAtlas = &rpakUIMGAtlases[atlasIndex];
	
	if (instances[instanceIndex].uiImageAtlas_10 != imageAtlas)
	{
		uiImageAtlas* currentAtlas = instances[instanceIndex].uiImageAtlas_10;

		if (!currentAtlas || instances[instanceIndex].dword_4 == batch->indexBufferSize)
		{
			instances[instanceIndex].uiImageAtlas_10 = imageAtlas;
		}
		else
		{
			instances[instanceIndex].dword_4 = batch->indexBufferSize;

			if (++batch->unsigned_int_8 == batch->dword_C)
				return;

			instances[instanceIndex + 1].dword_4 = batch->indexBufferSize;
			instances[instanceIndex + 1].uiFontAtlas_8 = 0;
			instances[instanceIndex + 1].dword_0 = instances[instanceIndex].dword_0;
			instances[instanceIndex + 1].uiImageAtlas_10 = imageAtlas;
		}
	}

	ruiDrawTriangle tri;
	ruiBaseUvStruct drawUv;
	__m128 correctionData[5];

	tri.size = 4;
	tri.size_ = 4;

	const __int16 correctionMask = (~baseUv->flags >> 8) & 0xF;
	if (correctionMask)
		sub_FFAE0(transform, (const __m128i*)& ruiData->header->elementWidth, correctionData);

	const uint16_t textureOffsetIndex = *reinterpret_cast<const uint16_t*>(nameHash + 4);
	const __m128 zero = _mm_setzero_ps();

	auto submitDraw = [&]() -> int
	{
		auto* drawInfo = ruiData->pvoid_38;
		return ruiDrawInfo_5f4560[drawInfo->type](
			drawInfo,
			&drawUv,
			&tri,
			batch);
	};

	auto setTriangleFromUv = [&](__m128 u, __m128 v, const __m128* correction, bool useAlternateOrientationShuffle, bool forceCorrection)
	{
		const __m128 row0 = transform[0];
		const __m128 row1 = transform[1];

		__m128 projected[2];
		projected[0] = _mm_add_ps(
			_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_PS(row0, 170), u), _mm_mul_ps(RUI_SHUFFLE_PS(row0, 0), v)), RUI_SHUFFLE_PS(row1, 0));
		projected[1] = _mm_add_ps(
			_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_PS(row0, 255), u), _mm_mul_ps(RUI_SHUFFLE_PS(row0, 85), v)), RUI_SHUFFLE_PS(row1, 85));

		if (correction && (forceCorrection || _mm_movemask_ps(_mm_cmpneq_ps(*correction, zero))))
			sub_FEF30_2(globals, ruiData, correctionData, correction, projected);

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

		_mm_storeu_ps(&tri.vert[0][0], quad0);
		_mm_storeu_ps(&tri.vert[1][0], quad1);
	};

	auto drawPiece =
		[&](__m128 u, __m128 v, const __m128* correction, bool useAlternateOrientationShuffle, __m128 base, __m128 xDir, __m128 yDir) -> int
	{
		setTriangleFromUv(u, v, correction, useAlternateOrientationShuffle, false);

		drawUv.yDir = yDir;
		drawUv.base = base;
		drawUv.xDir = xDir;
		*reinterpret_cast<__int64*>(&drawUv.assetIndex) = *reinterpret_cast<const __int64*>(&baseUv->assetIndex);
		memset(&drawUv.base2, 0, 48);
		return submitDraw();
	};

	if (textureOffsetIndex >= imageAtlas->textureOffsetsCount)
	{
		//const __m128 transformRow0 = _mm_castsi128_ps(_mm_load_si128(reinterpret_cast<const __m128i*>(transform)));
		__m128 transformRow0 = transform[0];
		const __m128 u = RUI_SHUFFLE_I32_AS_PS(*atlasUv, 125);
		const __m128 v = RUI_SHUFFLE_I32_AS_PS(*atlasUv, 160);

		__m128 projected[2];
		projected[0] = _mm_add_ps(
			_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 170), u),
					   _mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 0), v)),
			RUI_SHUFFLE_PS(transform[1], 0));
		projected[1] = _mm_add_ps(
			_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 255), u), _mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 85), v)),
			RUI_SHUFFLE_PS(transform[1], 85));

		if (correctionMask) {
			sub_FEF30(globals, ruiData, correctionData, xmmword_5F4740[correctionMask], projected);
		}
		__m128 quad0 = _mm_unpacklo_ps(projected[0], projected[1]);
		__m128 quad1 = _mm_unpackhi_ps(projected[0], projected[1]);

		if (orientation == 2)
		{
			quad0 = RUI_SHUFFLE_PS(quad0, _MM_SHUFFLE(1, 0, 3, 2));
			quad1 = RUI_SHUFFLE_PS(quad1, _MM_SHUFFLE(1, 0, 3, 2));
		}
		_mm_storeu_ps(&tri.vert[0][0], quad0);
		_mm_storeu_ps(&tri.vert[1][0], quad1);

		auto* drawInfo = ruiData->pvoid_38;
		ruiDrawInfo_5f4560[drawInfo->type](
			drawInfo,
			baseUv,
			&tri,
			batch);
		return;
	}
	const double* atlasRecord = reinterpret_cast<const double*>(reinterpret_cast<char*>(imageAtlas->pointer_20) + 32 * textureOffsetIndex);
	const __m128 atlasRect = _mm_loadu_ps(reinterpret_cast<const float*>(atlasRecord));

	const __m128 reciprocalScale = _mm_rcp_ps(*viewportScale);
	const __m128 reciprocalScaleError = _mm_sub_ps(xmmword_5F3E90, _mm_mul_ps(reciprocalScale, *viewportScale));
	const __m128 refinedReciprocalScale = _mm_add_ps(
		_mm_mul_ps(_mm_add_ps(_mm_mul_ps(reciprocalScaleError, reciprocalScaleError), reciprocalScaleError), reciprocalScale),
		reciprocalScale);

	const __m128 canvasSize = _mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(&ruiData->canvasWidth)));

	const __m128 flippedAtlasRect = _mm_xor_ps(atlasRect, xmmword_5F3E70);
	const __m128 atlasMin = _mm_and_ps(atlasRect, xmmword_12A14650);
	const __m128 atlasExtent = _mm_add_ps(RUI_SHUFFLE_PS(atlasRect, 238), flippedAtlasRect);
	const __m128 inverseAtlasExtent = _mm_sub_ps(xmmword_5F3E90, atlasExtent);

	const __m128 projectedCanvas =
		_mm_mul_ps(_mm_mul_ps(_mm_unpacklo_ps(canvasSize, canvasSize), RUI_SHUFFLE_PS(transform[0], 216)), refinedReciprocalScale);
	const __m128 projectedCanvasSq = _mm_mul_ps(projectedCanvas, projectedCanvas);
	const __m128 edgeSize = _mm_max_ps(
		_mm_mul_ps(
			_mm_sqrt_ps(_mm_add_ps(RUI_SHUFFLE_PS(projectedCanvasSq, 78), projectedCanvasSq)),
			_mm_castpd_ps(_mm_loaddup_pd(atlasRecord + 2))),
		_mm_castpd_ps(_mm_loaddup_pd(atlasRecord + 3)));

	const __m128 availableExtent = _mm_sub_ps(edgeSize, atlasExtent);
	const __m128 availableExtentReciprocal = _mm_rcp_ps(availableExtent);
	const __m128 availableExtentError = _mm_sub_ps(xmmword_5F3E90, _mm_mul_ps(availableExtentReciprocal, availableExtent));
	const __m128 refinedAvailableExtentReciprocal = _mm_add_ps(
		_mm_mul_ps(_mm_add_ps(_mm_mul_ps(availableExtentError, availableExtentError), availableExtentError), availableExtentReciprocal),
		availableExtentReciprocal);

	const __m128 edgeScale = _mm_movelh_ps(edgeSize, xmmword_5F3E90);
	const __m128 outerYDir = _mm_mul_ps(edgeScale, baseUv->yDir);
	const __m128 outerXDir = _mm_mul_ps(edgeScale, baseUv->xDir);
	const __m128 edgeYDir = _mm_add_ps(_mm_sub_ps(xmmword_5F3E90, edgeScale), outerYDir);
	const __m128 outerBase = _mm_mul_ps(edgeScale, baseUv->base);

	const __m128 innerScale =
		_mm_movelh_ps(_mm_mul_ps(_mm_mul_ps(inverseAtlasExtent, edgeSize), refinedAvailableExtentReciprocal), xmmword_5F3E90);
	const __m128 innerBase = _mm_mul_ps(innerScale, baseUv->base);
	const __m128 innerXDir = _mm_mul_ps(innerScale, baseUv->xDir);
	const __m128 innerYDir = _mm_add_ps(
		_mm_sub_ps(atlasMin, _mm_mul_ps(_mm_mul_ps(atlasMin, inverseAtlasExtent), refinedAvailableExtentReciprocal)),
		_mm_mul_ps(innerScale, baseUv->yDir));

	const __m128 atlasStep = _mm_mul_ps(
		_mm_sub_ps(
			_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_PS(refinedAvailableExtentReciprocal, 238), flippedAtlasRect), xmmword_5F4600), *uvBias),
		refinedReciprocalScale);
	const __m128 atlasStepShuffled = RUI_SHUFFLE_PS(atlasStep, 216);
	const __m128 atlasUvBase = RUI_SHUFFLE_I32_AS_PS(*atlasUv, 216);
	const __m128 uvHigh = _mm_unpackhi_ps(atlasUvBase, atlasStepShuffled);
	const __m128 uvLow = _mm_unpacklo_ps(atlasUvBase, atlasStepShuffled);

	int clipMaskX = _mm_movemask_ps(_mm_cmple_ps(*clipThreshold, _mm_xor_ps(atlasStep, xmmword_5F3E20)));
	int clipMaskY = _mm_movemask_ps(_mm_cmple_ps(*clipThreshold, _mm_xor_ps(RUI_SHUFFLE_PS(atlasStep, 78), xmmword_5F3E20)));

	auto blendByMask = [](__m128 keep, __m128 replace, __m128 mask)
	{ return _mm_or_ps(_mm_andnot_ps(mask, keep), _mm_and_ps(replace, mask)); };

	const int clipMaskY_5 = clipMaskY & 5;
	const int clipMaskY_A = clipMaskY & 0xA;

	if ((clipMaskX & 3) == 0 && !drawPiece(
									RUI_SHUFFLE_PS(uvHigh, 20),
									RUI_SHUFFLE_PS(uvLow, 80),
									&xmmword_5F4740[correctionMask & 5],
									false,
									outerBase,
									outerXDir,
									outerYDir))
	{
		return;
	}

	if ((clipMaskY_5 | (clipMaskX & 2)) || drawPiece(
											   RUI_SHUFFLE_PS(uvHigh, 20),
											   RUI_SHUFFLE_PS(uvLow, 245),
											   &xmmword_5F4740[correctionMask & 4],
											   true,
											   blendByMask(outerBase, innerBase, xmmword_12A146D0),
											   blendByMask(outerXDir, innerXDir, xmmword_12A146D0),
											   blendByMask(outerYDir, innerYDir, xmmword_12A146D0)))
	{
		// Original LABEL_29 case: if ((nameHasha & 6) == 0) draw this piece.
		if ((clipMaskX & 6) == 0)
		{
			if (!drawPiece(
					RUI_SHUFFLE_PS(uvHigh, 20),
					RUI_SHUFFLE_PS(uvLow, 175),
					&xmmword_5F4740[correctionMask & 6],
					false,
					outerBase,
					outerXDir,
					blendByMask(outerYDir, edgeYDir, xmmword_12A146D0)))
			{
				return;
			}
		}

		if ((clipMaskY_A | (clipMaskX & 1)) || drawPiece(
												   RUI_SHUFFLE_PS(uvHigh, 125),
												   RUI_SHUFFLE_PS(uvLow, 80),
												   &xmmword_5F4740[correctionMask & 1],
												   false,
												   blendByMask(outerBase, innerBase, xmmword_12A146A0),
												   blendByMask(outerXDir, innerXDir, xmmword_12A146A0),
												   blendByMask(outerYDir, innerYDir, xmmword_12A146A0)))
		{
			if (clipMaskY ||
				drawPiece(RUI_SHUFFLE_PS(uvHigh, 125), RUI_SHUFFLE_PS(uvLow, 245), nullptr, true, innerBase, innerXDir, innerYDir))
			{
				if ((clipMaskY_A | (clipMaskX & 4)) || drawPiece(
														   RUI_SHUFFLE_PS(uvHigh, 125),
														   RUI_SHUFFLE_PS(uvLow, 175),
														   &xmmword_5F4740[correctionMask & 2],
														   false,
														   blendByMask(outerBase, innerBase, xmmword_12A146A0),
														   blendByMask(outerXDir, innerXDir, xmmword_12A146A0),
														   blendByMask(edgeYDir, innerYDir, xmmword_12A146A0)))
				{
					if ((clipMaskX & 9) == 0 && !drawPiece(
													RUI_SHUFFLE_PS(uvHigh, 235),
													RUI_SHUFFLE_PS(uvLow, 80),
													&xmmword_5F4740[correctionMask & 9],
													true,
													outerBase,
													outerXDir,
													blendByMask(outerYDir, edgeYDir, xmmword_12A146A0)))
					{
						return;
					}

					if ((clipMaskY_5 | (clipMaskX & 8)) || drawPiece(
															   RUI_SHUFFLE_PS(uvHigh, 235),
															   RUI_SHUFFLE_PS(uvLow, 245),
															   &xmmword_5F4740[correctionMask & 8],
															   false,
															   blendByMask(outerBase, innerBase, xmmword_12A146D0),
															   blendByMask(outerXDir, innerXDir, xmmword_12A146D0),
															   blendByMask(edgeYDir, innerYDir, xmmword_12A146D0)))
					{
						if ((clipMaskX & 0xC) == 0)
						{
							drawPiece(
								RUI_SHUFFLE_PS(uvHigh, 235),
								RUI_SHUFFLE_PS(uvLow, 175),
								&xmmword_5F4740[correctionMask & 0xA],
								true,
								outerBase,
								outerXDir,
								edgeYDir);
						}
					}
				}
			}
		}
	}
}



DECLARE_HOOK(sub_F9B80, engine.dll + 0xF9B80, [](auto& hook,globals* g,
	ruiDataStruct* ds,
	struct_v3* drawState,
	ruiBaseUvStruct* baseUv,
	__m128* a5, // [0]=transform row, [1]=translate
	int a6, // winding (2 ⇒ swap halves)
	std::int64_t nameHash,
	__m128i* a8, // base UV pair
	__m128* a9, // visibility threshold
	__m128* a10, // UV offset
	__m128* a11)
	{
		//return hook.Original(g, ds, drawState, baseUv, a5, a6, nameHash, a8, a9, a10, a11);
		return sub_F9B80_rebuild(g, ds, drawState, baseUv, a5, a6, nameHash, a8, a9, a10, a11);
	});




void __fastcall ruiRenderAssetElipse_F7A80_rebuild(
	globals** globals,
	ruiDataStruct* ruiData,
	renderJobsStruct_2* assetElement,
	struct_v3* batch)
{
	(void)globals;

	const int styleDescriptorOffset = assetElement->uint8_18;
	styleDescriptorsStruct* styleOffsets = &ruiData->header->styleDescriptors[styleDescriptorOffset];

	auto dataFloat = [&](int offset) -> float
	{
		return *reinterpret_cast<const float*>(&ruiData->dataValues[offset]);
	};

	auto dataInt = [&](int offset) -> int
	{
		return *reinterpret_cast<const int*>(&ruiData->dataValues[offset]);
	};

	auto dataScalar = [&](int offset) -> __m128
	{
		return _mm_set_ss(dataFloat(offset));
	};

	if (dataFloat(styleOffsets->color_alpha) <= 0.0f)
		return;

	const __int64 transformIndex = assetElement->transformIndex;
	testStruct* transform = &ruiData->v1->m128_3A80[transformIndex];
	__m128 transformRow0 = _mm_castsi128_ps(transform->m128_0);
	__m128 transformRow1 = _mm_castsi128_ps(transform->m128_10);
	const __m128 determinant = _mm_sub_ps(
		_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 255), RUI_SHUFFLE_PS(transformRow0, 0)),
		_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, 170), RUI_SHUFFLE_PS(transformRow0, 85)));
	if (_mm_movemask_ps(_mm_cmpeq_ps(determinant, _mm_setzero_ps())) != 0)
		return;

	const int orientationMask = _mm_movemask_ps(determinant) & 2;
	const __m128 inverseBasis = _mm_div_ps(_mm_xor_ps(RUI_SHUFFLE_PS(transformRow0, 39), xmmword_5F3E50), determinant);
	const __m128 transformedOrigin = _mm_mul_ps(_mm_xor_ps(inverseBasis, xmmword_5F3DD0), RUI_SHUFFLE_PS(transformRow1, 216));
	const __m128 originSum = _mm_add_ps(RUI_SHUFFLE_PS(transformedOrigin, 78), transformedOrigin);

	const int assetDescriptorIndex = dataInt(assetElement->uint16_4);
	if (assetDescriptorIndex == -1)
		return;

	ruiBaseUvStruct uv;
	uv.assetIndex2 = -1;
	assetIndexData* assetDescriptor = &unk_12A2E508[8LL * assetDescriptorIndex];
	const uint16_t assetIndex = assetDescriptor->assetIndex;
	const uint8_t atlasIndex = assetDescriptor->atlasIndex;
	const uint8_t assetFlags = assetDescriptor->flags;
	spdlog::info("Asset descriptor index: {}, asset index: {}, atlas index: {}, asset flags: {}", assetDescriptorIndex, assetIndex, atlasIndex, assetFlags);

	const __int16 combinedFlags = assetElement->word_16 | assetFlags;
	uv.assetIndex = assetIndex;
	uv.flags = combinedFlags;
	uv.styleDescriptorIndex = static_cast<__int16>(styleDescriptorOffset + batch->styleDescriptorIndex);

	const __m128 ellipseU0 = dataScalar(assetElement->uint16_6);
	const __m128 ellipseV0 = dataScalar(assetElement->uint16_8);
	const __m128 ellipseU1 = dataScalar(assetElement->uint16_A);
	const __m128 ellipseV1 = dataScalar(assetElement->uint16_C);
	const float insetX = dataFloat(assetElement->uint16_E);
	const float insetY = dataFloat(assetElement->uint16_10);
	const __m128 ellipseWidth = dataScalar(assetElement->uint16_12);
	const __m128 ellipseHeight = dataScalar(assetElement->uint16_14);
	const float stretchX = dataFloat(styleOffsets->stretchXOffset);

	const __m128 transformSize = ruiData->v1->transformSizes[transformIndex];
	const float transformWidth = transformSize.m128_f32[0];
	const float transformHeight = transformSize.m128_f32[2];
	if ((transformWidth < transformHeight ? transformWidth : transformHeight) <= 0.0f)
		return;

	uiImageAtlas* imageAtlas = &rpakUIMGAtlases[atlasIndex];
	if (!sub_FC0C0(batch, imageAtlas))
		return;
	
	const uint8_t* atlasRecordBase = reinterpret_cast<const uint8_t*>(imageAtlas->textureOffsets);
	const uint8_t* atlasRecord = atlasRecordBase + 32ULL * assetIndex;

	const __m128 ellipseMax = _mm_movelh_ps(_mm_unpacklo_ps(ellipseWidth, ellipseHeight), _mm_unpacklo_ps(ellipseWidth, ellipseHeight));
	const __m128 ellipseMin = _mm_movelh_ps(_mm_unpacklo_ps(_mm_set_ss(insetX), _mm_set_ss(insetY)), _mm_unpacklo_ps(_mm_set_ss(insetX), _mm_set_ss(insetY)));
	const __m128 ellipseExtent = _mm_max_ps(_mm_sub_ps(ellipseMax, ellipseMin), xmmword_5F3F30);

	
	const __m128 axisMask = xmmword_12A4E830[16 * ((static_cast<__int64>(combinedFlags) >> 4) & 3)];
	const float scaledX = ((transformHeight * stretchX) * (ellipseWidth.m128_f32[0] - insetX)) / transformWidth;
	const float scaledY = (ellipseHeight.m128_f32[0] - insetY) * stretchX;
	const __m128 scaledOffset = _mm_movelh_ps(_mm_unpacklo_ps(_mm_set_ss(scaledX), _mm_set_ss(scaledY)), _mm_unpacklo_ps(_mm_set_ss(scaledX), _mm_set_ss(scaledY)));

	const __m128 atlasRect = _mm_loadu_ps(reinterpret_cast<const float*>(atlasRecord));
	const __m128 normalizedRect = _mm_div_ps(
		_mm_add_ps(
			_mm_sub_ps(atlasRect, _mm_xor_ps(_mm_and_ps(_mm_min_ps(ellipseMin, ellipseMax), axisMask), xmmword_5F3E20)),
			scaledOffset),
		_mm_or_ps(
			_mm_and_ps(_mm_andnot_ps(xmmword_5F3DD0, ellipseExtent), axisMask),
			_mm_andnot_ps(axisMask, xmmword_5F3E90)));
	if (_mm_movemask_ps(_mm_cmplt_ps(normalizedRect, xmmword_5F3F60)) != 0)
		return;

	const __m128 clampedUv = _mm_xor_ps(
		_mm_min_ps(
			_mm_movelh_ps(
				_mm_xor_ps(_mm_unpacklo_ps(ellipseU0, ellipseV0), xmmword_5F3DD0),
				_mm_unpacklo_ps(ellipseU1, ellipseV1)),
			normalizedRect),
		xmmword_5F3E20);
	if (_mm_movemask_ps(_mm_cmple_ps(RUI_SHUFFLE_PS(clampedUv, 238), RUI_SHUFFLE_PS(clampedUv, 68))) != 0)
		return;

	ruiDrawTriangle tri;
	memset(&uv.base2, 0, 48);
	tri.size = 4;
	tri.size_ = 4;

	const __m128 atlasScale = _mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(atlasRecord + 24)));
	const __m128 halfBasis = _mm_mul_ps(inverseBasis, xmmword_5F3E80);
	const __m128 scaledBasis = _mm_mul_ps(_mm_mul_ps(inverseBasis, ellipseExtent), atlasScale);
	const __m128 atlasBase = _mm_add_ps(
		_mm_mul_ps(_mm_add_ps(_mm_mul_ps(originSum, ellipseExtent), ellipseMin), atlasScale),
		_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(atlasRecord + 16))));

	uv.base = _mm_movelh_ps(scaledBasis, halfBasis);
	uv.xDir = _mm_movehl_ps(halfBasis, scaledBasis);
	uv.yDir = _mm_movelh_ps(atlasBase, _mm_sub_ps(_mm_mul_ps(originSum, xmmword_5F3E80), xmmword_5F3E90));

	const __m128 u = RUI_SHUFFLE_PS(clampedUv, 125);
	const __m128 v = RUI_SHUFFLE_PS(clampedUv, 160);
	const __m128i transformRows0 = _mm_load_si128(reinterpret_cast<const __m128i*>(&transform->m128_0));
	const __m128 projectedX = _mm_add_ps(
		_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_I32_AS_PS(transformRows0, 170), u), _mm_mul_ps(RUI_SHUFFLE_I32_AS_PS(transformRows0, 0), v)),
		RUI_SHUFFLE_PS(transformRow1, 0));
	const __m128 projectedY = _mm_add_ps(
		_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_I32_AS_PS(transformRows0, 255), u), _mm_mul_ps(RUI_SHUFFLE_I32_AS_PS(transformRows0, 85), v)),
		RUI_SHUFFLE_PS(transformRow1, 85));

	__m128 quad0 = _mm_unpacklo_ps(projectedX, projectedY);
	__m128 quad1 = _mm_unpackhi_ps(projectedX, projectedY);
	if (orientationMask == 2)
	{
		quad0 = RUI_SHUFFLE_PS(quad0, 78);
		quad1 = RUI_SHUFFLE_PS(quad1, 78);
	}

	_mm_storeu_ps(&tri.vert[0][0], quad0);
	_mm_storeu_ps(&tri.vert[1][0], quad1);

	ruiDrawInfoDataWeapon* drawInfo = ruiData->pvoid_38;
	ruiDrawInfo_5f4560[drawInfo->type](
		drawInfo,
		&uv,
		&tri,
		batch);
}


void __fastcall ruiRenderAsset_F72F0_rebuild(
	globals** globals,
	ruiDataStruct* ruiData,
	AssetRenderOffsets* assetElement,
	struct_v3* batch)
{
	const __int16 styleIndex = assetElement->styleIndex;
	styleDescriptorsStruct* styleOffsets = &ruiData->header->styleDescriptors[styleIndex];

	auto dataFloat = [&](int offset) -> float
	{
		return *reinterpret_cast<const float*>(&ruiData->dataValues[offset]);
	};

	auto dataInt = [&](int offset) -> int
	{
		return *reinterpret_cast<const int*>(&ruiData->dataValues[offset]);
	};

	auto dataScalar = [&](int offset) -> __m128
	{
		return _mm_set_ss(dataFloat(offset));
	};

	if (dataFloat(styleOffsets->color_alpha) <= 0.0f)
		return;

	testStruct* transform = &ruiData->v1->m128_3A80[assetElement->transformIndex];
	const __m128 transformRow0 = _mm_castsi128_ps(transform->m128_0);
	const __m128 transformRow1 = _mm_castsi128_ps(transform->m128_10);
	const __m128 determinant = _mm_sub_ps(
		_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(3, 3, 3, 3)), RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(0, 0, 0, 0))),
		_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(2, 2, 2, 2)), RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(1, 1, 1, 1))));
	if (_mm_movemask_ps(_mm_cmpeq_ps(_mm_setzero_ps(), determinant)) != 0)
		return;

	const __m128 inverseBasis = _mm_div_ps(_mm_xor_ps(RUI_SHUFFLE_PS(transformRow0, 39), xmmword_5F3E50), determinant);
	const int orientation = _mm_movemask_ps(determinant) & 2;
	const __m128 transformedOrigin = _mm_mul_ps(_mm_xor_ps(inverseBasis, xmmword_5F3DD0), RUI_SHUFFLE_PS(transformRow1, 216));
	const __m128 originSum = _mm_add_ps(RUI_SHUFFLE_PS(transformedOrigin, 78), transformedOrigin);

	const int primaryAssetDescriptorIndex = dataInt(assetElement->assetIndex_0);
	if (primaryAssetDescriptorIndex == -1)
		return;

	const auto* primaryAsset = &unk_12A2E508[primaryAssetDescriptorIndex];
	const __int64 primaryNameHash = reinterpret_cast<__int64>(primaryAsset);
	const uint8_t atlasIndex = primaryAsset->atlasIndex;
	const __int16 assetIndex = primaryAsset->assetIndex;
	__int16 secondaryAssetIndex = -1;
	__int16 flags = assetElement->flags | static_cast<uint8_t>(primaryAsset->flags);

	const int secondaryAssetDescriptorIndex = dataInt(assetElement->assetIndex_1);
	if (secondaryAssetDescriptorIndex != -1)
	{
		const auto* secondaryAsset = &unk_12A2E508[secondaryAssetDescriptorIndex];
		if (atlasIndex != secondaryAsset->atlasIndex)
			return;

		secondaryAssetIndex = secondaryAsset->assetIndex;
		flags |= static_cast<__int16>(4 * static_cast<uint8_t>(secondaryAsset->flags));
	}

	uiImageAtlas* imageAtlas = reinterpret_cast<uiImageAtlas*>(
		reinterpret_cast<uint8_t*>(rpakUIMGAtlases) + 72ULL * atlasIndex);
	const uint8_t* textureOffsets = reinterpret_cast<const uint8_t*>(imageAtlas->textureOffsets);
	const uint8_t* primaryTextureRecord = textureOffsets + 32ULL * static_cast<uint16_t>(assetIndex);

	const __m128 mins = _mm_unpacklo_ps(dataScalar(assetElement->mins.x), dataScalar(assetElement->mins.y));
	const __m128 maxs = _mm_unpacklo_ps(dataScalar(assetElement->maxs.x), dataScalar(assetElement->maxs.y));
	const __m128 texMinsLo = _mm_unpacklo_ps(dataScalar(assetElement->texMins.x), dataScalar(assetElement->texMins.y));
	__m128 texMins = _mm_movelh_ps(texMinsLo, texMinsLo);
	const __m128 texMaxsLo = _mm_unpacklo_ps(dataScalar(assetElement->texMaxs.x), dataScalar(assetElement->texMaxs.y));
	__m128 texMaxs = _mm_movelh_ps(texMaxsLo, texMaxsLo);
	__m128 geometryBounds = _mm_movelh_ps(_mm_xor_ps(xmmword_5F3DD0, mins), maxs);
	__m128 textureExtent = _mm_sub_ps(texMaxs, texMins);

	const __m128 axisMask = xmmword_12A4E830[((static_cast<__int64>(flags) >> 4) & 3)];
	const __m128 primaryTextureOffset = _mm_loadu_ps(reinterpret_cast<const float*>(primaryTextureRecord));
	const __m128 normalizedTextureOffset = _mm_div_ps(
		_mm_sub_ps(primaryTextureOffset, _mm_xor_ps(_mm_and_ps(_mm_min_ps(texMins, texMaxs), axisMask), xmmword_5F3E20)),
		_mm_or_ps(
			_mm_and_ps(_mm_andnot_ps(xmmword_5F3DD0, textureExtent), axisMask),
			_mm_andnot_ps(axisMask, xmmword_5F3E90)));
	if (_mm_movemask_ps(_mm_cmplt_ps(normalizedTextureOffset, xmmword_5F3F60)) != 0)
		return;

	__m128i atlasUv = _mm_castps_si128(_mm_xor_ps(_mm_min_ps(geometryBounds, normalizedTextureOffset), xmmword_5F3E20));
	if (_mm_movemask_ps(_mm_cmple_ps(RUI_SHUFFLE_I32_AS_PS(atlasUv, 238), RUI_SHUFFLE_I32_AS_PS(atlasUv, 68))) != 0)
		return;

	ruiBaseUvStruct baseUv;
	baseUv.assetIndex = assetIndex;
	baseUv.assetIndex2 = secondaryAssetIndex;
	baseUv.flags = flags;
	baseUv.styleDescriptorIndex = static_cast<__int16>(styleIndex + batch->styleDescriptorIndex);

	const __m128 texturePosition = _mm_add_ps(_mm_mul_ps(originSum, textureExtent), texMins);
	const __m128 textureScale = _mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(primaryTextureRecord + 24)));
	const __m128 basisExtent = _mm_mul_ps(inverseBasis, textureExtent);
	const __m128 primaryBase = _mm_add_ps(
		_mm_mul_ps(textureScale, texturePosition),
		_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(primaryTextureRecord + 16))));
	const __m128 primaryBasis = _mm_mul_ps(textureScale, basisExtent);

	__m128 maskBase = _mm_setzero_ps();
	__m128 maskBasis = _mm_setzero_ps();
	if (secondaryAssetIndex == -1)
	{
		const __m128 minXy = RUI_SHUFFLE_PS(mins, 68);
		const __m128 spanXy = _mm_max_ps(xmmword_5F3F30, _mm_sub_ps(RUI_SHUFFLE_PS(maxs, 68), minXy));
		const __m128 spanReciprocal = _mm_rcp_ps(spanXy);
		const __m128 spanError = _mm_sub_ps(xmmword_5F3E90, _mm_mul_ps(spanReciprocal, spanXy));
		const __m128 refinedSpanReciprocal = _mm_add_ps(
			_mm_mul_ps(_mm_add_ps(_mm_mul_ps(spanError, spanError), spanError), spanReciprocal),
			spanReciprocal);

		maskBasis = _mm_mul_ps(inverseBasis, refinedSpanReciprocal);
		maskBase = _mm_mul_ps(_mm_sub_ps(originSum, minXy), refinedSpanReciprocal);
	}
	else
	{
		const uint8_t* secondaryTextureRecord = textureOffsets + 32ULL * static_cast<uint16_t>(secondaryAssetIndex);
		const __m128 maskRotation = dataScalar(assetElement->maskRotation);
		const __m128 maskCenter = _mm_unpacklo_ps(dataScalar(assetElement->maskCenter.x), dataScalar(assetElement->maskCenter.y));
		const __m128 maskSize = _mm_unpacklo_ps(dataScalar(assetElement->maskSize.x), dataScalar(assetElement->maskSize.y));
		const __m128 maskTranslate = _mm_unpacklo_ps(dataScalar(assetElement->maskTranslate.x), dataScalar(assetElement->maskTranslate.y));

		const __m128 rotationTurns = _mm_mul_ps(
			_mm_add_ps(_mm_xor_ps(RUI_SHUFFLE_PS(maskRotation, 0), xmmword_5F3E00), xmmword_5F45D0),
			xmmword_5F34C0);
		const __m128i rotationQuadrant = _mm_cvtps_epi32(rotationTurns);
		const __m128 quadrantIsEven = _mm_castsi128_ps(_mm_cmpeq_epi32(
			_mm_and_si128(_mm_castps_si128(xmmword_5F3460), rotationQuadrant),
			_mm_setzero_si128()));
		const __m128 rotationFraction = _mm_sub_ps(rotationTurns, _mm_cvtepi32_ps(rotationQuadrant));
		const __m128 fractionSq = _mm_mul_ps(rotationFraction, rotationFraction);

		const __m128 cosApprox = _mm_sub_ps(
			xmmword_5F3E90,
			_mm_sub_ps(
				fractionSq,
				_mm_mul_ps(
					_mm_add_ps(
						_mm_mul_ps(
							_mm_add_ps(
								_mm_mul_ps(
									_mm_add_ps(_mm_mul_ps(xmmword_5F3470, fractionSq), xmmword_5F34F0),
									fractionSq),
								xmmword_5F34A0),
							fractionSq),
						xmmword_5F3500),
					fractionSq)));
		const __m128 sinApprox = _mm_add_ps(
			_mm_mul_ps(
				_mm_add_ps(
					_mm_mul_ps(
						_mm_add_ps(
							_mm_mul_ps(
								_mm_add_ps(_mm_mul_ps(xmmword_5F34E0, fractionSq), xmmword_5F3490),
								fractionSq),
							xmmword_5F3510),
						fractionSq),
					xmmword_5F34B0),
				rotationFraction),
			rotationFraction);
		const __m128 quadrantSign = _mm_castsi128_ps(_mm_slli_epi32(
			_mm_and_si128(_mm_castps_si128(xmmword_5CB2A0), rotationQuadrant),
			0x1E));
		const __m128 rotationBasis = _mm_mul_ps(
			_mm_xor_ps(_mm_or_ps(_mm_andnot_ps(quadrantIsEven, cosApprox), _mm_and_ps(sinApprox, quadrantIsEven)), quadrantSign),
			_mm_movelh_ps(maskSize, maskSize));

		const __m128 maskTextureScale = _mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(secondaryTextureRecord + 24)));
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
		maskBase = _mm_add_ps(maskTexturePosition, _mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(secondaryTextureRecord + 16))));
	}

	const __m128 zero = _mm_setzero_ps();
	baseUv.base = _mm_movelh_ps(primaryBasis, maskBasis);
	baseUv.yDir2 = zero;
	baseUv.yDir = _mm_movelh_ps(primaryBase, maskBase);
	baseUv.xDir = _mm_movehl_ps(maskBasis, primaryBasis);
	baseUv.base2 = zero;
	baseUv.xDir2 = zero;

	sub_F9B80_rebuild(
		*globals,
		ruiData,
		batch,
		&baseUv,
		reinterpret_cast<__m128*>(transform),
		orientation,
		primaryNameHash,
		&atlasUv,
		&geometryBounds,
		&texMins,
		&textureExtent);
};

DECLARE_HOOK(renderAsset_F72F0, engine.dll + 0xF72F0, [](auto& hook, globals** a1, ruiDataStruct* a2, AssetRenderOffsets* a3, struct_v3* a4)
	{
		ruiRenderAsset_F72F0_rebuild(a1, a2, a3, a4);
		//hook.Original(a1, a2, a3, a4);
	});

DECLARE_HOOK(
	ruiUnknown9Func_2,
	engine.dll + 0xF7A80,
	[](auto& hook, globals** a1, ruiDataStruct* a2, renderJobsStruct_2* a3, struct_v3* a4)
	{
		//ruiRenderAssetElipse_F7A80_rebuild(a1, a2, a3, a4);
		hook.Original(a1, a2, a3, a4);
	});


void __fastcall renderText_F5840_rebuild(
	ruiRenderList* renderList,
	ruiDataStruct* ruiData,
	AssetRenderOffsets* textElement,
	struct_v3* batch)
{
	struct_v1* runtime = ruiData->v1;
	const __int64 transformIndex = textElement->transformIndex;
	const __m128 transformSize = runtime->transformSizes[transformIndex];
	if (_mm_movemask_ps(_mm_cmpeq_ps(_mm_setzero_ps(), transformSize)) != 0)
		return;

	testStruct* transform = &runtime->m128_3A80[transformIndex];
	__m128* transformRows = reinterpret_cast<__m128*>(transform);
	__m128 transformRow0 = _mm_castsi128_ps(transform->m128_0);
	__m128 transformRow1 = _mm_castsi128_ps(transform->m128_10);
	const __m128 determinant = _mm_sub_ps(
		_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(3, 3, 3, 3)), RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(0, 0, 0, 0))),
		_mm_mul_ps(RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(2, 2, 2, 2)), RUI_SHUFFLE_PS(transformRow0, _MM_SHUFFLE(1, 1, 1, 1))));
	if (_mm_movemask_ps(_mm_cmpeq_ps(determinant, _mm_setzero_ps())) != 0)
		return;

	const __m128 inverseBasis = _mm_div_ps(_mm_xor_ps(RUI_SHUFFLE_PS(transformRow0, 39), xmmword_5F3E50), determinant);
	const int orientation = _mm_movemask_ps(determinant) & 2;
	const __m128 transformedOrigin = _mm_mul_ps(_mm_xor_ps(inverseBasis, xmmword_5F3DD0), RUI_SHUFFLE_PS(transformRow1, 216));
	const __m128 originSum = _mm_add_ps(RUI_SHUFFLE_PS(transformedOrigin, 78), transformedOrigin);

	ruiHeader* header = ruiData->header;
	styleDescriptorsStruct* descriptors = header->styleDescriptors;
	styleDescriptorsStruct* textStyles[4] = {
		&descriptors[LOBYTE(textElement->assetIndex_0)],
		&descriptors[HIBYTE(textElement->assetIndex_0)],
		&descriptors[LOBYTE(textElement->assetIndex_1)],
		&descriptors[HIBYTE(textElement->assetIndex_1)],
	};

	auto dataFloat = [&](int offset) -> float
	{
		return *reinterpret_cast<const float*>(&ruiData->dataValues[offset]);
	};

	auto fontFromIndex = [](__int64 fontIndex) -> rpakFont*
	{
		return rpakFontPointers[fontIndex];
	};

	rpakFont* fonts[4] = {
		fontFromIndex(textStyles[0]->fontIndex),
		fontFromIndex(textStyles[1]->fontIndex),
		fontFromIndex(textStyles[2]->fontIndex),
		fontFromIndex(textStyles[3]->fontIndex),
	};

	const __m128 transformSizeReciprocal = _mm_rcp_ps(transformSize);
	const __m128 transformSizeError = _mm_sub_ps(xmmword_5F3E90, _mm_mul_ps(transformSizeReciprocal, transformSize));
	__m128 refinedTransformSizeReciprocal = _mm_add_ps(
		_mm_mul_ps(_mm_add_ps(_mm_mul_ps(transformSizeError, transformSizeError), transformSizeError), transformSizeReciprocal),
		transformSizeReciprocal);

	auto maxStyleTextWidth = [&]() -> float
	{
		float maxWidth = 0.0f;
		for (int i = 0; i != 4; ++i)
		{
			const float width = dataFloat(textStyles[i]->textSize) * *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(fonts[i]) + 36)
				- dataFloat(textStyles[i]->uint16_32);
			maxWidth = fmaxf(maxWidth, width);
		}
		return maxWidth;
	};

	float maxTextWidth = RUI_SHUFFLE_PS(refinedTransformSizeReciprocal, 255).m128_f32[0] * maxStyleTextWidth();
	const float textOriginX = RUI_SHUFFLE_PS(refinedTransformSizeReciprocal, 255).m128_f32[0] * dataFloat(textElement->texMins.x);
	const float lineAdvanceScale = refinedTransformSizeReciprocal.m128_f32[0] * dataFloat(textElement->maxs.y);

	const unsigned __int64 renderJobIndex =
		(static_cast<unsigned int>(reinterpret_cast<uintptr_t>(textElement) - reinterpret_cast<uintptr_t>(header->renderJobs))) >> 4;

	// Inline image spans are stored in runtime->gap_28D0 and are rendered before the text glyph pass.
	const uint8_t inlineImageBegin = runtime->unk2[renderJobIndex].byte_6;
	const uint8_t inlineImageCount = runtime->unk2[renderJobIndex].byte_7;
	if (inlineImageCount)
	{
		const __m128 scaledTransformSize = RUI_SHUFFLE_PS(refinedTransformSizeReciprocal, 216);
		const __m128 maxTextWidthVec = RUI_SHUFFLE_PS(_mm_set_ss(maxTextWidth), 17);
		__m128 clipUnit = xmmword_5F4600;

		for (uint32_t inlineImageIndex = inlineImageBegin; inlineImageIndex != inlineImageBegin + inlineImageCount; ++inlineImageIndex)
		{
			const uint16_t assetDescriptorIndex = *reinterpret_cast<uint16_t*>(&runtime->gap_28D0[20 * inlineImageIndex]);
			const auto* assetDescriptor = &unk_12A2E508[assetDescriptorIndex];
			const __int16 assetIndex = assetDescriptor->assetIndex;
			const __int64 nameHash = reinterpret_cast<__int64>(assetDescriptor);

			uiImageAtlas* imageAtlas = reinterpret_cast<uiImageAtlas*>(
				reinterpret_cast<uint8_t*>(rpakUIMGAtlases) + 72ULL * static_cast<uint8_t>(assetDescriptor->atlasIndex));
			const uint8_t* textureRecord = reinterpret_cast<const uint8_t*>(imageAtlas->textureOffsets) + 32ULL * static_cast<uint16_t>(assetIndex);

			const __m128 imageMin = _mm_mul_ps(
				_mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(&runtime->gap_28D0[20 * inlineImageIndex + 4]))),
				scaledTransformSize);
			const __m128 imageExtent = _mm_sub_ps(
				_mm_mul_ps(
					_mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(&runtime->gap_28D0[20 * inlineImageIndex + 12]))),
					scaledTransformSize),
				imageMin);
			const __m128 imageBase = _mm_add_ps(maxTextWidthVec, imageMin);
			const __m128 imageExtentReciprocal = _mm_rcp_ps(imageExtent);
			const __m128 imageExtentError = _mm_sub_ps(xmmword_5F3E90, _mm_mul_ps(imageExtentReciprocal, imageExtent));
			const __m128 refinedImageExtentReciprocal = _mm_add_ps(
				_mm_mul_ps(_mm_add_ps(_mm_mul_ps(imageExtentError, imageExtentError), imageExtentError), imageExtentReciprocal),
				imageExtentReciprocal);

			const __m128 textureOffset = _mm_loadu_ps(reinterpret_cast<const float*>(textureRecord));
			const __m128 atlasUv = _mm_add_ps(_mm_mul_ps(_mm_xor_ps(textureOffset, xmmword_5F3E20), imageExtent), imageBase);
			const __m128 atlasScale = _mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(textureRecord + 24)));
			const __m128 inlineMaskBase = _mm_xor_ps(_mm_mul_ps(refinedImageExtentReciprocal, imageBase), xmmword_5F3DD0);
			const __m128 inlineMaskTransform = _mm_mul_ps(_mm_mul_ps(_mm_sub_ps(originSum, imageBase), refinedImageExtentReciprocal), atlasScale);
			const __m128 inlineMaskBasis = _mm_mul_ps(_mm_mul_ps(inverseBasis, refinedImageExtentReciprocal), atlasScale);

			ruiBaseUvStruct imageUv;
			imageUv.assetIndex = assetIndex;
			imageUv.assetIndex2 = -1;
			imageUv.styleDescriptorIndex = static_cast<__int16>(
				batch->styleDescriptorIndex + *(reinterpret_cast<uint8_t*>(&textElement->assetIndex_0)
					+ *reinterpret_cast<uint16_t*>(&runtime->gap_28D0[20 * inlineImageIndex + 2])));
			imageUv.flags = 7936;
			imageUv.yDir = _mm_add_ps(inlineMaskTransform, _mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(textureRecord + 16))));
			imageUv.base = RUI_SHUFFLE_PS(_mm_castsi128_ps(_mm_castps_si128(inlineMaskBasis)), 68);
			imageUv.xDir = RUI_SHUFFLE_PS(_mm_castsi128_ps(_mm_castps_si128(inlineMaskBasis)), 238);
			memset(&imageUv.base2, 0, 48);

			sub_F9B80_rebuild(
					renderList->globals,
					ruiData,
					batch,
					&imageUv,
					transformRows,
					orientation,
					nameHash,
					reinterpret_cast<__m128i*>(const_cast<__m128*>(&atlasUv)),
					&clipUnit,
					const_cast<__m128*>(&inlineMaskBase),
					const_cast<__m128*>(&refinedImageExtentReciprocal));
		}
	}

	{
		const __int64 defaultFontIndex = textStyles[0]->fontIndex;
		unknownRuiListElement* instances = batch->ruiInstance;
		uiFontAtlas* fontAtlas = reinterpret_cast<uiFontAtlas*>(
			&(&uiFontAtlases)[6 * *reinterpret_cast<uint8_t*>(fontIndices + defaultFontIndex)]);
		const __int64 instanceIndex = batch->unsigned_int_8;
		uiFontAtlas* currentFontAtlas = instances[instanceIndex].uiFontAtlas_8;
		if (currentFontAtlas != fontAtlas)
		{
			if (!currentFontAtlas || instances[instanceIndex].dword_4 == batch->indexBufferSize)
			{
				instances[instanceIndex].uiFontAtlas_8 = fontAtlas;
			}
			else
			{
				const unsigned int nextInstanceIndex = static_cast<unsigned int>(instanceIndex + 1);
				batch->unsigned_int_8 = nextInstanceIndex;
				if (nextInstanceIndex != batch->dword_C)
				{
					instances[instanceIndex].dword_4 = batch->indexBufferSize;
					instances[instanceIndex + 1].dword_4 = batch->indexBufferSize;
					instances[instanceIndex + 1].uiFontAtlas_8 = fontAtlas;
					instances[instanceIndex + 1].dword_0 = instances[instanceIndex].dword_0;
					instances[instanceIndex + 1].uiImageAtlas_10 = nullptr;
				}
			}
		}
	}

	char* textCursor = *reinterpret_cast<char**>(&ruiData->dataValues[textElement->mins.x]);
	char* activeCursor = textCursor;
	char* originalText = textCursor;
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

	const uint8_t lineBegin = runtime->unk2[renderJobIndex].byte_4;
	const uint8_t lineEnd = static_cast<uint8_t>(lineBegin + runtime->unk2[renderJobIndex].byte_5);
	uint32_t nextLineGlyph = static_cast<uint32_t>(-1);
	uint32_t lineCursor = lineBegin;
	float currentAdvance = 0.0f;
	if (lineCursor < lineEnd)
	{
		nextLineGlyph = static_cast<uint32_t>(runtime->float_25C4[3 * lineCursor + 1]);
		currentAdvance = (transformSize.m128_f32[0] - runtime->float_25C4[3 * lineCursor + 2]) * lineAdvanceScale;
		++lineCursor;
	}

	const float lineHeightScale = runtime->unk2[renderJobIndex].float_0;
	float carryAdvance = 0.0f;
	__m128 correctionData[5];
	sub_FFAE0(transformRows, reinterpret_cast<const __m128i*>(&header->elementWidth), correctionData);

	float previousLineMax = 0.0f;
	__m128 previousGlyphState = _mm_setzero_ps();
	const __m128 transformSizeXY = RUI_SHUFFLE_PS(transformSize, 216);
	uint64_t includeStack[4] = {};
	uint32_t includeDepth = 0;

	for (;;)
	{
		__m128i firstGlyphState = _mm_setzero_si128();
		__m128i lastGlyphState = _mm_setzero_si128();
		__m128i currentGlyphState = _mm_setzero_si128();
		rpakFontGlyph* glyph = nullptr;
		rpakFont* font = fonts[activeStyle];
		auto* styleWords = reinterpret_cast<uint16_t*>(textStyles[activeStyle]);
		const uint16_t styleDescriptorIndex = static_cast<uint16_t>(
			batch->styleDescriptorIndex + *(reinterpret_cast<uint8_t*>(&textElement->assetIndex_0) + activeStyle));

		ruiBaseUvStruct glyphUv;
		glyphUv.flags = 0;
		glyphUv.styleDescriptorIndex = styleDescriptorIndex;

		const float baselineOffset = fmaxf(dataFloat(styleWords[23]), previousLineMax);
		__m128 glyphScaleX = _mm_set_ss(lineHeightScale);
		__m128 glyphScaleY = _mm_set_ss(dataFloat(styleWords[20]));
		glyphScaleX.m128_f32[0] = (lineHeightScale * dataFloat(styleWords[21])) * glyphScaleY.m128_f32[0];
		const float lineExtra = dataFloat(styleWords[22]);
		__m128 glyphScale = _mm_movelh_ps(_mm_unpacklo_ps(glyphScaleX, glyphScaleY), _mm_unpacklo_ps(glyphScaleX, glyphScaleY));
		const __m128 glyphScaleReciprocal = _mm_rcp_ps(glyphScale);
		const __m128 glyphScaleError = _mm_sub_ps(xmmword_5F3E90, _mm_mul_ps(glyphScaleReciprocal, glyphScale));
		const bool hasDecorations =
			fmaxf(dataFloat(styleWords[4]), fminf(fmaxf(dataFloat(styleWords[8]), dataFloat(styleWords[12])), lineExtra)) > 0.0f;
		const float glyphAdvanceScale = refinedTransformSizeReciprocal.m128_f32[0] * glyphScaleX.m128_f32[0];
		const __m128 styleOffset = _mm_unpacklo_ps(_mm_set_ss(dataFloat(styleWords[17])), _mm_set_ss(dataFloat(styleWords[18])));
		__m128 textHeight = _mm_set_ss(dataFloat(styleWords[19]));
		__m128 glyphScaleYScreen = RUI_SHUFFLE_PS(refinedTransformSizeReciprocal, 255);
		glyphScaleYScreen = _mm_set_ss(
			glyphScaleYScreen.m128_f32[0] * glyphScaleY.m128_f32[0]);
		const __m128 outlinePad = _mm_mul_ps(_mm_set1_ps(dataFloat(styleWords[24])), xmmword_5F3EB0);
		const __m128 textBoundsPad = _mm_max_ps(
			_mm_add_ps(_mm_mul_ps(_mm_set1_ps(textHeight.m128_f32[0]), xmmword_5F3EB0), _mm_xor_ps(_mm_movelh_ps(styleOffset, styleOffset), xmmword_5F3E20)),
			outlinePad);
		textHeight.m128_f32[0] = lineExtra + baselineOffset;
		const __m128 glyphBoundsOffset = _mm_mul_ps(
			_mm_xor_ps(_mm_add_ps(textBoundsPad, _mm_set1_ps(textHeight.m128_f32[0])), xmmword_5F3E20),
			RUI_SHUFFLE_PS(refinedTransformSizeReciprocal, 216));
		const __m128 fontAtlasScale = _mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(reinterpret_cast<uint8_t*>(font) + 28)));
		const float glyphBoundsMaxY = glyphBoundsOffset.m128_f32[3];
		const float glyphBoundsMinY = glyphBoundsOffset.m128_f32[1];

		ruiDrawTriangle tri;
		tri.size = 4;
		tri.size_ = 4;

		uint32_t previousCodepoint = 0;
		uint32_t pendingGlyphCount = 0;
		float batchStartX = 0.0f;
		__m128 batchMinY = _mm_setzero_ps();
		__m128 batchMaxY = _mm_setzero_ps();

		const __m128 glyphUvScale = _mm_mul_ps(
			_mm_mul_ps(_mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(glyphScaleError, glyphScaleError), glyphScaleError), glyphScaleReciprocal), glyphScaleReciprocal), transformSizeXY),
			fontAtlasScale);
		const __m128 glyphBasis = _mm_mul_ps(inverseBasis, glyphUvScale);
		const __m128 glyphOrigin = _mm_mul_ps(originSum, glyphUvScale);
		__m128 correctionMask = xmmword_5F4610;

		for (;;)
		{
			uint32_t parsedCount = styleEscapeCount;
			int codepoint;
			bool haveCodepoint = false;
			for (;;)
			{
				for (;;)
				{
					codepoint = readUnicodeCharacter_F2C40(&activeCursor);
					++parsedCount;
					styleEscapeCount = parsedCount;
					if (codepoint == '%')
						break;

					if (codepoint || !includeDepth)
					{
						haveCodepoint = true;
						break;
					}

					activeCursor = reinterpret_cast<char*>(includeStack[--includeDepth]);
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
				char* includeText = reinterpret_cast<char*>(sub_F98F0(ruiData, reinterpret_cast<__int64>(renderList), reinterpret_cast<char**>(&activeCursor), reinterpret_cast<__int64>(includeScratch)));
				if (!includeText)
					return;

				includeStack[includeDepth++] = reinterpret_cast<uint64_t>(activeCursor);
				activeCursor = includeText;
			}

			if (!haveCodepoint)
				++activeCursor;

			const bool controlCode = static_cast<unsigned int>(codepoint - 1) >= 0xEFFFF || codepoint == '`';
			float glyphAdvance = 0.0f;
			float currentGlyphX;
			float* glyphMetrics;
			if (controlCode)
			{
				currentGlyphX = _mm_castsi128_ps(currentGlyphState).m128_f32[0];
				glyphMetrics = reinterpret_cast<float*>(currentGlyphState.m128i_u64[1]);
			}
			else
			{
				const uint32_t glyphIndex = getFontGlyphIndex(font, codepoint);
				glyph = &font->textures[glyphIndex];
				int kernIndex = glyph->unk_4;
				const int kernEnd = glyph[1].unk_4;
				float kernOffset = 0.0f;
				if (kernIndex < kernEnd)
				{
					UIFont_UNK_t* kernTable = font->unk_58;
					while (static_cast<uint16_t>(kernIndex) < kernEnd &&
						kernTable[static_cast<uint16_t>(kernIndex)].unk_0 != previousCodepoint)
					{
						kernIndex = static_cast<uint16_t>(kernIndex + 1);
					}
					if (static_cast<uint16_t>(kernIndex) < kernEnd)
						kernOffset = kernTable[static_cast<uint16_t>(kernIndex)].unk_4;
				}
				currentGlyphState.m128i_i32[1] = glyphIndex;
				glyphAdvance = glyphAdvanceScale * glyph->unk_0;
				glyphMetrics = &glyph->unk_0;
				currentGlyphState.m128i_u64[1] = reinterpret_cast<uint64_t>(glyph);
				currentAdvance += kernOffset * glyphAdvanceScale;
				currentGlyphX = currentAdvance;
				_mm_store_ss(reinterpret_cast<float*>(&currentGlyphState), _mm_set_ss(currentAdvance));
			}

			const bool reachedLineBreak = parsedCount >= nextLineGlyph;
			auto submitGlyphBatch = [&](float drawCenterX) -> bool
			{
				const uint64_t firstGlyphPtr2 = firstGlyphState.m128i_u64[1];
				const uint64_t lastGlyphPtr2 = lastGlyphState.m128i_u64[1];
				__m128 drawCenter = _mm_set_ss(drawCenterX);
				__m128 drawStart = _mm_set_ss(batchStartX);
				const __m128 batchUv = _mm_shuffle_ps(drawStart, drawCenter, 0);
				const __m128 proportionScale = _mm_shuffle_ps(
					_mm_set_ss(font->proportions[*reinterpret_cast<uint8_t*>(firstGlyphPtr2 + 7)].scaleBounds),
					_mm_set_ss(font->proportions[*reinterpret_cast<uint8_t*>(lastGlyphPtr2 + 7)].scaleBounds),
					0);

				const __m128 glyphTextureHigh = _mm_castpd_ps(
					_mm_loadh_pd(_mm_setzero_pd(), reinterpret_cast<const double*>(lastGlyphPtr2 + 8)));
				glyphUv.yDir = _mm_add_ps(
					_mm_mul_ps(
						_mm_sub_ps(
							glyphOrigin,
							_mm_mul_ps(
								_mm_unpacklo_ps(
									_mm_unpacklo_ps(_mm_castsi128_ps(_mm_set_epi32(0, 0, lastGlyphState.m128i_u32[0], firstGlyphState.m128i_u32[0])),
										_mm_set1_ps(maxTextWidth)),
									_mm_unpacklo_ps(_mm_set_ss(maxTextWidth), _mm_set_ss(maxTextWidth))),
								glyphUvScale)),
						proportionScale),
					glyphTextureHigh);
				glyphUv.base = _mm_mul_ps(RUI_SHUFFLE_PS(glyphBasis, 68), proportionScale);
				glyphUv.yDir2 = RUI_SHUFFLE_PS(proportionScale, 216);
				glyphUv.xDir = _mm_mul_ps(RUI_SHUFFLE_PS(glyphBasis, 238), proportionScale);
				memset(&glyphUv.base2, 0, 32);
				glyphUv.assetIndex = font->textureIndex + firstGlyphState.m128i_i16[2];
				glyphUv.assetIndex2 = font->textureIndex + lastGlyphState.m128i_i16[2];

				const __m128 bounds = _mm_add_ps(
					_mm_unpacklo_ps(_mm_unpacklo_ps(batchMinY, batchMaxY), _mm_unpacklo_ps(batchMaxY, batchMinY)),
					_mm_set1_ps(maxTextWidth));
				const __m128i transform0 = _mm_load_si128(reinterpret_cast<const __m128i*>(transformRows));
				alignas(16) __m128 projected[2];
				projected[0] = _mm_add_ps(
					_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_I32_AS_PS(transform0, 0), batchUv), _mm_mul_ps(RUI_SHUFFLE_I32_AS_PS(transform0, 170), bounds)),
					RUI_SHUFFLE_PS(transformRows[1], 0));
				projected[1] = _mm_add_ps(
					_mm_add_ps(_mm_mul_ps(RUI_SHUFFLE_I32_AS_PS(transform0, 85), batchUv), _mm_mul_ps(RUI_SHUFFLE_I32_AS_PS(transform0, 255), bounds)),
					RUI_SHUFFLE_PS(transformRows[1], 85));

				__m128 correction = correctionMask;
				sub_FEF30(renderList->globals, ruiData, correctionData, correction, projected);

				__m128 quad0 = _mm_unpacklo_ps(projected[0], projected[1]);
				__m128 quad1 = _mm_unpackhi_ps(projected[0], projected[1]);
				if (orientation == 2)
				{
					quad0 = RUI_SHUFFLE_PS(quad0, 78);
					quad1 = RUI_SHUFFLE_PS(quad1, 78);
				}

				_mm_storeu_ps(&tri.vert[0][0], quad0);
				_mm_storeu_ps(&tri.vert[1][0], quad1);
				ruiDrawInfoDataWeapon* drawInfo = ruiData->pvoid_38;
				if (!ruiDrawInfo_5f4560[drawInfo->type](drawInfo, &glyphUv, &tri, batch))
					return false;

				batchStartX = drawCenterX;
				correctionMask = _mm_and_ps(correctionMask, xmmword_12A146B0);
				return true;
			};

			if (hasDecorations)
			{
				if (reachedLineBreak || controlCode)
				{
					__m128 drawCorrection;
					if (pendingGlyphCount > 1)
					{
						drawCorrection = correctionMask;
					}
					else
					{
						if (!pendingGlyphCount)
						{
							// Nothing accumulated yet; fall through to line-break/control handling.
						}
						else
						{
							drawCorrection = _mm_add_ps(correctionMask, xmmword_5F3EE0);
							firstGlyphState = lastGlyphState;
							correctionMask = drawCorrection;

							const uint64_t firstGlyphPtr = firstGlyphState.m128i_u64[1];
							const uint64_t lastGlyphPtr = lastGlyphState.m128i_u64[1];
							pendingGlyphCount = 0;
							__m128 minY = _mm_set_ss(fminf(*reinterpret_cast<float*>(firstGlyphPtr + 20), *reinterpret_cast<float*>(lastGlyphPtr + 20)));
							__m128 maxY = _mm_set_ss(fmaxf(*reinterpret_cast<float*>(firstGlyphPtr + 28), *reinterpret_cast<float*>(lastGlyphPtr + 28)));
							const float batchEndX = (glyphAdvanceScale * *reinterpret_cast<float*>(lastGlyphPtr + 24)) + *reinterpret_cast<float*>(&lastGlyphState);
							correctionMask = _mm_add_ps(drawCorrection, xmmword_5F3EF0);
							const float drawCenterX = batchEndX + glyphBoundsOffset.m128_f32[2];
							minY.m128_f32[0] = (minY.m128_f32[0] * glyphScaleYScreen.m128_f32[0]) + glyphBoundsMinY;
							maxY.m128_f32[0] = (maxY.m128_f32[0] * glyphScaleYScreen.m128_f32[0]) + glyphBoundsMaxY;
							batchMinY = minY;
							batchMaxY = maxY;
							if (!submitGlyphBatch(drawCenterX))
								return;
						}
					}
					if (pendingGlyphCount > 1)
					{
						const uint64_t firstGlyphPtr = firstGlyphState.m128i_u64[1];
						const uint64_t lastGlyphPtr = lastGlyphState.m128i_u64[1];
						pendingGlyphCount = 0;
						__m128 minY = _mm_set_ss(fminf(*reinterpret_cast<float*>(firstGlyphPtr + 20), *reinterpret_cast<float*>(lastGlyphPtr + 20)));
						__m128 maxY = _mm_set_ss(fmaxf(*reinterpret_cast<float*>(firstGlyphPtr + 28), *reinterpret_cast<float*>(lastGlyphPtr + 28)));
						const float batchEndX = (glyphAdvanceScale * *reinterpret_cast<float*>(lastGlyphPtr + 24)) + *reinterpret_cast<float*>(&lastGlyphState);
						correctionMask = _mm_add_ps(drawCorrection, xmmword_5F3EF0);
						const float drawCenterX = batchEndX + glyphBoundsOffset.m128_f32[2];
						minY.m128_f32[0] = (minY.m128_f32[0] * glyphScaleYScreen.m128_f32[0]) + glyphBoundsMinY;
						maxY.m128_f32[0] = (maxY.m128_f32[0] * glyphScaleYScreen.m128_f32[0]) + glyphBoundsMaxY;
						batchMinY = minY;
						batchMaxY = maxY;
						if (!submitGlyphBatch(drawCenterX))
							return;
					}
				}
				else
				{
					const float posMinX = glyph->posMinX;
					if (posMinX == glyph->posMaxX)
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
						minY.m128_f32[0] = (glyphScaleYScreen.m128_f32[0] * glyph->posMinY) + glyphBoundsMinY;
						maxY.m128_f32[0] = (glyphScaleYScreen.m128_f32[0] * glyph->posMaxY) + glyphBoundsMaxY;

						if (pendingGlyphCount)
						{
							batchMinY.m128_f32[0] = fminf(batchMinY.m128_f32[0], minY.m128_f32[0]);
							batchMaxY.m128_f32[0] = fmaxf(batchMaxY.m128_f32[0], maxY.m128_f32[0]);
						}
						else
						{
							batchMinY = minY;
							batchMaxY = maxY;
							batchStartX = (glyphAdvanceScale * posMinX) + glyphBoundsOffset.m128_f32[0] + currentAdvance;
							correctionMask = _mm_sub_ps(correctionMask, xmmword_5F3EE0);
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
						const uint64_t firstGlyphPtr = firstGlyphState.m128i_u64[1];
						const uint64_t lastGlyphPtr = lastGlyphState.m128i_u64[1];
						batchMinY = _mm_set_ss(*reinterpret_cast<float*>(firstGlyphPtr + 20));
						batchMaxY = _mm_set_ss(*reinterpret_cast<float*>(firstGlyphPtr + 28));
						float drawCenterX = (((*reinterpret_cast<float*>(firstGlyphPtr + 24) + glyphMetrics[4]) * glyphAdvanceScale)
							+ (*reinterpret_cast<float*>(&firstGlyphState) + currentGlyphX)) * 0.5f;
						batchMinY.m128_f32[0] =
							(fminf(fminf(batchMinY.m128_f32[0], *reinterpret_cast<float*>(lastGlyphPtr + 20)), glyphMetrics[5])
								* glyphScaleYScreen.m128_f32[0])
							+ glyphBoundsMinY;
						batchMaxY.m128_f32[0] =
							(fmaxf(fmaxf(batchMaxY.m128_f32[0], *reinterpret_cast<float*>(lastGlyphPtr + 28)), glyphMetrics[7])
								* glyphScaleYScreen.m128_f32[0])
							+ glyphBoundsMaxY;

						if (!submitGlyphBatch(drawCenterX))
							return;
					}
				}
			}

			if (reachedLineBreak)
			{
				maxTextWidth += glyphScaleYScreen.m128_f32[0] + textOriginX;
				if (lineCursor >= lineEnd)
				{
					nextLineGlyph = static_cast<uint32_t>(-1);
					currentAdvance = transformSize.m128_f32[0] - runtime->float_25C4[3 * lineEnd];
				}
				else
				{
					const __int64 lineRecord = 3LL * lineCursor;
					nextLineGlyph = static_cast<uint32_t>(runtime->float_25C4[3 * lineCursor + 1]);
					++lineCursor;
					currentAdvance = transformSize.m128_f32[0] - runtime->float_25C4[lineRecord + 2];
				}
				currentAdvance *= lineAdvanceScale;

				if (!glyph || glyph->posMinX == glyph->posMaxX)
				{
					pendingGlyphCount = 0;
				}
				else
				{
					currentGlyphState.m128i_i32[0] = _mm_castps_si128(_mm_set_ss(currentAdvance)).m128i_i32[0];
					pendingGlyphCount = 1;
					const float posMinX = glyph->posMinX;
					const float xOffset = glyphAdvanceScale * posMinX;
					batchStartX = xOffset + glyphBoundsOffset.m128_f32[0] + currentAdvance;
					batchMaxY = glyphScaleYScreen;
					batchMaxY.m128_f32[0] = (glyphScaleYScreen.m128_f32[0] * glyph->posMaxY) + glyphBoundsMaxY;
					batchMinY = glyphScaleYScreen;
					batchMinY.m128_f32[0] = (glyphScaleYScreen.m128_f32[0] * glyph->posMinY) + glyphBoundsMinY;
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
				return;

			if (codepoint == '`')
				break;

			if (static_cast<unsigned int>(codepoint - 0xF0000) >= 0x2000)
			{
				if (codepoint != 991233)
				{
					previousCodepoint = codepoint;
					glyph = nullptr;
					continue;
				}

				currentAdvance += carryAdvance;
				previousCodepoint = 991233;
				glyph = nullptr;
				carryAdvance = 0.0f;
			}
			else
			{
				const uint8_t* unicodeAsset = *reinterpret_cast<const uint8_t**>(assetIndexData_12A4E510) + 8LL * static_cast<uint16_t>(codepoint);
				const __int16 unicodeTextureIndex = *reinterpret_cast<const __int16*>(unicodeAsset + 4);
				const uint8_t unicodeAtlasIndex = unicodeAsset[6];
				uiImageAtlas* unicodeAtlas = &rpakUIMGAtlases[ unicodeAtlasIndex];
				const uint8_t* unicodeDimensions = reinterpret_cast<const uint8_t*>(unicodeAtlas->textureDimensions) + 4LL * static_cast<uint16_t>(unicodeTextureIndex);
				const float unicodeWidth = static_cast<float>(*reinterpret_cast<const uint16_t*>(unicodeDimensions));

				if (previousCodepoint == 991232)
				{
					if (static_cast<uint16_t>(unicodeTextureIndex) >= unicodeAtlas->textureOffsetsCount)
					{
						carryAdvance = 0.0f;
						previousCodepoint = codepoint;
						glyph = nullptr;
					}
					else
					{
						const float* unicodeOffset = reinterpret_cast<const float*>(
							reinterpret_cast<const uint8_t*>(unicodeAtlas->pointer_20) + 32LL * static_cast<uint16_t>(unicodeTextureIndex));
						const float scaledWidth = refinedTransformSizeReciprocal.m128_f32[0] * unicodeWidth;
						currentAdvance += scaledWidth * unicodeOffset[0];
						carryAdvance = scaledWidth * unicodeOffset[2];
						previousCodepoint = codepoint;
						glyph = nullptr;
					}
				}
				else
				{
					currentAdvance += (unicodeWidth / static_cast<float>(*reinterpret_cast<const uint16_t*>(unicodeDimensions + 2))) * glyphAdvanceScale;
					previousCodepoint = codepoint;
					glyph = nullptr;
				}
			}
		}

		activeStyle = static_cast<uint8_t>(*activeCursor - '0');
		if (activeStyle >= 4)
			return;

		++activeCursor;
		previousLineMax = previousGlyphState.m128_f32[0];
	}
}

DECLARE_HOOK(RenderText, engine.dll + 0xF5840, [](auto& hook, ruiRenderList* a1, ruiDataStruct* a2, AssetRenderOffsets* a3, struct_v3* a4)
{
		//renderText_F5840_rebuild(a1, a2, a3, a4);

		hook.Original(a1, a2, a3, a4);
});

ON_DLL_LOAD("rtech_game.DLL", AtlasRpak, [](CModule module)
{
	DISPATCH_MODULE(AtlasTest);
});




ON_DLL_LOAD("engine.dll", AtlasTest, [](CModule module)
{
	// AUTOHOOK_DISPATCH_MODULE(engine.dll)
	DISPATCH_MODULE(AtlasTest);
	ruiDrawInfo_5f4560 = module.Offset(0x5F4560).RCast<ruiDrawInfoFunc*>();
	xmmword_5F3DD0 = *module.Offset(0x5F3DD0).RCast<__m128*>();
	xmmword_5F3E20 = *module.Offset(0x5F3E20).RCast<__m128*>();
	xmmword_5F3E50 = *module.Offset(0x5F3E50).RCast<__m128*>();
	xmmword_5F3E70 = *module.Offset(0x5F3E70).RCast<__m128*>();
	xmmword_5F3E80 = *module.Offset(0x5F3E80).RCast<__m128*>();
	xmmword_5F3E90 = *module.Offset(0x5F3E90).RCast<__m128*>();
	xmmword_5F3EB0 = *module.Offset(0x5F3EB0).RCast<__m128*>();
	xmmword_5F3EE0 = *module.Offset(0x5F3EE0).RCast<__m128*>();
	xmmword_5F3EF0 = *module.Offset(0x5F3EF0).RCast<__m128*>();
	xmmword_5F3F30 = *module.Offset(0x5F3F30).RCast<__m128*>();
	xmmword_5F3F60 = *module.Offset(0x5F3F60).RCast<__m128*>();
	xmmword_5F4600 = *module.Offset(0x5F4600).RCast<__m128*>();
	xmmword_5F4610 = *module.Offset(0x5F4610).RCast<__m128*>();

	xmmword_5CB2A0 = *module.Offset(0x5CB2A0).RCast<__m128*>();
	xmmword_5F34E0 = *module.Offset(0x5F34E0).RCast<__m128*>();
	xmmword_5F34B0 = *module.Offset(0x5F34B0).RCast<__m128*>();
	xmmword_5F3510 = *module.Offset(0x5F3510).RCast<__m128*>();
	xmmword_5F3490 = *module.Offset(0x5F3490).RCast<__m128*>();
	xmmword_5F3500 = *module.Offset(0x5F3500).RCast<__m128*>();
	xmmword_5F34A0 = *module.Offset(0x5F34A0).RCast<__m128*>();
	xmmword_5F3470 = *module.Offset(0x5F3470).RCast<__m128*>();
	xmmword_5F34F0 = *module.Offset(0x5F34F0).RCast<__m128*>();
	xmmword_5F3460 = *module.Offset(0x5F3460).RCast<__m128*>();
	xmmword_5F3E00 = *module.Offset(0x5F3E00).RCast<__m128*>();
	xmmword_5F34C0 = *module.Offset(0x5F34C0).RCast<__m128*>();
	xmmword_5F45D0 = *module.Offset(0x5F45D0).RCast<__m128*>();

	xmmword_12A14650 = *module.Offset(0x12A14650).RCast<__m128*>();
	xmmword_12A146A0 = *module.Offset(0x12A146A0).RCast<__m128*>();
	xmmword_12A146B0 = *module.Offset(0x12A146B0).RCast<__m128*>();
	xmmword_12A146D0 = *module.Offset(0x12A146D0).RCast<__m128*>();
	rpakUIMGAtlases = module.Offset(0x12A26140).RCast<uiImageAtlas*>();
	assetIndexList = module.Offset(0x12A4E508).RCast<struct_a1_2*>();

	word_12A2E50C = module.Offset(0x12A2E50C).RCast<short*>();
	byte_12A2E50E = module.Offset(0x12A2E50E).RCast<uint8_t*>();
	byte_12A2E50F = module.Offset(0x12A2E50F).RCast<uint8_t*>();
	uiFontAtlases = module.Offset(0x12A26080).RCast<uiFontAtlas*>(); // font atlas lol
	unk_12A2E508 = module.Offset(0x12A2E508).RCast<assetIndexData*>();

	xmmword_12A4E830 = module.Offset(0x12A4E830).RCast<__m128*>();
	funcs_5F4560 = module.Offset(0x5F4560).RCast<funcs5F4560Type*>();
	sub_FC0C0 = module.Offset(0xFC0C0).RCast<sub_FC0C0Type>();
	readUnicodeCharacter_F2C40 = module.Offset(0xF2C40).RCast<readUnicodeCharacter_F2C40Type>();
	rpakFontPointers = module.Offset(0x12A4E550).RCast<rpakFont**>();

	getFontGlyphIndex = module.Offset(0xFAE80).RCast<getFontGlyphIndexType>();
	getUnicodeCharacter_GPT = module.Offset(0x00F2C40).RCast<getUnicodeCharacter_GPTType>();
	sub_F98F0 = module.Offset(0xF98F0).RCast<sub_F98F0Type>();
	// void (*sub_F9B80)(__int64 a1, __int64 a2, _QWORD *a3, __m128 *a4, const __m128i *a5, int a6, __int64 a7, __m128i *a8, __m128 *a9,
	// __m128 *a10, __m128 *a11);
	sub_F9B80 = module.Offset(0xF9B80).RCast<sub_F9B80Type>();
	sub_FFAE0 = module.Offset(0xFFAE0).RCast<sub_FFAE0Type>();
	sub_FEF30 = module.Offset(0xFEF30).RCast<sub_FEF30Type>();
	sub_FEF30_2 = module.Offset(0xFEF30).RCast<sub_FEF30_2Type>();
	sub_F3BB0 = module.Offset(0xF3BB0).RCast<sub_F3BB0Type>();
	sub_F3E30 = module.Offset(0xF3E30).RCast<sub_F3E30Type>();
	xmmword_5F4740 = module.Offset(0x5F4740).RCast<__m128*>();
	fontIndices = module.Offset(0x12A4E650).RCast<BYTE*>();
	assetIndexData_12A4E510 = module.Offset(0x12A4E510).RCast<assetIndexData*>();
});
