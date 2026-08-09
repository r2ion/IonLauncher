#pragma once

#include "cmodel.h"
#include "engine/ICollideable.h"
#include "gametrace.h"
#include "mathlib/vector.h"
#include <cstddef>
#include <cstdint>




enum TraceType_t : std::int32_t
{
	TRACE_EVERYTHING = 0,
	TRACE_WORLD_ONLY,
	TRACE_ENTITIES_ONLY,
	TRACE_EVERYTHING_FILTER_PROPS,
};

class ITraceFilter
{
  public:
	virtual bool ShouldHitEntity(IHandleEntity* pEntity, std::uint32_t contentsMask) = 0; // 0
	virtual TraceType_t GetTraceType() const = 0; // 1
	virtual void OnTraceHit(GameTrace* pTrace) = 0; // 2
};

class IEntityEnumerator
{
  public:
	virtual bool EnumEntity(IHandleEntity* pEntity) = 0; // 0
};

enum IterationRetval_t : std::int32_t
{
	ITERATION_CONTINUE = 0,
	ITERATION_STOP,
};

class IPartitionEnumerator
{
  public:
	virtual IterationRetval_t EnumElement(IHandleEntity* pHandleEntity) = 0; // [0]
};

class ITraceListData
{
  public:
	virtual ~ITraceListData() = default;
	virtual void Reset() = 0;
	virtual bool IsEmpty() const = 0;
	virtual bool CanTraceRay(const Ray_t* pRay) const = 0;
};

struct CBrushQuery;
using BrushQueryReleaseFn = void (*)(CBrushQuery* pQuery);

struct CBrushQuery
{
	std::int32_t m_nBrushCount;
	std::uint32_t m_Padding04;
	const std::int32_t* m_pBrushIndices;
	std::int32_t m_nMaxSideCount;
	std::uint32_t m_Padding14;
	BrushQueryReleaseFn m_pRelease;
	void* m_pReleaseContext;
};

static_assert(sizeof(ITraceFilter) == sizeof(void*));
static_assert(sizeof(IEntityEnumerator) == sizeof(void*));
static_assert(sizeof(IPartitionEnumerator) == sizeof(void*));
static_assert(sizeof(ITraceListData) == sizeof(void*));
static_assert(sizeof(CBrushQuery) == 0x28);
static_assert(offsetof(CBrushQuery, m_pBrushIndices) == 0x8);
static_assert(offsetof(CBrushQuery, m_pRelease) == 0x18);

inline constexpr char ENGINE_TRACE_SERVER_INTERFACE_VERSION[] = "EngineTraceServer004";
inline constexpr char ENGINE_TRACE_CLIENT_INTERFACE_VERSION[] = "EngineTraceClient004";
inline constexpr char ENGINE_TRACE_CLIENT_DECALS_INTERFACE_VERSION[] = "EngineTraceClientDecals004";

struct EngineTraceBrushPlane_t
{
	float m_Normal[3];
	float m_Distance;
	std::uint16_t m_Type;
	std::uint16_t m_SignBits;
};

