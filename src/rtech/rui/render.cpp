#include "rtech/rui/rui_internal.h"

#include <cstdint>
#include <cstring>
#include <immintrin.h>

DECLARE_MODULE(RuiRenderHooks)

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
	const uint32_t materialBatchIndex = batch->materialBatchIndex;
	RuiDrawMaterialBatch* materialBatches = batch->materialBatches;
	const uint8_t atlasIndex = descriptor->atlasIndex;
	RuiImageAtlas* imageAtlas = &g_RuiImageAtlases[atlasIndex];

	if (materialBatches[materialBatchIndex].imageAtlas != imageAtlas)
	{
		RuiImageAtlas* currentAtlas = materialBatches[materialBatchIndex].imageAtlas;

		if (!currentAtlas || materialBatches[materialBatchIndex].firstIndex == batch->indexBufferSize)
		{
			materialBatches[materialBatchIndex].imageAtlas = imageAtlas;
		}
		else
		{
			materialBatches[materialBatchIndex].firstIndex = batch->indexBufferSize;

			if (++batch->materialBatchIndex == batch->materialBatchCapacity)
				return false;

			materialBatches[materialBatchIndex + 1].firstIndex = batch->indexBufferSize;
			materialBatches[materialBatchIndex + 1].fontAtlas = nullptr;
			materialBatches[materialBatchIndex + 1].firstVertex = materialBatches[materialBatchIndex].firstVertex;
			materialBatches[materialBatchIndex + 1].imageAtlas = imageAtlas;
		}
	}

	RuiDrawQuad quad;
	RuiBaseUv drawUv;
	__m128 correctionData[5];

	quad.vertexCount = 4;
	quad.vertexCapacity = 4;

	const uint16_t correctionMask = (~baseUv->flags >> 8) & 0xF;
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
			&quad,
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

		_mm_storeu_ps(&quad.positions[0][0], quad0);
		_mm_storeu_ps(&quad.positions[1][0], quad1);
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
		_mm_storeu_ps(&quad.positions[0][0], quad0);
		_mm_storeu_ps(&quad.positions[1][0], quad1);

		RuiDrawInfo* drawInfo = rui->drawInfo;
		return g_RuiDrawInfoHandlers[static_cast<uint32_t>(drawInfo->mode)](
			drawInfo,
			baseUv,
			&quad,
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
	const auto* styleDescriptors = reinterpret_cast<const RuiStyleType2DescriptorOffsets*>(rui->header->styleDescriptors);
	const RuiStyleType2DescriptorOffsets& style = styleDescriptors[styleIndex];
	if (dataFloat(style.common.primaryColor.alpha) <= 0.0f)
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
	const float edgeSoftness = dataFloat(style.edgeSoftness);

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
	const float stretchCorrectionX = ((transformHeight * edgeSoftness) * (texMaxX - texMinX)) / transformWidth;
	const float stretchCorrectionY = (texMaxY - texMinY) * edgeSoftness;
	const __m128 stretchCorrection = _mm_setr_ps(stretchCorrectionX, stretchCorrectionY, stretchCorrectionX, stretchCorrectionY);

	const __m128 textureBounds = _mm_loadu_ps(textureRecord.pixelBounds);
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



bool RuiRenderImageJob(
	RuiRenderContext* context,
	RuiInstance* rui,
	const RuiImageRenderJob* job,
	RuiDrawBatch* batch)
{
	const uint16_t styleIndex = job->styleIndex;
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

	if (dataFloat(styleOffsets->common.primaryColor.alpha) <= 0.0f)
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
	const int16_t assetIndex = primaryAsset->imageIndex;
	int16_t secondaryAssetIndex = -1;
	uint16_t flags = job->flags | static_cast<uint8_t>(primaryAsset->flags);

	const int secondaryAssetDescriptorIndex = dataInt(job->maskImageOffset);
	if (secondaryAssetDescriptorIndex != -1)
	{
		const auto* secondaryAsset = &g_RuiImageDescriptors[secondaryAssetDescriptorIndex];
		if (atlasIndex != secondaryAsset->atlasIndex)
			return true;

		secondaryAssetIndex = secondaryAsset->imageIndex;
		flags |= static_cast<uint16_t>(4 * static_cast<uint8_t>(secondaryAsset->flags));
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

	const __m128 axisMask = g_RuiEllipseAxisMasks[(flags >> 4) & 3];
	const __m128 primaryTextureOffset = _mm_loadu_ps(primaryTextureRecord.pixelBounds);
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
	baseUv.computedStyleIndex = static_cast<int16_t>(styleIndex + batch->computedStyleCount);

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

void RuiRender_DispatchHooks()
{
	DISPATCH_MODULE(RuiRenderHooks);
}
