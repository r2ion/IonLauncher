//=============================================================================//
//
// Purpose: Generalized RUI rendering helpers
//
//=============================================================================//

#include "modsystem/modatlas.h"
#include "rtech/rui/atlas.h"
#include "rtech/rui/font.h"
#include "rtech/rui/rui.h"
#include "rtech/rui/topology.h"
#include "tier0/module.h"
#include "tier1/convar.h"
#include "tier1/strtools.h"
#include "tools/particleeditor/particletoolssystem.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

DECLARE_MODULE(RuiRenderHooks)

static RuiTessellate_t* g_RuiDrawInfoHandlers;
static decltype(RuiFunctionTable_t::findImageAsset) RuiFindImageAsset;
static RuiEvaluateProjectionBasis_t RuiEvaluateMeshBasis;
static RuiEvaluateProjectionBasis_t RuiEvaluateAngularBasis;
static ConVar* g_pRuiPadDistance;

const uint32_t* g_RuiLrtbClampMasksByMode;
const Vector4D* g_uiAaLrtbScaleForEdgeMask;

RuiFontAtlas* g_RuiFontAtlases;
RuiFont** g_RuiFonts;
uint8_t* g_RuiFontAtlasIndices;

char* RuiText_ResolveEscape(RuiInstance* rui, RuiRenderContext* context, char** cursor, char* scratch)
{
    char* token = *cursor;
    char* closingPercent = std::strchr(token, '%');
    if (!closingPercent)
        return nullptr;
    *cursor = closingPercent + 1;

    if (*token == '$')
    {
        char* imageNameEnd = closingPercent;
        if (imageNameEnd[-1] == '[')
            --imageNameEnd;
        const size_t imageNameLength = static_cast<size_t>(imageNameEnd - (token + 1));
        if (imageNameLength >= 100)
            return nullptr;

        char imageName[100];
        std::memcpy(imageName, token + 1, imageNameLength);
        imageName[imageNameLength] = '\0';
        const RuiImageHandle image = RuiFindImageAsset(rui, imageName);
        if (image == -1)
            return const_cast<char*>("");

        const uint32_t codepoint = static_cast<uint32_t>(image) + RUI_CODEPOINT_ICON_FIRST;
        scratch[0] = static_cast<char>((codepoint >> 18) | 0xF0);
        scratch[1] = static_cast<char>(((codepoint >> 12) & 0x3F) | 0x80);
        scratch[2] = static_cast<char>(((codepoint >> 6) & 0x3F) | 0x80);
        scratch[3] = static_cast<char>((codepoint & 0x3F) | 0x80);
        scratch[4] = '\0';
        return scratch;
    }

    if (*token == '<' || *token == '>')
    {
        if (closingPercent != token + 1)
            return nullptr;
        const uint32_t codepoint = *token == '<' ? RUI_CODEPOINT_IMAGE_BORDER_BEGIN : RUI_CODEPOINT_IMAGE_BORDER_END;
        scratch[0] = static_cast<char>((codepoint >> 18) | 0xF0);
        scratch[1] = static_cast<char>(((codepoint >> 12) & 0x3F) | 0x80);
        scratch[2] = static_cast<char>(((codepoint >> 6) & 0x3F) | 0x80);
        scratch[3] = static_cast<char>((codepoint & 0x3F) | 0x80);
        scratch[4] = '\0';
        return scratch;
    }

    const auto& table = *context->stringTable;
    char* keyBegin = token;
    char* keyEnd = closingPercent;
    const char openingBracket = *token;
    if ((static_cast<unsigned char>(openingBracket) | 0x20u) == '{')
    {
        if (closingPercent[-1] != openingBracket + 2)
            return nullptr;
        char* separator = std::strchr(token, '|');
        if (!separator || separator > closingPercent)
            return nullptr;
        if (table.usePrimaryVariant)
        {
            keyBegin = token + 1;
            keyEnd = separator;
        }
        else
        {
            keyBegin = separator + 1;
            keyEnd = closingPercent - 1;
        }
    }

    const size_t keyLength = static_cast<size_t>(keyEnd - keyBegin);
    if (keyLength < table.minimumKeyLength || keyLength > table.maximumKeyLength)
        return table.fallback;

    uint32_t first = table.lengthBuckets[keyLength];
    uint32_t last = table.lengthBuckets[keyLength + 1];
    while (first < last)
    {
        const uint32_t middle = (first + last) / 2;
        const int comparison = String_CompareInsensitiveN(keyBegin, table.entries[middle].key, keyLength);
        if (comparison < 0)
            last = middle;
        else if (comparison > 0)
            first = middle + 1;
        else
            return table.entries[middle].value;
    }
    return table.fallback;
}

static fltx4 RuiProjectionDot(const FourVectors& vector, const float row[4], bool point)
{
    fltx4 result = MulSIMD(vector.x, ReplicateX4(row[0]));
    result = AddSIMD(result, MulSIMD(vector.y, ReplicateX4(row[1])));
    result = AddSIMD(result, MulSIMD(vector.z, ReplicateX4(row[2])));
    if (point)
        result = AddSIMD(result, ReplicateX4(row[3]));
    return result;
}

