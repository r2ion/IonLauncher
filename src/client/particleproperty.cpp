//========= Copyright Valve Corporation, All rights reserved. ============//
#include "client/particleproperty.h"
#include "client/baseanimating.h"
#include "tier1/strtools.h"

CParticleEffect* (*s_ParticleCreateNamedAttachment)(CParticleProperty*, const char*, ParticleAttachment_t, const char*);
CParticleEffect* (*s_ParticleCreateNamed)(CParticleProperty*, const char*, ParticleAttachment_t, int, Vector3D, matrix3x4_t*);
CParticleEffect* (*s_ParticleCreateDefinition)(CParticleProperty*, C_ParticleSystemDefinition*, ParticleAttachment_t, int, Vector3D,
                                                      matrix3x4_t*);
CParticleEffect* (*s_ParticleCreatePrecached)(CParticleProperty*, int, ParticleAttachment_t, int, Vector3D, matrix3x4_t*);
void (*s_ParticleAddNamedControlPoint)(CParticleProperty*, CParticleEffect*, int, C_BaseEntity*, ParticleAttachment_t, const char*, Vector3D,
                                              matrix3x4_t*);
bool (*s_ParticleAddControlPoint)(CParticleProperty*, CParticleEffect*, int, C_BaseEntity*, ParticleAttachment_t, int, Vector3D, matrix3x4_t*);
void (*s_ParticleAddControlPointByIndex)(CParticleProperty*, int, int, C_BaseEntity*, ParticleAttachment_t, int, Vector3D, matrix3x4_t*);
void (*s_ParticleRemoveControlPoint)(CParticleProperty*, CParticleEffect*, int);
const Vector3D* (*s_ParticleGetControlPoint)(const CParticleProperty*, CParticleEffect*, int);
void (*s_ParticleSetControlPointParent)(CParticleEffect*, int, int);
void (*s_ParticleStopEmission)(CParticleProperty*, CParticleEffect*, bool, bool, bool, bool);
void (*s_ParticleDestroyImmediately)(CParticleProperty*, CParticleEffect*);
void (*s_ParticleStopInvolving)(CParticleProperty*, C_BaseEntity*, bool);
int (*s_ParticleStopNamed)(CParticleProperty*, const char*, bool, int, bool, bool);
bool (*s_ParticleSystemUpdated)(CParticleProperty*, CParticleEffect*, float);
void (*s_ParticleSystemDeleted)(CParticleProperty*, CParticleEffect*);
void (*s_ParticleOwnerDormancy)(CParticleProperty*, bool);
void (*s_ParticleReplaceEffect)(CParticleProperty*, CParticleEffect*, CParticleEffect*);
void (*s_ParticleTransferOwnership)(CParticleProperty*, CParticleProperty*);
int (*s_ParticleFindNamed)(CParticleProperty*, const char*);
void (*s_ParticleUpdateControlPoint)(CParticleProperty*, ParticleEffectList_t*, int, bool);
bool (*s_ParticleSpawnCull)(CParticleProperty*, Vector3D*, C_ParticleSystemDefinition*, C_BaseEntity*, ParticleAttachment_t, int, Vector3D);
const char* (*s_ParticleEffectName)(CParticleEffect*);

