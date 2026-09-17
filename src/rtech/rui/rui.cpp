#include "rtech/rui/rui.h"

#include "rtech/rui/atlas.h"
#include "rtech/rui/font.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <immintrin.h>
#include <limits>

bool RuiIsIconCodePoint(uint32_t codepoint)
{
    return codepoint - RUI_CODEPOINT_ICON_FIRST < RUI_CODEPOINT_ICON_COUNT;
}

bool RuiIsStringDiv(uint32_t codepoint)
{
    return codepoint == 0 || codepoint == '`' || codepoint >= RUI_CODEPOINT_ICON_FIRST;
}

bool RuiIsStandalonePercent(char marker)
{
    const int markerIndex = static_cast<int>(static_cast<int8_t>(marker)) - ' ';
    return markerIndex <= 0 || (markerIndex <= '?' - ' ' && ((1u << markerIndex) & RUI_STANDALONE_PERCENT_MASK) != 0);
}

bool RuiInvertTransform(const RuiTransform& transform, RuiInverseTransform& inverse)
{
    const float determinant = transform.gradient[3] * transform.gradient[0] - transform.gradient[2] * transform.gradient[1];
    if (determinant == 0.0f)
        return false;

    const float reciprocal = 1.0f / determinant;
    inverse.gradient[0] = transform.gradient[3] * reciprocal;
    inverse.gradient[1] = -transform.gradient[1] * reciprocal;
    inverse.gradient[2] = -transform.gradient[2] * reciprocal;
    inverse.gradient[3] = transform.gradient[0] * reciprocal;
    inverse.origin.x = -(inverse.gradient[0] * transform.origin[0] + inverse.gradient[2] * transform.origin[1]);
    inverse.origin.y = -(inverse.gradient[1] * transform.origin[0] + inverse.gradient[3] * transform.origin[1]);
    inverse.sign = determinant < 0.0f ? RuiTransformSign::Negative : RuiTransformSign::Positive;
    return true;
}

bool RuiClipImageBounds(const RuiImageAtlasEntry& image, const RuiBounds& requested, const Vector2D& uvMin, const Vector2D& uvMax,
                        const uint32_t* clampMasks, uint16_t flags, const float* usefulPadding, RuiBounds& clipped)
{
    const float uvExtent[4] = {uvMax.x - uvMin.x, uvMax.y - uvMin.y, uvMax.x - uvMin.x, uvMax.y - uvMin.y};
    const float minimumUv[4] = {
        std::min(uvMin.x, uvMax.x),
        std::min(uvMin.y, uvMax.y),
        std::min(uvMin.x, uvMax.x),
        std::min(uvMin.y, uvMax.y),
    };
    float usefulBounds[4];
    std::copy_n(image.usefulBounds, 4, usefulBounds);

    const uint32_t* clampMask = &clampMasks[((flags >> 4) & 3) * 4];
    for (size_t lane = 0; lane < 4; ++lane)
    {
        if (!clampMask[lane])
            continue;

        const float extent = std::abs(uvExtent[lane]);
        if (extent <= std::numeric_limits<float>::min())
            return false;

        if (usefulPadding)
            usefulBounds[lane] += usefulPadding[lane];
        usefulBounds[lane] = lane < 2 ? (usefulBounds[lane] + minimumUv[lane]) / extent : (usefulBounds[lane] - minimumUv[lane]) / extent;
    }

    clipped = {
        .left = -std::min(-requested.left, usefulBounds[0]),
        .top = -std::min(-requested.top, usefulBounds[1]),
        .right = std::min(requested.right, usefulBounds[2]),
        .bottom = std::min(requested.bottom, usefulBounds[3]),
    };
    return std::isfinite(clipped.left) && std::isfinite(clipped.top) && std::isfinite(clipped.right) && std::isfinite(clipped.bottom) &&
           clipped.right > clipped.left && clipped.bottom > clipped.top;
}

void RuiProjectBounds(const RuiTransform& transform, const RuiBounds& bounds, RuiProjectedQuad& positions)
{
    const float localX[4] = {bounds.left, bounds.left, bounds.right, bounds.right};
    const float localY[4] = {bounds.top, bounds.bottom, bounds.bottom, bounds.top};
    for (size_t vertex = 0; vertex < 4; ++vertex)
    {
        positions.x[vertex] = transform.gradient[0] * localX[vertex] + transform.gradient[2] * localY[vertex] + transform.origin[0];
        positions.y[vertex] = transform.gradient[1] * localX[vertex] + transform.gradient[3] * localY[vertex] + transform.origin[1];
    }
}