void RuiApplyAntiAliasPadding(RuiGlobalState* globals, RuiInstance* rui, const RuiAntiAliasSetup* setup, const Vector4D* lrtbScale,
                              RuiProjectedQuad* positions)
{
    FourVectors point;
    FourVectors axisX;
    FourVectors axisY;
    if (static_cast<uint32_t>(rui->drawInfo->mode) <= static_cast<uint32_t>(RuiDrawInfoMode::Clipped))
    {
        const RuiProjectionBasis& basis = reinterpret_cast<const RuiDrawInfoPlanar*>(rui->drawInfo)->basis;
        alignas(16) float origin[4];
        alignas(16) float basisX[4];
        alignas(16) float basisY[4];
        StoreAlignedSIMD(origin, basis.positionOrigin);
        StoreAlignedSIMD(basisX, basis.positionBasisX);
        StoreAlignedSIMD(basisY, basis.positionBasisY);

        const fltx4 surfaceX = LoadAlignedSIMD(positions->x);
        const fltx4 surfaceY = LoadAlignedSIMD(positions->y);
        for (size_t component = 0; component < 3; ++component)
        {
            point[component] = MulSIMD(surfaceX, ReplicateX4(basisX[component]));
            point[component] = AddSIMD(point[component], ReplicateX4(origin[component]));
            point[component] = AddSIMD(point[component], MulSIMD(surfaceY, ReplicateX4(basisY[component])));
            axisX[component] = ReplicateX4(basisX[component]);
            axisY[component] = ReplicateX4(basisY[component]);
        }
    }
    else if (rui->drawInfo->mode == RuiDrawInfoMode::Mesh)
    {
        RuiEvaluateMeshBasis(rui, positions, &point, &axisX, &axisY);
    }
    else
    {
        RuiEvaluateAngularBasis(rui, positions, &point, &axisX, &axisY);
    }

    const fltx4 scale = LoadUnalignedSIMD(lrtbScale->Base());
    const fltx4 horizontalScale = ShuffleXXYY(scale);
    const fltx4 verticalScale = _mm_shuffle_ps(scale, scale, MM_SHUFFLE_REV(2, 3, 3, 2));
    const fltx4 normGradX_x = LoadUnalignedSIMD(setup->normGradX_x.Base());
    const fltx4 normGradX_y = LoadUnalignedSIMD(setup->normGradX_y.Base());
    const fltx4 normGradY_x = LoadUnalignedSIMD(setup->normGradY_x.Base());
    const fltx4 normGradY_y = LoadUnalignedSIMD(setup->normGradY_y.Base());
    const fltx4 localOffsetX = AddSIMD(MulSIMD(normGradX_x, horizontalScale), MulSIMD(normGradY_x, verticalScale));
    const fltx4 localOffsetY = AddSIMD(MulSIMD(normGradX_y, horizontalScale), MulSIMD(normGradY_y, verticalScale));

    FourVectors direction = axisX;
    direction *= localOffsetX;
    FourVectors verticalDirection = axisY;
    verticalDirection *= localOffsetY;
    direction += verticalDirection;

    const fltx4 clipW = RuiProjectionDot(point, globals->viewProjection[3], true);
    const fltx4 directionW = RuiProjectionDot(direction, globals->viewProjection[3], false);
    const fltx4 directionClipX = RuiProjectionDot(direction, globals->viewProjection[0], false);
    const fltx4 pointClipX = RuiProjectionDot(point, globals->viewProjection[0], true);
    const fltx4 directionClipY = RuiProjectionDot(direction, globals->viewProjection[1], false);
    const fltx4 pointClipY = RuiProjectionDot(point, globals->viewProjection[1], true);
    const fltx4 projectedX = MulSIMD(SubSIMD(MulSIMD(directionClipX, clipW), MulSIMD(pointClipX, directionW)), ReplicateX4(globals->viewportWidth));
    const fltx4 projectedY = MulSIMD(SubSIMD(MulSIMD(directionClipY, clipW), MulSIMD(pointClipY, directionW)), ReplicateX4(globals->viewportHeight));

    const fltx4 padDiameter = ReplicateX4(g_pRuiPadDistance->GetFloat() * 2.0f);
    const fltx4 padDiameterSq = MulSIMD(padDiameter, padDiameter);
    const fltx4 cornerScale = LoadUnalignedSIMD(setup->cornerExtraScaleSq.Base());
    const fltx4 cornerPadding = MulSIMD(MulSIMD(MulSIMD(horizontalScale, verticalScale), cornerScale), padDiameterSq);
    const fltx4 padding = AddSIMD(cornerPadding, padDiameterSq);
    const fltx4 clipWSqPadding = MulSIMD(MulSIMD(padding, clipW), clipW);
    const fltx4 projectedLengthSq = AddSIMD(MulSIMD(projectedX, projectedX), MulSIMD(projectedY, projectedY));
    const fltx4 directionWSqPadding = MulSIMD(MulSIMD(clipWSqPadding, directionW), directionW);
    const fltx4 discriminant = SubSIMD(projectedLengthSq, directionWSqPadding);

    const fltx4 zero = _mm_setzero_ps();
    const fltx4 positive = CmpGtSIMD(discriminant, zero);
    const fltx4 reciprocalInput = MaxSIMD(ReplicateX4(std::numeric_limits<float>::min()), discriminant);
    const fltx4 reciprocalEstimate = ReciprocalEstSIMD(reciprocalInput);
    const fltx4 reciprocalError = SubSIMD(ReplicateX4(1.0f), MulSIMD(reciprocalEstimate, reciprocalInput));
    const fltx4 reciprocalCorrection = AddSIMD(MulSIMD(reciprocalError, reciprocalError), reciprocalError);
    const fltx4 reciprocal = AddSIMD(reciprocalEstimate, MulSIMD(reciprocalCorrection, reciprocalEstimate));

    const fltx4 linearBase = MulSIMD(clipWSqPadding, clipW);
    const fltx4 linearTerm = MulSIMD(linearBase, directionW);
    const fltx4 rootExpression = AddSIMD(MulSIMD(MulSIMD(linearBase, clipW), discriminant), MulSIMD(linearTerm, linearTerm));
    const fltx4 squareRootTerm = SqrtSIMD(AndSIMD(rootExpression, positive));
    const fltx4 distance = MulSIMD(AndSIMD(AddSIMD(squareRootTerm, linearTerm), positive), reciprocal);

    const fltx4 paddedX = AddSIMD(LoadAlignedSIMD(positions->x), MulSIMD(distance, localOffsetX));
    const fltx4 paddedY = AddSIMD(LoadAlignedSIMD(positions->y), MulSIMD(distance, localOffsetY));
    const fltx4 nextX = RotateLeft(paddedX);
    const fltx4 nextY = RotateLeft(paddedY);
    const fltx4 edgeX = SubSIMD(nextX, paddedX);
    const fltx4 edgeY = SubSIMD(nextY, paddedY);
    const fltx4 cross = SubSIMD(MulSIMD(RotateRight(edgeY), edgeX), MulSIMD(RotateRight(edgeX), edgeY));
    const fltx4 basisOrientation = SubSIMD(MulSIMD(normGradY_y, normGradX_x), MulSIMD(normGradX_y, normGradY_x));
    const fltx4 invalid = CmpLeSIMD(MulSIMD(cross, basisOrientation), zero);
    StoreAlignedSIMD(positions->x, MaskedAssign(invalid, nextX, paddedX));
    StoreAlignedSIMD(positions->y, MaskedAssign(invalid, nextY, paddedY));
}

static float RuiRefinedReciprocal(float value)
{
    const fltx4 input = _mm_set_ss(value);
    const fltx4 estimate = _mm_rcp_ss(input);
    const fltx4 error = _mm_sub_ss(_mm_set_ss(1.0f), _mm_mul_ss(estimate, input));
    const fltx4 correction = _mm_add_ss(_mm_mul_ss(error, error), error);
    return _mm_cvtss_f32(_mm_add_ss(estimate, _mm_mul_ss(correction, estimate)));
}

