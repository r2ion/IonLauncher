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
	__m128 m128_2DD0[1];
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


struct unknown8dataStruct
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

struct unknown9dataStruct_2
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
	uint16_t unknown9Count;
	uint16_t argClusterCount;
	unknown8dataStruct* unknown8data;
	uint8_t* unknown9data;
	ruiUnknown10* unknown_10;
	void(__fastcall* dllFunc)(void* a1, void*, void*, char*);
	void(__fastcall* dllFuncHidden)(void*, void*, void*, void*);
};

struct unknown9dataStruct_0
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
	float float_0;
	uint16_t word_4;
	uint8_t byte_6;
	_BYTE gap_7[25];
};

struct rpakFont
{
	char* fontName;
	_BYTE gap_8[6];
	unsigned __int16 unsigned___int16_E;
	_BYTE gap_10[4];
	_DWORD dword_14;
	_BYTE gap_18[4];
	float float_1C;
	_BYTE gap_20[4];
	float float_24;
	_BYTE gap_28[8];
	uint16_t* qword_30;
	uint16_t* qword_38;
	_QWORD* qword_40;
	_BYTE gap_48[8];
	rpakFontGlyph* fontGlyphs;
	unknownFontStruct* pointer_58;
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
	_DWORD unk_StartIndex;
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
typedef char* (*sub_F98F0Type)(ruiDataStruct* a1, __int64 a2, char** a3, __int64 a4, const char* a5);
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

typedef __int64 (*funcs5F4560Type)(__m128* a1, __m128* a2, ruiDrawTriangle* a3, struct_v3* a4);

// uiImageAtlas rpakUIMGAtlases[50];
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
//AUTOHOOK(addAssetLoader, rtech_game.dll + 0x7BE0, __int64, __fastcall, (assetLoader * a1, unsigned int a2, unsigned int a3))
//{
//	if (a1->hash == 0xA676D6975)
//	{
//		a1->listElementAmount = 20; // sizeof(rpakUIMGAtlases)/sizeof(uiImageAtlas);
//		a1->listPointer = rpakUIMGAtlases;
//	}
//	return addAssetLoader(a1, a2, a3);
//}

short* word_12A2E50C;
uint8_t* byte_12A2E50E;
uint8_t* byte_12A2E50F;
uiFontAtlas* uiFontAtlases;
char* unk_12A2E508;

__m128* xmmword_12A4E830;
funcs5F4560Type* funcs_5F4560;
unsigned int (*sub_FC0C0)(struct_v3* a1, uiImageAtlas* a2);
rpakFont** rpakFontPointers;
struct_a1_2* assetIndexList;

uint64_t (*getFontGlyphIndex)(rpakFont* a1, int c);
uint64_t (*getUnicodeCharacter_GPT)(char** a1);
char* (*sub_F98F0)(ruiDataStruct* a1, __int64 a2, char** a3, __int64 a4, const char* a5);
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

BYTE* fontIndices;
void __fastcall renderJobElipse(__int64 a1, ruiDataStruct* a2, unknown9dataStruct_2* a3, struct_v3* a4)
{
	__int16 v6; // r9
	unknown8dataStruct* v7; // rdx
	__int64 v8; // r14
	testStruct* v9; // r11
	__m128 v10; // xmm2
	int v11; // ebx
	__m128 v12; // xmm6
	__m128i v13; // xmm1
	__m128 v14; // xmm7
	__int64 v15; // rcx
	__int64 v16; // rax
	__int64 v17; // rsi
	__int16 v18; // r15
	__int16 v19; // r12
	__int16 v20; // cx
	__int16 v21; // r15
	__m128 v22; // xmm10
	__m128 v23; // xmm11
	__m128 v24; // xmm12
	__m128 v25; // xmm13
	__m128 v26; // xmm15
	__m128 v27; // xmm14
	__m128* v28; // rax
	float v29; // xmm9_4
	__m128 v34; // xmm4
	__m128 v35; // xmm3
	__int64 v36; // rcx
	__m128 v37; // xmm0
	__m128 v38; // xmm4
	__m128 v39; // xmm1
	__m128 v40; // xmm3
	__m128 v41; // xmm5
	__m128i v42; // xmm4
	__m128 v43; // xmm0
	__m128 v44; // xmm3
	__m128 v45; // xmm6
	__m128 v46; // xmm3
	__m128 v47; // xmm2
	__m128 v48; // xmm0
	__m128 v49; // xmm6
	__m128 v50; // xmm3
	__m128 v51; // xmm4
	__m128i v52; // xmm2
	__m128 v53; // xmm6
	__m128 v54; // xmm1
	__m128i v55; // xmm0
	__m128i v56; // xmm6
	__m128* v57; // rcx
	float v58; // [rsp+20h] [rbp-198h]
	__m128 v59; // [rsp+30h] [rbp-188h]
	__m128 v60; // [rsp+40h] [rbp-178h]
	ruiDrawTriangle v61; // [rsp+50h] [rbp-168h] BYREF
	__m128 v64[6]; // [rsp+80h] [rbp-138h] BYREF
	__int16 v65; // [rsp+E0h] [rbp-D8h]
	__int16 v66; // [rsp+E2h] [rbp-D6h]
	__int16 v67; // [rsp+E4h] [rbp-D4h]
	__int16 v68; // [rsp+E6h] [rbp-D2h]
	float v69; // [rsp+1C8h] [rbp+10h]
	float v70; // [rsp+1D0h] [rbp+18h]

	v6 = a3->uint8_18;
	v7 = &a2->header->unknown8data[a3->uint8_18];
	if (*(float*)&a2->dataValues[v7->color_alpha] > 0.0)
	{
		v8 = a3->transformIndex;
		v9 = &a2->v1->m128_3A80[v8];
		v10 = _mm_sub_ps(
			_mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v9->m128_0, 255)), _mm_castsi128_ps(_mm_shuffle_epi32(v9->m128_0, 0))),
			_mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v9->m128_0, 170)), _mm_castsi128_ps(_mm_shuffle_epi32(v9->m128_0, 85))));
		if (!_mm_movemask_ps(_mm_cmpeq_ps(v10, _mm_setzero_ps())))
		{
			v11 = _mm_movemask_ps(v10) & 2;
			v12 = _mm_div_ps(_mm_xor_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v9->m128_0, 39)), (__m128)xmmword_5F3E50), v10);
			v13 = _mm_castps_si128(
				_mm_mul_ps(_mm_xor_ps(v12, (__m128)xmmword_5F3DD0), _mm_castsi128_ps(_mm_shuffle_epi32(v9->m128_10, 216))));
			v14 = _mm_add_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v13, 78)), _mm_castsi128_ps(v13));
			v15 = *(int*)&a2->dataValues[a3->uint16_4];
			if ((_DWORD)v15 != -1)
			{
				v66 = -1;
				v16 = a3->uint16_6;
				v17 = v15;
				v18 = *(unsigned __int8*)byte_12A2E50F[8 * v15];
				v19 = *(_WORD*) word_12A2E50C[8 * v15];
				v20 = a4->unk_StartIndex;
				v21 = a3->word_16 | v18;
				v65 = v19;
				v68 = v21;
				v67 = v6 + v20;
				v22 = _mm_load_ps((float*)&a2->dataValues[v16]);
				v23 = _mm_load_ps((float*)&a2->dataValues[a3->uint16_8]);
				v24 = _mm_load_ps((float*)&a2->dataValues[a3->uint16_A]);
				v25 = _mm_load_ps((float*)&a2->dataValues[a3->uint16_C]);
				v69 = *(float*)&a2->dataValues[a3->uint16_E];
				v70 = *(float*)&a2->dataValues[a3->uint16_10];
				v26 = _mm_load_ps((float*)&a2->dataValues[a3->uint16_12]);
				v27 = _mm_load_ps((float*)&a2->dataValues[a3->uint16_14]);
				v58 = *(float*)&a2->dataValues[v7->stretchXOffset];
				v28 = &a2->v1->m128_2DD0[v8];
				v29 = v28->m128_f32[0];
				if (fminf(v28->m128_f32[0], v28->m128_f32[2]) > 0.0)
				{
					if ((unsigned int)sub_FC0C0(a4, &rpakUIMGAtlases[(unsigned __int8)byte_12A2E50E[8 * v17]]))
					{
						v34 = _mm_unpacklo_ps(v26, v27);
						v35 = _mm_setzero_ps();
						v36 = *(_QWORD*)rpakUIMGAtlases[(unsigned __int8)byte_12A2E50E[8 * v17]].textureOffsets +
							  32i64 * v19; // arg2 of function above
						v37 = _mm_unpacklo_ps(_mm_load_ps(&v69), _mm_load_ps(&v70));
						v38 = _mm_movelh_ps(v34, v34);
						v27.m128_f32[0] = (float)(v27.m128_f32[0] - v70) * v58;
						v59 = _mm_movelh_ps(v37, v37);
						v60 = _mm_max_ps(_mm_sub_ps(v38, v59), (__m128)xmmword_5F3F30);
						v39 = (__m128)xmmword_12A4E830[((__int64)v21 >> 4) & 3];
						v35.m128_f32[0] =
							(float)((float)(v28->m128_f32[2] * v58) * (float)(v26.m128_f32[0] - v69)) / v29; // v33 = v28.m128_f32[2]
						v40 = _mm_unpacklo_ps(v35, v27);
						v41 = _mm_div_ps(
							_mm_add_ps(
								_mm_sub_ps(*(__m128*)v36, _mm_xor_ps(_mm_and_ps(_mm_min_ps(v59, v38), v39), (__m128)xmmword_5F3E20)),
								_mm_movelh_ps(v40, v40)),
							_mm_or_ps(
								_mm_and_ps(_mm_andnot_ps((__m128)xmmword_5F3DD0, v60), v39), _mm_andnot_ps(v39, (__m128)xmmword_5F3E90)));
						if (!_mm_movemask_ps(_mm_cmplt_ps(v41, (__m128)xmmword_5F3F60)))
						{
							v42 = _mm_castps_si128(_mm_xor_ps(
								_mm_min_ps(
									_mm_movelh_ps(_mm_xor_ps(_mm_unpacklo_ps(v22, v23), (__m128)xmmword_5F3DD0), _mm_unpacklo_ps(v24, v25)),
									v41),
								(__m128)xmmword_5F3E20));
							if (!_mm_movemask_ps(_mm_cmple_ps(
									_mm_castsi128_ps(_mm_shuffle_epi32(v42, 238)), _mm_castsi128_ps(_mm_shuffle_epi32(v42, 68)))))
							{
								v43 = _mm_castpd_ps(_mm_loaddup_pd((const double*)(v36 + 24)));
								v44 = v12;
								v45 = _mm_mul_ps(v12, (__m128)xmmword_5F3E80);
								v46 = _mm_mul_ps(_mm_mul_ps(v44, v60), v43);
								v64[5] = _mm_setzero_ps();
								v64[3] = _mm_setzero_ps();
								v64[4] = _mm_setzero_ps();
								v61.size = 4;
								v47 = _mm_add_ps(
									_mm_mul_ps(_mm_add_ps(_mm_mul_ps(v14, v60), v59), v43),
									_mm_castsi128_ps(_mm_loadl_epi64((const __m128i*)(v36 + 16))));
								v61.size_ = 4;
								v48 = _mm_movelh_ps(v46, v45);
								v49 = _mm_movehl_ps(v45, v46);
								v50 = _mm_castsi128_ps(_mm_shuffle_epi32(v42, 125));
								v51 = _mm_castsi128_ps(_mm_shuffle_epi32(v42, 160));
								v64[0] = v48;
								v64[1] = v49;
								v64[2] = _mm_movelh_ps(v47, _mm_sub_ps(_mm_mul_ps(v14, (__m128)xmmword_5F3E80), (__m128)xmmword_5F3E90));
								v52 = _mm_load_si128(&v9->m128_0); // v32 = v9
								v53 = _mm_add_ps(
									_mm_add_ps(
										_mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v52, 170)), v50),
										_mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v52, 0)), v51)),
									_mm_castsi128_ps(_mm_shuffle_epi32(v9->m128_10, 0)));
								v54 = _mm_add_ps(
									_mm_add_ps(
										_mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v52, 255)), v50),
										_mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v52, 85)), v51)),
									_mm_castsi128_ps(_mm_shuffle_epi32(v9->m128_10, 85)));
								v55 = _mm_castps_si128(_mm_unpacklo_ps(v53, v54));
								v56 = _mm_castps_si128(_mm_unpackhi_ps(v53, v54));
								if (v11 == 2)
								{
									v55 = _mm_shuffle_epi32(v55, 78);
									v56 = _mm_shuffle_epi32(v56, 78);
								}
								v57 = (__m128*)a2->pvoid_38;
								*(__m128i*)&v61.vert[0][0] = v55;
								*(__m128i*)&v61.vert[2][0] = v56;
								funcs_5F4560[v57->m128_u32[0]](v57, v64, &v61, a4);
							}
						}
					}
				}
			}
		}
	}
}