void RuiStoreQuad(RuiDrawQuad& quad, const RuiProjectedQuad& positions, RuiTransformSign transformSign)
{
    const bool reversed = transformSign == RuiTransformSign::Negative;
    const size_t order[4] = {
        reversed ? 1u : 0u,
        reversed ? 0u : 1u,
        reversed ? 3u : 2u,
        reversed ? 2u : 3u,
    };
    for (size_t vertex = 0; vertex < 4; ++vertex)
    {
        quad.positions[vertex / 2][(vertex % 2) * 2] = positions.x[order[vertex]];
        quad.positions[vertex / 2][(vertex % 2) * 2 + 1] = positions.y[order[vertex]];
    }
}

void RuiBuildPrimaryMapping(RuiBaseUv& mapping, const RuiInverseTransform& inverse, const RuiImageAtlasEntry& image, const Vector2D& uvMin,
                            const Vector2D& uvMax)
{
    const float uvWidth = uvMax.x - uvMin.x;
    const float uvHeight = uvMax.y - uvMin.y;
    mapping.primaryBasisX[0] = inverse.gradient[0] * uvWidth * image.uvGradient[0];
    mapping.primaryBasisX[1] = inverse.gradient[1] * uvHeight * image.uvGradient[1];
    mapping.primaryBasisY[0] = inverse.gradient[2] * uvWidth * image.uvGradient[0];
    mapping.primaryBasisY[1] = inverse.gradient[3] * uvHeight * image.uvGradient[1];
    mapping.primaryOrigin[0] = (inverse.origin.x * uvWidth + uvMin.x) * image.uvGradient[0] + image.uvOrigin[0];
    mapping.primaryOrigin[1] = (inverse.origin.y * uvHeight + uvMin.y) * image.uvGradient[1] + image.uvOrigin[1];
}

void RuiBuildDefaultMaskMapping(RuiBaseUv& mapping, const RuiInverseTransform& inverse, const RuiBounds& bounds)
{
    const float width = std::max(bounds.right - bounds.left, std::numeric_limits<float>::min());
    const float height = std::max(bounds.bottom - bounds.top, std::numeric_limits<float>::min());
    mapping.primaryBasisX[2] = inverse.gradient[0] / width;
    mapping.primaryBasisX[3] = inverse.gradient[1] / height;
    mapping.primaryBasisY[2] = inverse.gradient[2] / width;
    mapping.primaryBasisY[3] = inverse.gradient[3] / height;
    mapping.primaryOrigin[2] = (inverse.origin.x - bounds.left) / width;
    mapping.primaryOrigin[3] = (inverse.origin.y - bounds.top) / height;
}

void RuiBuildImageMaskMapping(RuiBaseUv& mapping, const RuiInstance& rui, const RuiImageRenderJob& job, const RuiInverseTransform& inverse,
                              const RuiImageAtlasEntry& image, const Vector2D& uvMin, const Vector2D& uvMax)
{
    const Vector2D center = {rui.GetValue<float>(job.maskCenterOffsets.x), rui.GetValue<float>(job.maskCenterOffsets.y)};
    const Vector2D translation = {rui.GetValue<float>(job.maskTranslationOffsets.x), rui.GetValue<float>(job.maskTranslationOffsets.y)};
    const Vector2D scale = {rui.GetValue<float>(job.maskScaleOffsets.x), rui.GetValue<float>(job.maskScaleOffsets.y)};
    const float angle = rui.GetValue<float>(job.maskRotationOffset) * 6.2831853071795864769f;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float uvWidth = uvMax.x - uvMin.x;
    const float uvHeight = uvMax.y - uvMin.y;
    const Vector2D pivot = {uvMin.x + center.x * uvWidth, uvMin.y + center.y * uvHeight};

    const float matrix00 = cosine * scale.x;
    const float matrix01 = -sine * scale.x;
    const float matrix10 = sine * scale.y;
    const float matrix11 = cosine * scale.y;
    const float localOriginX = inverse.origin.x * uvWidth + uvMin.x - pivot.x;
    const float localOriginY = inverse.origin.y * uvHeight + uvMin.y - pivot.y;

    mapping.primaryBasisX[2] = (matrix00 * inverse.gradient[0] * uvWidth + matrix01 * inverse.gradient[1] * uvHeight) * image.uvGradient[0];
    mapping.primaryBasisX[3] = (matrix10 * inverse.gradient[0] * uvWidth + matrix11 * inverse.gradient[1] * uvHeight) * image.uvGradient[1];
    mapping.primaryBasisY[2] = (matrix00 * inverse.gradient[2] * uvWidth + matrix01 * inverse.gradient[3] * uvHeight) * image.uvGradient[0];
    mapping.primaryBasisY[3] = (matrix10 * inverse.gradient[2] * uvWidth + matrix11 * inverse.gradient[3] * uvHeight) * image.uvGradient[1];
    mapping.primaryOrigin[2] =
        (pivot.x + translation.x + matrix00 * localOriginX + matrix01 * localOriginY) * image.uvGradient[0] + image.uvOrigin[0];
    mapping.primaryOrigin[3] =
        (pivot.y + translation.y + matrix10 * localOriginX + matrix11 * localOriginY) * image.uvGradient[1] + image.uvOrigin[1];
}

