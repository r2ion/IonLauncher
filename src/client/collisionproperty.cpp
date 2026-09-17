//========= Copyright Valve Corporation, All rights reserved. ============//
// Source SDK 2013 collisionproperty.cpp API with retail R2 native operations.
#include "client/collisionproperty.h"

void (*s_CollisionInit)(CCollisionProperty*, C_BaseEntity*);
void (*s_CollisionCreatePartitionHandle)(CCollisionProperty*);
void (*s_CollisionDestroyPartitionHandle)(CCollisionProperty*);
void (*s_CollisionMarkPartitionHandleDirty)(CCollisionProperty*);
void (*s_CollisionUpdatePartition)(CCollisionProperty*);
void (*s_CollisionSetCollisionBounds)(CCollisionProperty*, const Vector3D&, const Vector3D&);
void (*s_CollisionSetSolid)(CCollisionProperty*, SolidType_t);
void (*s_CollisionSetSolidFlags)(CCollisionProperty*, int);
void (*s_CollisionUseTriggerBounds)(CCollisionProperty*, bool, float);
void (*s_CollisionSetSurroundingBoundsType)(CCollisionProperty*, SurroundingBoundsType_t, const Vector3D*, const Vector3D*);
float (*s_CollisionBoundingRadius2D)(const CCollisionProperty*);
const Vector3D& (*s_CollisionCollisionToWorldSpace)(const CCollisionProperty*, const Vector3D&, Vector3D*);
const Vector3D& (*s_CollisionWorldToCollisionSpace)(const CCollisionProperty*, const Vector3D&, Vector3D*);
const Vector3D& (*s_CollisionWorldDirectionToCollisionSpace)(const CCollisionProperty*, const Vector3D&, Vector3D*);
void (*s_CollisionRandomPointInBounds)(const CCollisionProperty*, const Vector3D&, const Vector3D&, Vector3D*);
bool (*s_CollisionIsPointInBounds)(const CCollisionProperty*, const Vector3D&);
void (*s_CollisionCollisionAABBToWorldAABB)(const CCollisionProperty*, const Vector3D&, const Vector3D&, Vector3D*, Vector3D*);
const Vector3D& (*s_CollisionCollisionToNormalizedSpace)(const CCollisionProperty*, const Vector3D&, Vector3D*);
const Vector3D& (*s_CollisionWorldToNormalizedSpace)(const CCollisionProperty*, const Vector3D&, Vector3D*);
const Vector3D& (*s_CollisionNormalizedToWorldSpace)(const CCollisionProperty*, const Vector3D&, Vector3D*);
void (*s_CollisionCalcNearestPoint)(const CCollisionProperty*, const Vector3D&, Vector3D*);
float (*s_CollisionCalcDistanceFromPoint)(const CCollisionProperty*, const Vector3D&);
float (*s_CollisionCalcSqrDistanceFromPoint)(const CCollisionProperty*, const Vector3D&);
bool (*s_CollisionDoesVPhysicsInvalidateSurroundingBox)(const CCollisionProperty*);
void (*s_CollisionMarkSurroundingBoundsDirty)(CCollisionProperty*);
float (*s_CollisionComputeSupportMap)(const CCollisionProperty*, const Vector3D&);
void (*s_CollisionComputeVPhysicsSurroundingBox)(CCollisionProperty*, Vector3D*, Vector3D*);
bool (*s_CollisionComputeHitboxSurroundingBox)(CCollisionProperty*, Vector3D*, Vector3D*);
bool (*s_CollisionComputeEntitySpaceHitboxSurroundingBox)(CCollisionProperty*, Vector3D*, Vector3D*);
void (*s_CollisionComputeCollisionSurroundingBox)(CCollisionProperty*, bool, Vector3D*, Vector3D*);
void (*s_CollisionComputeRotationExpandedBounds)(CCollisionProperty*, Vector3D*, Vector3D*);
void (*s_CollisionComputeSurroundingBox)(CCollisionProperty*, Vector3D*, Vector3D*);
void (*s_UpdateDirtySpatialPartitionEntities)();

