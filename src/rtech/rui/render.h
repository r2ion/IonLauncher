#pragma once

#include <immintrin.h>

struct RuiBaseUv;
struct RuiDrawBatch;
struct RuiGlobalState;
struct RuiImageAssetDescriptor;
struct RuiInstance;
struct RuiTransform;

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
