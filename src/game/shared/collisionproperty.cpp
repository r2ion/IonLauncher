//========= Copyright Valve Corporation, All rights reserved. ============//

#include "game/shared/collisionproperty.h"

void (*C_CollisionProperty__Init)(C_CollisionProperty*, C_BaseEntity*);
void (*C_CollisionProperty__CreatePartitionHandle)(C_CollisionProperty*);
void (*C_CollisionProperty__DestroyPartitionHandle)(C_CollisionProperty*);
void (*C_CollisionProperty__MarkPartitionHandleDirty)(C_CollisionProperty*);
void (*C_CollisionProperty__UpdatePartition)(C_CollisionProperty*);
void (*C_CollisionProperty__SetCollisionBounds)(C_CollisionProperty*, const Vector3D&, const Vector3D&);
void (*C_CollisionProperty__SetSolid)(C_CollisionProperty*, SolidType_t);
void (*C_CollisionProperty__SetSolidFlags)(C_CollisionProperty*, int);
void (*C_CollisionProperty__UseTriggerBounds)(C_CollisionProperty*, bool, float);
void (*C_CollisionProperty__SetSurroundingBoundsType)(C_CollisionProperty*, SurroundingBoundsType_t, const Vector3D*, const Vector3D*);
float (*C_CollisionProperty__BoundingRadius2D)(const C_CollisionProperty*);
const Vector3D& (*C_CollisionProperty__CollisionToWorldSpace)(const C_CollisionProperty*, const Vector3D&, Vector3D*);
const Vector3D& (*C_CollisionProperty__WorldToCollisionSpace)(const C_CollisionProperty*, const Vector3D&, Vector3D*);
const Vector3D& (*C_CollisionProperty__WorldDirectionToCollisionSpace)(const C_CollisionProperty*, const Vector3D&, Vector3D*);
void (*C_CollisionProperty__RandomPointInBounds)(const C_CollisionProperty*, const Vector3D&, const Vector3D&, Vector3D*);
bool (*C_CollisionProperty__IsPointInBounds)(const C_CollisionProperty*, const Vector3D&);
void (*C_CollisionProperty__CollisionAABBToWorldAABB)(const C_CollisionProperty*, const Vector3D&, const Vector3D&, Vector3D*, Vector3D*);
const Vector3D& (*C_CollisionProperty__CollisionToNormalizedSpace)(const C_CollisionProperty*, const Vector3D&, Vector3D*);
const Vector3D& (*C_CollisionProperty__WorldToNormalizedSpace)(const C_CollisionProperty*, const Vector3D&, Vector3D*);
const Vector3D& (*C_CollisionProperty__NormalizedToWorldSpace)(const C_CollisionProperty*, const Vector3D&, Vector3D*);
void (*C_CollisionProperty__CalcNearestPoint)(const C_CollisionProperty*, const Vector3D&, Vector3D*);
float (*C_CollisionProperty__CalcDistanceFromPoint)(const C_CollisionProperty*, const Vector3D&);
float (*C_CollisionProperty__CalcSqrDistanceFromPoint)(const C_CollisionProperty*, const Vector3D&);
bool (*C_CollisionProperty__DoesVPhysicsInvalidateSurroundingBox)(const C_CollisionProperty*);
void (*C_CollisionProperty__MarkSurroundingBoundsDirty)(C_CollisionProperty*);
float (*C_CollisionProperty__ComputeSupportMap)(const C_CollisionProperty*, const Vector3D&);
void (*C_CollisionProperty__ComputeVPhysicsSurroundingBox)(C_CollisionProperty*, Vector3D*, Vector3D*);
bool (*C_CollisionProperty__ComputeHitboxSurroundingBox)(C_CollisionProperty*, Vector3D*, Vector3D*);
bool (*C_CollisionProperty__ComputeEntitySpaceHitboxSurroundingBox)(C_CollisionProperty*, Vector3D*, Vector3D*);
void (*C_CollisionProperty__ComputeCollisionSurroundingBox)(C_CollisionProperty*, bool, Vector3D*, Vector3D*);
void (*C_CollisionProperty__ComputeRotationExpandedBounds)(C_CollisionProperty*, Vector3D*, Vector3D*);
void (*C_CollisionProperty__ComputeSurroundingBox)(C_CollisionProperty*, Vector3D*, Vector3D*);
void (*v_UpdateDirtySpatialPartitionEntities)();

