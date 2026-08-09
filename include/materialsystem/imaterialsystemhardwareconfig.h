#pragma once

#include <cstdint>

inline constexpr char MATERIAL_SYSTEM_HARDWARE_CONFIG_INTERFACE_VERSION[] = "MaterialSystemHardwareConfig015";

enum HDRType_t : std::int32_t
{
	HDR_TYPE_NONE = 0,
	HDR_TYPE_INTEGER = 1,
	HDR_TYPE_FLOAT = 2,
};

class IMaterialSystemHardwareConfig
{
public:
	virtual bool HasDestAlphaBuffer() const = 0; // 0
	virtual int GetFrameBufferColorDepth() const = 0; // 1
	virtual bool HasStencilBuffer() const = 0; // 2
	virtual int GetSamplerCount() const = 0; // 3
	virtual int MaximumAnisotropicLevel() const = 0; // 4
	virtual int MaxTextureWidth() const = 0; // 5
	virtual int MaxTextureHeight() const = 0; // 6
	virtual bool SupportsMipmappedCubemaps() const = 0; // 7
	virtual int NumVertexShaderConstants() const = 0; // 8
	virtual int NumPixelShaderConstants() const = 0; // 9
	virtual int MaxNumLights() const = 0; // 10
	virtual bool UseFastClipping() const = 0; // 11
	virtual int GetDXSupportLevel() const = 0; // 12
	virtual bool ReadPixelsFromFrontBuffer() const = 0; // 13
	virtual bool PreferDynamicTextures() const = 0; // 14
	virtual bool NeedsAAClamp() const = 0; // 15
	virtual int GetMaxDXSupportLevel() const = 0; // 16
	virtual bool SpecifiesFogColorInLinearSpace() const = 0; // 17
	virtual bool FakeSRGBWrite() const = 0; // 18
	virtual bool CanDoSRGBReadFromRTs() const = 0; // 19
	virtual bool IsAAEnabled() const = 0; // 20
	virtual int GetVertexSamplerCount() const = 0; // 21
	virtual int GetMaxVertexTextureDimension() const = 0; // 22
	virtual int MaxTextureDepth() const = 0; // 23
	virtual HDRType_t GetHDRType() const = 0; // 24
	virtual HDRType_t GetHardwareHDRType() const = 0; // 25
	virtual bool SupportsStreamOffset() const = 0; // 26
	virtual bool SupportsSRGB() const = 0; // 27
	virtual bool SupportsGLMixedSizeTargets() const = 0; // 28
	virtual bool SupportsHDRMode(HDRType_t mode) const = 0; // 29
	virtual bool SupportsHDR() const = 0; // 30
	virtual void SetHDREnabled(bool enabled) = 0; // 31
	virtual bool SupportsBorderColor() const = 0; // 32
	virtual bool SupportsFetch4() const = 0; // 33
	virtual int GetShadowDepthTextureFormat() const = 0; // 34
	virtual int GetNullTextureFormat() const = 0; // 35
	virtual int GetMinDXSupportLevel() const = 0; // 36
	virtual bool IsUnsupported() const = 0; // 37
	virtual float GetLightMapScaleFactor() const = 0; // 38
	virtual bool SupportsCascadedShadowMapping() const = 0; // 39
	virtual int GetCSMQuality() const = 0; // 40
	virtual bool SupportsBilinearPCFSampling() const = 0; // 41
	virtual int GetCSMShaderMode(int qualityLevel) const = 0; // 42
	virtual bool GetCSMAccurateBlending() const = 0; // 43
	virtual int GetCachedHardwareFeatureMode44() const = 0; // 44
	virtual bool IsCachedHardwareFeatureEnabled45() const = 0; // 45
	virtual bool IsCachedHardwareFeatureEnabled46() const = 0; // 46
	virtual bool IsCachedHardwareFeatureEnabled47() const = 0; // 47
	virtual bool IsCachedHardwareFeatureEnabled48() const = 0; // 48
};
static_assert(sizeof(IMaterialSystemHardwareConfig) == sizeof(void*));