bool RuiDrawImageAtlasEntry(RuiGlobalState* globals, RuiInstance* rui, RuiDrawBatch* batch, const RuiBaseUv* baseUv, const RuiTransform* transform,
                            RuiTransformSign orientation, const RuiResolvedImageAsset& image, const RuiBounds* clippedBounds,
                            const RuiBounds* geometryBounds, const Vector4D* uvMin, const Vector4D* uvExtent)
{
    RuiImageAtlas* atlas = image.atlas;
    if (!atlas)
        return true;
    if (!RuiDrawBatch_BindImageAtlas(batch, atlas))
        return false;
    if (!rui->drawInfo || static_cast<uint32_t>(rui->drawInfo->mode) >= 4)
        return false;

    const uint16_t edgeMask = static_cast<uint16_t>((~baseUv->flags >> 8) & 0xF);
    RuiAntiAliasSetup antiAlias;
    if (edgeMask)
        RuiBuildAntiAliasSetup(transform, &rui->header->elementWidth, &antiAlias);

    if (image.imageIndex < 0 || static_cast<uint16_t>(image.imageIndex) >= atlas->nineSliceImageCount || !atlas->nineSliceData)
    {
        RuiProjectedQuad positions;
        RuiProjectBounds(*transform, *clippedBounds, positions);
        if (edgeMask)
            RuiApplyAntiAliasPadding(globals, rui, &antiAlias, &g_uiAaLrtbScaleForEdgeMask[edgeMask], &positions);
        RuiDrawQuad quad{.vertexCount = 4, .vertexCapacity = 4};
        RuiStoreQuad(quad, positions, orientation);
        return g_RuiDrawInfoHandlers[static_cast<uint32_t>(rui->drawInfo->mode)](rui->drawInfo, baseUv, &quad, batch);
    }

    const RuiImageAtlasNineSlice& nineSlice = atlas->nineSliceData[image.imageIndex];
    const float inverseUvX = RuiRefinedReciprocal((*uvExtent)[0]);
    const float inverseUvY = RuiRefinedReciprocal((*uvExtent)[1]);
    const float transformedXx = rui->actualWidth * transform->gradient[0] * inverseUvX;
    const float transformedXy = rui->actualHeight * transform->gradient[1] * inverseUvX;
    const float transformedYx = rui->actualWidth * transform->gradient[2] * inverseUvY;
    const float transformedYy = rui->actualHeight * transform->gradient[3] * inverseUvY;
    const float transformedPixelsX = std::sqrt(transformedXx * transformedXx + transformedXy * transformedXy);
    const float transformedPixelsY = std::sqrt(transformedYx * transformedYx + transformedYy * transformedYy);
    const float slopeX = std::max(transformedPixelsX * nineSlice.edgeScale[0], nineSlice.minimumEdgeSize[0]);
    const float slopeY = std::max(transformedPixelsY * nineSlice.edgeScale[1], nineSlice.minimumEdgeSize[1]);
    const float totalX = nineSlice.normalizedBounds[0] + nineSlice.normalizedBounds[2];
    const float totalY = nineSlice.normalizedBounds[1] + nineSlice.normalizedBounds[3];
    const float centerFractionX = 1.0f - totalX;
    const float centerFractionY = 1.0f - totalY;
    const float centerReciprocalX = RuiRefinedReciprocal(slopeX - totalX);
    const float centerReciprocalY = RuiRefinedReciprocal(slopeY - totalY);
    const float slopeReciprocalX = RuiRefinedReciprocal(slopeX);
    const float slopeReciprocalY = RuiRefinedReciprocal(slopeY);

    const float sliceLeft = (nineSlice.normalizedBounds[0] * slopeReciprocalX - (*uvMin)[0]) * inverseUvX;
    const float sliceTop = (nineSlice.normalizedBounds[1] * slopeReciprocalY - (*uvMin)[1]) * inverseUvY;
    const float sliceRight = (1.0f - nineSlice.normalizedBounds[2] * slopeReciprocalX - (*uvMin)[2]) * inverseUvX;
    const float sliceBottom = (1.0f - nineSlice.normalizedBounds[3] * slopeReciprocalY - (*uvMin)[3]) * inverseUvY;
    const float gridX[4] = {clippedBounds->left, sliceLeft, sliceRight, clippedBounds->right};
    const float gridY[4] = {clippedBounds->top, sliceTop, sliceBottom, clippedBounds->bottom};

    const float mappingScaleX[3] = {slopeX, centerFractionX * slopeX * centerReciprocalX, slopeX};
    const float mappingScaleY[3] = {slopeY, centerFractionY * slopeY * centerReciprocalY, slopeY};
    const float mappingBiasX[3] = {
        0.0f,
        nineSlice.normalizedBounds[0] - nineSlice.normalizedBounds[0] * centerFractionX * centerReciprocalX,
        1.0f - slopeX,
    };
    const float mappingBiasY[3] = {
        0.0f,
        nineSlice.normalizedBounds[1] - nineSlice.normalizedBounds[1] * centerFractionY * centerReciprocalY,
        1.0f - slopeY,
    };

    for (size_t row = 0; row < 3; ++row)
    {
        for (size_t column = 0; column < 3; ++column)
        {
            if (gridX[column + 1] <= geometryBounds->left || gridX[column] >= geometryBounds->right || gridY[row + 1] <= geometryBounds->top ||
                gridY[row] >= geometryBounds->bottom)
            {
                continue;
            }

            RuiBaseUv mapping{};
            mapping.primaryBasisX = baseUv->primaryBasisX;
            mapping.primaryBasisY = baseUv->primaryBasisY;
            mapping.primaryOrigin = baseUv->primaryOrigin;
            mapping.primaryBasisX[0] *= mappingScaleX[column];
            mapping.primaryBasisX[1] *= mappingScaleY[row];
            mapping.primaryBasisY[0] *= mappingScaleX[column];
            mapping.primaryBasisY[1] *= mappingScaleY[row];
            mapping.primaryOrigin[0] = mapping.primaryOrigin[0] * mappingScaleX[column] + mappingBiasX[column];
            mapping.primaryOrigin[1] = mapping.primaryOrigin[1] * mappingScaleY[row] + mappingBiasY[row];
            mapping.imageIndex = baseUv->imageIndex;
            mapping.maskImageIndex = baseUv->maskImageIndex;
            mapping.computedStyleIndex = baseUv->computedStyleIndex;
            mapping.flags = baseUv->flags;

            const RuiBounds pieceBounds = {gridX[column], gridY[row], gridX[column + 1], gridY[row + 1]};
            RuiProjectedQuad positions;
            RuiProjectBounds(*transform, pieceBounds, positions);
            const uint16_t pieceEdges = static_cast<uint16_t>(edgeMask & ((column == 0   ? 1
                                                                           : column == 2 ? 2
                                                                                         : 0) |
                                                                          (row == 0   ? 4
                                                                           : row == 2 ? 8
                                                                                      : 0)));
            if (pieceEdges)
                RuiApplyAntiAliasPadding(globals, rui, &antiAlias, &g_uiAaLrtbScaleForEdgeMask[pieceEdges], &positions);

            RuiDrawQuad quad{.vertexCount = 4, .vertexCapacity = 4};
            RuiStoreQuad(quad, positions, orientation);
            if (!g_RuiDrawInfoHandlers[static_cast<uint32_t>(rui->drawInfo->mode)](rui->drawInfo, &mapping, &quad, batch))
                return false;
        }
    }
    return true;
}

void RuiApplyAntiAlias(RuiGlobalState& globals, RuiInstance& rui, const RuiTransform& transform, uint16_t flags, RuiProjectedQuad& positions)
{
    const uint16_t edgeMask = static_cast<uint16_t>((~flags >> 8) & 0xF);
    if (!edgeMask)
        return;

    RuiAntiAliasSetup setup;
    RuiBuildAntiAliasSetup(&transform, &rui.header->elementWidth, &setup);
    RuiApplyAntiAliasPadding(&globals, &rui, &setup, &g_uiAaLrtbScaleForEdgeMask[edgeMask], &positions);
}

bool RuiDrawImage(RuiGlobalState& globals, RuiInstance& rui, RuiDrawBatch& batch, const RuiTransform& transform, const RuiInverseTransform& inverse,
                  const RuiResolvedImageAsset& image, const RuiResolvedImageAsset* mask, const RuiImageRenderJob* job, const RuiBounds& bounds,
                  const Vector2D& uvMin, const Vector2D& uvMax, uint16_t flags, uint16_t styleIndex, bool buildDefaultMask)
{
    if (!image.atlas || !image.atlas->images || image.imageIndex < 0 || static_cast<uint16_t>(image.imageIndex) >= image.atlas->imageCount)
    {
        return true;
    }
    if (mask && (mask->atlas != image.atlas || mask->imageIndex < 0 || static_cast<uint16_t>(mask->imageIndex) >= image.atlas->imageCount))
        return true;

    RuiBounds clipped;
    if (!RuiClipImageBounds(image.atlas->images[image.imageIndex], bounds, uvMin, uvMax, g_RuiLrtbClampMasksByMode, flags, nullptr, clipped))
        return true;

    RuiBaseUv mapping{};
    RuiBuildPrimaryMapping(mapping, inverse, image.atlas->images[image.imageIndex], uvMin, uvMax);
    if (mask && job)
        RuiBuildImageMaskMapping(mapping, rui, *job, inverse, image.atlas->images[mask->imageIndex], uvMin, uvMax);
    else if (buildDefaultMask)
        RuiBuildDefaultMaskMapping(mapping, inverse, bounds);

    mapping.imageIndex = image.imageIndex;
    mapping.maskImageIndex = mask ? mask->imageIndex : -1;
    mapping.computedStyleIndex = static_cast<int16_t>(batch.computedStyleCount + styleIndex);
    mapping.flags = flags;

    alignas(16) const Vector4D repeatedUvMin{uvMin.x, uvMin.y, uvMin.x, uvMin.y};
    alignas(16) const Vector4D uvExtent{uvMax.x - uvMin.x, uvMax.y - uvMin.y, uvMax.x - uvMin.x, uvMax.y - uvMin.y};
    return RuiDrawImageAtlasEntry(&globals, &rui, &batch, &mapping, &transform, inverse.sign, image, &clipped, &bounds, &repeatedUvMin, &uvExtent);
}