void C_CollisionProperty::Init(C_BaseEntity* entity)
{
    C_CollisionProperty__Init(this, entity);
}
void C_CollisionProperty::CreatePartitionHandle()
{
    C_CollisionProperty__CreatePartitionHandle(this);
}
void C_CollisionProperty::DestroyPartitionHandle()
{
    C_CollisionProperty__DestroyPartitionHandle(this);
}
void C_CollisionProperty::MarkPartitionHandleDirty()
{
    C_CollisionProperty__MarkPartitionHandleDirty(this);
}
void C_CollisionProperty::UpdatePartition()
{
    C_CollisionProperty__UpdatePartition(this);
}
void C_CollisionProperty::SetCollisionBounds(const Vector3D& mins, const Vector3D& maxs)
{
    C_CollisionProperty__SetCollisionBounds(this, mins, maxs);
}
void C_CollisionProperty::SetSolid(SolidType_t solid)
{
    C_CollisionProperty__SetSolid(this, solid);
}
void C_CollisionProperty::SetSolidFlags(int flags)
{
    C_CollisionProperty__SetSolidFlags(this, flags);
}
void C_CollisionProperty::UseTriggerBounds(bool enable, float bloat)
{
    C_CollisionProperty__UseTriggerBounds(this, enable, bloat);
}
void C_CollisionProperty::SetSurroundingBoundsType(SurroundingBoundsType_t type, const Vector3D* mins, const Vector3D* maxs)
{
    C_CollisionProperty__SetSurroundingBoundsType(this, type, mins, maxs);
}
float C_CollisionProperty::BoundingRadius2D() const
{
    return C_CollisionProperty__BoundingRadius2D(this);
}
const Vector3D& C_CollisionProperty::CollisionToWorldSpace(const Vector3D& in, Vector3D* result) const
{
    return C_CollisionProperty__CollisionToWorldSpace(this, in, result);
}
const Vector3D& C_CollisionProperty::WorldToCollisionSpace(const Vector3D& in, Vector3D* result) const
{
    return C_CollisionProperty__WorldToCollisionSpace(this, in, result);
}
const Vector3D& C_CollisionProperty::WorldDirectionToCollisionSpace(const Vector3D& in, Vector3D* result) const
{
    return C_CollisionProperty__WorldDirectionToCollisionSpace(this, in, result);
}
void C_CollisionProperty::RandomPointInBounds(const Vector3D& mins, const Vector3D& maxs, Vector3D* point) const
{
    C_CollisionProperty__RandomPointInBounds(this, mins, maxs, point);
}
bool C_CollisionProperty::IsPointInBounds(const Vector3D& point) const
{
    return C_CollisionProperty__IsPointInBounds(this, point);
}
void C_CollisionProperty::CollisionAABBToWorldAABB(const Vector3D& mins, const Vector3D& maxs, Vector3D* worldMins, Vector3D* worldMaxs) const
{
    C_CollisionProperty__CollisionAABBToWorldAABB(this, mins, maxs, worldMins, worldMaxs);
}
const Vector3D& C_CollisionProperty::CollisionToNormalizedSpace(const Vector3D& in, Vector3D* result) const
{
    return C_CollisionProperty__CollisionToNormalizedSpace(this, in, result);
}
const Vector3D& C_CollisionProperty::WorldToNormalizedSpace(const Vector3D& in, Vector3D* result) const
{
    return C_CollisionProperty__WorldToNormalizedSpace(this, in, result);
}
const Vector3D& C_CollisionProperty::NormalizedToWorldSpace(const Vector3D& in, Vector3D* result) const
{
    return C_CollisionProperty__NormalizedToWorldSpace(this, in, result);
}
void C_CollisionProperty::CalcNearestPoint(const Vector3D& point, Vector3D* nearest) const
{
    C_CollisionProperty__CalcNearestPoint(this, point, nearest);
}
float C_CollisionProperty::CalcDistanceFromPoint(const Vector3D& point) const
{
    return C_CollisionProperty__CalcDistanceFromPoint(this, point);
}
float C_CollisionProperty::CalcSqrDistanceFromPoint(const Vector3D& point) const
{
    return C_CollisionProperty__CalcSqrDistanceFromPoint(this, point);
}
bool C_CollisionProperty::DoesVPhysicsInvalidateSurroundingBox() const
{
    return C_CollisionProperty__DoesVPhysicsInvalidateSurroundingBox(this);
}
void C_CollisionProperty::MarkSurroundingBoundsDirty()
{
    C_CollisionProperty__MarkSurroundingBoundsDirty(this);
}
float C_CollisionProperty::ComputeSupportMap(const Vector3D& direction) const
{
    return C_CollisionProperty__ComputeSupportMap(this, direction);
}
void C_CollisionProperty::ComputeVPhysicsSurroundingBox(Vector3D* mins, Vector3D* maxs)
{
    C_CollisionProperty__ComputeVPhysicsSurroundingBox(this, mins, maxs);
}
bool C_CollisionProperty::ComputeHitboxSurroundingBox(Vector3D* mins, Vector3D* maxs)
{
    return C_CollisionProperty__ComputeHitboxSurroundingBox(this, mins, maxs);
}
bool C_CollisionProperty::ComputeEntitySpaceHitboxSurroundingBox(Vector3D* mins, Vector3D* maxs)
{
    return C_CollisionProperty__ComputeEntitySpaceHitboxSurroundingBox(this, mins, maxs);
}
void C_CollisionProperty::ComputeCollisionSurroundingBox(bool physics, Vector3D* mins, Vector3D* maxs)
{
    C_CollisionProperty__ComputeCollisionSurroundingBox(this, physics, mins, maxs);
}
void C_CollisionProperty::ComputeRotationExpandedBounds(Vector3D* mins, Vector3D* maxs)
{
    C_CollisionProperty__ComputeRotationExpandedBounds(this, mins, maxs);
}
void C_CollisionProperty::ComputeSurroundingBox(Vector3D* mins, Vector3D* maxs)
{
    C_CollisionProperty__ComputeSurroundingBox(this, mins, maxs);
}