void RuiBuildInlineMapping(RuiBaseUv& mapping, const RuiInverseTransform& inverse, const RuiImageAtlasEntry& image, const RuiBounds& bounds)
{
    const float inverseWidth = 1.0f / (bounds.right - bounds.left);
    const float inverseHeight = 1.0f / (bounds.bottom - bounds.top);
    const float basisXU = inverse.gradient[0] * inverseWidth * image.uvGradient[0];
    const float basisXV = inverse.gradient[1] * inverseHeight * image.uvGradient[1];
    const float basisYU = inverse.gradient[2] * inverseWidth * image.uvGradient[0];
    const float basisYV = inverse.gradient[3] * inverseHeight * image.uvGradient[1];
    mapping.primaryBasisX.Init(basisXU, basisXV, basisXU, basisXV);
    mapping.primaryBasisY.Init(basisYU, basisYV, basisYU, basisYV);
    const float originU = (inverse.origin.x - bounds.left) * inverseWidth * image.uvGradient[0] + image.uvOrigin[0];
    const float originV = (inverse.origin.y - bounds.top) * inverseHeight * image.uvGradient[1] + image.uvOrigin[1];
    mapping.primaryOrigin.Init(originU, originV, originU, originV);
}

bool RuiDrawBatch_BindImageAtlas(RuiDrawBatch* batch, RuiImageAtlas* atlas)
{
    RuiDrawMaterialBatch& materialBatch = batch->materialBatches[batch->materialBatchIndex];
    if (materialBatch.imageAtlas == atlas)
        return true;

    if (!materialBatch.imageAtlas || materialBatch.firstIndex == batch->indexBufferSize)
    {
        materialBatch.imageAtlas = atlas;
        return true;
    }

    materialBatch.firstIndex = batch->indexBufferSize;
    if (++batch->materialBatchIndex == batch->materialBatchCapacity)
        return false;

    RuiDrawMaterialBatch& nextMaterialBatch = batch->materialBatches[batch->materialBatchIndex];
    nextMaterialBatch.firstVertex = materialBatch.firstVertex;
    nextMaterialBatch.firstIndex = batch->indexBufferSize;
    nextMaterialBatch.fontAtlas = nullptr;
    nextMaterialBatch.imageAtlas = atlas;
    return true;
}

