#pragma once

#include "rtech/rhashmap.h"
#include "rtech/rui/rui_types.h"
#include "tier0/module.h"

#define RUI_SHUFFLE_PS(value, imm) _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(value), imm))
#define RUI_SHUFFLE_I32_AS_PS(value, imm) _mm_castsi128_ps(_mm_shuffle_epi32((value), imm))

extern RuiImageAtlas g_RuiImageAtlases[RUI_IMAGE_ATLAS_CAPACITY];
extern RuiFontAtlas* g_RuiFontAtlases;
extern RuiImageAssetDescriptor* g_RuiImageDescriptors;
extern RuiFont** g_RuiFonts;
extern uint8_t* g_RuiFontAtlasIndices;
extern RHashMapU32* g_RuiImageDescriptorMap;
extern __m128* g_RuiEllipseAxisMasks;
extern __m128* g_RuiEdgeCorrectionMasks;
extern RuiDrawInfoHandlerFn* g_RuiDrawInfoHandlers;

extern BindImageAtlasFn s_BindImageAtlas;
extern ReadUnicodeCharacterFn s_ReadUnicodeCharacter;
extern GetFontGlyphIndexFn s_GetFontGlyphIndex;
extern ResolveTextEscapeFn s_ResolveTextEscape;
extern BuildEdgeCorrectionFn s_BuildEdgeCorrection;
extern ApplyEdgeCorrectionFn s_ApplyEdgeCorrection;

extern __m128 g_RuiSignMaskAll;
extern __m128 g_RuiSignMaskLowHalf;
extern __m128 g_RuiSignMaskMiddleLanes;
extern __m128 g_RuiSignMaskHighHalf;
extern __m128 g_RuiSignMaskLane2;
extern __m128 g_RuiFloatTwo;
extern __m128 g_RuiFloatOne;
extern __m128 g_RuiFloatHalf;
extern __m128 g_RuiFloatFour;
extern __m128 g_RuiFloatMinNormal;
extern __m128 g_RuiFloatAbsMask;
extern __m128 g_RuiUnitX;
extern __m128 g_RuiUnitY;
extern __m128 g_RuiHighHalfOne;
extern __m128 g_RuiHighHalfSignedOne;
extern __m128 g_RuiQuarterEndpoints;
extern __m128 g_RuiIntOne;
extern __m128 g_RuiIntTwo;

extern __m128 g_RuiBlendMaskLowHalf;
extern __m128 g_RuiBlendMaskLane0;
extern __m128 g_RuiBlendMaskLane1;
extern __m128 g_RuiBlendMaskHighHalf;

extern __m128 g_RuiSinApproxCoeff0;
extern __m128 g_RuiSinApproxCoeff1;
extern __m128 g_RuiSinApproxCoeff2;
extern __m128 g_RuiSinApproxCoeff3;
extern __m128 g_RuiCosApproxCoeff0;
extern __m128 g_RuiCosApproxCoeff1;
extern __m128 g_RuiCosApproxCoeff2;
extern __m128 g_RuiCosApproxCoeff3;

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
	const __m128* viewportScale);

void RuiRender_DispatchHooks();
void RuiText_DispatchHooks();
