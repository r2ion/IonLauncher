#pragma once

#include "mathlib/vector.h"

#include <cstddef>
#include <cstdint>

class IClientModelRenderable;
class IClientUnknown;
struct CViewSetup;
struct matrix3x4_t;
struct model_t;

using ClientRenderHandle_t = std::uint16_t;
using ClientShadowHandle_t = std::uint16_t;
using ModelInstanceHandle_t = std::uint16_t;

inline constexpr ClientRenderHandle_t INVALID_CLIENT_RENDER_HANDLE = 0xFFFFu;
inline constexpr ClientShadowHandle_t CLIENTSHADOW_INVALID_HANDLE = 0xFFFFu;
inline constexpr ModelInstanceHandle_t MODEL_INSTANCE_INVALID = 0xFFFFu;

struct RenderableInstance_t
{
	float m_Alpha;
	std::uint32_t m_Flags;
};

static_assert(sizeof(RenderableInstance_t) == 0x8);
static_assert(offsetof(RenderableInstance_t, m_Alpha) == 0x0);
static_assert(offsetof(RenderableInstance_t, m_Flags) == 0x4);

class IClientRenderable
{
public:
	virtual IClientUnknown* GetIClientUnknown() = 0; // 0
	virtual const Vector3& GetRenderOrigin() = 0; // 1
	virtual const QAngle& GetRenderAngles() = 0; // 2
	virtual bool ShouldDraw() = 0; // 3
	virtual bool IsTransparent() = 0; // 4
	virtual void Unused() const = 0; // 5
	virtual ClientRenderHandle_t& RenderHandle() = 0; // 6
	virtual ClientShadowHandle_t GetShadowHandle() const = 0; // 7
	virtual const model_t* GetModel() const = 0; // 8
	virtual std::int32_t DrawModel(const CViewSetup* pView, std::uint32_t flags,
		const RenderableInstance_t& instance) = 0; // 9
	virtual std::int32_t GetBody() = 0; // 10
	virtual void GetColorModulation(float* pColor) = 0; // 11
	virtual bool LODTest() = 0; // 12
	virtual bool SetupBones(matrix3x4_t* pBoneToWorldOut, std::int32_t maxBones,
		std::int32_t boneMask, float currentTime) = 0; // 13
	virtual void SetupWeights(const matrix3x4_t* pBoneToWorld, std::int32_t flexWeightCount,
		float* pFlexWeights, float* pFlexDelayedWeights) = 0; // 14
	virtual void DoAnimationEvents() = 0; // 15
	virtual void GetRenderBounds(Vector3& mins, Vector3& maxs) = 0; // 16
	virtual void GetRenderBoundsWorldspace(Vector3& mins, Vector3& maxs) = 0; // 17
	virtual void CreateModelInstance() = 0; // 18
	virtual ModelInstanceHandle_t GetModelInstance() = 0; // 19
	virtual const matrix3x4_t& RenderableToWorldTransform() = 0; // 20
	virtual float GetModelScale() const = 0; // 21
	virtual std::int32_t LookupAttachment(const char* pAttachmentName) = 0; // 22
	virtual bool GetAttachment(std::int32_t number, matrix3x4_t& matrix) = 0; // 23
	virtual bool GetAttachment(std::int32_t number, Vector3& origin, QAngle& angles) = 0; // 24
	virtual float* GetRenderClipPlane() = 0; // 25
	virtual bool UsesPowerOfTwoFrameBufferTexture(std::int32_t splitScreenSlot) = 0; // 26
	virtual bool UsesFullFrameBufferTexture(std::int32_t splitScreenSlot) = 0; // 27
	virtual bool UsesFlexDelayedWeights() = 0; // 28
	virtual bool ShouldReceiveProjectedTextures(std::int32_t flags) = 0; // 29
	virtual float OverrideAlphaModulation(float alpha) = 0; // 30
	virtual IClientModelRenderable* GetClientModelRenderable() = 0; // 31
};

static_assert(sizeof(IClientRenderable) == sizeof(void*));