void CCollisionProperty::Init(C_BaseEntity* entity)
{
    s_CollisionInit(this, entity);
}
void CCollisionProperty::CreatePartitionHandle()
{
    s_CollisionCreatePartitionHandle(this);
}
void CCollisionProperty::DestroyPartitionHandle()
{
    s_CollisionDestroyPartitionHandle(this);
}
void CCollisionProperty::MarkPartitionHandleDirty()
{
    s_CollisionMarkPartitionHandleDirty(this);
}
void CCollisionProperty::UpdatePartition()
{
    s_CollisionUpdatePartition(this);
}
void CCollisionProperty::SetCollisionBounds(const Vector3D& mins, const Vector3D& maxs)
{
    s_CollisionSetCollisionBounds(this, mins, maxs);
}
void CCollisionProperty::SetSolid(SolidType_t solid)
{
    s_CollisionSetSolid(this, solid);
}
void CCollisionProperty::SetSolidFlags(int flags)
{
    s_CollisionSetSolidFlags(this, flags);
}
void CCollisionProperty::UseTriggerBounds(bool enable, float bloat)
{
    s_CollisionUseTriggerBounds(this, enable, bloat);
}
void CCollisionProperty::SetSurroundingBoundsType(SurroundingBoundsType_t type, const Vector3D* mins, const Vector3D* maxs)
{
    s_CollisionSetSurroundingBoundsType(this, type, mins, maxs);
}
float CCollisionProperty::BoundingRadius2D() const
{
    return s_CollisionBoundingRadius2D(this);
}
const Vector3D& CCollisionProperty::CollisionToWorldSpace(const Vector3D& in, Vector3D* result) const
{
    return s_CollisionCollisionToWorldSpace(this, in, result);
}
const Vector3D& CCollisionProperty::WorldToCollisionSpace(const Vector3D& in, Vector3D* result) const
{
    return s_CollisionWorldToCollisionSpace(this, in, result);
}
const Vector3D& CCollisionProperty::WorldDirectionToCollisionSpace(const Vector3D& in, Vector3D* result) const
{
    return s_CollisionWorldDirectionToCollisionSpace(this, in, result);
}
void CCollisionProperty::RandomPointInBounds(const Vector3D& mins, const Vector3D& maxs, Vector3D* point) const
{
    s_CollisionRandomPointInBounds(this, mins, maxs, point);
}
bool CCollisionProperty::IsPointInBounds(const Vector3D& point) const
{
    return s_CollisionIsPointInBounds(this, point);
}
void CCollisionProperty::CollisionAABBToWorldAABB(const Vector3D& mins, const Vector3D& maxs, Vector3D* worldMins, Vector3D* worldMaxs) const
{
    s_CollisionCollisionAABBToWorldAABB(this, mins, maxs, worldMins, worldMaxs);
}
const Vector3D& CCollisionProperty::CollisionToNormalizedSpace(const Vector3D& in, Vector3D* result) const
{
    return s_CollisionCollisionToNormalizedSpace(this, in, result);
}
const Vector3D& CCollisionProperty::WorldToNormalizedSpace(const Vector3D& in, Vector3D* result) const
{
    return s_CollisionWorldToNormalizedSpace(this, in, result);
}
const Vector3D& CCollisionProperty::NormalizedToWorldSpace(const Vector3D& in, Vector3D* result) const
{
    return s_CollisionNormalizedToWorldSpace(this, in, result);
}
void CCollisionProperty::CalcNearestPoint(const Vector3D& point, Vector3D* nearest) const
{
    s_CollisionCalcNearestPoint(this, point, nearest);
}
float CCollisionProperty::CalcDistanceFromPoint(const Vector3D& point) const
{
    return s_CollisionCalcDistanceFromPoint(this, point);
}
float CCollisionProperty::CalcSqrDistanceFromPoint(const Vector3D& point) const
{
    return s_CollisionCalcSqrDistanceFromPoint(this, point);
}
bool CCollisionProperty::DoesVPhysicsInvalidateSurroundingBox() const
{
    return s_CollisionDoesVPhysicsInvalidateSurroundingBox(this);
}
void CCollisionProperty::MarkSurroundingBoundsDirty()
{
    s_CollisionMarkSurroundingBoundsDirty(this);
}
float CCollisionProperty::ComputeSupportMap(const Vector3D& direction) const
{
    return s_CollisionComputeSupportMap(this, direction);
}
void CCollisionProperty::ComputeVPhysicsSurroundingBox(Vector3D* mins, Vector3D* maxs)
{
    s_CollisionComputeVPhysicsSurroundingBox(this, mins, maxs);
}
bool CCollisionProperty::ComputeHitboxSurroundingBox(Vector3D* mins, Vector3D* maxs)
{
    return s_CollisionComputeHitboxSurroundingBox(this, mins, maxs);
}
bool CCollisionProperty::ComputeEntitySpaceHitboxSurroundingBox(Vector3D* mins, Vector3D* maxs)
{
    return s_CollisionComputeEntitySpaceHitboxSurroundingBox(this, mins, maxs);
}
void CCollisionProperty::ComputeCollisionSurroundingBox(bool physics, Vector3D* mins, Vector3D* maxs)
{
    s_CollisionComputeCollisionSurroundingBox(this, physics, mins, maxs);
}
void CCollisionProperty::ComputeRotationExpandedBounds(Vector3D* mins, Vector3D* maxs)
{
    s_CollisionComputeRotationExpandedBounds(this, mins, maxs);
}
void CCollisionProperty::ComputeSurroundingBox(Vector3D* mins, Vector3D* maxs)
{
    s_CollisionComputeSurroundingBox(this, mins, maxs);
}