CParticleEffect* CParticleProperty::Create(const char* name, ParticleAttachment_t type, const char* attachment)
{
    return s_ParticleCreateNamedAttachment(this, name, type, attachment);
}
CParticleEffect* CParticleProperty::Create(const char* name, ParticleAttachment_t type, int attachment, Vector3D offset, matrix3x4_t* transform)
{
    return s_ParticleCreateNamed(this, name, type, attachment, offset, transform);
}
CParticleEffect* CParticleProperty::Create(C_ParticleSystemDefinition* definition, ParticleAttachment_t type, int attachment, Vector3D offset,
                                           matrix3x4_t* transform)
{
    return s_ParticleCreateDefinition(this, definition, type, attachment, offset, transform);
}
CParticleEffect* CParticleProperty::Create(int precacheIndex, ParticleAttachment_t type, int attachment, Vector3D offset, matrix3x4_t* transform)
{
    return s_ParticleCreatePrecached(this, precacheIndex, type, attachment, offset, transform);
}
void CParticleProperty::AddControlPoint(CParticleEffect* effect, int point, C_BaseEntity* entity, ParticleAttachment_t type, const char* attachment,
                                        Vector3D offset, matrix3x4_t* transform)
{
    s_ParticleAddNamedControlPoint(this, effect, point, entity, type, attachment, offset, transform);
}
bool CParticleProperty::AddControlPoint(CParticleEffect* effect, int point, C_BaseEntity* entity, ParticleAttachment_t type, int attachment,
                                        Vector3D offset, matrix3x4_t* transform)
{
    return s_ParticleAddControlPoint(this, effect, point, entity, type, attachment, offset, transform);
}
void CParticleProperty::AddControlPoint(int effect, int point, C_BaseEntity* entity, ParticleAttachment_t type, int attachment, Vector3D offset,
                                        matrix3x4_t* transform)
{
    s_ParticleAddControlPointByIndex(this, effect, point, entity, type, attachment, offset, transform);
}
void CParticleProperty::RemoveControlPoint(CParticleEffect* effect, int point)
{
    s_ParticleRemoveControlPoint(this, effect, point);
}
const Vector3D* CParticleProperty::GetControlPoint(CParticleEffect* effect, int point) const
{
    return s_ParticleGetControlPoint(this, effect, point);
}
void CParticleProperty::SetControlPointParent(CParticleEffect* effect, int point, int parent)
{
    s_ParticleSetControlPointParent(effect, point, parent);
}
void CParticleProperty::SetControlPointParent(int effect, int point, int parent)
{
    SetControlPointParent(GetParticleEffectFromIdx(effect), point, parent);
}
void CParticleProperty::StopEmission(CParticleEffect* effect, bool wake, bool destroyAsleep, bool force, bool endCap)
{
    s_ParticleStopEmission(this, effect, wake, destroyAsleep, force, endCap);
}
void CParticleProperty::StopEmissionAndDestroyImmediately(CParticleEffect* effect)
{
    s_ParticleDestroyImmediately(this, effect);
}
void CParticleProperty::StopParticlesInvolving(C_BaseEntity* entity, bool force)
{
    s_ParticleStopInvolving(this, entity, force);
}
int CParticleProperty::StopParticlesNamed(const char* name, bool force, int splitScreenSlot, bool wake, bool endCap)
{
    return s_ParticleStopNamed(this, name, force, splitScreenSlot, wake, endCap);
}
void CParticleProperty::StopParticlesWithNameAndAttachment(const char* name, int attachment, bool force)
{
    for (int i = m_ParticleEffects.Count() - 1; i >= 0; --i)
    {
        ParticleEffectList_t& entry = m_ParticleEffects[i];
        CParticleEffect* effect = entry.pParticleEffect.GetObject();
        if (V_stricmp(s_ParticleEffectName(effect), name) != 0)
            continue;
        for (int j = 0; j < entry.pControlPoints.Count(); ++j)
        {
            if (entry.pControlPoints[j].iAttachmentPoint == attachment)
            {
                if (force)
                    StopEmissionAndDestroyImmediately(effect);
                else
                    StopEmission(effect);
                break;
            }
        }
    }
}
bool CParticleProperty::OnParticleSystemUpdated(CParticleEffect* effect, float delta)
{
    return s_ParticleSystemUpdated(this, effect, delta);
}
void CParticleProperty::OnParticleSystemDeleted(CParticleEffect* effect)
{
    s_ParticleSystemDeleted(this, effect);
}
void CParticleProperty::OwnerSetDormantTo(bool dormant)
{
    s_ParticleOwnerDormancy(this, dormant);
}
void CParticleProperty::ReplaceParticleEffect(CParticleEffect* oldEffect, CParticleEffect* newEffect)
{
    s_ParticleReplaceEffect(this, oldEffect, newEffect);
}
void CParticleProperty::TransferOwnership(CParticleProperty& newOwner)
{
    s_ParticleTransferOwnership(this, &newOwner);
}
int CParticleProperty::FindEffect(const char* name)
{
    return s_ParticleFindNamed(this, name);
}
void CParticleProperty::DebugPrintEffects()
{
    for (int i = 0; i < m_ParticleEffects.Count(); ++i)
    {
        CParticleEffect* effect = GetParticleEffectFromIdx(i);
        if (effect)
            spdlog::info("({}) EffectName \"{}\"", i, s_ParticleEffectName(effect));
    }
}
int CParticleProperty::FindEffect(CParticleEffect* effect)
{
    for (int i = 0; i < m_ParticleEffects.Count(); ++i)
        if (GetParticleEffectFromIdx(i) == effect)
            return i;
    return -1;
}
int CParticleProperty::GetParticleAttachment(C_BaseEntity* entity, const char* attachment, const char* particleName)
{
    C_BaseAnimating* animating = entity ? entity->GetBaseAnimating() : nullptr;
    return animating && attachment ? animating->LookupAttachment(attachment) : INVALID_PARTICLE_ATTACHMENT;
}
void CParticleProperty::UpdateParticleEffect(ParticleEffectList_t* effect, bool initializing, int onlyThisControlPoint)
{
    if (onlyThisControlPoint != -1)
    {
        UpdateControlPoint(effect, onlyThisControlPoint, initializing);
        return;
    }
    for (int i = 0; i < effect->pControlPoints.Count(); ++i)
        UpdateControlPoint(effect, i, initializing);
}
void CParticleProperty::UpdateControlPoint(ParticleEffectList_t* effect, int point, bool initializing)
{
    s_ParticleUpdateControlPoint(this, effect, point, initializing);
}
bool CParticleProperty::ComputeControlPointForSpawnCull(Vector3D* position, C_ParticleSystemDefinition* definition, C_BaseEntity* entity,
                                                        ParticleAttachment_t type, int attachment, Vector3D offset)
{
    return s_ParticleSpawnCull(this, position, definition, entity, type, attachment, offset);
}