void RuiBuildAntiAliasSetup(const RuiTransform* transform, const float* elementSize, RuiAntiAliasSetup* setup)
{
    fltx4 repeatedElementSize = _mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(elementSize)));
    repeatedElementSize = _mm_unpacklo_ps(repeatedElementSize, repeatedElementSize);

    const fltx4 gradient = _mm_load_ps(transform->gradient);
    const fltx4 scaledGradient = _mm_mul_ps(repeatedElementSize, gradient);
    const fltx4 squaredGradient = _mm_mul_ps(scaledGradient, scaledGradient);
    const fltx4 swappedSquaredGradient = _mm_shuffle_ps(squaredGradient, squaredGradient, 0xB1);
    const fltx4 squaredLength = _mm_max_ps(_mm_set1_ps(std::numeric_limits<float>::min()), _mm_add_ps(squaredGradient, swappedSquaredGradient));

    fltx4 reciprocalLength = _mm_rsqrt_ps(squaredLength);
    fltx4 correction = _mm_mul_ps(_mm_set1_ps(0.5f), squaredLength);
    correction = _mm_mul_ps(correction, reciprocalLength);
    correction = _mm_mul_ps(correction, reciprocalLength);
    correction = _mm_sub_ps(_mm_set1_ps(1.5f), correction);
    reciprocalLength = _mm_mul_ps(correction, reciprocalLength);

    const fltx4 normalizedGradient = _mm_mul_ps(reciprocalLength, gradient);
    _mm_store_ps(setup->normGradX_x.Base(), _mm_shuffle_ps(normalizedGradient, normalizedGradient, 0x00));
    _mm_store_ps(setup->normGradX_y.Base(), _mm_shuffle_ps(normalizedGradient, normalizedGradient, 0x55));
    _mm_store_ps(setup->normGradY_x.Base(), _mm_shuffle_ps(normalizedGradient, normalizedGradient, 0xAA));
    _mm_store_ps(setup->normGradY_y.Base(), _mm_shuffle_ps(normalizedGradient, normalizedGradient, 0xFF));
    _mm_store_ps(setup->cornerExtraScaleSq.Base(), _mm_set1_ps(std::bit_cast<float>(uint32_t{0x3ED413CD})));
}

uint32_t RuiDecodeCodePoint(char** cursor)
{
    const auto* bytes = reinterpret_cast<const uint8_t*>(*cursor);
    const uint8_t first = bytes[0];
    if (first < 0x80)
    {
        ++*cursor;
        return first;
    }

    const uint8_t second = bytes[1];
    if ((second & 0xC0) != 0x80)
        return 0;

    if (first < 0xE0)
    {
        if (first < 0xC2)
            return 0;

        *cursor += 2;
        return (static_cast<uint32_t>(first & 0x3F) << 6) | (second & 0x3F);
    }

    const uint8_t third = bytes[2];
    if ((third & 0xC0) != 0x80)
        return 0;

    uint32_t codepoint = (static_cast<uint32_t>(first & 0x0F) << 12) | (static_cast<uint32_t>(second & 0x3F) << 6) | (third & 0x3F);
    if (first < 0xF0)
    {
        if (codepoint - 0xD800u <= 0x7FFu)
            return 0;

        *cursor += 3;
        return codepoint;
    }

    const uint8_t fourth = bytes[3];
    if ((fourth & 0xC0) != 0x80)
        return 0;

    codepoint = (codepoint << 6) | (fourth & 0x3F);
    if (codepoint > 0x10FFFF)
        return 0;

    *cursor += 4;
    return codepoint;
}

uint64_t RuiFont_GetGlyphIndex(RuiFont* font, int32_t codepoint)
{
    for (;;)
    {
        const uint32_t relativeCodepoint = static_cast<uint32_t>(codepoint) - static_cast<uint32_t>(font->unicodeChunkBase);
        const uint32_t chunkIndex = relativeCodepoint >> 6;
        if (chunkIndex < font->unicodeChunkCount)
        {
            const uint32_t bitIndex = relativeCodepoint & 0x3F;
            const uint16_t packedChunkIndex = font->unicodeChunks[chunkIndex];
            const uint64_t chunkMask = font->unicodeChunkMasks[packedChunkIndex];
            const uint64_t codepointBit = uint64_t{1} << bitIndex;
            if (chunkMask & codepointBit)
            {
                return static_cast<uint64_t>(font->unicodeChunkIndices[packedChunkIndex]) + std::popcount(chunkMask & (codepointBit - 1));
            }
        }

        if (codepoint == RUI_CODEPOINT_MISSING_GLYPH)
        {
            spdlog::warn("Font {} doesn't have the code point U+{:04X} which is required to display a missing glyph.", font->name,
                         static_cast<uint32_t>(RUI_CODEPOINT_MISSING_GLYPH));
            return UINT32_MAX;
        }
        codepoint = RUI_CODEPOINT_MISSING_GLYPH;
    }
}
