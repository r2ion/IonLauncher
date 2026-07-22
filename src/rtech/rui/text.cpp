#include "rtech/rui/rui_internal.h"

#include <cstdint>
#include <cstring>
#include <immintrin.h>

DECLARE_MODULE(RuiTextHooks)

bool __fastcall RuiRenderTextJob(
	RuiRenderContext* context,
	RuiInstance* rui,
	const RuiTextRenderJob* job,
	RuiDrawBatch* batch)
{
	RuiRuntimeState* runtime = rui->runtime;
	const uint16_t transformIndex = job->transformIndex;
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
	auto* descriptors = reinterpret_cast<RuiTextStyleDescriptorOffsets*>(header->styleDescriptors);
	RuiTextStyleDescriptorOffsets* textStyles[4] = {
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
		return dataFloat(textStyles[styleIndex]->textSize) * fonts[styleIndex]->ascentFraction
			- dataFloat(textStyles[styleIndex]->ascentAdjustment);
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
			const int16_t assetIndex = assetDescriptor->imageIndex;
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

			const __m128 textureOffset = _mm_loadu_ps(textureRecord.pixelBounds);
			const __m128 atlasUv = _mm_add_ps(_mm_mul_ps(_mm_xor_ps(textureOffset, g_RuiSignMaskLowHalf), imageExtent), imageBase);
			const __m128 atlasScale = _mm_castpd_ps(_mm_loaddup_pd(reinterpret_cast<const double*>(textureRecord.uvScale)));
			const __m128 inlineMaskBase = _mm_xor_ps(_mm_mul_ps(refinedImageExtentReciprocal, imageBase), g_RuiSignMaskAll);
			const __m128 inlineMaskTransform = _mm_mul_ps(_mm_mul_ps(_mm_sub_ps(originSum, imageBase), refinedImageExtentReciprocal), atlasScale);
			const __m128 inlineMaskBasis = _mm_mul_ps(_mm_mul_ps(inverseBasis, refinedImageExtentReciprocal), atlasScale);

			RuiBaseUv imageUv{};
			imageUv.imageIndex = assetIndex;
			imageUv.maskImageIndex = -1;
			imageUv.computedStyleIndex = static_cast<int16_t>(
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
		const uint16_t defaultFontIndex = textStyles[0]->fontIndex;
		RuiDrawMaterialBatch* materialBatches = batch->materialBatches;
		const uint8_t fontAtlasIndex = g_RuiFontAtlasIndices[defaultFontIndex];
		RuiFontAtlas* fontAtlas = &g_RuiFontAtlases[fontAtlasIndex];
		const uint32_t materialBatchIndex = batch->materialBatchIndex;
		RuiFontAtlas* currentFontAtlas = materialBatches[materialBatchIndex].fontAtlas;
		if (currentFontAtlas != fontAtlas)
		{
			if (!currentFontAtlas || materialBatches[materialBatchIndex].firstIndex == batch->indexBufferSize)
			{
				materialBatches[materialBatchIndex].fontAtlas = fontAtlas;
			}
			else
			{
				const uint32_t nextMaterialBatchIndex = materialBatchIndex + 1;
				batch->materialBatchIndex = nextMaterialBatchIndex;
				if (nextMaterialBatchIndex != batch->materialBatchCapacity)
				{
					materialBatches[materialBatchIndex].firstIndex = batch->indexBufferSize;
					materialBatches[nextMaterialBatchIndex].firstIndex = batch->indexBufferSize;
					materialBatches[nextMaterialBatchIndex].fontAtlas = fontAtlas;
					materialBatches[nextMaterialBatchIndex].firstVertex = materialBatches[materialBatchIndex].firstVertex;
					materialBatches[nextMaterialBatchIndex].imageAtlas = nullptr;
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
		return runtime->textLines[lineIndex].breakGlyph;
	};
	uint32_t nextLineGlyph = static_cast<uint32_t>(-1);
	uint32_t lineCursor = lineBegin;
	float currentAdvance = 0.0f;
	if (lineCursor < lineEnd)
	{
		nextLineGlyph = lineBreakGlyph(lineCursor);
		currentAdvance = (transformSize.m128_f32[0] - runtime->textLines[lineCursor].wrappedWidth) * horizontalAlignScale;
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
		const RuiTextStyleDescriptorOffsets& style = *textStyles[activeStyle];
		const uint16_t styleDescriptorIndex = static_cast<uint16_t>(
			batch->computedStyleCount + job->styleIndices[activeStyle]);

		RuiBaseUv glyphUv{};
		glyphUv.flags = 0;
		glyphUv.computedStyleIndex = styleDescriptorIndex;

		const float baselineOffset = maxScalar(dataFloat(style.distanceBias), previousLineMax);
		__m128 glyphScaleX = _mm_set_ss(lineHeightScale);
		__m128 glyphScaleY = _mm_set_ss(dataFloat(style.textSize));
		glyphScaleX.m128_f32[0] = (lineHeightScale * dataFloat(style.horizontalStretch)) * glyphScaleY.m128_f32[0];

		const float lineExtra = dataFloat(style.strokeWidth);
		const __m128 glyphScale = _mm_movelh_ps(_mm_unpacklo_ps(glyphScaleX, glyphScaleY), _mm_unpacklo_ps(glyphScaleX, glyphScaleY));
		const __m128 refinedGlyphScaleReciprocal = refineReciprocal(glyphScale);
		const bool styleRenderable =
			maxScalar(
				dataFloat(style.common.primaryColor.alpha),
				minScalar(
					maxScalar(dataFloat(style.common.secondaryColor.alpha), dataFloat(style.common.tertiaryColor.alpha)),
					lineExtra)) > 0.0f;
		const float glyphAdvanceScale = refinedTransformSizeReciprocal.m128_f32[0] * glyphScaleX.m128_f32[0];
		const __m128 styleOffset = _mm_unpacklo_ps(
			_mm_set_ss(dataFloat(style.shadowOffsetX)),
			_mm_set_ss(dataFloat(style.shadowOffsetY)));
		__m128 textHeight = _mm_set_ss(dataFloat(style.shadowFilterWidth));
		__m128 glyphScaleYScreen = RUI_SHUFFLE_PS(refinedTransformSizeReciprocal, 255);
		glyphScaleYScreen.m128_f32[0] *= glyphScaleY.m128_f32[0];
		const __m128 symmetricEffectHalfWidth = _mm_mul_ps(_mm_set1_ps(dataFloat(style.filterWidth)), g_RuiFloatHalf);
		const __m128 effectBoundsPadding = _mm_max_ps(
			_mm_add_ps(_mm_mul_ps(_mm_set1_ps(textHeight.m128_f32[0]), g_RuiFloatHalf), _mm_xor_ps(_mm_movelh_ps(styleOffset, styleOffset), g_RuiSignMaskLowHalf)),
			symmetricEffectHalfWidth);
		textHeight.m128_f32[0] = lineExtra + baselineOffset;
		const __m128 glyphBoundsOffset = _mm_mul_ps(
			_mm_xor_ps(_mm_add_ps(effectBoundsPadding, _mm_set1_ps(textHeight.m128_f32[0])), g_RuiSignMaskLowHalf),
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
				glyphUv.imageIndex = static_cast<int16_t>(font->atlasGlyphBase + static_cast<int16_t>(firstGlyphState.glyphIndex));
				glyphUv.maskImageIndex = static_cast<int16_t>(font->atlasGlyphBase + static_cast<int16_t>(lastGlyphState.glyphIndex));

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
					currentAdvance = transformSize.m128_f32[0] - runtime->GetTextLineTerminalWidth(lineEnd);
				}
				else
				{
					nextLineGlyph = lineBreakGlyph(lineCursor);
					currentAdvance = transformSize.m128_f32[0] - runtime->textLines[lineCursor].wrappedWidth;
					++lineCursor;
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
				const auto* unicodeAssetTable =
					static_cast<const RuiImageAssetDescriptor*>(g_RuiImageDescriptorMap->entryStorage);
				const uint16_t unicodeAssetIndex = static_cast<uint16_t>(codepoint);
				const RuiImageAssetDescriptor& unicodeAsset = unicodeAssetTable[unicodeAssetIndex];
				const int16_t unicodeTextureIndex = unicodeAsset.imageIndex;
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

__m128 __fastcall RuiMeasureTextJob(RuiInstance* rui, uint32_t renderJobOffset)
{
	RuiRuntimeState* runtime = rui->runtime;
	const auto* job = reinterpret_cast<const RuiTextRenderJob*>(
		rui->header->renderJobs + renderJobOffset);
	RuiRenderContext* includeContext = runtime->textContext;
	auto* descriptors = reinterpret_cast<RuiTextStyleDescriptorOffsets*>(rui->header->styleDescriptors);
	RuiTextStyleDescriptorOffsets* textStyles[4] = {
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
			textSizes[styleIndex] * dataFloat(textStyles[styleIndex]->horizontalStretch);
		ascents[styleIndex] =
			(textSizes[styleIndex] * fonts[styleIndex]->ascentFraction)
			- dataFloat(textStyles[styleIndex]->ascentAdjustment);
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

		runtime->textLines[lineIndex].breakGlyph = breakGlyph;
		runtime->textLines[lineIndex].wrappedWidth = width;
		runtime->GetTextLineTerminalWidth(lineIndex + 1) = 0.0f;
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
				static_cast<const RuiImageAssetDescriptor*>(g_RuiImageDescriptorMap->entryStorage);
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
		runtime->GetTextLineTerminalWidth(finalLineCount) = currentAdvance;

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

void RuiText_DispatchHooks()
{
	DISPATCH_MODULE(RuiTextHooks);
}