ON_DLL_LOAD_CLIENT("client.dll", ParticlePropertyMethods, [](CModule module)
{
    s_ParticleCreateNamedAttachment = module.Offset(0x27BFA0).RCast<decltype(s_ParticleCreateNamedAttachment)>();
    s_ParticleCreateNamed = module.Offset(0x27C090).RCast<decltype(s_ParticleCreateNamed)>();
    s_ParticleCreateDefinition = module.Offset(0x27C120).RCast<decltype(s_ParticleCreateDefinition)>();
    s_ParticleCreatePrecached = module.Offset(0x27C370).RCast<decltype(s_ParticleCreatePrecached)>();
    s_ParticleAddNamedControlPoint = module.Offset(0x27BB40).RCast<decltype(s_ParticleAddNamedControlPoint)>();
    s_ParticleAddControlPoint = module.Offset(0x27BC80).RCast<decltype(s_ParticleAddControlPoint)>();
    s_ParticleAddControlPointByIndex = module.Offset(0x27BD30).RCast<decltype(s_ParticleAddControlPointByIndex)>();
    s_ParticleRemoveControlPoint = module.Offset(0x27D5E0).RCast<decltype(s_ParticleRemoveControlPoint)>();
    s_ParticleGetControlPoint = module.Offset(0x27C950).RCast<decltype(s_ParticleGetControlPoint)>();
    s_ParticleSetControlPointParent = module.Offset(0x291150).RCast<decltype(s_ParticleSetControlPointParent)>();
    s_ParticleStopEmission = module.Offset(0x27D870).RCast<decltype(s_ParticleStopEmission)>();
    s_ParticleDestroyImmediately = module.Offset(0x27DA20).RCast<decltype(s_ParticleDestroyImmediately)>();
    s_ParticleStopInvolving = module.Offset(0x27DB10).RCast<decltype(s_ParticleStopInvolving)>();
    s_ParticleStopNamed = module.Offset(0x27DC40).RCast<decltype(s_ParticleStopNamed)>();
    s_ParticleSystemUpdated = module.Offset(0x27D110).RCast<decltype(s_ParticleSystemUpdated)>();
    s_ParticleSystemDeleted = module.Offset(0x27D080).RCast<decltype(s_ParticleSystemDeleted)>();
    s_ParticleOwnerDormancy = module.Offset(0x27D1C0).RCast<decltype(s_ParticleOwnerDormancy)>();
    s_ParticleReplaceEffect = module.Offset(0x27D680).RCast<decltype(s_ParticleReplaceEffect)>();
    s_ParticleTransferOwnership = module.Offset(0x27DEA0).RCast<decltype(s_ParticleTransferOwnership)>();
    s_ParticleFindNamed = module.Offset(0x27C7E0).RCast<decltype(s_ParticleFindNamed)>();
    s_ParticleUpdateControlPoint = module.Offset(0x27E0E0).RCast<decltype(s_ParticleUpdateControlPoint)>();
    s_ParticleSpawnCull = module.Offset(0x27C410).RCast<decltype(s_ParticleSpawnCull)>();
    s_ParticleEffectName = module.Offset(0x644F20).RCast<decltype(s_ParticleEffectName)>();
})
