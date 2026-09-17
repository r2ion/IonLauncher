//========= Copyright Valve Corporation, All rights reserved. ============//

#pragma once

#include "engine/ehandle.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "particle_parse.h"
#include "tier1/smartptr.h"
#include "tier1/utlvector.h"

#include <cstddef>

class C_BaseEntity;
class CParticleEffect;
struct datamap_t;
class C_ParticleSystemDefinition;

inline constexpr int INVALID_PARTICLE_ATTACHMENT = -1;

struct ParticleControlPoint_t
{
    int iControlPoint;
    ParticleControlPoint_t()
        : iControlPoint(0), iAttachType(PATTACH_ABSORIGIN_FOLLOW), iAttachmentPoint(0),
          vecOriginOffset(0.0f, 0.0f, 0.0f)
    {
        matOffset.Invalidate();
    }

    ParticleAttachment_t iAttachType;
    int iAttachmentPoint;
    Vector3D vecOriginOffset;
    matrix3x4_t matOffset;
    CHandle<C_BaseEntity> hEntity;
};

struct ParticleEffectList_t
{
    CUtlVector<ParticleControlPoint_t> pControlPoints;
    CSmartPtr<CParticleEffect, CRefCountAccessor> pParticleEffect;
};

class CParticleProperty
{
  public:
    CParticleProperty();
    virtual datamap_t* GetPredDescMap();
    virtual const datamap_t* GetPredDescMapConst() const;
    virtual datamap_t* GetDataDescMap();
    virtual ~CParticleProperty();

    void Init(C_BaseEntity* pEntity) { m_pOuter = pEntity; }
    C_BaseEntity* GetOuter() { return m_pOuter; }
    const C_BaseEntity* GetOuter() const { return m_pOuter; }

    CParticleEffect* Create(const char* pszParticleName, ParticleAttachment_t iAttachType, const char* pszAttachmentName);
    CParticleEffect* Create(const char* pszParticleName, ParticleAttachment_t iAttachType,
                           int iAttachmentPoint = INVALID_PARTICLE_ATTACHMENT, Vector3D vecOriginOffset = Vector3D(0, 0, 0),
                           matrix3x4_t* matOffset = nullptr);
    CParticleEffect* Create(C_ParticleSystemDefinition* pDef, ParticleAttachment_t iAttachType,
                           int iAttachmentPoint = INVALID_PARTICLE_ATTACHMENT, Vector3D vecOriginOffset = Vector3D(0, 0, 0),
                           matrix3x4_t* matOffset = nullptr);
    CParticleEffect* Create(int nPrecacheIndex, ParticleAttachment_t iAttachType,
                           int iAttachmentPoint = INVALID_PARTICLE_ATTACHMENT, Vector3D vecOriginOffset = Vector3D(0, 0, 0),
                           matrix3x4_t* matOffset = nullptr);
    void AddControlPoint(CParticleEffect* pEffect, int iPoint, C_BaseEntity* pEntity, ParticleAttachment_t iAttachType,
                         const char* pszAttachmentName = nullptr, Vector3D vecOriginOffset = Vector3D(0, 0, 0),
                         matrix3x4_t* matOffset = nullptr);
    bool AddControlPoint(CParticleEffect* pEffect, int iPoint, C_BaseEntity* pEntity, ParticleAttachment_t iAttachType,
                         int iAttachmentPoint, Vector3D vecOriginOffset = Vector3D(0, 0, 0), matrix3x4_t* matOffset = nullptr);
    void AddControlPoint(int iEffectIndex, int iPoint, C_BaseEntity* pEntity, ParticleAttachment_t iAttachType,
                         int iAttachmentPoint = INVALID_PARTICLE_ATTACHMENT, Vector3D vecOriginOffset = Vector3D(0, 0, 0),
                         matrix3x4_t* matOffset = nullptr);
    void RemoveControlPoint(CParticleEffect* pEffect, int iPoint);
    const Vector3D* GetControlPoint(CParticleEffect* pEffect, int iPoint) const;
    void SetControlPointParent(CParticleEffect* pEffect, int whichControlPoint, int parentIdx);
    void SetControlPointParent(int iEffectIndex, int whichControlPoint, int parentIdx);

    void StopEmission(CParticleEffect* pEffect = nullptr, bool bWakeOnStop = false, bool bDestroyAsleepSystems = false,
                      bool bForceRemoveInstantly = false, bool bPlayEndCap = true);
    void StopEmissionAndDestroyImmediately(CParticleEffect* pEffect = nullptr);
    void StopParticlesInvolving(C_BaseEntity* pEntity, bool bForceRemoveInstantly = false);
    int StopParticlesNamed(const char* pszEffectName, bool bForceRemoveInstantly = false,
                           int nSplitScreenSlot = -1, bool bWakeOnStop = false, bool bPlayEndCap = false);
    void StopParticlesWithNameAndAttachment(const char* pszEffectName, int iAttachmentPoint, bool bForceRemoveInstantly = false);
    bool OnParticleSystemUpdated(CParticleEffect* pEffect, float flTimeDelta);
    void OnParticleSystemDeleted(CParticleEffect* pEffect);
    void OwnerSetDormantTo(bool bDormant);
    void ReplaceParticleEffect(CParticleEffect* pOldEffect, CParticleEffect* pNewEffect);
    void TransferOwnership(CParticleProperty& newOwner);
    void DebugPrintEffects();
    int FindEffect(const char* pEffectName);
    CParticleEffect* GetParticleEffectFromIdx(int idx) { return m_ParticleEffects[idx].pParticleEffect.GetObject(); }

  private:
    int GetParticleAttachment(C_BaseEntity* pEntity, const char* pszAttachmentName, const char* pszParticleName);
    int FindEffect(CParticleEffect* pEffect);
    void UpdateParticleEffect(ParticleEffectList_t* pEffect, bool bInitializing = false, int iOnlyThisControlPoint = -1);
    void UpdateControlPoint(ParticleEffectList_t* pEffect, int iPoint, bool bInitializing);
    bool ComputeControlPointForSpawnCull(Vector3D* pVecOutPos, C_ParticleSystemDefinition* pDef,
                                        C_BaseEntity* pEntity, ParticleAttachment_t iAttachType,
                                        int iAttachmentPoint, Vector3D vecOriginOffset);

  public:

    C_BaseEntity* m_pOuter;
    CUtlVector<ParticleEffectList_t> m_ParticleEffects;
    int m_iDormancyChangedAtFrame;
};