bool RuiDrawInlineImage(RuiGlobalState& globals, RuiInstance& rui, RuiDrawBatch& batch, const RuiTransform& transform,
                        const RuiInverseTransform& inverse, const RuiResolvedImageAsset& image, const RuiBounds& bounds, uint16_t styleIndex)
{
    if (!image.atlas || !image.atlas->images || image.imageIndex < 0 || static_cast<uint16_t>(image.imageIndex) >= image.atlas->imageCount)
    {
        return true;
    }

    const float width = bounds.right - bounds.left;
    const float height = bounds.bottom - bounds.top;
    if (width <= 0.0f || height <= 0.0f)
        return true;

    const RuiImageAtlasEntry& entry = image.atlas->images[image.imageIndex];
    RuiBaseUv mapping{};
    RuiBuildInlineMapping(mapping, inverse, entry, bounds);
    mapping.imageIndex = image.imageIndex;
    mapping.maskImageIndex = -1;
    mapping.computedStyleIndex = static_cast<int16_t>(batch.computedStyleCount + styleIndex);
    mapping.flags = 0x1F00;

    alignas(16) const RuiBounds usefulBounds = {
        bounds.left - entry.usefulBounds[0] * width,
        bounds.top - entry.usefulBounds[1] * height,
        bounds.left + entry.usefulBounds[2] * width,
        bounds.top + entry.usefulBounds[3] * height,
    };
    if (image.atlasHandle < RUI_RUNTIME_IMAGE_ATLAS_HANDLE_BASE)
    {
        alignas(16) const RuiBounds clipBounds = {0.0f, 0.0f, 1.0f, 1.0f};
        alignas(16) const Vector4D uvMin{-bounds.left / width, -bounds.top / height, -bounds.left / width, -bounds.top / height};
        alignas(16) const Vector4D uvExtent{1.0f / width, 1.0f / height, 1.0f / width, 1.0f / height};
        return RuiDrawImageAtlasEntry(&globals, &rui, &batch, &mapping, &transform, inverse.sign, image, &usefulBounds, &clipBounds, &uvMin,
                                      &uvExtent);
    }

    if (static_cast<uint16_t>(image.imageIndex) < image.atlas->nineSliceImageCount)
        return true;
    if (!RuiDrawBatch_BindImageAtlas(&batch, image.atlas))
        return false;

    RuiProjectedQuad positions;
    RuiProjectBounds(transform, usefulBounds, positions);
    RuiDrawQuad quad{.vertexCount = 4, .vertexCapacity = 4};
    RuiStoreQuad(quad, positions, inverse.sign);
    if (!rui.drawInfo || static_cast<uint32_t>(rui.drawInfo->mode) >= 4)
        return false;
    return g_RuiDrawInfoHandlers[static_cast<uint32_t>(rui.drawInfo->mode)](rui.drawInfo, &mapping, &quad, &batch);
}

bool RuiRenderDynamicImage(RuiRenderContext& context, RuiInstance& rui, const RuiImageRenderJob& job, RuiDrawBatch& batch)
{
    const RuiStyleDescriptorOffsets& style = rui.header->styleDescriptors[job.styleIndex];
    if (rui.GetValue<float>(style.common.primaryColor.alpha) <= 0.0f)
        return true;

    RuiResolvedImageAsset image;
    const RuiImageHandle imageHandle = rui.GetValue<RuiImageHandle>(job.imageOffset);
    if (!RuiResolveImageAsset(imageHandle, image))
        return true;

    RuiResolvedImageAsset mask;
    RuiResolvedImageAsset* maskPointer = nullptr;
    const RuiImageHandle maskHandle = rui.GetValue<RuiImageHandle>(job.maskImageOffset);
    if (maskHandle != -1)
    {
        if (!RuiResolveImageAsset(maskHandle, mask))
            return true;
        maskPointer = &mask;
    }

    const RuiTransform& transform = rui.runtime->transforms[job.transformIndex];
    RuiInverseTransform inverse;
    if (!RuiInvertTransform(transform, inverse))
        return true;

    if (maskPointer && mask.atlas != image.atlas)
        return true;

    const RuiBounds bounds = {
        rui.GetValue<float>(job.boundsMinOffsets.x),
        rui.GetValue<float>(job.boundsMinOffsets.y),
        rui.GetValue<float>(job.boundsMaxOffsets.x),
        rui.GetValue<float>(job.boundsMaxOffsets.y),
    };
    const Vector2D uvMin = {rui.GetValue<float>(job.uvMinOffsets.x), rui.GetValue<float>(job.uvMinOffsets.y)};
    const Vector2D uvMax = {rui.GetValue<float>(job.uvMaxOffsets.x), rui.GetValue<float>(job.uvMaxOffsets.y)};
    uint16_t flags = static_cast<uint16_t>(job.flags | image.flags);
    if (maskPointer)
        flags = static_cast<uint16_t>(flags | (static_cast<uint16_t>(mask.flags) << 2));

    return RuiDrawImage(*context.globals, rui, batch, transform, inverse, image, maskPointer, &job, bounds, uvMin, uvMax, flags, job.styleIndex,
                        maskPointer == nullptr);
}

bool RuiRenderDynamicEllipse(RuiRenderContext& context, RuiInstance& rui, const RuiEllipseRenderJob& job, RuiDrawBatch& batch)
{
    const auto* styles = reinterpret_cast<const RuiEllipseStyleDescriptorOffsets*>(rui.header->styleDescriptors);
    const RuiEllipseStyleDescriptorOffsets& style = styles[job.styleIndex];
    if (rui.GetValue<float>(style.common.primaryColor.alpha) <= 0.0f)
        return true;

    RuiResolvedImageAsset image;
    if (!RuiResolveImageAsset(rui.GetValue<RuiImageHandle>(job.imageOffset), image) || !image.atlas || !image.atlas->images || image.imageIndex < 0 ||
        static_cast<uint16_t>(image.imageIndex) >= image.atlas->imageCount ||
        static_cast<uint16_t>(image.imageIndex) < image.atlas->nineSliceImageCount)
    {
        return true;
    }

    const RuiTransform& transform = rui.runtime->transforms[job.transformIndex];
    RuiInverseTransform inverse;
    if (!RuiInvertTransform(transform, inverse))
        return true;

    const float transformWidth = rui.runtime->transformSizes[job.transformIndex][0];
    const float transformHeight = rui.runtime->transformSizes[job.transformIndex][2];
    if (std::min(transformWidth, transformHeight) <= 0.0f)
        return true;

    const RuiBounds bounds = {
        rui.GetValue<float>(job.boundsMinOffsets.x),
        rui.GetValue<float>(job.boundsMinOffsets.y),
        rui.GetValue<float>(job.boundsMaxOffsets.x),
        rui.GetValue<float>(job.boundsMaxOffsets.y),
    };
    const Vector2D uvMin = {rui.GetValue<float>(job.uvMinOffsets.x), rui.GetValue<float>(job.uvMinOffsets.y)};
    const Vector2D uvMax = {rui.GetValue<float>(job.uvMaxOffsets.x), rui.GetValue<float>(job.uvMaxOffsets.y)};
    const uint16_t flags = static_cast<uint16_t>(job.flags | image.flags);
    const float edgeSoftness = rui.GetValue<float>(style.edgeSoftness);
    const float usefulPadding[4] = {
        transformHeight * edgeSoftness * (uvMax.x - uvMin.x) / transformWidth,
        (uvMax.y - uvMin.y) * edgeSoftness,
        transformHeight * edgeSoftness * (uvMax.x - uvMin.x) / transformWidth,
        (uvMax.y - uvMin.y) * edgeSoftness,
    };

    RuiBounds clipped;
    if (!RuiClipImageBounds(image.atlas->images[image.imageIndex], bounds, uvMin, uvMax, g_RuiLrtbClampMasksByMode, flags, usefulPadding, clipped))
        return true;
    if (!RuiDrawBatch_BindImageAtlas(&batch, image.atlas))
        return false;

    RuiBaseUv mapping{};
    RuiBuildPrimaryMapping(mapping, inverse, image.atlas->images[image.imageIndex], uvMin, uvMax);
    mapping.primaryBasisX[2] = inverse.gradient[0] * 2.0f;
    mapping.primaryBasisX[3] = inverse.gradient[1] * 2.0f;
    mapping.primaryBasisY[2] = inverse.gradient[2] * 2.0f;
    mapping.primaryBasisY[3] = inverse.gradient[3] * 2.0f;
    mapping.primaryOrigin[2] = inverse.origin.x * 2.0f - 1.0f;
    mapping.primaryOrigin[3] = inverse.origin.y * 2.0f - 1.0f;
    mapping.imageIndex = image.imageIndex;
    mapping.maskImageIndex = -1;
    mapping.computedStyleIndex = static_cast<int16_t>(batch.computedStyleCount + job.styleIndex);
    mapping.flags = flags;

    RuiProjectedQuad positions;
    RuiProjectBounds(transform, clipped, positions);
    RuiDrawQuad quad{.vertexCount = 4, .vertexCapacity = 4};
    RuiStoreQuad(quad, positions, inverse.sign);
    if (!rui.drawInfo || static_cast<uint32_t>(rui.drawInfo->mode) >= 4)
        return false;
    return g_RuiDrawInfoHandlers[static_cast<uint32_t>(rui.drawInfo->mode)](rui.drawInfo, &mapping, &quad, &batch);
}

