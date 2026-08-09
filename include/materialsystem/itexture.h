#pragma once

#include <cstdint>

class ITexture
{
public:
	virtual const char * GetName() const = 0; // 0
	virtual int GetMappingWidth() const = 0; // 1
	virtual int GetMappingHeight() const = 0; // 2
	virtual int GetActualWidth() const = 0; // 3
	virtual int GetActualHeight() const = 0; // 4
	virtual int GetNumAnimationFrames() const = 0; // 5
	virtual int GetNumMipLevels() const = 0; // 6
	virtual bool IsTranslucent() const = 0; // 7
	virtual bool IsMipmapped() const = 0; // 8
	virtual void GetLowResColorSample(float s, float t, float *color) const = 0; // 9
	virtual void * GetResourceData(unsigned int dataType, std::uint64_t *numBytes) const = 0; // 10
	virtual void IncrementReferenceCount() = 0; // 11
	virtual void DecrementReferenceCount() = 0; // 12
	virtual int GetReferenceCount() const = 0; // 13
	virtual void IncrementReferenceCount(int referenceClass) = 0; // 14
	virtual void DecrementReferenceCount(int referenceClass) = 0; // 15
	virtual void SetTextureRegenerator(void *textureRegenerator) = 0; // 16
	virtual void Download(void *rect = nullptr, int additionalCreationFlags = 0) = 0; // 17
	virtual int GetApproximateVidMemBytes() const = 0; // 18
	virtual bool IsError() const = 0; // 19
	virtual bool IsVolumeTexture() const = 0; // 20
	virtual int GetMappingDepth() const = 0; // 21
	virtual int GetActualDepth() const = 0; // 22
	virtual int GetImageFormat() const = 0; // 23
	virtual int GetNormalDecodeMode() const = 0; // 24
	virtual bool IsRenderTarget() const = 0; // 25
	virtual bool IsCubeMap() const = 0; // 26
	virtual bool IsNormalMap() const = 0; // 27
	virtual bool IsProcedural() const = 0; // 28
	virtual bool IsDefaultPool() const = 0; // 29
	virtual void DeleteIfUnreferenced() = 0; // 30
	virtual void SwapContents(ITexture *other) = 0; // 31
	virtual unsigned int GetFlags() const = 0; // 32
	virtual void ForceLODOverride(int lodOverride) = 0; // 33
	virtual bool SaveToFile(const char *fileName) = 0; // 34
	virtual void CopyToStagingTexture(ITexture *destination) = 0; // 35
	virtual void SetErrorTexture(bool isErrorTexture) = 0; // 36
	virtual bool BDownload(void *rect = nullptr, int additionalCreationFlags = 0) = 0; // 37
};
static_assert(sizeof(ITexture) == sizeof(void*));