void UpdateDirtySpatialPartitionEntities()
{
    s_UpdateDirtySpatialPartitionEntities();
}

const Vector3D& CCollisionProperty::OBBSize() const
{
    Vector3D& result = AllocTempVector();
    VectorSubtract(m_vecMaxs, m_vecMins, result);
    return result;
}
const Vector3D& CCollisionProperty::OBBCenter() const
{
    Vector3D& result = AllocTempVector();
    VectorLerp(m_vecMins, m_vecMaxs, 0.5f, result);
    return result;
}
const Vector3D& CCollisionProperty::WorldSpaceCenter() const
{
    Vector3D& result = AllocTempVector();
    return CollisionToWorldSpace(OBBCenter(), &result);
}
void CCollisionProperty::WorldSpaceAABB(Vector3D* mins, Vector3D* maxs) const
{
    CollisionAABBToWorldAABB(m_vecMins, m_vecMaxs, mins, maxs);
}
const Vector3D& CCollisionProperty::NormalizedToCollisionSpace(const Vector3D& in, Vector3D* result) const
{
    for (int i = 0; i < 3; ++i)
        (*result)[i] = m_vecMins[i] + (m_vecMaxs[i] - m_vecMins[i]) * in[i];
    return *result;
}
bool CCollisionProperty::DoesRotationInvalidateSurroundingBox() const
{
    if (IsSolidFlagSet(FSOLID_ROOT_PARENT_ALIGNED))
        return true;
    switch (m_nSurroundType)
    {
    case USE_OBB_COLLISION_BOUNDS:
    case USE_BEST_COLLISION_BOUNDS:
    case USE_COLLISION_BOUNDS_NEVER_VPHYSICS:
        return IsBoundsDefinedInEntitySpace();
    case USE_ROTATION_EXPANDED_BOUNDS:
    case USE_SPECIFIED_BOUNDS:
        return false;
    default:
        return true;
    }
}

