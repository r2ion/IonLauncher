//========= Copyright Valve Corporation, All rights reserved. ============//
// Source SDK 2013 collisionproperty.h API, adapted to retail R2.
// R2 has no pre-scaled bounds or uniform-trigger-bloat member; its native
// ICollideable vtable and scratch-transform arguments must not be replaced.
#pragma once

#include "engine/ICollideable.h"
#include "mathlib/vector.h"
#include "mathlib/mathlib.h"

void UpdateDirtySpatialPartitionEntities();

enum SurroundingBoundsType_t
{
    USE_OBB_COLLISION_BOUNDS = 0,
    USE_BEST_COLLISION_BOUNDS = 1,
    USE_MODEL_AND_OBB_BOUNDS = 2,
    USE_HITBOXES = 3,
    USE_HITBOXES_AND_OBB_BOUNDS = 4,
    USE_SPECIFIED_BOUNDS = 5,
    USE_GAME_CODE = 6,
    USE_ROTATION_EXPANDED_BOUNDS = 7,
    USE_COLLISION_BOUNDS_NEVER_VPHYSICS = 8,
    USE_SEQUENCE_BOUNDS = 9,
    USE_SEQUENCE_BOUNDS_WITH_POSE = 10,
    SURROUNDING_TYPE_BIT_COUNT = 4
};

class C_BaseEntity;

class CCollisionProperty : public ICollideable
{
  public:
    CCollisionProperty();
    ~CCollisionProperty();
    void Init(C_BaseEntity* pEntity);
    IHandleEntity* GetEntityHandle() override;
    const Vector3D& OBBMins() const override;
    const Vector3D& OBBMaxs() const override;
    float HitboxTestRadius() const override;
    void WorldSpaceTriggerBounds(Vector3D* pWorldMins, Vector3D* pWorldMaxs) const override;
    bool TestCollision(const Ray_t* pRay, std::uint32_t contentsMask, GameTrace* pTrace) override;
    bool TestHitboxes(const Ray_t* pRay, std::uint32_t contentsMask, GameTrace* pTrace) override;
    int GetCollisionModelIndex() override;
    const model_t* GetCollisionModel() override;
    const Vector3D& GetCollisionOrigin() const override;
    const QAngle& GetCollisionAngles() const override;
    float GetCollisionScale() const override;
    const matrix3x4_t& CollisionToWorldTransform(matrix3x4_t* pScratchTransform) const override;
    SolidType_t GetSolid() const override;
    SolidFlags_t GetSolidFlags() const override;
    IClientUnknown* GetIClientUnknown() override;
    int GetCollisionGroup() const override;
    std::uint32_t GetCollisionContentsMask() const override;
    void WorldSpaceSurroundingBounds(Vector3D* pWorldMins, Vector3D* pWorldMaxs) override;
    std::uint32_t GetSpatialPartitionMask() const override;
    const matrix3x4_t* GetRootParentToWorldTransform(matrix3x4_t* pScratchTransform) const override;
    CPhysCollide* GetPhysicsCollide() override;
    int GetCustomCollisionCount() const override;
    void GetCustomCollisionData(int index, const CollideableCustomCollisionRecord_t** ppRecords, int* pRecordCount,
                                int* pAuxiliaryCount) const override;
    void GetCustomCollisionDataCoarse(int index, const CollideableCustomCollisionRecord_t** ppRecords, int* pRecordCount) const override;
    void MarkPartitionHandleDirty() override;

    void CreatePartitionHandle();
    void DestroyPartitionHandle();
    unsigned short GetPartitionHandle() const { return m_Partition; }
    void UpdatePartition();
    void SetCollisionBounds(const Vector3D& mins, const Vector3D& maxs);
    void UseTriggerBounds(bool bEnable, float flBloat = 0.0f);
    void SetSurroundingBoundsType(SurroundingBoundsType_t type, const Vector3D* pMins = nullptr, const Vector3D* pMaxs = nullptr);
    void SetSolid(SolidType_t val);
    const Vector3D& OBBSize() const;
    float BoundingRadius() const { return m_flRadius; }
    float BoundingRadius2D() const;
    const Vector3D& OBBCenter() const;
    const Vector3D& WorldSpaceCenter() const;

    void ClearSolidFlags() { SetSolidFlags(0); }
    void RemoveSolidFlags(int flags) { SetSolidFlags(m_usSolidFlags & ~flags); }
    void AddSolidFlags(int flags) { SetSolidFlags(m_usSolidFlags | flags); }
    bool IsSolidFlagSet(int flagMask) const { return (m_usSolidFlags & flagMask) != 0; }
    void SetSolidFlags(int flags);
    bool IsSolid() const { return m_nSolidType != SOLID_NONE && !IsSolidFlagSet(FSOLID_NOT_SOLID); }
    bool IsBoundsDefinedInEntitySpace() const
    {
        return !IsSolidFlagSet(FSOLID_FORCE_WORLD_ALIGNED) && ((m_nSolidType & 0xF5) != 0 || m_nSolidType == 8);
    }