DECLARE_HOOK(
	ruiUnknown9Func_2,
	engine.dll + 0xF7A80,
	[](auto& hook, __int64 a1, ruiDataStruct* a2, unknown9dataStruct_2* a3, struct_v3* a4)
	{
		//renderJobElipse(a1, a2, a3, a4);
		hook.Original(a1, a2, a3, a4);
	});

//DECLARE_HOOK(ruiUnknown9Func_2, engine.dll + 0xF7A80, [] (auto& hook, __int64 a1, ruiDataStruct* a2, unknown9dataStruct_2* a3, struct_v3* a4)

struct ruiRenderList
{
	void* globals;
	_QWORD qword_8;
	uint16_t word_10;
	uint16_t word_12;
	_WORD word_14;
	uint16_t ruiCount;
	ruiDataStruct* ruiInstances[1];
};

/*
DECLARE_HOOK(
	sub_F9B80,
	0,
	[]
	(
	auto& hook,
	void* a1,
	 ruiDataStruct* a2,
	 struct_v3* a3,
	 __m128* a4,
	 const __m128i* a5,
	 int a6,
	 __int64 a7,
	 __m128i* a8,
	 __m128* a9,
	 __m128* a10,
	 __m128* a11) -> int64_t
{

*/
//
//int64_t renderText(ruiRenderList* a1, ruiDataStruct* a2, unknown9dataStruct_0* a3, struct_v3* a4)
//{
//	struct_v1* v4; // r12
//	__int64 transformIndex; // r10
//	__m128i v8; // xmm10
//	testStruct* v10; // rcx MAPDST
//	__m128 v11; // xmm2
//	__m128 v13; // xmm0
//	__m128 v14; // xmm12
//	__m128i v15; // xmm1
//	__m128 v16; // xmm13
//	ruiHeader* v17; // r15
//	__m128 v18; // xmm6
//	unknown8dataStruct* styleDescriptorBasePointer; // rax
//	__m128 v26; // xmm3
//	__m128i v31; // xmm15
//	float v38; // xmm0_4
//	float v39; // xmm2_4
//	float v40; // xmm0_4
//	float v42; // xmm0_4
//	unsigned __int64 v43; // rsi
//	unsigned __int8 v44; // al
//	__int64 v45; // rbx
//	__m128i v46; // xmm0
//	__m128 v47; // xmm6
//	int v48; // edi
//	__m128 v49; // xmm7
//	__int64 v50; // rax
//	__int64 v51; // rdx
//	char* v52; // r10
//	__m128 v53; // xmm0
//	__m128 v54; // xmm2
//	__m128 v55; // xmm4
//	__m128 v56; // xmm1
//	__int64 v57; // r8
//	__int64 v58; // rax
//	__m128 v59; // xmm8
//	__m128 v60; // xmm0
//	__int16 v61; // cx
//	__m128 v62; // xmm0
//	__m128 v63; // xmm1
//	__m128i v64; // xmm2
//	__m128 v65; // xmm0
//	int v66; // eax
//	unknownRuiListElement* v69; // rdx
//	uiFontAtlas* v70; // r8
//	__int64 v71; // rax
//	__int64 v72; // rcx
//	uiFontAtlas* v73; // r9
//	int v74; // er9
//	unsigned int v75; // eax
//	int v76; // eax
//	int v77; // ecx
//	unsigned __int8 selectedFontIndex; // bl
//	char* printText; // rdx
//	bool v80; // zf
//	char* currentCharPointer1; // rax
//	__int64 v82; // rdx
//	unsigned int v83; // eax
//	unsigned int v84; // er13
//	float v85; // xmm14_4
//	float v86; // xmm14_4
//	unsigned int v87; // eax
//	float v88; // xmm0_4
//	float v89; // xmm12_4
//	rpakFontGlyph* v91; // rbx
//	unsigned __int16* v93; // rcx
//	float v94; // xmm11_4
//	__m128 v95; // xmm12
//	__m128 v96; // xmm13
//	float v97; // xmm10_4
//	__m128 v98; // xmm8
//	__m128 v99; // xmm8
//	__m128 v100; // xmm5
//	__m128 v101; // xmm4
//	int v102; // xmm6_4
//	__m128 v103; // xmm9
//	__m128 v104; // xmm3
//	__m128i v105; // xmm12
//	__m128 v106; // xmm0
//	__m128 v107; // xmm3
//	__m128 v108; // xmm0
//	float v109; // xmm9_4
//	float v110; // xmm8_4
//	int v111; // er14
//	unsigned int v112; // esi
//	float v113; // xmm10_4
//	__m128 v114; // xmm13
//	__m128 v115; // xmm15
//	unsigned int v116; // er15
//	int currentUnicodeChar; // eax
//	int v118; // edi
//	char v119; // dl
//	char* v120; // rax
//	__int64 v121; // rdx
//	bool v122; // r12
//	float v123; // xmm11_4
//	__int32 v124; // eax
//	rpakFont* v125; // r10
//	int v126; // edx
//	int v127; // er8
//	unknownFontStruct* v128; // rcx
//	float v129; // xmm0_4
//	float* v130; // rax
//	float v131; // xmm5_4
//	bool v132; // r15
//	float v133; // xmm0_4
//	__m128i v134; // xmm4
//	__m128i v135; // xmm5
//	__m128 v136; // xmm0
//	__m128 v137; // xmm1
//	unsigned __int64 v138; // rdx
//	float* v139; // r8
//	float v140; // xmm7_4
//	__m128 v141; // xmm8
//	__m128 v142; // xmm9
//	__m128 v143; // xmm4
//	__m128 v144; // xmm2
//	__m128 v145; // xmm3
//	float v146; // xmm7_4
//	__m128 v147; // xmm1
//	__m128 v148; // xmm5
//	__m128 v149; // xmm5
//	__m128i v150; // xmm6
//	__m128 v151; // xmm6
//	__m128i v152; // xmm2
//	void* v153; // rcx
//	__m128i v154; // xmm1
//	__m128i v155; // xmm2
//	unsigned int* v156; // rcx
//	__int64 v158; // rax
//	float v159; // xmm0_4
//	float v160; // xmm0_4
//	__m128i v161; // xmm1
//	float v162; // xmm10_4
//	__m128i v163; // xmm0
//	__int64 v164; // r9
//	float v165; // xmm1_4
//	float* v166; // rdx
//	float v167; // xmm1_4
//	bool v168; // [rsp+60h] [rbp-A0h]
//	char* currentCharPointer; // [rsp+68h] [rbp-98h] BYREF
//	unsigned int v170; // [rsp+70h] [rbp-90h]
//	unsigned int v171; // [rsp+74h] [rbp-8Ch]
//	float v172; // [rsp+78h] [rbp-88h]
//	float v173; // [rsp+7Ch] [rbp-84h]
//	unsigned int v174; // [rsp+80h] [rbp-80h]
//	__m128 v175; // [rsp+90h] [rbp-70h] BYREF
//	int v176; // [rsp+A0h] [rbp-60h]
//	float v177; // [rsp+A4h] [rbp-5Ch]
//	__m128 v178; // [rsp+B0h] [rbp-50h] BYREF
//	__m128 v179; // [rsp+C0h] [rbp-40h]
//	__m128 v180; // [rsp+D0h] [rbp-30h] BYREF
//	__m128 v181; // [rsp+E0h] [rbp-20h]
//	unsigned int v182; // [rsp+F0h] [rbp-10h]
//	int v183; // [rsp+F4h] [rbp-Ch]
//	float v184; // [rsp+F8h] [rbp-8h]
//	rpakFont* a1a; // [rsp+100h] [rbp+0h]
//	float v186; // [rsp+108h] [rbp+8h]
//	struct_v1* v188; // [rsp+118h] [rbp+18h]
//	__m128 v189; // [rsp+120h] [rbp+20h] BYREF
//	char* v190; // [rsp+130h] [rbp+30h]
//	char v191[8]; // [rsp+138h] [rbp+38h] BYREF
//	__m128i v192; // [rsp+140h] [rbp+40h]
//	unknown8dataStruct* styleDescriptors[4]; // [rsp+150h] [rbp+50h]
//	__m128 v194; // [rsp+170h] [rbp+70h] BYREF
//	__m128 v195; // [rsp+180h] [rbp+80h]
//	rpakFont* fontArray[4]; // [rsp+190h] [rbp+90h]
//	__m128 v197; // [rsp+1B0h] [rbp+B0h]
//	__m128 v198; // [rsp+1C0h] [rbp+C0h]
//	__m128 v199; // [rsp+1D0h] [rbp+D0h]
//	__m128i v200; // [rsp+1E0h] [rbp+E0h]
//	int v201[2]; // [rsp+1F0h] [rbp+F0h] BYREF
//	__m128i v202; // [rsp+1F8h] [rbp+F8h]
//	__m128i v203; // [rsp+208h] [rbp+108h]
//	__m128 v204; // [rsp+220h] [rbp+120h]
//	__m128i v205; // [rsp+230h] [rbp+130h]
//	__m128 v206; // [rsp+240h] [rbp+140h]
//	__m128 v207[7]; // [rsp+250h] [rbp+150h] BYREF
//	__int16 v208; // [rsp+2B0h] [rbp+1B0h]
//	__int16 v209; // [rsp+2B2h] [rbp+1B2h]
//	__int16 v210; // [rsp+2B4h] [rbp+1B4h]
//	__int16 v211; // [rsp+2B6h] [rbp+1B6h]
//	__m128 v212[6]; // [rsp+2C0h] [rbp+1C0h] BYREF
//	__int16 v213; // [rsp+320h] [rbp+220h]
//	__int16 v214; // [rsp+322h] [rbp+222h]
//	__int16 v215; // [rsp+324h] [rbp+224h]
//	__int16 v216; // [rsp+326h] [rbp+226h]
//	__m128 v217[5]; // [rsp+330h] [rbp+230h] BYREF
//
//	v4 = a2->v1;
//	transformIndex = a3->transformIndex;
//	v188 = v4;
//	v8 = _mm_castps_si128(v4->m128_2DD0[transformIndex]);
//	v205 = v8;
//	if (_mm_movemask_ps(_mm_cmpeq_ps(_mm_setzero_ps(), _mm_castsi128_ps(v8))))
//		return 1i64;
//	v10 = &v4->m128_3A80[transformIndex];
//	// test if ui is in screen
//	v11 = _mm_sub_ps(
//		_mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v10->m128_0, 255)), _mm_castsi128_ps(_mm_shuffle_epi32(v10->m128_0, 0))),
//		_mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v10->m128_0, 170)), _mm_castsi128_ps(_mm_shuffle_epi32(v10->m128_0, 85))));
//	if (_mm_movemask_ps(_mm_cmpeq_ps(v11, _mm_setzero_ps())))
//		return 1i64;
//
//	v13 = _mm_castsi128_ps(_mm_shuffle_epi32(v10->m128_10, 216));
//	v14 = _mm_div_ps(_mm_xor_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v10->m128_0, 39)), (__m128)xmmword_5F3E50), v11);
//	v176 = _mm_movemask_ps(v11) & 2;
//	v197 = v14;
//	v15 = _mm_castps_si128(_mm_mul_ps(_mm_xor_ps(v14, (__m128)xmmword_5F3DD0), v13));
//	v16 = _mm_add_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v15, 78)), _mm_castsi128_ps(v15));
//	v198 = v16;
//	v17 = a2->header;
//	v18 = _mm_rcp_ps(_mm_castsi128_ps(v8));
//	v26 = _mm_sub_ps((__m128)xmmword_5F3E90, _mm_mul_ps(v18, _mm_castsi128_ps(v8)));
//	v31 = _mm_castps_si128(_mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(v26, v26), v26), v18), v18));
//	v192 = v31;
//
//	float unk1 = std::numeric_limits<float>::min();
//	styleDescriptorBasePointer = a2->header->unknown8data;
//	for (int i = 0; i < 4; i++)
//	{
//		styleDescriptors[i] = &styleDescriptorBasePointer[a3->styleDescriptorsIndices[i]];
//		fontArray[i] = rpakFontPointers[styleDescriptors[i]->fontIndex];
//		unk1 = fmax(
//			unk1,
//			(float)(*(float*)&a2->dataValues[styleDescriptors[i]->textSize] * fontArray[i]->float_24) -
//				*(float*)&a2->dataValues[styleDescriptors[i]->uint16_32]);
//	}
//
//	v38 = *(float*)_mm_shuffle_epi32(v31, 255).m128i_i32;
//	v39 = v38 * unk1;
//	v172 = v39;
//	v40 = v38 * *(float*)&a2->dataValues[a3->uint16_10];
//	v184 = v40;
//	v42 = *(float*)v31.m128i_i32 * *(float*)&a2->dataValues[a3->uint16_E];
//	v43 = (unsigned __int64)(unsigned int)((_DWORD)a3 - LODWORD(v17->unknown9data)) >> 4;
//	v177 = v42;
//	v44 = v4->unk2[v43].byte_7;
//	if (v44)
//	{
//		v45 = (unsigned __int8)v4->unk2[v43].byte_6;
//		v46 = _mm_setzero_si128();
//		*(float*)v46.m128i_i32 = v39;
//		v47 = _mm_castsi128_ps(_mm_shuffle_epi32(v31, 216));
//		v48 = v45 + v44;
//		v49 = _mm_castsi128_ps(_mm_shuffle_epi32(v46, 17));
//		v180 = (__m128)xmmword_5F4600;
//		while (1)
//		{
//			v50 = *(unsigned __int16*)&v4->gap_28D0[20 * v45];
//			v51 = *((__int16*)&unk_12A2E508 + 4 * v50 + 2);
//			v52 = (char*)&unk_12A2E508 + 8 * v50;
//			v53 = _mm_mul_ps(_mm_castpd_ps(_mm_loaddup_pd((const double*)&v4->gap_28D0[20 * v45 + 4])), v47);
//			v54 = _mm_sub_ps(_mm_mul_ps(_mm_castpd_ps(_mm_loaddup_pd((const double*)&v4->gap_28D0[20 * v45 + 12])), v47), v53);
//			v55 = _mm_add_ps(v49, v53);
//			v56 = _mm_rcp_ps(v54);
//			v57 = *(_QWORD*)rpakUIMGAtlases[(unsigned __int8)v52[6]].textureOffsets + 32 * v51;
//			v58 = *(unsigned __int16*)&v4->gap_28D0[20 * v45 + 2];
//			v59 = _mm_sub_ps(xmmword_5F3E90, _mm_mul_ps(v56, v54));
//			v60 = *(__m128*)v57;
//			v207[6].m128_i16[0] = v51;
//			v207[6].m128_i16[1] = -1;
//			v61 = LOWORD(a4->unk_StartIndex) + *(&a3->styleDescriptorsIndices[0] + v58);
//			v175 = _mm_add_ps(_mm_mul_ps(_mm_xor_ps(v60, (__m128)xmmword_5F3E20), v54), v55);
//			v189 = _mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(v59, v59), v59), v56), v56);
//			v207[6].m128_i16[2] = v61;
//			v207[6].m128_i16[3] = 7936;
//			v62 = _mm_castpd_ps(_mm_loaddup_pd((const double*)(v57 + 24)));
//			v63 = _mm_mul_ps(_mm_mul_ps(_mm_sub_ps(v16, v55), v189), v62);
//			v64 = _mm_castps_si128(_mm_mul_ps(_mm_mul_ps(v14, v189), v62));
//			v178 = _mm_xor_ps(_mm_mul_ps(v189, v55), xmmword_5F3DD0);
//			v65 = _mm_castsi128_ps(_mm_loadl_epi64((const __m128i*)(v57 + 16)));
//			v207[5] = _mm_setzero_ps();
//			v207[3] = _mm_setzero_ps();
//			v207[2] = _mm_add_ps(v63, v65);
//			v207[0] = _mm_castsi128_ps(_mm_shuffle_epi32(v64, 68));
//			v207[4] = _mm_setzero_ps();
//			v207[1] = _mm_castsi128_ps(_mm_shuffle_epi32(v64, 238));
//			v66 = sub_F9B80hk(
//				a1->globals,
//				a2,
//				a4,
//				v207,
//				&v10->m128_0,
//				v176,
//				(__int64)v52,
//				(__m128i*)&v175,
//				&v180,
//				&v178,
//				&v189);
//			if (!v66)
//				return 0i64;
//			v45 = (unsigned int)(v45 + 1);
//			if ((_DWORD)v45 == v48)
//				break;
//		}
//		v42 = v177;
//	}
//	v69 = a4->ruiInstance;
//	v70 = &uiFontAtlases[fontIndices[styleDescriptors[0]->fontIndex]];
//	v71 = a4->unsigned_int_8;
//	v72 = v71;
//	v73 = a4->ruiInstance[v71].uiFontAtlas_8;
//	if (v73 != v70)
//	{
//		if (!v73 || (v74 = a4->dword_40, v69[v71].dword_4 == v74))
//		{
//			v69[v71].uiFontAtlas_8 = v70;
//		}
//		else
//		{
//			v75 = v71 + 1;
//			a4->unsigned_int_8 = v75;
//			if (v75 != a4->dword_C)
//			{
//				v69[v72].dword_4 = v74;
//				v69[v72 + 1].dword_4 = a4->dword_40;
//				v76 = v69[v72].dword_0;
//				v69[v72 + 1].uiFontAtlas_8 = v70;
//				v69[v72 + 1].dword_0 = v76;
//				v69[v72 + 1].uiImageAtlas_10 = 0i64;
//			}
//		}
//	}
//	v77 = 0;
//	selectedFontIndex = 0;
//	printText = *(char**)&a2->dataValues[a3->textOffset];
//	v170 = 0;
//	currentCharPointer = printText;
//
//	// select font
//	if (*printText == '`')
//	{
//		do
//		{
//			selectedFontIndex = currentCharPointer[1] - 48;
//			if (selectedFontIndex >= 4u)
//				break;
//			currentCharPointer += 2;
//			++v77;
//			;
//		} while (*currentCharPointer == '`');
//		v170 = v77;
//	}
//	v82 = (unsigned __int8)v4->unk2[v43].byte_4;
//	v83 = v82 + (unsigned __int8)v4->unk2[v43].byte_5;
//	v84 = 0;
//	v171 = -1;
//	v85 = 0.0;
//	v174 = v82;
//	v182 = v83;
//	if ((unsigned int)v82 < v83)
//	{
//		v86 = *(float*)v8.m128i_i32 - v4->float_25C4[3 * v82 + 2];
//		v87 = LODWORD(v4->float_25C4[3 * v82 + 1]);
//		v174 = v82 + 1;
//		v171 = v87;
//		v85 = v86 * v42;
//	}
//	v88 = v4->unk2[v43].float_0;
//	v173 = 0.0;
//	v186 = v88;
//	sub_FFAE0((__m128*)v10, (const __m128i*)&v17->elementWidth, (__int64)v217);
//	v89 = 0.0;
//	v189 = _mm_setzero_ps();
//	v204 = _mm_castsi128_ps(_mm_shuffle_epi32(v8, 216));
//	while (2)
//	{
//		v181 = _mm_setzero_ps();
//		v179 = _mm_setzero_ps();
//		v175 = _mm_setzero_ps();
//
//		v91 = 0i64;
//		a1a = fontArray[selectedFontIndex];
//		v93 = (unsigned __int16*)styleDescriptors[selectedFontIndex];
//
//		v216 = 0;
//		v215 = LOWORD(a4->unk_StartIndex) + *(&a3->styleDescriptorsIndices[0] + selectedFontIndex);
//		v94 = fmaxf(*(float*)&a2->dataValues[v93[23]], v89);
//		v95 = _mm_load_ps(&v186);
//		v96 = _mm_load_ps((float*)&a2->dataValues[v93[20]]);
//		v95.m128_f32[0] = (float)(v186 * *(float*)&a2->dataValues[v93[21]]) * v96.m128_f32[0];
//		v97 = *(float*)&a2->dataValues[v93[22]];
//		v98 = _mm_unpacklo_ps(v95, v96);
//		v99 = _mm_movelh_ps(v98, v98);
//		v100 = _mm_rcp_ps(v99);
//		v101 = _mm_sub_ps((__m128)xmmword_5F3E90, _mm_mul_ps(v100, v99));
//		v168 = fmaxf(
//				   *(float*)&a2->dataValues[v93[4]],
//				   fminf(fmaxf(*(float*)&a2->dataValues[v93[8]], *(float*)&a2->dataValues[v93[12]]), v97)) > 0.0;
//		*(float*)&v102 = *(float*)v31.m128i_i32 * v95.m128_f32[0];
//		v103 = _mm_unpacklo_ps(_mm_load_ps((float*)&a2->dataValues[v93[17]]), _mm_load_ps((float*)&a2->dataValues[v93[18]]));
//		v104 = _mm_load_ps((float*)&a2->dataValues[v93[19]]);
//		v105 = _mm_shuffle_epi32(v31, 255);
//		v183 = v102;
//		*(float*)v105.m128i_i32 = *(float*)v105.m128i_i32 * v96.m128_f32[0];
//		v106 = _mm_mul_ps(
//			_mm_shuffle_ps(_mm_load_ps((float*)&a2->dataValues[v93[24]]), _mm_load_ps((float*)&a2->dataValues[v93[24]]), 0),
//			(__m128)xmmword_5F3EB0);
//		v107 = _mm_max_ps(
//			_mm_add_ps(
//				_mm_mul_ps(_mm_shuffle_ps(v104, v104, 0), (__m128)xmmword_5F3EB0),
//				_mm_xor_ps(_mm_movelh_ps(v103, v103), (__m128)xmmword_5F3E20)),
//			v106);
//		v106.m128_f32[0] = v97 + v94;
//		v178 = _mm_mul_ps(
//			_mm_xor_ps(_mm_add_ps(v107, _mm_shuffle_ps(v106, v106, 0)), (__m128)xmmword_5F3E20),
//			_mm_castsi128_ps(_mm_shuffle_epi32(v31, 216)));
//		v108 = _mm_castpd_ps(_mm_loaddup_pd((const double*)(&a1a->float_1C)));
//		v109 = v178.m128_f32[3];
//		v110 = v178.m128_f32[1];
//		v201[0] = 4;
//		v201[1] = 4;
//		v111 = 0;
//		v112 = 0;
//		v113 = 0.0;
//		v114 = _mm_setzero_ps();
//		v115 = _mm_setzero_ps();
//		v199 = _mm_mul_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(v101, v101), v101), v100), v100), v204), v108);
//		v200 = _mm_castps_si128(_mm_mul_ps(v197, v199));
//		v206 = _mm_mul_ps(v198, v199);
//		v180 = (__m128)xmmword_5F4610;
//		while (1)
//		{
//			v116 = v170;
//			while (1)
//			{
//				while (1)
//				{
//					currentUnicodeChar = getUnicodeCharacter_GPT(&currentCharPointer);
//					++v116;
//					v118 = currentUnicodeChar;
//					v170 = v116;
//					if (currentUnicodeChar == '%')
//						break;
//					if (currentUnicodeChar || !v84)
//						goto LABEL_35;
//					currentCharPointer = (char*)v207[0].m128_u64[--v84];
//				}
//				v119 = *currentCharPointer;
//				if (*currentCharPointer <= 32 || v119 <= 63 && ((1 << (v119 - 32)) & 0x80005002) != 0)
//					goto LABEL_35;
//				if (v119 == 37)
//					break;
//				v120 = (char*)sub_F98F0(a2, (__int64)a1, &currentCharPointer, (__int64)v191, *(char**)&a2->dataValues[a3->textOffset]);
//				if (!v120)
//					return 1i64;
//				v121 = v84++;
//				v207[0].m128_u64[v121] = (unsigned __int64)currentCharPointer;
//				currentCharPointer = v120;
//			}
//			++currentCharPointer;
//		LABEL_35:
//			if ((unsigned int)(currentUnicodeChar - 1) < 0xEFFFF)
//				v122 = currentUnicodeChar == '`';
//			else
//				v122 = 1;
//			v123 = 0.0;
//			if (v122)
//			{
//				v131 = v175.m128_f32[0];
//				v130 = (float*)v175.m128_u64[1];
//				v125 = a1a;
//			}
//			else
//			{
//				v124 = getFontGlyphIndex(a1a, currentUnicodeChar);
//				v125 = a1a;
//				v91 = &a1a->fontGlyphs[v124];
//				v126 = v91->word_4;
//				v127 = v91[1].word_4;
//				if (v126 >= v127)
//				{
//				LABEL_46:
//					v129 = 0.0;
//				}
//				else
//				{
//					v128 = a1a->pointer_58;
//					while (v128[(unsigned __int16)v126].dword_0 != v111)
//					{
//						v126 = v126 + 1;
//						if ((unsigned __int16)v126 >= v127)
//							goto LABEL_46;
//					}
//					v129 = v128[(unsigned __int16)v126].float_4;
//				}
//				v175.m128_i32[1] = v124;
//				v123 = *(float*)&v102 * v91->float_0;
//				v130 = &v91->float_0;
//				v175.m128_u64[1] = (unsigned __int64)v91;
//				v85 = v85 + (float)(v129 * *(float*)&v102);
//				v131 = v85;
//				v175.m128_f32[0] = v85;
//			}
//			v132 = v116 >= v171;
//			if (v168)
//			{
//				if (v132 || v122)
//				{
//					if (v112 > 1)
//					{
//						v143 = v180;
//					}
//					else
//					{
//						if (!v112)
//							goto LABEL_72;
//						v143 = _mm_add_ps(v180, (__m128)xmmword_5F3EE0);
//						v181 = v179;
//						v180 = v143;
//					}
//					v139 = (float*)v179.m128_u64[1];
//					v138 = v181.m128_u64[1];
//					v112 = 0;
//					v144 = _mm_load_ps((float*)(v181.m128_u64[1] + 20));
//					v145 = _mm_load_ps((float*)(v181.m128_u64[1] + 28));
//					v144.m128_f32[0] = fminf(v144.m128_f32[0], *(float*)(v179.m128_u64[1] + 20));
//					v145.m128_f32[0] = fmaxf(v145.m128_f32[0], *(float*)(v179.m128_u64[1] + 28));
//					v146 = (float)(*(float*)&v102 * *(float*)(v179.m128_u64[1] + 24)) + v179.m128_f32[0];
//					v180 = _mm_add_ps(v143, (__m128)xmmword_5F3EF0);
//					v140 = v146 + v178.m128_f32[2];
//					v144.m128_f32[0] = (float)(v144.m128_f32[0] * *(float*)v105.m128i_i32) + v110;
//					v145.m128_f32[0] = (float)(v145.m128_f32[0] * *(float*)v105.m128i_i32) + v109;
//					v141 = v144;
//					v142 = v145;
//				}
//				else
//				{
//					v133 = *(float*)&v91->gap_7[9];
//					if (v133 == *(float*)&v91->gap_7[17])
//					{
//						v85 = v85 + v123;
//						v111 = v118;
//						v91 = 0i64;
//						continue;
//					}
//					if (v112 <= 1)
//					{
//						v134 = v105;
//						v135 = v105;
//						*(float*)v134.m128i_i32 = (float)(*(float*)v105.m128i_i32 * *(float*)&v91->gap_7[13]) + v110;
//						*(float*)v135.m128i_i32 = (float)(*(float*)v105.m128i_i32 * *(float*)&v91->gap_7[21]) + v109;
//						if (v112)
//						{
//							v136 = _mm_setzero_ps();
//							v137 = _mm_setzero_ps();
//							v136.m128_f32[0] = v114.m128_f32[0];
//							v137.m128_f32[0] = v115.m128_f32[0];
//							v114 = v136;
//							v115 = v137;
//							v114.m128_f32[0] = fminf(v136.m128_f32[0], *(float*)v134.m128i_i32);
//							v115.m128_f32[0] = fmaxf(v137.m128_f32[0], *(float*)v135.m128i_i32);
//						}
//						else
//						{
//							v114 = _mm_castsi128_ps(v134);
//							v115 = _mm_castsi128_ps(v135);
//							v113 = (float)((float)(*(float*)&v102 * v133) + v178.m128_f32[0]) + v85;
//							v180 = _mm_sub_ps(v180, (__m128)xmmword_5F3EE0);
//						}
//						++v112;
//						v85 = v85 + v123;
//					LABEL_60:
//						v181 = v179;
//						v179 = v175;
//					LABEL_61:
//						v111 = v118;
//						v91 = 0i64;
//						continue;
//					}
//					v138 = v181.m128_u64[1];
//					v139 = (float*)v179.m128_u64[1];
//					v141 = _mm_load_ps((float*)(v181.m128_u64[1] + 20));
//					v142 = _mm_load_ps((float*)(v181.m128_u64[1] + 28));
//					v140 = (float)((float)((float)(*(float*)(v181.m128_u64[1] + 24) + v130[4]) * *(float*)&v102) +
//								   (float)(v181.m128_f32[0] + v131)) *
//						   0.5;
//					v141.m128_f32[0] =
//						(float)(fminf(fminf(v141.m128_f32[0], *(float*)(v179.m128_u64[1] + 20)), v130[5]) * *(float*)v105.m128i_i32) +
//						v178.m128_f32[1];
//					v142.m128_f32[0] =
//						(float)(fmaxf(fmaxf(v142.m128_f32[0], *(float*)(v179.m128_u64[1] + 28)), v130[7]) * *(float*)v105.m128i_i32) +
//						v178.m128_f32[3];
//				}
//				v147 = _mm_setzero_ps();
//				v148 = _mm_setzero_ps();
//				v147.m128_f32[0] = v140;
//				v148.m128_f32[0] = v113;
//				v149 = _mm_shuffle_ps(v148, v147, 0);
//				v150 = _mm_castps_si128(_mm_shuffle_ps(
//					_mm_load_ps((float*)(*(_QWORD*)v125->gap_48 + 8i64 * *(unsigned __int8*)(v138 + 7))),
//					_mm_load_ps((float*)(*(_QWORD*)v125->gap_48 + 8i64 * *((unsigned __int8*)v139 + 7))),
//					0));
//				v212[2] = _mm_add_ps(
//					_mm_mul_ps(
//						_mm_sub_ps(
//							v206,
//							_mm_mul_ps(
//								_mm_unpacklo_ps(_mm_unpacklo_ps(v181, v179), _mm_unpacklo_ps(_mm_load_ps(&v172), _mm_load_ps(&v172))),
//								v199)),
//						_mm_castsi128_ps(v150)),
//					_mm_load_ps(v139 + 1));
//				v212[0] = _mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v200, 68)), _mm_castsi128_ps(v150));
//				v212[5] = _mm_castsi128_ps(_mm_shuffle_epi32(v150, 216));
//				v212[1] = _mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v200, 238)), _mm_castsi128_ps(v150));
//				v212[3] = _mm_setzero_ps();
//				v212[4] = _mm_setzero_ps();
//				v213 = *(_WORD*)&v125->gap_28[4] + v181.m128_i16[2];
//				v214 = v179.m128_i16[2] + *(_WORD*)&v125->gap_28[4];
//				v151 = _mm_add_ps(
//					_mm_unpacklo_ps(_mm_unpacklo_ps(v114, v142), _mm_unpacklo_ps(v115, v141)),
//					_mm_shuffle_ps(_mm_load_ps((float*)&v172), _mm_load_ps((float*)&v172), 0));
//				v152 = _mm_load_si128(&v10->m128_0);
//				v194 = _mm_add_ps(
//					_mm_add_ps(
//						_mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v152, 0)), v149),
//						_mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v152, 170)), v151)),
//					_mm_castsi128_ps(_mm_shuffle_epi32(v10->m128_10, 0)));
//				v153 = a1->globals;
//				v195 = _mm_add_ps(
//					_mm_add_ps(
//						_mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v152, 85)), v149),
//						_mm_mul_ps(_mm_castsi128_ps(_mm_shuffle_epi32(v152, 255)), v151)),
//					_mm_castsi128_ps(_mm_shuffle_epi32(v10->m128_10, 85)));
//				sub_FEF30(v153, a2, v217, (__m128i*)&v180, &v194);
//				v154 = _mm_castps_si128(_mm_unpackhi_ps(v194, v195));
//				v155 = _mm_castps_si128(_mm_unpacklo_ps(v194, v195));
//				if (v176 == 2)
//				{
//					v155 = _mm_shuffle_epi32(v155, 78);
//					v154 = _mm_shuffle_epi32(v154, 78);
//				}
//				v202 = v155;
//				v156 = (unsigned int*)a2->pvoid_38;
//				v203 = v154;
//				if (!funcs_5F4560[*v156]((__m128*)v156, v212, (ruiDrawTriangle*)v201, a4))
//					return 0i64;
//				v102 = v183;
//				v113 = v140;
//				v114 = v141;
//				v115 = v142;
//				v109 = v178.m128_f32[3];
//				v110 = v178.m128_f32[1];
//				v180 = _mm_and_ps(v180, (__m128)xmmword_12A146B0);
//			}
//		LABEL_72:
//			if (v132)
//			{
//				v172 = v172 + (float)(*(float*)v105.m128i_i32 + v184);
//				if (v174 >= v182)
//				{
//					v171 = -1;
//					v159 = v188->float_25C4[3 * v182];
//				}
//				else
//				{
//					v158 = 3i64 * v174;
//					v171 = LODWORD(v188->float_25C4[3 * v174++ + 1]);
//					v159 = v188->float_25C4[v158 + 2];
//				}
//				v85 = (float)(*(float*)v205.m128i_i32 - v159) * v177;
//				if (!v91 || (v160 = *(float*)&v91->gap_7[9], v160 == *(float*)&v91->gap_7[17]))
//				{
//					v112 = 0;
//				}
//				else
//				{
//					v161 = v105;
//					v175.m128_f32[0] = v85;
//					v112 = 1;
//					v162 = *(float*)&v102 * v160;
//					v163 = v105;
//					*(float*)v161.m128i_i32 = (float)(*(float*)v105.m128i_i32 * *(float*)&v91->gap_7[21]) + v109;
//					v113 = (float)(v162 + v178.m128_f32[0]) + v85;
//					v115 = _mm_castsi128_ps(v161);
//					*(float*)v163.m128i_i32 = (float)(*(float*)v105.m128i_i32 * *(float*)&v91->gap_7[13]) + v110;
//					v114 = _mm_castsi128_ps(v163);
//				}
//			}
//			v85 = v85 + v123;
//			if (!v122)
//				goto LABEL_60;
//			if (!v118)
//				return 1i64;
//			if (v118 == '`')
//				break;
//			if ((unsigned int)(v118 - 0xF0000) >= 0x2000)
//			{
//				if (v118 != 991233)
//					goto LABEL_61;
//				v85 = v85 + v173;
//				v111 = 991233;
//				v91 = 0i64;
//				v173 = 0.0;
//			}
//			else
//			{
//				v164 = *(__int16*)(assetIndexList->pointer_8 + 8i64 * (unsigned __int16)v118 + 4);
//				v165 = (float)*(unsigned __int16*)(*(_QWORD*)&rpakUIMGAtlases[*(unsigned __int8*)(assetIndexList->pointer_8 +
//																								  8i64 * (unsigned __int16)v118 + 6)]
//														.textureDimensions +
//												   4 * v164);
//				if (v111 == 991232)
//				{
//					if ((unsigned __int16)v164 >=
//						rpakUIMGAtlases[*(unsigned __int8*)(assetIndexList->pointer_8 + 8i64 * (unsigned __int16)v118 + 6)].textureOffsetsCount)
//					{
//						v173 = 0.0;
//						v111 = v118;
//						v85 = v85 + 0.0;
//						v91 = 0i64;
//					}
//					else
//					{
//						v111 = v118;
//						v166 =
//							(float*)((char*)
//										 rpakUIMGAtlases[*(unsigned __int8*)(assetIndexList->pointer_8 + 8i64 * (unsigned __int16)v118 + 6)]
//											 .pointer_20 +
//									 32 * v164);
//						v91 = 0i64;
//						v167 = *(float*)v192.m128i_i32 * v165;
//						v85 = v85 + (float)(v167 * *v166);
//						v173 = v167 * v166[2];
//					}
//				}
//				else
//				{
//					v111 = v118;
//					v91 = 0i64;
//					v85 = v85 +
//						  (float)((float)(v165 / (float)*(unsigned __int16*)(*(_QWORD*)&rpakUIMGAtlases
//																				  [*(unsigned __int8*)(assetIndexList->pointer_8 +
//																									   8i64 * (unsigned __int16)v118 + 6)]
//																					  .textureDimensions +
//																			 4 * v164 + 2)) *
//								  *(float*)&v102);
//				}
//			}
//		}
//		selectedFontIndex = *currentCharPointer - 48;
//		if (selectedFontIndex < 4u)
//		{
//			++currentCharPointer;
//			v31 = v192;
//			v89 = v189.m128_f32[0];
//			continue;
//		}
//		return 1i64;
//	}
//}

