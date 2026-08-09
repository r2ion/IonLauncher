#pragma once

#include "appframework/IAppSystem.h"
#include "gametrace.h"
#include "mathlib/vector.h"

#include <cstddef>
#include <cstdint>

inline constexpr char PARTICLE_SYSTEM_QUERY_INTERFACE_VERSION[] = "VParticleSystemQuery004";

using ParticleTraceHandle_t = std::int32_t;

inline constexpr ParticleTraceHandle_t PARTICLE_TRACE_HANDLE_INVALID = -1;

struct ParticleTrace_t
{
	Vector3 m_StartPosition;
	std::uint32_t m_Pad000C;
	Vector3 m_EndPosition;
	std::uint32_t m_Pad001C;
	cplanetrace_t m_Plane;
	float m_Fraction;
	std::int32_t m_Contents;
	std::uint16_t m_DisplacementFlags;
	bool m_AllSolid;
	bool m_StartSolid;
	bool m_Hit;
	std::uint8_t m_Pad003D[3];
};

struct ParticleDeferredTraceResult
{
	float m_Fraction;
	bool m_DidHit;
	std::uint8_t m_Pad0005[3];
	Vector3 m_EndPosition;
};

struct ParticleModelHitBoxInfo
{
	Vector3 m_BoxMins;
	Vector3 m_BoxMaxes;
	float m_Transform[3][4];
};

struct ParticleViewMatrix
{
	float m_Values[4][4];
};

static_assert(sizeof(ParticleTraceHandle_t) == 0x4);
static_assert(sizeof(ParticleTrace_t) == 0x40);
static_assert(offsetof(ParticleTrace_t, m_EndPosition) == 0x10);
static_assert(offsetof(ParticleTrace_t, m_Plane) == 0x20);
static_assert(offsetof(ParticleTrace_t, m_Fraction) == 0x30);
static_assert(offsetof(ParticleTrace_t, m_Hit) == 0x3C);
static_assert(sizeof(ParticleDeferredTraceResult) == 0x14);
static_assert(offsetof(ParticleDeferredTraceResult, m_EndPosition) == 0x8);
static_assert(sizeof(ParticleModelHitBoxInfo) == 0x48);
static_assert(sizeof(ParticleViewMatrix) == 0x40);

class IParticleSystemQuery : public IAppSystem
{
public:
	virtual bool IsEditor() = 0; // 8
	virtual void GetLightingAtPoint(const Vector3& origin, Vector3& tint, float scale) = 0; // 9
	virtual void TraceLine(const Vector3& start, const Vector3& end, std::uint32_t contentsMask,
		void* pIgnoreEntity, int collisionGroup, ParticleTrace_t* pTrace) = 0; // 10
	virtual void InitializeTraceHandles(ParticleTraceHandle_t* pHandles, std::size_t count) = 0; // 11
	virtual void ReleaseTraceHandles(ParticleTraceHandle_t* pHandles, std::size_t count) = 0; // 12
	virtual void SubmitTraceLine(const Vector3& start, const Vector3& end, std::uint32_t contentsMask,
		void* pIgnoreEntity, int collisionGroup, ParticleTraceHandle_t* pHandle) = 0; // 13
	virtual bool GetTraceResult(ParticleTraceHandle_t* pHandle, ParticleDeferredTraceResult* pResult) = 0; // 14
	virtual bool IsPointInSolid(const Vector3& position, int contentsMask) = 0; // 15
	virtual bool MovePointInsideControllingObject(void* pParticles, void* pObject, Vector3* pPoint) = 0; // 16
	virtual bool IsPointInControllingObjectHitBox(void* pParticles, int controlPoint,
		const Vector3& position, bool bboxOnly, const char* pHitboxSetName) = 0; // 17
	virtual int GetCollisionGroupFromName(const char* pName) = 0; // 18
	virtual void GetRandomPointsOnControllingObjectHitBox(void* pParticles, int controlPoint,
		int pointCount, float bboxScale, int attempts, Vector3* pPoints, const Vector3& directionBias,
		Vector3* pRelativePoints, int* pHitboxIndices, int desiredHitbox,
		const char* pHitboxSetName) = 0; // 19
	virtual void GetClosestControllingObjectHitBox(void* pParticles, int controlPoint, int pointCount,
		float bboxScale, const Vector3* pPoints, Vector3* pRelativePoints, int* pHitboxIndices,
		int desiredHitbox, const char* pHitboxSetName) = 0; // 20
	virtual int GetControllingObjectHitBoxInfo(void* pParticles, int controlPoint, int bufferCount,
		ParticleModelHitBoxInfo* pBuffer, const char* pHitboxSetName) = 0; // 21
	virtual void GetControllingObjectOBBox(void* pParticles, int controlPoint,
		Vector3& mins, Vector3& maxs) = 0; // 22
	virtual Vector3 GetLocalPlayerPos() = 0; // 23
	virtual void GetLocalPlayerEyeVectors(Vector3* pForward, Vector3* pRight, Vector3* pUp) = 0; // 24
	virtual void GetViewportSize(int* pWidth, int* pHeight) = 0; // 25
	virtual const ParticleViewMatrix* GetCachedViewMatrix(std::uint8_t splitScreenSlot) = 0; // 26

	virtual void NullSub27() = 0;
	virtual void NullSub28() = 0;
	virtual int GetActivityCount() = 0; // 29
	virtual const char* GetActivityNameFromIndex(int activityIndex) = 0; // 30
	virtual int GetActivityNumber(void* pModel, const char* pActivityName) = 0; // 31
	virtual float GetPixelVisibility(void* pQueryHandle, void* pReserved, const Vector3& origin,
		float scale, bool scaleByFov, bool proxyTest) = 0; // 32
	virtual void QueueTraceImpact(void* pParticles, void* pUserData, std::uint32_t* pParticleId,
		const Vector3* pOrigin, int flags, const Vector3* pDirection, float distance,
		bool option0, bool option1) = 0; // 33
	virtual void NullSub34() = 0; // 34
	virtual void FlushQueuedTraceImpacts() = 0; // 35
	virtual void DebugDrawLine(const Vector3& origin, const Vector3& destination, int red, int green,
		int blue, bool noDepthTest, float duration) = 0; // 36
	virtual void DebugDrawText(const Vector3& origin, const char* pText, int red, int green,
		int blue, int alpha, float duration) = 0; // 37
	virtual void* GetModel(const char* pModelName) = 0; // 38
	virtual int GetModelRenderStateSize() = 0; // 39
	virtual void UpdateModelRenderState(void* pModel, int flags, const Vector3* pPosition, void* pState) = 0; // 40
	virtual bool IsModelRenderStateValid(const void* pState) = 0; // 41
	virtual void SubmitModelDrawRecord(void* pModel, std::uint32_t flags, void* pDrawRecord,
		void* pUserData, float scale, std::uint16_t arg7, int arg8, int arg9, int arg10,
		int arg11, int arg12, int arg13, int arg14, int arg15, int arg16) = 0; // 42
	virtual void SubmitModelDrawRecords(void* pModel, std::uint32_t flags, float scale,
		void* pDrawRecords, std::uint32_t count) = 0; // 43
	virtual void RefreshViewCache() = 0; // 44
	virtual void RefreshLocalPlayerCache() = 0; // 45
	virtual bool QuerySoundMeter(std::uint32_t index, float* pValue) = 0;
	virtual void ResetQueuedTraceImpacts() = 0; // 47
};

static_assert(sizeof(IParticleSystemQuery) == sizeof(void*));
