#pragma once

#include "gametrace.h"

#include <cstddef>
#include <cstdint>

class CPhysCollide;
class IClientUnknown;
class IHandleEntity;
class QAngle;
struct matrix3x4_t;
struct model_t;
struct Ray_t;

enum SolidType_t : std::int32_t
{
	SOLID_NONE = 0,
	SOLID_BSP = 1,
	SOLID_BBOX = 2,
	SOLID_OBB = 3,
	SOLID_OBB_YAW = 4,
	SOLID_CUSTOM = 5,
	SOLID_VPHYSICS = 6,
	SOLID_CUSTOM_GEOMETRY = 11,
};

enum SolidFlags_t : std::uint32_t
{
	FSOLID_CUSTOMRAYTEST = 0x0001,
	FSOLID_CUSTOMBOXTEST = 0x0002,
	FSOLID_NOT_SOLID = 0x0004,
	FSOLID_TRIGGER = 0x0008,
	FSOLID_NOT_STANDABLE = 0x0010,
	FSOLID_VOLUME_CONTENTS = 0x0020,
	FSOLID_FORCE_WORLD_ALIGNED = 0x0040,
	FSOLID_USE_TRIGGER_BOUNDS = 0x0080,
	FSOLID_ROOT_PARENT_ALIGNED = 0x0100,
	FSOLID_TRIGGER_TOUCH_DEBRIS = 0x0200,
};

struct CollideableCustomCollisionRecord_t
{
	cplanetrace_t m_SeparatingPlane;
	std::byte m_EngineCollisionData[0x320];
};

class ICollideable
{
  public:
	virtual IHandleEntity* GetEntityHandle() = 0; // 0
	virtual const Vector3& OBBMins() const = 0; // 1
	virtual const Vector3& OBBMaxs() const = 0; // 2
	virtual float BoundingRadius() const = 0; // 3
	virtual void WorldSpaceTriggerBounds(Vector3* pWorldMins, Vector3* pWorldMaxs) const = 0; // 4
	virtual bool TestCollision(const Ray_t* pRay, std::uint32_t contentsMask, GameTrace* pTrace) = 0; // 5
	virtual bool TestHitboxes(const Ray_t* pRay, std::uint32_t contentsMask, GameTrace* pTrace) = 0; // 6
	virtual int GetCollisionModelIndex() = 0; // 7
	virtual const model_t* GetCollisionModel() = 0; // 8
	virtual const Vector3& GetCollisionOrigin() const = 0; // 9
	virtual const QAngle& GetCollisionAngles() const = 0; // 10
	virtual float GetCollisionScale() const = 0; // 11
	virtual const matrix3x4_t& CollisionToWorldTransform(matrix3x4_t* pScratchTransform) const = 0; // 12
	virtual SolidType_t GetSolid() const = 0; // 13
	virtual SolidFlags_t GetSolidFlags() const = 0; // 14
	virtual IClientUnknown* GetIClientUnknown() = 0; // 15
	virtual int GetCollisionGroup() const = 0; // 16
	virtual std::uint32_t GetCollisionContentsMask() const = 0; // 17
	virtual void WorldSpaceSurroundingBounds(Vector3* pWorldMins, Vector3* pWorldMaxs) = 0; // 18
	virtual std::uint32_t GetSpatialPartitionMask() const = 0; // 19
	virtual const matrix3x4_t* GetRootParentToWorldTransform(matrix3x4_t* pScratchTransform) const = 0; // 20
	virtual CPhysCollide* GetPhysicsCollide() = 0; // 21
	virtual int GetCustomCollisionCount() const = 0; // 22
	virtual void GetCustomCollisionData(int index,
		const CollideableCustomCollisionRecord_t** ppRecords, int* pRecordCount,
		int* pAuxiliaryCount) const = 0; // 23
	virtual void GetCustomCollisionDataCoarse(int index,
		const CollideableCustomCollisionRecord_t** ppRecords, int* pRecordCount) const = 0; // 24
	virtual void RefreshCollisionBounds() = 0; // 25
};

static_assert(sizeof(SolidType_t) == 0x4);
static_assert(sizeof(SolidFlags_t) == 0x4);
static_assert(sizeof(CollideableCustomCollisionRecord_t) == 0x330);
static_assert(offsetof(CollideableCustomCollisionRecord_t, m_SeparatingPlane) == 0x0);
static_assert(sizeof(ICollideable) == sizeof(void*));