//__int64 __fastcall ruiUnknown9Func_0(ruiRenderList *a1, ruiDataStruct *a2, unknown9dataStruct_0 *a3, struct_v3 *a4)
DECLARE_HOOK(
	renderTextHook,
	engine.dll + 0xF5840,
	[](auto& hook,ruiRenderList * a1, ruiDataStruct* a2, unknown9dataStruct_0* a3, struct_v3* a4) -> int64_t {
	//return renderText(a1, a2, a3, a4);
	return hook.Original(a1, a2, a3, a4);
	});
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

	auto storeTriangle = [&](__m128 quad0, __m128 quad1)
	{
		_mm_storeu_ps(&tri.vert[0][0], quad0);
		_mm_storeu_ps(&tri.vert[1][0], quad1);
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

		storeTriangle(quad0, quad1);
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
	// weird one
	xmmword_5F3E20 = *module.Offset(0x5F3E20).RCast<__m128*>();
	//const __m128 signMask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000u));
	//xmmword_5F3E20 = signMask;
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
	unk_12A2E508 = module.Offset(0x12A2E508).RCast<char*>();

	xmmword_12A4E830 = module.Offset(0x12A4E830).RCast<__m128*>();
	funcs_5F4560 = module.Offset(0x5F4560).RCast<funcs5F4560Type*>();
	sub_FC0C0 = module.Offset(0xFC0C0).RCast<sub_FC0C0Type>();
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
});