    const Vector3D& CollisionToWorldSpace(const Vector3D& in, Vector3D* pResult) const;
    const Vector3D& WorldToCollisionSpace(const Vector3D& in, Vector3D* pResult) const;
    const Vector3D& WorldDirectionToCollisionSpace(const Vector3D& in, Vector3D* pResult) const;
    void RandomPointInBounds(const Vector3D& vecNormalizedMins, const Vector3D& vecNormalizedMaxs, Vector3D* pPoint) const;
    bool IsPointInBounds(const Vector3D& vecWorldPt) const;
    void WorldSpaceAABB(Vector3D* pWorldMins, Vector3D* pWorldMaxs) const;
    const Vector3D& CollisionSpaceMins() const { return m_vecMins; }
    const Vector3D& CollisionSpaceMaxs() const { return m_vecMaxs; }
    const Vector3D& NormalizedToCollisionSpace(const Vector3D& in, Vector3D* pResult) const;
    const Vector3D& NormalizedToWorldSpace(const Vector3D& in, Vector3D* pResult) const;
    const Vector3D& WorldToNormalizedSpace(const Vector3D& in, Vector3D* pResult) const;
    const Vector3D& CollisionToNormalizedSpace(const Vector3D& in, Vector3D* pResult) const;
    void CalcNearestPoint(const Vector3D& vecWorldPt, Vector3D* pVecNearestWorldPt) const;
    float CalcDistanceFromPoint(const Vector3D& vecWorldPt) const;
    float CalcSqrDistanceFromPoint(const Vector3D& vecWorldPt) const;
    bool DoesRotationInvalidateSurroundingBox() const;
    bool DoesVPhysicsInvalidateSurroundingBox() const;
    void MarkSurroundingBoundsDirty();
    float ComputeSupportMap(const Vector3D& vecDirection) const;
    C_BaseEntity* GetOuter() { return m_pOuter; }
    const C_BaseEntity* GetOuter() const { return m_pOuter; }

  private:
    void CollisionAABBToWorldAABB(const Vector3D& entityMins, const Vector3D& entityMaxs,
                                Vector3D* pWorldMins, Vector3D* pWorldMaxs) const;
    void ComputeVPhysicsSurroundingBox(Vector3D* pVecWorldMins, Vector3D* pVecWorldMaxs);
    bool ComputeHitboxSurroundingBox(Vector3D* pVecWorldMins, Vector3D* pVecWorldMaxs);
    bool ComputeEntitySpaceHitboxSurroundingBox(Vector3D* pVecWorldMins, Vector3D* pVecWorldMaxs);
    void ComputeCollisionSurroundingBox(bool bUseVPhysics, Vector3D* pVecWorldMins, Vector3D* pVecWorldMaxs);
    void ComputeRotationExpandedBounds(Vector3D* pVecWorldMins, Vector3D* pVecWorldMaxs);
    void ComputeSurroundingBox(Vector3D* pVecWorldMins, Vector3D* pVecWorldMaxs);
    void CheckForUntouch();
    void UpdateServerPartitionMask();

  public:

    C_BaseEntity* m_pOuter;
    Vector3D m_vecMins;
    Vector3D m_vecMaxs;
    unsigned int m_usSolidFlags;
    unsigned char m_nSolidType;
    unsigned char m_triggerBloat;
    unsigned int m_recvSolidFlags;
    unsigned short m_recvSolidType;
    float m_flRadius;
    unsigned short m_Partition;
    unsigned char m_nSurroundType;
    bool m_hiddenFromSpatialQueries;
    unsigned int m_dirtyFlags;
    Vector3D m_vecSpecifiedSurroundingMins;
    Vector3D m_vecSpecifiedSurroundingMaxs;
    Vector3D m_vecSurroundingMins;
    Vector3D m_vecSurroundingMaxs;
    float m_hitboxTestRadius;
};

static_assert(sizeof(CCollisionProperty) == 0x78);
static_assert(offsetof(CCollisionProperty, m_usSolidFlags) == 0x28);
static_assert(offsetof(CCollisionProperty, m_recvSolidFlags) == 0x30);
static_assert(offsetof(CCollisionProperty, m_flRadius) == 0x38);
static_assert(offsetof(CCollisionProperty, m_Partition) == 0x3C);
static_assert(offsetof(CCollisionProperty, m_dirtyFlags) == 0x40);
static_assert(offsetof(CCollisionProperty, m_vecSpecifiedSurroundingMins) == 0x44);
static_assert(offsetof(CCollisionProperty, m_hitboxTestRadius) == 0x74);