class IEngineTrace
{
public:
	virtual void ClipRayToEntity(const Ray_t* pRay, std::uint32_t contentsMask,
		IHandleEntity* pEntity, GameTrace* pTraceOut) = 0; // 0
	virtual void ClipRayToCollideable(const Ray_t* pRay, std::uint32_t contentsMask,
		ICollideable* pCollideable, GameTrace* pTraceOut) = 0; // 1
	virtual bool ClipRayToCustomCollideable(const Ray_t* pRay, std::uint32_t contentsMask,
		ICollideable* pCollideable, bool useExactGeometry, GameTrace* pTraceOut) = 0; // 2
	virtual void TraceRayFiltered(const Ray_t* pRay, std::uint32_t contentsMask,
		ITraceFilter* pFilter, GameTrace* pTraceOut) = 0; // 3
	virtual void TraceRay(const Ray_t* pRay, std::uint32_t contentsMask,
		GameTrace* pTraceOut) = 0; // 4
	virtual void SetupLeafAndEntityListRay(const Ray_t* pRay,
		ITraceListData* pTraceData) = 0; // 5
	virtual void SetupLeafAndEntityListBox(const Vector3* pMins, const Vector3* pMaxs,
		ITraceListData* pTraceData) = 0; // 6
	virtual void TraceRayAgainstLeafAndEntityList(const Ray_t* pRay,
		ITraceListData* pTraceData, std::uint32_t contentsMask,
		ITraceFilter* pFilter, GameTrace* pTraceOut) = 0; // 7
	virtual void SweepCollideable(ICollideable* pCollideable, const Vector3* pStart,
		const Vector3* pEnd, const QAngle* pAngles, std::uint32_t contentsMask,
		ITraceFilter* pFilter, GameTrace* pTraceOut) = 0; // 8
	virtual void EnumerateEntitiesInBox(const Vector3* pMins, const Vector3* pMaxs,
		IEntityEnumerator* pEnumerator) = 0; // 9
	virtual void EnumerateEntitiesAlongRay(const Ray_t* pRay, bool includeTriggers,
		IEntityEnumerator* pEnumerator) = 0; // 10
	virtual void EnumerateLinkedEntitiesAlongRay(const Ray_t* pRay, IHandleEntity* pEntity,
		bool includeTriggers, IEntityEnumerator* pEnumerator) = 0; // 11
	virtual ICollideable* GetCollideable(IHandleEntity* pEntity) = 0; // 12
	virtual int GetStatByIndex(int index, bool clear) = 0; // 13
	virtual std::size_t GetBrushesInAABB(int collisionModelIndex, const Vector3* pMins,
		const Vector3* pMaxs, std::uint32_t contentsMask, void* pBrushOutput,
		std::size_t brushCapacity, void* pCollideableOutput,
		int* pCollideableCount, int collideableCapacity) = 0; // 14
	virtual CPhysCollide* GetCollidableFromDisplacementsInAABB(int unknownPartition,
		const Vector3* pMins, const Vector3* pMaxs) = 0; // 15
	virtual int GetBrushInfo(int brushIndex, int* pContentsOut,
		EngineTraceBrushPlane_t* pPlanesOut, int planeCapacity) = 0; // 16
	virtual bool PointOutsideWorld(const Vector3* pPoint) = 0; // 17
	virtual ITraceListData* AllocTraceListData() = 0; // 18
	virtual void FreeTraceListData(ITraceListData* pTraceData) = 0; // 19
	virtual int GetSetDebugTraceCounter(int value, int behavior) = 0; // 20
	virtual void GetBrushesInCollideable(ICollideable* pCollideable,
		CBrushQuery* pBrushQuery) = 0; // 21
	virtual void TraceRayAgainstBrushModel(const Ray_t* pRay,
		const matrix3x4_t* pTransform, float scale, void* pBrushData,
		int modelIndex, GameTrace* pTraceOut) = 0; // 22
	virtual void GetBrushModelBounds(void* pBrushData, int modelIndex,
		Vector3* pMins, Vector3* pMaxs) = 0; // 23
	virtual ICollideable* HandleEntityToCollideable(IHandleEntity* pEntity) = 0; // 24
	virtual ICollideable* GetWorldCollideable() = 0; // 25
	virtual const char* GetEntityDebugName(IHandleEntity* pEntity) = 0; // 26
	virtual void SetTraceEntity(ICollideable* pCollideable, GameTrace* pTraceOut) = 0; // 27
	virtual int SpatialPartitionMask() const = 0; // 28
	virtual int SpatialPartitionStaticPropsMask() const = 0; // 29
	virtual int SpatialPartitionTriggerMask() const = 0; // 30
};

static_assert(sizeof(IEngineTrace) == sizeof(void*));
static_assert(sizeof(EngineTraceBrushPlane_t) == 0x14);
