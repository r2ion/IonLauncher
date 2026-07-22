#include "rtech/rui/imageatlas_internal.h"
#include "rtech/rui/rui_internal.h"

RuiImageAtlas g_RuiImageAtlases[RUI_IMAGE_ATLAS_CAPACITY];
RuiFontAtlas* g_RuiFontAtlases;
RuiImageAssetDescriptor* g_RuiImageDescriptors;
RuiFont** g_RuiFonts;
uint8_t* g_RuiFontAtlasIndices;
RHashMapU32* g_RuiImageDescriptorMap;
__m128* g_RuiEllipseAxisMasks;
__m128* g_RuiEdgeCorrectionMasks;
RuiDrawInfoHandlerFn* g_RuiDrawInfoHandlers;

BindImageAtlasFn s_BindImageAtlas;
ReadUnicodeCharacterFn s_ReadUnicodeCharacter;
GetFontGlyphIndexFn s_GetFontGlyphIndex;
ResolveTextEscapeFn s_ResolveTextEscape;
BuildEdgeCorrectionFn s_BuildEdgeCorrection;
ApplyEdgeCorrectionFn s_ApplyEdgeCorrection;

__m128 g_RuiSignMaskAll;
__m128 g_RuiSignMaskLowHalf;
__m128 g_RuiSignMaskMiddleLanes;
__m128 g_RuiSignMaskHighHalf;
__m128 g_RuiSignMaskLane2;
__m128 g_RuiFloatTwo;
__m128 g_RuiFloatOne;
__m128 g_RuiFloatHalf;
__m128 g_RuiFloatFour;
__m128 g_RuiFloatMinNormal;
__m128 g_RuiFloatAbsMask;
__m128 g_RuiUnitX;
__m128 g_RuiUnitY;
__m128 g_RuiHighHalfOne;
__m128 g_RuiHighHalfSignedOne;
__m128 g_RuiQuarterEndpoints;
__m128 g_RuiIntOne;
__m128 g_RuiIntTwo;

__m128 g_RuiBlendMaskLowHalf;
__m128 g_RuiBlendMaskLane0;
__m128 g_RuiBlendMaskLane1;
__m128 g_RuiBlendMaskHighHalf;

__m128 g_RuiSinApproxCoeff0;
__m128 g_RuiSinApproxCoeff1;
__m128 g_RuiSinApproxCoeff2;
__m128 g_RuiSinApproxCoeff3;
__m128 g_RuiCosApproxCoeff0;
__m128 g_RuiCosApproxCoeff1;
__m128 g_RuiCosApproxCoeff2;
__m128 g_RuiCosApproxCoeff3;

ON_DLL_LOAD("rtech_game.DLL", RuiRtechRuntime, [](CModule module)
{
	(void)module;
	RuiImageAtlas_DispatchRtechHooks();
});

ON_DLL_LOAD("engine.dll", RuiEngineRuntime, [](CModule module)
{
	RuiImageAtlas_OnEngineLoaded(module);
	RuiRender_DispatchHooks();
	RuiText_DispatchHooks();
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
	g_RuiImageDescriptorMap = module.Offset(0x12A4E508).RCast<RHashMapU32*>();

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
	g_RuiEdgeCorrectionMasks = module.Offset(0x5F4740).RCast<__m128*>();
	g_RuiFontAtlasIndices = module.Offset(0x12A4E650).RCast<BYTE*>();
});