void UpdateDirtySpatialPartitionEntities()
{
    v_UpdateDirtySpatialPartitionEntities();
}

const Vector3D& C_CollisionProperty::OBBSize() const
{
    Vector3D& result = AllocTempVector();
    VectorSubtract(m_vecMaxs, m_vecMins, result);
    return result;
}
const Vector3D& C_CollisionProperty::OBBCenter() const
{
    Vector3D& result = AllocTempVector();
    VectorLerp(m_vecMins, m_vecMaxs, 0.5f, result);
    return result;
}
const Vector3D& C_CollisionProperty::WorldSpaceCenter() const
{
    Vector3D& result = AllocTempVector();
    return CollisionToWorldSpace(OBBCenter(), &result);
}
void C_CollisionProperty::WorldSpaceAABB(Vector3D* mins, Vector3D* maxs) const
{
    CollisionAABBToWorldAABB(m_vecMins, m_vecMaxs, mins, maxs);
}
const Vector3D& C_CollisionProperty::NormalizedToCollisionSpace(const Vector3D& in, Vector3D* result) const
{
    for (int i = 0; i < 3; ++i)
        (*result)[i] = m_vecMins[i] + (m_vecMaxs[i] - m_vecMins[i]) * in[i];
    return *result;
}
bool C_CollisionProperty::DoesRotationInvalidateSurroundingBox() const
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
    C_CollisionProperty__Init = module.Offset(0x1B8990).RCast<decltype(C_CollisionProperty__Init)>();
    C_CollisionProperty__CreatePartitionHandle = module.Offset(0x1B83E0).RCast<decltype(C_CollisionProperty__CreatePartitionHandle)>();
    C_CollisionProperty__DestroyPartitionHandle = module.Offset(0x1B8420).RCast<decltype(C_CollisionProperty__DestroyPartitionHandle)>();
    C_CollisionProperty__MarkPartitionHandleDirty = module.Offset(0x1B8DC0).RCast<decltype(C_CollisionProperty__MarkPartitionHandleDirty)>();
    C_CollisionProperty__UpdatePartition = module.Offset(0x1B9D00).RCast<decltype(C_CollisionProperty__UpdatePartition)>();
    C_CollisionProperty__SetCollisionBounds = module.Offset(0x1B98D0).RCast<decltype(C_CollisionProperty__SetCollisionBounds)>();
    C_CollisionProperty__SetSolid = module.Offset(0x1B9990).RCast<decltype(C_CollisionProperty__SetSolid)>();
    C_CollisionProperty__SetSolidFlags = module.Offset(0x1B99C0).RCast<decltype(C_CollisionProperty__SetSolidFlags)>();
    C_CollisionProperty__UseTriggerBounds = module.Offset(0x1B9EC0).RCast<decltype(C_CollisionProperty__UseTriggerBounds)>();
    C_CollisionProperty__SetSurroundingBoundsType = module.Offset(0x1B9A10).RCast<decltype(C_CollisionProperty__SetSurroundingBoundsType)>();
    C_CollisionProperty__BoundingRadius2D = module.Offset(0x1B6820).RCast<decltype(C_CollisionProperty__BoundingRadius2D)>();
    C_CollisionProperty__CollisionToWorldSpace = module.Offset(0xBF580).RCast<decltype(C_CollisionProperty__CollisionToWorldSpace)>();
    C_CollisionProperty__WorldToCollisionSpace = module.Offset(0x1BA1D0).RCast<decltype(C_CollisionProperty__WorldToCollisionSpace)>();
    C_CollisionProperty__WorldDirectionToCollisionSpace = module.Offset(0x1B9F20).RCast<decltype(C_CollisionProperty__WorldDirectionToCollisionSpace)>();
    C_CollisionProperty__RandomPointInBounds = module.Offset(0x1B9600).RCast<decltype(C_CollisionProperty__RandomPointInBounds)>();
    C_CollisionProperty__IsPointInBounds = module.Offset(0x1B8A80).RCast<decltype(C_CollisionProperty__IsPointInBounds)>();
    C_CollisionProperty__CollisionAABBToWorldAABB = module.Offset(0x1B6C70).RCast<decltype(C_CollisionProperty__CollisionAABBToWorldAABB)>();
    C_CollisionProperty__CollisionToNormalizedSpace = module.Offset(0x1B6DF0).RCast<decltype(C_CollisionProperty__CollisionToNormalizedSpace)>();
    C_CollisionProperty__WorldToNormalizedSpace = module.Offset(0x1BA2B0).RCast<decltype(C_CollisionProperty__WorldToNormalizedSpace)>();
    C_CollisionProperty__NormalizedToWorldSpace = module.Offset(0x1B8F10).RCast<decltype(C_CollisionProperty__NormalizedToWorldSpace)>();
    C_CollisionProperty__CalcNearestPoint = module.Offset(0x1B6A20).RCast<decltype(C_CollisionProperty__CalcNearestPoint)>();
    C_CollisionProperty__CalcDistanceFromPoint = module.Offset(0x1B6900).RCast<decltype(C_CollisionProperty__CalcDistanceFromPoint)>();
    C_CollisionProperty__CalcSqrDistanceFromPoint = module.Offset(0x1B6B30).RCast<decltype(C_CollisionProperty__CalcSqrDistanceFromPoint)>();
    C_CollisionProperty__DoesVPhysicsInvalidateSurroundingBox = module.Offset(0x1B8480).RCast<decltype(C_CollisionProperty__DoesVPhysicsInvalidateSurroundingBox)>();
    C_CollisionProperty__MarkSurroundingBoundsDirty = module.Offset(0x1B8E40).RCast<decltype(C_CollisionProperty__MarkSurroundingBoundsDirty)>();
    C_CollisionProperty__ComputeSupportMap = module.Offset(0x1B7C40).RCast<decltype(C_CollisionProperty__ComputeSupportMap)>();
    C_CollisionProperty__ComputeVPhysicsSurroundingBox = module.Offset(0x1B8180).RCast<decltype(C_CollisionProperty__ComputeVPhysicsSurroundingBox)>();
    C_CollisionProperty__ComputeHitboxSurroundingBox = module.Offset(0x1B7120).RCast<decltype(C_CollisionProperty__ComputeHitboxSurroundingBox)>();
    C_CollisionProperty__ComputeEntitySpaceHitboxSurroundingBox = module.Offset(0x1B6F70).RCast<decltype(C_CollisionProperty__ComputeEntitySpaceHitboxSurroundingBox)>();
    C_CollisionProperty__ComputeCollisionSurroundingBox = module.Offset(0x1B6F50).RCast<decltype(C_CollisionProperty__ComputeCollisionSurroundingBox)>();
    C_CollisionProperty__ComputeRotationExpandedBounds = module.Offset(0x1B74D0).RCast<decltype(C_CollisionProperty__ComputeRotationExpandedBounds)>();
    C_CollisionProperty__ComputeSurroundingBox = module.Offset(0x1B7D10).RCast<decltype(C_CollisionProperty__ComputeSurroundingBox)>();
    v_UpdateDirtySpatialPartitionEntities = module.Offset(0x1B9C80).RCast<decltype(v_UpdateDirtySpatialPartitionEntities)>();
})