ON_DLL_LOAD_CLIENT("client.dll", CollisionPropertyMethods, [](CModule module)
{
    s_CollisionInit = module.Offset(0x1B8990).RCast<decltype(s_CollisionInit)>();
    s_CollisionCreatePartitionHandle = module.Offset(0x1B83E0).RCast<decltype(s_CollisionCreatePartitionHandle)>();
    s_CollisionDestroyPartitionHandle = module.Offset(0x1B8420).RCast<decltype(s_CollisionDestroyPartitionHandle)>();
    s_CollisionMarkPartitionHandleDirty = module.Offset(0x1B8DC0).RCast<decltype(s_CollisionMarkPartitionHandleDirty)>();
    s_CollisionUpdatePartition = module.Offset(0x1B9D00).RCast<decltype(s_CollisionUpdatePartition)>();
    s_CollisionSetCollisionBounds = module.Offset(0x1B98D0).RCast<decltype(s_CollisionSetCollisionBounds)>();
    s_CollisionSetSolid = module.Offset(0x1B9990).RCast<decltype(s_CollisionSetSolid)>();
    s_CollisionSetSolidFlags = module.Offset(0x1B99C0).RCast<decltype(s_CollisionSetSolidFlags)>();
    s_CollisionUseTriggerBounds = module.Offset(0x1B9EC0).RCast<decltype(s_CollisionUseTriggerBounds)>();
    s_CollisionSetSurroundingBoundsType = module.Offset(0x1B9A10).RCast<decltype(s_CollisionSetSurroundingBoundsType)>();
    s_CollisionBoundingRadius2D = module.Offset(0x1B6820).RCast<decltype(s_CollisionBoundingRadius2D)>();
    s_CollisionCollisionToWorldSpace = module.Offset(0xBF580).RCast<decltype(s_CollisionCollisionToWorldSpace)>();
    s_CollisionWorldToCollisionSpace = module.Offset(0x1BA1D0).RCast<decltype(s_CollisionWorldToCollisionSpace)>();
    s_CollisionWorldDirectionToCollisionSpace = module.Offset(0x1B9F20).RCast<decltype(s_CollisionWorldDirectionToCollisionSpace)>();
    s_CollisionRandomPointInBounds = module.Offset(0x1B9600).RCast<decltype(s_CollisionRandomPointInBounds)>();
    s_CollisionIsPointInBounds = module.Offset(0x1B8A80).RCast<decltype(s_CollisionIsPointInBounds)>();
    s_CollisionCollisionAABBToWorldAABB = module.Offset(0x1B6C70).RCast<decltype(s_CollisionCollisionAABBToWorldAABB)>();
    s_CollisionCollisionToNormalizedSpace = module.Offset(0x1B6DF0).RCast<decltype(s_CollisionCollisionToNormalizedSpace)>();
    s_CollisionWorldToNormalizedSpace = module.Offset(0x1BA2B0).RCast<decltype(s_CollisionWorldToNormalizedSpace)>();
    s_CollisionNormalizedToWorldSpace = module.Offset(0x1B8F10).RCast<decltype(s_CollisionNormalizedToWorldSpace)>();
    s_CollisionCalcNearestPoint = module.Offset(0x1B6A20).RCast<decltype(s_CollisionCalcNearestPoint)>();
    s_CollisionCalcDistanceFromPoint = module.Offset(0x1B6900).RCast<decltype(s_CollisionCalcDistanceFromPoint)>();
    s_CollisionCalcSqrDistanceFromPoint = module.Offset(0x1B6B30).RCast<decltype(s_CollisionCalcSqrDistanceFromPoint)>();
    s_CollisionDoesVPhysicsInvalidateSurroundingBox = module.Offset(0x1B8480).RCast<decltype(s_CollisionDoesVPhysicsInvalidateSurroundingBox)>();
    s_CollisionMarkSurroundingBoundsDirty = module.Offset(0x1B8E40).RCast<decltype(s_CollisionMarkSurroundingBoundsDirty)>();
    s_CollisionComputeSupportMap = module.Offset(0x1B7C40).RCast<decltype(s_CollisionComputeSupportMap)>();
    s_CollisionComputeVPhysicsSurroundingBox = module.Offset(0x1B8180).RCast<decltype(s_CollisionComputeVPhysicsSurroundingBox)>();
    s_CollisionComputeHitboxSurroundingBox = module.Offset(0x1B7120).RCast<decltype(s_CollisionComputeHitboxSurroundingBox)>();
    s_CollisionComputeEntitySpaceHitboxSurroundingBox = module.Offset(0x1B6F70).RCast<decltype(s_CollisionComputeEntitySpaceHitboxSurroundingBox)>();
    s_CollisionComputeCollisionSurroundingBox = module.Offset(0x1B6F50).RCast<decltype(s_CollisionComputeCollisionSurroundingBox)>();
    s_CollisionComputeRotationExpandedBounds = module.Offset(0x1B74D0).RCast<decltype(s_CollisionComputeRotationExpandedBounds)>();
    s_CollisionComputeSurroundingBox = module.Offset(0x1B7D10).RCast<decltype(s_CollisionComputeSurroundingBox)>();
    s_UpdateDirtySpatialPartitionEntities = module.Offset(0x1B9C80).RCast<decltype(s_UpdateDirtySpatialPartitionEntities)>();
})
