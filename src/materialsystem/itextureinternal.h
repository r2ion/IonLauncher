#pragma once

#include "materialsystem/itexture.h"

#include <cstdint>
#include <type_traits>

// i cba man
enum Sampler_t : int;
struct Rect_t;

class ITextureInternal : public ITexture
{
  public:
    virtual void Bind(Sampler_t sampler) = 0;                                                                                      // 38
    virtual void Bind(Sampler_t sampler1, int frame, Sampler_t sampler2 = static_cast<Sampler_t>(-1)) = 0;                         // 37
    virtual int GetReferenceCount() = 0;                                                                                           // 39
    virtual void Release() = 0;                                                                                                    // 40
    virtual void OnRestore() = 0;                                                                                                  // 41
    virtual void SetFilteringAndClampingMode() = 0;                                                                                // 42
    virtual void Precache() = 0;                                                                                                   // 43
    virtual void CopyFrameBufferToMe(int renderTargetID = 0, Rect_t* sourceRect = nullptr, Rect_t* destinationRect = nullptr) = 0; // 44
    virtual ITexture* GetEmbeddedTexture(int index) = 0;                                                                           // 45
    virtual std::uint16_t GetTextureHandle(int frame) = 0;                                                                         // 46
    virtual std::uint16_t GetDepthTextureHandle() = 0;                                                                             // 47
    virtual ~ITextureInternal() = default;                                                                                         // 48
    virtual void GetRenderTargetHandles(unsigned int renderTargetID, std::uint16_t* colorTarget, std::uint16_t* depthTarget) = 0;  // 49
    virtual bool IsMultiRenderTarget() = 0;                                                                                        // 50
};