fltx4 RuiMeasureText(RuiInstance& rui, uint32_t renderJobOffset)
{
    RuiRuntimeState& runtime = *rui.runtime;
    const auto& job = *reinterpret_cast<const RuiTextRenderJob*>(rui.header->renderJobs + renderJobOffset);
    const auto* descriptors = reinterpret_cast<const RuiTextStyleDescriptorOffsets*>(rui.header->styleDescriptors);
    const RuiTextStyleDescriptorOffsets* styles[4] = {
        &descriptors[job.styleIndices[0]],
        &descriptors[job.styleIndices[1]],
        &descriptors[job.styleIndices[2]],
        &descriptors[job.styleIndices[3]],
    };
    RuiFont* fonts[4] = {
        g_RuiFonts[styles[0]->fontIndex],
        g_RuiFonts[styles[1]->fontIndex],
        g_RuiFonts[styles[2]->fontIndex],
        g_RuiFonts[styles[3]->fontIndex],
    };

    float textSizes[4];
    float glyphAdvanceScales[4];
    float ascents[4];
    for (uint32_t styleIndex = 0; styleIndex < 4; ++styleIndex)
    {
        textSizes[styleIndex] = rui.GetValue<float>(styles[styleIndex]->textSize);
        glyphAdvanceScales[styleIndex] = textSizes[styleIndex] * rui.GetValue<float>(styles[styleIndex]->horizontalStretch);
        ascents[styleIndex] = textSizes[styleIndex] * fonts[styleIndex]->ascentFraction - rui.GetValue<float>(styles[styleIndex]->ascentAdjustment);
    }

    const float maximumAscent = std::max({ascents[0], ascents[1], ascents[2], ascents[3]});
    const float maximumDescent = std::max({
        textSizes[0] - ascents[0],
        textSizes[1] - ascents[1],
        textSizes[2] - ascents[2],
        textSizes[3] - ascents[3],
    });
    const float lineHeight = maximumAscent + maximumDescent;
    const float lineAdvance = rui.GetValue<float>(job.lineSpacingOffset) + lineHeight;
    const float wrapWidth = rui.GetValue<float>(job.wrapWidthOffset);

    const uint32_t initialLineCount = runtime.textLineCount;
    const uint32_t initialInlineImageCount = runtime.inlineImageCount;
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

    auto AppendLineBreak = [&](uint32_t breakGlyph, float width)
    {
        const uint32_t lineIndex = runtime.textLineCount++;
        if (lineIndex >= RUI_TEXT_LINE_CAPACITY)
            return;
        runtime.textLines[lineIndex].breakGlyph = breakGlyph;
        runtime.textLines[lineIndex].wrappedWidth = width;
        runtime.GetTextLineTerminalWidth(lineIndex + 1) = 0.0f;
    };

    const RuiFontAtlas& wordBreakAtlas = g_RuiFontAtlases[g_RuiFontAtlasIndices[styles[0]->fontIndex]];
    RuiFont* font = fonts[0];
    RuiInlineImageSpan* pendingInlineImage = nullptr;
    char* cursor = rui.GetValue<char*>(job.textOffset);
    std::array<char*, 29> includeStack{};
    uint32_t includeDepth = 0;
    char includeScratch[8];

    for (;;)
    {
        const int32_t codepoint = static_cast<int32_t>(RuiDecodeCodePoint(&cursor));
        ++parsedGlyphCount;
        const bool ordinaryCodepoint = !RuiIsStringDiv(static_cast<uint32_t>(codepoint));
        if (ordinaryCodepoint)
        {
            if (codepoint == '%')
            {
                const char marker = *cursor;
                const bool literalPercent = RuiIsStandalonePercent(marker);
                if (!literalPercent)
                {
                    if (marker == '%')
                    {
                        ++cursor;
                    }
                    else
                    {
                        char* includeText = RuiText_ResolveEscape(&rui, runtime.textContext, &cursor, includeScratch);
                        if (!includeText || includeDepth == includeStack.size())
                            break;
                        includeStack[includeDepth++] = cursor;
                        cursor = includeText;
                        continue;
                    }
                }
            }

            const uint32_t glyphIndex = static_cast<uint32_t>(RuiFont_GetGlyphIndex(font, codepoint));
            const RuiFontGlyph& glyph = font->glyphs[glyphIndex];
            uint16_t kerningIndex = glyph.firstKerning;
            const uint16_t kerningEnd = font->glyphs[glyphIndex + 1].firstKerning;
            while (kerningIndex < kerningEnd && font->kerning[kerningIndex].codepoint != previousCodepoint)
                ++kerningIndex;
            const float kerning = kerningIndex < kerningEnd ? font->kerning[kerningIndex].offset : 0.0f;

            previousCodepoint = codepoint;
            const float beforeGlyph = glyphAdvanceScales[activeStyle] * kerning + currentAdvance;
            currentAdvance = glyphAdvanceScales[activeStyle] * glyph.advance + beforeGlyph;
            if (pendingInlineImage)
                continue;
            if (codepoint == ' ')
            {
                pendingSpace = true;
                continue;
            }
            if (codepoint == '\n')
            {
                AppendLineBreak(parsedGlyphCount, currentLineWidth);
                currentAdvance = 0.0f;
                savedInlineImageCount = runtime.inlineImageCount;
                previousBreakClass = glyph.wordBreakClass;
                pendingSpace = false;
                maximumLineWidth = std::max(maximumLineWidth, currentLineWidth);
                verticalOffset += lineAdvance;
                currentLineWidth = 0.0f;
                continue;
            }

            const uint32_t breakBitIndex =
                static_cast<uint32_t>(pendingSpace) +
                2u * (static_cast<uint32_t>(glyph.wordBreakClass) + static_cast<uint32_t>(previousBreakClass) * wordBreakAtlas.wordBreakClassCount);
            if ((wordBreakAtlas.wordBreakTable[breakBitIndex >> 3] & static_cast<uint8_t>(1u << (breakBitIndex & 7))) != 0)
            {
                savedLineWidth = currentLineWidth;
                savedBreakGlyph = parsedGlyphCount;
                savedInlineImageCount = runtime.inlineImageCount;
                savedBreakX = beforeGlyph;
            }

            if (currentAdvance > wrapWidth)
            {
                for (uint32_t imageIndex = savedInlineImageCount; imageIndex < runtime.inlineImageCount; ++imageIndex)
                {
                    RuiInlineImageSpan& image = runtime.inlineImages[imageIndex];
                    image.boundsMin[0] -= savedBreakX;
                    image.boundsMin[1] += lineAdvance;
                    image.boundsMax[0] -= savedBreakX;
                    image.boundsMax[1] += lineAdvance;
                }
                AppendLineBreak(savedBreakGlyph, savedLineWidth);
                currentAdvance -= savedBreakX;
                verticalOffset += lineAdvance;
                savedInlineImageCount = runtime.inlineImageCount;
                maximumLineWidth = std::max(maximumLineWidth, savedLineWidth);
            }

            pendingSpace = false;
            currentLineWidth = currentAdvance;
            previousBreakClass = glyph.wordBreakClass;
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

        if (RuiIsIconCodePoint(static_cast<uint32_t>(codepoint)))
        {
            const uint32_t inlineImageIndex = runtime.inlineImageCount++;
            RuiInlineImageSpan& image = runtime.inlineImages[std::min<uint32_t>(inlineImageIndex, RUI_INLINE_IMAGE_CAPACITY - 1)];
            image.descriptorIndex = static_cast<uint16_t>(codepoint);
            image.styleIndex = activeStyle;
            image.boundsMin[0] = currentAdvance;
            pendingInlineImage = &image;
            activeStyleMask = 1u << activeStyle;

            RuiResolvedImageAsset asset;
            if (!RuiResolveImageAsset(image.descriptorIndex, asset) || !asset.atlas->imageDimensions)
                continue;
            const RuiImageDimensions& dimensions = asset.atlas->imageDimensions[asset.imageIndex];
            if (previousCodepoint == RUI_CODEPOINT_IMAGE_BORDER_BEGIN)
            {
                if (static_cast<uint16_t>(asset.imageIndex) < asset.atlas->nineSliceImageCount)
                {
                    const RuiImageAtlasNineSlice& trim = asset.atlas->nineSliceData[asset.imageIndex];
                    currentAdvance += (trim.normalizedBounds[2] + trim.normalizedBounds[0]) * static_cast<float>(dimensions.width);
                }
            }
            else
            {
                const float imageMinY = verticalOffset - ascents[activeStyle];
                const float imageWidth =
                    static_cast<float>(dimensions.width) / static_cast<float>(dimensions.height) * glyphAdvanceScales[activeStyle];
                image.boundsMin[1] = imageMinY;
                image.boundsMax[0] = imageWidth + image.boundsMin[0];
                image.boundsMax[1] = imageMinY + textSizes[activeStyle];
                currentAdvance += imageWidth;
                pendingInlineImage = nullptr;
            }
            previousBreakClass = 0;
            pendingSpace = false;
            currentLineWidth = currentAdvance;
            previousCodepoint = codepoint;
            continue;
        }

        if (codepoint == RUI_CODEPOINT_IMAGE_BORDER_END && pendingInlineImage)
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
                imageMinY = std::min(imageMinY, styleMinY);
                imageMaxY = std::max(imageMaxY, styleMinY + textSizes[styleIndex]);
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

    const uint32_t finalLineCount = runtime.textLineCount;
    if (finalLineCount != initialLineCount && finalLineCount <= RUI_TEXT_LINE_CAPACITY)
        runtime.GetTextLineTerminalWidth(finalLineCount) = currentAdvance;

    const float measuredWidth = std::max(maximumLineWidth, currentAdvance);
    const float measuredHeight = verticalOffset + lineHeight;
    const float targetWidth = rui.GetValue<float>(job.targetWidthOffset);
    const float horizontalScale = targetWidth / std::max(targetWidth, measuredWidth);
    const float fittedWidth = horizontalScale * measuredWidth;

    RuiRenderJobState& state = runtime.renderJobStates[renderJobOffset >> 4];
    state.fittedScale = horizontalScale;
    state.firstLine = static_cast<uint8_t>(initialLineCount);
    state.lineCount = static_cast<uint8_t>(runtime.textLineCount - initialLineCount);
    state.firstInlineImage = static_cast<uint8_t>(initialInlineImageCount);
    state.inlineImageCount = static_cast<uint8_t>(runtime.inlineImageCount - initialInlineImageCount);
    return fltx4{fittedWidth, fittedWidth, measuredHeight, measuredHeight};
}

bool RuiTextUsesDynamicImage(RuiInstance& rui, RuiRenderContext* context, const RuiTextRenderJob& job)
{
    char* cursor = rui.GetValue<char*>(job.textOffset);
    std::array<char*, 29> includeStack{};
    uint32_t includeDepth = 0;
    char includeScratch[8];

    for (;;)
    {
        const uint32_t codepoint = RuiDecodeCodePoint(&cursor);
        if (codepoint == 0)
        {
            if (includeDepth)
            {
                cursor = includeStack[--includeDepth];
                continue;
            }
            return false;
        }

        if (codepoint == '%')
        {
            const char marker = *cursor;
            const bool literalPercent = RuiIsStandalonePercent(marker);
            if (!literalPercent)
            {
                if (marker == '%')
                {
                    ++cursor;
                }
                else
                {
                    char* includeText = RuiText_ResolveEscape(&rui, context, &cursor, includeScratch);
                    if (!includeText || includeDepth == includeStack.size())
                        return false;
                    includeStack[includeDepth++] = cursor;
                    cursor = includeText;
                }
            }
            continue;
        }

        if (RuiIsIconCodePoint(codepoint) && RuiIsDynamicImageAsset(static_cast<uint16_t>(codepoint)))
            return true;
    }
}

bool RuiBindFontAtlas(RuiDrawBatch& batch, uint8_t atlasIndex)
{
    if (batch.materialBatchIndex >= batch.materialBatchCapacity)
        return false;

    RuiFontAtlas* fontAtlas = &g_RuiFontAtlases[atlasIndex];
    RuiDrawMaterialBatch& current = batch.materialBatches[batch.materialBatchIndex];
    if (current.fontAtlas == fontAtlas)
        return true;
    if (!current.fontAtlas || current.firstIndex == batch.indexBufferSize)
    {
        current.fontAtlas = fontAtlas;
        return true;
    }

    const uint32_t nextIndex = batch.materialBatchIndex + 1;
    if (nextIndex >= batch.materialBatchCapacity)
        return false;
    batch.materialBatchIndex = nextIndex;
    RuiDrawMaterialBatch& next = batch.materialBatches[nextIndex];
    next.firstVertex = current.firstVertex;
    next.firstIndex = batch.indexBufferSize;
    next.fontAtlas = fontAtlas;
    next.imageAtlas = nullptr;
    return true;
}

bool RuiDrawTextGlyph(RuiGlobalState& globals, RuiInstance& rui, RuiDrawBatch& batch, const RuiTransform& transform,
                      const RuiInverseTransform& inverse, const RuiAntiAliasSetup& antiAlias, const RuiTextDrawStyle& style,
                      const RuiFontGlyph& glyph, uint32_t glyphIndex, uint16_t styleIndex, float transformWidth, float transformHeight, float penX,
                      float baselineY)
{
    if (!style.renderable || glyph.boundsMax[0] <= glyph.boundsMin[0] || glyph.boundsMax[1] <= glyph.boundsMin[1])
        return true;
    const uint32_t atlasGlyphIndex = style.font->atlasGlyphBase + glyphIndex;
    if (atlasGlyphIndex > static_cast<uint32_t>(INT16_MAX) || !RuiBindFontAtlas(batch, style.fontAtlasIndex))
        return false;

    const RuiFontProportion& proportion = style.font->proportions[glyph.proportionIndex];
    const float glyphWidthScale = style.glyphScaleX;
    const float glyphHeightScale = style.textSize;
    if (glyphWidthScale <= 0.0f || glyphHeightScale <= 0.0f)
        return true;

    const float uvScaleX = transformWidth / glyphWidthScale * style.font->atlasScale[0] * proportion.boundsScale;
    const float uvScaleY = transformHeight / glyphHeightScale * style.font->atlasScale[1] * proportion.boundsScale;
    RuiBaseUv mapping{};
    mapping.primaryBasisX[0] = inverse.gradient[0] * uvScaleX;
    mapping.primaryBasisX[1] = inverse.gradient[1] * uvScaleY;
    mapping.primaryBasisX[2] = mapping.primaryBasisX[0];
    mapping.primaryBasisX[3] = mapping.primaryBasisX[1];
    mapping.primaryBasisY[0] = inverse.gradient[2] * uvScaleX;
    mapping.primaryBasisY[1] = inverse.gradient[3] * uvScaleY;
    mapping.primaryBasisY[2] = mapping.primaryBasisY[0];
    mapping.primaryBasisY[3] = mapping.primaryBasisY[1];
    const float uvOriginX = (inverse.origin.x - penX) * uvScaleX + glyph.uvOrigin[0];
    const float uvOriginY = (inverse.origin.y - baselineY) * uvScaleY + glyph.uvOrigin[1];
    mapping.primaryOrigin.Init(uvOriginX, uvOriginY, uvOriginX, uvOriginY);
    mapping.secondaryOrigin.Init(proportion.boundsScale, proportion.boundsScale, proportion.boundsScale, proportion.boundsScale);
    mapping.imageIndex = static_cast<int16_t>(atlasGlyphIndex);
    mapping.maskImageIndex = mapping.imageIndex;
    mapping.computedStyleIndex = static_cast<int16_t>(batch.computedStyleCount + styleIndex);

    const RuiBounds bounds = {
        penX + glyphWidthScale / transformWidth * glyph.boundsMin[0] + style.boundsOffset[0],
        baselineY + glyphHeightScale / transformHeight * glyph.boundsMin[1] + style.boundsOffset[1],
        penX + glyphWidthScale / transformWidth * glyph.boundsMax[0] + style.boundsOffset[2],
        baselineY + glyphHeightScale / transformHeight * glyph.boundsMax[1] + style.boundsOffset[3],
    };
    RuiProjectedQuad positions;
    RuiProjectBounds(transform, bounds, positions);
    RuiApplyAntiAliasPadding(&globals, &rui, &antiAlias, &g_uiAaLrtbScaleForEdgeMask[RUI_TEXT_ANTI_ALIAS_EDGE_MASK], &positions);

    RuiDrawQuad quad{.vertexCount = 4, .vertexCapacity = 4};
    RuiStoreQuad(quad, positions, inverse.sign);
    if (!rui.drawInfo || static_cast<uint32_t>(rui.drawInfo->mode) >= 4)
        return false;
    return g_RuiDrawInfoHandlers[static_cast<uint32_t>(rui.drawInfo->mode)](rui.drawInfo, &mapping, &quad, &batch);
}

bool RuiRenderDynamicText(RuiRenderContext& context, RuiInstance& rui, const RuiTextRenderJob& job, RuiDrawBatch& batch)
{
    RuiRuntimeState& runtime = *rui.runtime;
    const RuiTransform& transform = runtime.transforms[job.transformIndex];
    RuiInverseTransform inverse;
    if (!RuiInvertTransform(transform, inverse))
        return true;

    const float transformWidth = runtime.transformSizes[job.transformIndex][0];
    const float transformHeight = runtime.transformSizes[job.transformIndex][2];
    if (transformWidth <= 0.0f || transformHeight <= 0.0f)
        return true;

    const auto* descriptors = reinterpret_cast<const RuiTextStyleDescriptorOffsets*>(rui.header->styleDescriptors);
    const RuiTextStyleDescriptorOffsets* styles[4] = {
        &descriptors[job.styleIndices[0]],
        &descriptors[job.styleIndices[1]],
        &descriptors[job.styleIndices[2]],
        &descriptors[job.styleIndices[3]],
    };
    const RuiRenderJobState& jobState = runtime.renderJobStates[(reinterpret_cast<const uint8_t*>(&job) - rui.header->renderJobs) >> 4];

    RuiTextDrawStyle drawStyles[4];
    float maximumAscent = 0.0f;
    for (uint32_t styleIndex = 0; styleIndex < 4; ++styleIndex)
    {
        const RuiTextStyleDescriptorOffsets& offsets = *styles[styleIndex];
        RuiTextDrawStyle& style = drawStyles[styleIndex];
        style.font = g_RuiFonts[offsets.fontIndex];
        style.fontAtlasIndex = g_RuiFontAtlasIndices[offsets.fontIndex];
        style.textSize = rui.GetValue<float>(offsets.textSize);
        style.glyphScaleX = jobState.fittedScale * style.textSize * rui.GetValue<float>(offsets.horizontalStretch);
        style.advanceScale = style.glyphScaleX / transformWidth;
        style.ascent = style.textSize * style.font->ascentFraction - rui.GetValue<float>(offsets.ascentAdjustment);
        maximumAscent = std::max(maximumAscent, style.ascent);

        const float shadowFilter = rui.GetValue<float>(offsets.shadowFilterWidth) * 0.5f;
        const float shadowX = rui.GetValue<float>(offsets.shadowOffsetX);
        const float shadowY = rui.GetValue<float>(offsets.shadowOffsetY);
        const float primaryAlpha = rui.GetValue<float>(offsets.common.primaryColor.alpha);
        const float secondaryAlpha = rui.GetValue<float>(offsets.common.secondaryColor.alpha);
        const float tertiaryAlpha = rui.GetValue<float>(offsets.common.tertiaryColor.alpha);
        const float strokeWidth = rui.GetValue<float>(offsets.strokeWidth);
        const float filter = rui.GetValue<float>(offsets.filterWidth) * 0.5f;
        const float effect = strokeWidth + std::max(rui.GetValue<float>(offsets.distanceBias), 0.0f);
        style.boundsOffset[0] = -(std::max(shadowFilter - shadowX, filter) + effect) / transformWidth;
        style.boundsOffset[1] = -(std::max(shadowFilter - shadowY, filter) + effect) / transformHeight;
        style.boundsOffset[2] = (std::max(shadowFilter + shadowX, filter) + effect) / transformWidth;
        style.boundsOffset[3] = (std::max(shadowFilter + shadowY, filter) + effect) / transformHeight;
        style.renderable = std::max(primaryAlpha, std::min(std::max(secondaryAlpha, tertiaryAlpha), strokeWidth)) > 0.0f;
    }

    const float initialBaseline = maximumAscent / transformHeight;
    const float lineSpacing = rui.GetValue<float>(job.lineSpacingOffset) / transformHeight;
    const uint32_t firstInlineImage = jobState.firstInlineImage;
    const uint32_t inlineImageEnd = std::min<uint32_t>(firstInlineImage + jobState.inlineImageCount, runtime.inlineImageCount);
    for (uint32_t imageIndex = firstInlineImage; imageIndex < inlineImageEnd; ++imageIndex)
    {
        const RuiInlineImageSpan& span = runtime.inlineImages[imageIndex];
        RuiResolvedImageAsset image;
        if (!RuiResolveImageAsset(span.descriptorIndex, image))
            continue;
        const RuiBounds imageBounds = {
            span.boundsMin[0] / transformWidth,
            initialBaseline + span.boundsMin[1] / transformHeight,
            span.boundsMax[0] / transformWidth,
            initialBaseline + span.boundsMax[1] / transformHeight,
        };
        if (!RuiDrawInlineImage(*context.globals, rui, batch, transform, inverse, image, imageBounds,
                                job.styleIndices[std::min<uint16_t>(span.styleIndex, 3)]))
        {
            return false;
        }
    }

    RuiAntiAliasSetup antiAlias;
    RuiBuildAntiAliasSetup(&transform, &rui.header->elementWidth, &antiAlias);
    const float horizontalAlignment = rui.GetValue<float>(job.horizontalAlignmentOffset);
    const uint32_t lineEnd = std::min<uint32_t>(jobState.firstLine + jobState.lineCount, runtime.textLineCount);
    uint32_t lineCursor = jobState.firstLine;
    uint32_t nextLineGlyph = UINT32_MAX;
    float baselineY = initialBaseline;
    float currentAdvance = 0.0f;
    if (lineCursor < lineEnd)
    {
        nextLineGlyph = runtime.textLines[lineCursor].breakGlyph;
        currentAdvance = (transformWidth - runtime.textLines[lineCursor].wrappedWidth) * horizontalAlignment / transformWidth;
        ++lineCursor;
    }
    float carryAdvance = 0.0f;
    uint32_t parsedGlyphCount = 0;
    uint32_t inlineImageIndex = firstInlineImage;
    uint8_t activeStyle = 0;
    int32_t previousCodepoint = 0;
    char* cursor = rui.GetValue<char*>(job.textOffset);
    std::array<char*, 29> includeStack{};
    uint32_t includeDepth = 0;
    char includeScratch[8];

    for (;;)
    {
        const int32_t codepoint = static_cast<int32_t>(RuiDecodeCodePoint(&cursor));
        ++parsedGlyphCount;
        if (codepoint == 0)
        {
            if (includeDepth)
            {
                cursor = includeStack[--includeDepth];
                continue;
            }
            return true;
        }

        if (codepoint == '%')
        {
            const char marker = *cursor;
            const bool literalPercent = RuiIsStandalonePercent(marker);
            if (!literalPercent)
            {
                if (marker == '%')
                {
                    ++cursor;
                }
                else
                {
                    char* includeText = RuiText_ResolveEscape(&rui, &context, &cursor, includeScratch);
                    if (!includeText || includeDepth == includeStack.size())
                        return true;
                    includeStack[includeDepth++] = cursor;
                    cursor = includeText;
                    continue;
                }
            }
        }

        bool beganLine = false;
        if (parsedGlyphCount >= nextLineGlyph)
        {
            baselineY += drawStyles[activeStyle].textSize / transformHeight + lineSpacing;
            if (lineCursor >= lineEnd)
            {
                nextLineGlyph = UINT32_MAX;
                currentAdvance = (transformWidth - runtime.GetTextLineTerminalWidth(lineEnd)) * horizontalAlignment / transformWidth;
            }
            else
            {
                nextLineGlyph = runtime.textLines[lineCursor].breakGlyph;
                currentAdvance = (transformWidth - runtime.textLines[lineCursor].wrappedWidth) * horizontalAlignment / transformWidth;
                ++lineCursor;
            }
            previousCodepoint = 0;
            beganLine = true;
        }

        if (!RuiIsStringDiv(static_cast<uint32_t>(codepoint)))
        {
            RuiTextDrawStyle& style = drawStyles[activeStyle];
            const uint32_t glyphIndex = static_cast<uint32_t>(RuiFont_GetGlyphIndex(style.font, codepoint));
            const RuiFontGlyph& glyph = style.font->glyphs[glyphIndex];
            uint16_t kerningIndex = glyph.firstKerning;
            const uint16_t kerningEnd = style.font->glyphs[glyphIndex + 1].firstKerning;
            while (kerningIndex < kerningEnd && style.font->kerning[kerningIndex].codepoint != previousCodepoint)
                ++kerningIndex;
            const float kerning = beganLine || kerningIndex == kerningEnd ? 0.0f : style.font->kerning[kerningIndex].offset * style.advanceScale;
            const float penX = currentAdvance + kerning;
            if (!RuiDrawTextGlyph(*context.globals, rui, batch, transform, inverse, antiAlias, style, glyph, glyphIndex,
                                  job.styleIndices[activeStyle], transformWidth, transformHeight, penX, baselineY))
            {
                return false;
            }
            currentAdvance = penX + glyph.advance * style.advanceScale;
            previousCodepoint = codepoint;
            continue;
        }

        if (codepoint == '`')
        {
            const uint8_t nextStyle = static_cast<uint8_t>(*cursor - '0');
            if (nextStyle >= 4)
                return true;
            activeStyle = nextStyle;
            ++cursor;
            previousCodepoint = 0;
            continue;
        }

        if (codepoint == RUI_CODEPOINT_IMAGE_BORDER_END)
        {
            currentAdvance += carryAdvance;
            carryAdvance = 0.0f;
            previousCodepoint = codepoint;
            continue;
        }

        if (RuiIsIconCodePoint(static_cast<uint32_t>(codepoint)) && inlineImageIndex < inlineImageEnd)
        {
            const RuiInlineImageSpan& span = runtime.inlineImages[inlineImageIndex++];
            RuiResolvedImageAsset image;
            if (!RuiResolveImageAsset(span.descriptorIndex, image) || !image.atlas->imageDimensions)
                continue;

            const RuiImageDimensions& dimensions = image.atlas->imageDimensions[image.imageIndex];
            if (previousCodepoint == RUI_CODEPOINT_IMAGE_BORDER_BEGIN)
            {
                if (static_cast<uint16_t>(image.imageIndex) < image.atlas->nineSliceImageCount)
                {
                    const RuiImageAtlasNineSlice& trim = image.atlas->nineSliceData[image.imageIndex];
                    const float imageWidth = static_cast<float>(dimensions.width) / transformWidth;
                    currentAdvance += trim.normalizedBounds[0] * imageWidth;
                    carryAdvance = trim.normalizedBounds[2] * imageWidth;
                }
                else
                {
                    carryAdvance = 0.0f;
                }
            }
            else
            {
                currentAdvance += static_cast<float>(dimensions.width) / static_cast<float>(dimensions.height) * drawStyles[activeStyle].advanceScale;
            }
            previousCodepoint = codepoint;
            continue;
        }
        previousCodepoint = codepoint;
    }
}

DECLARE_HOOK(RuiRenderImageJob, engine.dll + 0xF72F0,
             [](auto& hook, RuiRenderContext* context, RuiInstance* rui, const RuiImageRenderJob* job, RuiDrawBatch* batch) -> bool
{
    const RuiImageHandle image = rui->GetValue<RuiImageHandle>(job->imageOffset);
    const RuiImageHandle mask = rui->GetValue<RuiImageHandle>(job->maskImageOffset);
    std::shared_lock atlasLock(g_RuiImageAtlasMutex);
    const RuiImageAtlasHandle imageAtlas = RuiGetImageAtlasHandle(image);
    const RuiImageAtlasHandle maskAtlas = RuiGetImageAtlasHandle(mask);
    const bool nativeImage = imageAtlas == RUI_INVALID_IMAGE_ATLAS || imageAtlas < RUI_NATIVE_IMAGE_ATLAS_CAPACITY;
    const bool nativeMask = maskAtlas == RUI_INVALID_IMAGE_ATLAS || maskAtlas < RUI_NATIVE_IMAGE_ATLAS_CAPACITY;
    if (nativeImage && nativeMask)
    {
        return hook.Original(context, rui, job, batch);
    }
    return RuiRenderDynamicImage(*context, *rui, *job, *batch);
})

DECLARE_HOOK(RuiRenderEllipseJob, engine.dll + 0xF7A80,
             [](auto& hook, RuiRenderContext* context, RuiInstance* rui, const RuiEllipseRenderJob* job, RuiDrawBatch* batch) -> bool
{
    const RuiImageHandle image = rui->GetValue<RuiImageHandle>(job->imageOffset);
    std::shared_lock atlasLock(g_RuiImageAtlasMutex);
    if (!RuiIsDynamicImageAsset(image))
    {
        return hook.Original(context, rui, job, batch);
    }
    return RuiRenderDynamicEllipse(*context, *rui, *job, *batch);
})

DECLARE_HOOK(RuiMeasureTextJob, engine.dll + 0xF6980, [](auto& hook, RuiInstance* rui, uint32_t renderJobOffset) -> fltx4
{
    const auto& job = *reinterpret_cast<const RuiTextRenderJob*>(rui->header->renderJobs + renderJobOffset);
    std::shared_lock atlasLock(g_RuiImageAtlasMutex);
    if (!RuiTextUsesDynamicImage(*rui, rui->runtime->textContext, job))
    {
        return hook.Original(rui, renderJobOffset);
    }
    return RuiMeasureText(*rui, renderJobOffset);
})

DECLARE_HOOK(RuiRenderTextJob, engine.dll + 0xF5840,
             [](auto& hook, RuiRenderContext* context, RuiInstance* rui, const RuiTextRenderJob* job, RuiDrawBatch* batch) -> bool
{
    std::shared_lock atlasLock(g_RuiImageAtlasMutex);
    if (!RuiTextUsesDynamicImage(*rui, context, *job))
    {
        return hook.Original(context, rui, job, batch);
    }
    return RuiRenderDynamicText(*context, *rui, *job, *batch);
})

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", RuiRender, RuiImageAtlasRegistry, [](CModule module)
{
    g_RuiDrawInfoHandlers = module.Offset(0x5F4560).RCast<RuiTessellate_t*>();
    RuiFindImageAsset = module.Offset(0xF8000).RCast<decltype(RuiFindImageAsset)>();
    RuiEvaluateMeshBasis = module.Offset(0xFFC50).RCast<RuiEvaluateProjectionBasis_t>();
    RuiEvaluateAngularBasis = module.Offset(0xFFEB0).RCast<RuiEvaluateProjectionBasis_t>();
    g_pRuiPadDistance = module.Offset(0x12A4E8A0).RCast<ConVar*>();
    g_RuiLrtbClampMasksByMode = module.Offset(0x12A4E830).RCast<const uint32_t*>();
    g_uiAaLrtbScaleForEdgeMask = module.Offset(0x5F4740).RCast<const Vector4D*>();
    g_RuiFontAtlases = module.Offset(0x12A26080).RCast<RuiFontAtlas*>();
    g_RuiFonts = module.Offset(0x12A4E550).RCast<RuiFont**>();
    g_RuiFontAtlasIndices = module.Offset(0x12A4E650).RCast<uint8_t*>();
    DISPATCH_MODULE(RuiRenderHooks);
})

DECLARE_HOOK(SuppressParticleEditorRui, engine.dll + 0xFC7A0, [](auto& hook, RuiRenderContext* context)
{
    if (context)
        RuiBeginImageAtlasFrame(context->stage);

    if (!context || !ParticleTools::GetParticleToolSystem().IsEditorInputEnabled())
    {
        hook.Original(context);
        return;
    }

    const std::uint16_t instanceCount = context->instanceCount;
    context->instanceCount = 0;
    context->drawBatchCount = 0;
    hook.Original(context);
    context->instanceCount = instanceCount;
})
