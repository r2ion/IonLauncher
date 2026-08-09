#pragma once

#include "materialsystem/cshaderglue.h"
#include "materialsystem/imaterialinternal.h"
#include "rendersystem/schema/texture.g.h"

#include <cstddef>
#include <cstdint>
#include <d3d11.h>


struct CBufUberStatic // sizeof = 0xE0
{
	float c_uv1RotScaleX[2];        // 0x00
	float c_uv1RotScaleY[2];        // 0x08
	float c_uv1Translate[2];        // 0x10
	float c_uv2RotScaleX[2];        // 0x18
	float c_uv2RotScaleY[2];        // 0x20
	float c_uv2Translate[2];        // 0x28
	float c_uv3RotScaleX[2];        // 0x30
	float c_uv3RotScaleY[2];        // 0x38
	float c_uv3Translate[2];        // 0x40
	float c_uvDistortionIntensity[2];   // 0x48
	float c_uvDistortion2Intensity[2];  // 0x50
	float c_fogColorFactor;         // 0x58
	float c_layerBlendRamp;         // 0x5C
	float c_albedoTint[3];          // 0x60
	float c_opacity;                // 0x6C
	float c_useAlphaModulateSpecular;   // 0x70
	float c_alphaEdgeFadeExponent;      // 0x74
	float c_alphaEdgeFadeInner;         // 0x78
	float c_alphaEdgeFadeOuter;         // 0x7C
	float c_useAlphaModulateEmissive;   // 0x80
	float c_emissiveEdgeFadeExponent;   // 0x84
	float c_emissiveEdgeFadeInner;      // 0x88
	float c_emissiveEdgeFadeOuter;      // 0x8C
	float c_alphaDistanceFadeScale;     // 0x90
	float c_alphaDistanceFadeBias;      // 0x94
	float c_alphaTestReference;         // 0x98
	float c_aspectRatioMulV;            // 0x9C
	float c_emissiveTint[3];            // 0xA0
	float c_shadowBias;                 // 0xAC
	float c_tsaaDepthAlphaThreshold;    // 0xB0
	float c_tsaaMotionAlphaThreshold;   // 0xB4
	float c_tsaaMotionAlphaRamp;        // 0xB8
	uint32_t c_tsaaResponsiveFlag;      // 0xBC
	float c_dofOpacityLuminanceScale;   // 0xC0
	float c_glitchStrength;             // 0xC4
	float c_padding[2];                 // 0xC8
	float c_perfGloss;                  // 0xD0
	float c_perfSpecColor[3];           // 0xD4
};
static_assert(sizeof(CBufUberStatic) == 0xE0);

class CMaterialGlue : public IMaterialInternal
{
public:
	std::uint8_t unknown08[8];
	std::uint64_t guid;
	const char* name;
	const char* surfaceProps[2];
	CMaterialGlue* DepthShadow_ref;
	CMaterialGlue* DepthPrepass_ref;
	CMaterialGlue* DepthVSM_ref;
	CMaterialGlue* Colpass_ref;
	std::uint8_t gap_50[64];
	CShaderGlue* shaderSet;
	TextureAsset_s** textureHandles;
	TextureAsset_s** streamingTextures;
	std::uint16_t streamingTextureCount;
	std::uint8_t samplerIndices[4];
	std::uint16_t unknownAE;
	std::uint8_t gap_B0[12];
	std::uint16_t unknownWord[2];
	std::uint32_t flags;
	std::uint32_t flags2;
	std::uint16_t width;
	std::uint16_t height;
	std::uint8_t gap_CC[2];
	std::uint16_t animationFrameCount;
	void* textureAnimation;
	CBufUberStatic* cbufUberStatic;
	ID3D11Buffer* buffer;
	std::uint32_t* atlasBufferIndices;
	std::uint32_t unknownF0;
	std::uint8_t gap_F4[12];
};
static_assert(sizeof(CMaterialGlue) == 0x100);
static_assert(offsetof(CMaterialGlue, unknown08) == 0x8);
static_assert(offsetof(CMaterialGlue, guid) == 0x10);
static_assert(offsetof(CMaterialGlue, shaderSet) == 0x90);
static_assert(offsetof(CMaterialGlue, textureHandles) == 0x98);
static_assert(offsetof(CMaterialGlue, streamingTextures) == 0xA0);
static_assert(offsetof(CMaterialGlue, flags) == 0xC0);
static_assert(offsetof(CMaterialGlue, width) == 0xC8);
static_assert(offsetof(CMaterialGlue, cbufUberStatic) == 0xD8);
static_assert(offsetof(CMaterialGlue, buffer) == 0xE0);
static_assert(offsetof(CMaterialGlue, atlasBufferIndices) == 0xE8);
static_assert(offsetof(CMaterialGlue, unknownF0) == 0xF0);
static_assert(offsetof(CMaterialGlue, gap_F4) == 0xF4);
