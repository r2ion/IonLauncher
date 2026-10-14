//========= Copyright Valve Corporation, All rights reserved. ============//
#include "game/shared/particle_property.h"
#include "game/client/c_baseanimating.h"
#include "tier1/strtools.h"

CParticleEffect* (*CParticleProperty__CreateByName)(CParticleProperty*, const char*, ParticleAttachment_t, const char*);
CParticleEffect* (*CParticleProperty__CreateByNameWithOffset)(CParticleProperty*, const char*, ParticleAttachment_t, int, Vector3D, matrix3x4_t*);
CParticleEffect* (*CParticleProperty__CreateFromDefWithOffset)(CParticleProperty*, C_ParticleSystemDefinition*, ParticleAttachment_t, int, Vector3D,
                                                      matrix3x4_t*);
CParticleEffect* (*CParticleProperty__CreatePrecachedWithOffset)(CParticleProperty*, int, ParticleAttachment_t, int, Vector3D, matrix3x4_t*);
void (*CParticleProperty__AddControlPointByName)(CParticleProperty*, CParticleEffect*, int, C_BaseEntity*, ParticleAttachment_t, const char*, Vector3D,
                                              matrix3x4_t*);
bool (*CParticleProperty__AddControlPoint)(CParticleProperty*, CParticleEffect*, int, C_BaseEntity*, ParticleAttachment_t, int, Vector3D, matrix3x4_t*);
void (*CParticleProperty__AddControlPointByIndex)(CParticleProperty*, int, int, C_BaseEntity*, ParticleAttachment_t, int, Vector3D, matrix3x4_t*);
void (*CParticleProperty__RemoveControlPoint)(CParticleProperty*, CParticleEffect*, int);
const Vector3D* (*CParticleProperty__GetControlPoint)(const CParticleProperty*, CParticleEffect*, int);
void (*s_ParticleSetControlPointParent)(CParticleEffect*, int, int);
void (*CParticleProperty__StopEmission)(CParticleProperty*, CParticleEffect*, bool, bool, bool, bool);
void (*CParticleProperty__StopEmissionAndDestroyImmediately)(CParticleProperty*, CParticleEffect*);
void (*CParticleProperty__StopParticlesInvolving)(CParticleProperty*, C_BaseEntity*, bool);
int (*CParticleProperty__StopParticlesNamed)(CParticleProperty*, const char*, bool, int, bool, bool);
bool (*CParticleProperty__OnParticleSystemUpdated)(CParticleProperty*, CParticleEffect*, float);
void (*CParticleProperty__OnParticleSystemDeleted)(CParticleProperty*, CParticleEffect*);
void (*CParticleProperty__OwnerSetDormantTo)(CParticleProperty*, bool);
void (*CParticleProperty__ReplaceParticleEffect)(CParticleProperty*, CParticleEffect*, CParticleEffect*);
void (*CParticleProperty__TransferOwnership)(CParticleProperty*, CParticleProperty*);
int (*CParticleProperty__FindEffectByName)(CParticleProperty*, const char*);
void (*CParticleProperty__UpdateControlPoint)(CParticleProperty*, ParticleEffectList_t*, int, bool);
bool (*CParticleProperty__ComputeControlPointForSpawnCull)(CParticleProperty*, Vector3D*, C_ParticleSystemDefinition*, C_BaseEntity*, ParticleAttachment_t, int, Vector3D);
const char* (*s_ParticleEffectName)(CParticleEffect*);

CParticleEffect* CParticleProperty::Create(const char* name, ParticleAttachment_t type, const char* attachment)
{
    return CParticleProperty__CreateByName(this, name, type, attachment);
}
CParticleEffect* CParticleProperty::Create(const char* name, ParticleAttachment_t type, int attachment, Vector3D offset, matrix3x4_t* transform)
{
    return CParticleProperty__CreateByNameWithOffset(this, name, type, attachment, offset, transform);
}
CParticleEffect* CParticleProperty::Create(C_ParticleSystemDefinition* definition, ParticleAttachment_t type, int attachment, Vector3D offset,
                                           matrix3x4_t* transform)
{
    return CParticleProperty__CreateFromDefWithOffset(this, definition, type, attachment, offset, transform);
}
CParticleEffect* CParticleProperty::Create(int precacheIndex, ParticleAttachment_t type, int attachment, Vector3D offset, matrix3x4_t* transform)
{
    return CParticleProperty__CreatePrecachedWithOffset(this, precacheIndex, type, attachment, offset, transform);
}
void CParticleProperty::AddControlPoint(CParticleEffect* effect, int point, C_BaseEntity* entity, ParticleAttachment_t type, const char* attachment,
                                        Vector3D offset, matrix3x4_t* transform)
{
    CParticleProperty__AddControlPointByName(this, effect, point, entity, type, attachment, offset, transform);
}
bool CParticleProperty::AddControlPoint(CParticleEffect* effect, int point, C_BaseEntity* entity, ParticleAttachment_t type, int attachment,
                                        Vector3D offset, matrix3x4_t* transform)
{
    return CParticleProperty__AddControlPoint(this, effect, point, entity, type, attachment, offset, transform);
}
void CParticleProperty::AddControlPoint(int effect, int point, C_BaseEntity* entity, ParticleAttachment_t type, int attachment, Vector3D offset,
                                        matrix3x4_t* transform)
{
    CParticleProperty__AddControlPointByIndex(this, effect, point, entity, type, attachment, offset, transform);
}
void CParticleProperty::RemoveControlPoint(CParticleEffect* effect, int point)
{
    CParticleProperty__RemoveControlPoint(this, effect, point);
}
const Vector3D* CParticleProperty::GetControlPoint(CParticleEffect* effect, int point) const
{
    return CParticleProperty__GetControlPoint(this, effect, point);
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
    CParticleProperty__StopEmission(this, effect, wake, destroyAsleep, force, endCap);
}
void CParticleProperty::StopEmissionAndDestroyImmediately(CParticleEffect* effect)
{
    CParticleProperty__StopEmissionAndDestroyImmediately(this, effect);
}
void CParticleProperty::StopParticlesInvolving(C_BaseEntity* entity, bool force)
{
    CParticleProperty__StopParticlesInvolving(this, entity, force);
}
int CParticleProperty::StopParticlesNamed(const char* name, bool force, int splitScreenSlot, bool wake, bool endCap)
{
    return CParticleProperty__StopParticlesNamed(this, name, force, splitScreenSlot, wake, endCap);
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
    return CParticleProperty__OnParticleSystemUpdated(this, effect, delta);
}
void CParticleProperty::OnParticleSystemDeleted(CParticleEffect* effect)
{
    CParticleProperty__OnParticleSystemDeleted(this, effect);
}
void CParticleProperty::OwnerSetDormantTo(bool dormant)
{
    CParticleProperty__OwnerSetDormantTo(this, dormant);
}
void CParticleProperty::ReplaceParticleEffect(CParticleEffect* oldEffect, CParticleEffect* newEffect)
{
    CParticleProperty__ReplaceParticleEffect(this, oldEffect, newEffect);
}
void CParticleProperty::TransferOwnership(CParticleProperty& newOwner)
{
    CParticleProperty__TransferOwnership(this, &newOwner);
}
int CParticleProperty::FindEffect(const char* name)
{
    return CParticleProperty__FindEffectByName(this, name);
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
    CParticleProperty__UpdateControlPoint(this, effect, point, initializing);
}
bool CParticleProperty::ComputeControlPointForSpawnCull(Vector3D* position, C_ParticleSystemDefinition* definition, C_BaseEntity* entity,
                                                        ParticleAttachment_t type, int attachment, Vector3D offset)
{
    return CParticleProperty__ComputeControlPointForSpawnCull(this, position, definition, entity, type, attachment, offset);
}

ON_DLL_LOAD_CLIENT("client.dll", ParticlePropertyMethods, [](CModule module)
{
    CParticleProperty__CreateByName = module.Offset(0x27BFA0).RCast<decltype(CParticleProperty__CreateByName)>();
    CParticleProperty__CreateByNameWithOffset = module.Offset(0x27C090).RCast<decltype(CParticleProperty__CreateByNameWithOffset)>();
    CParticleProperty__CreateFromDefWithOffset = module.Offset(0x27C120).RCast<decltype(CParticleProperty__CreateFromDefWithOffset)>();
    CParticleProperty__CreatePrecachedWithOffset = module.Offset(0x27C370).RCast<decltype(CParticleProperty__CreatePrecachedWithOffset)>();
    CParticleProperty__AddControlPointByName = module.Offset(0x27BB40).RCast<decltype(CParticleProperty__AddControlPointByName)>();
    CParticleProperty__AddControlPoint = module.Offset(0x27BC80).RCast<decltype(CParticleProperty__AddControlPoint)>();
    CParticleProperty__AddControlPointByIndex = module.Offset(0x27BD30).RCast<decltype(CParticleProperty__AddControlPointByIndex)>();
    CParticleProperty__RemoveControlPoint = module.Offset(0x27D5E0).RCast<decltype(CParticleProperty__RemoveControlPoint)>();
    CParticleProperty__GetControlPoint = module.Offset(0x27C950).RCast<decltype(CParticleProperty__GetControlPoint)>();
    s_ParticleSetControlPointParent = module.Offset(0x291150).RCast<decltype(s_ParticleSetControlPointParent)>();
    CParticleProperty__StopEmission = module.Offset(0x27D870).RCast<decltype(CParticleProperty__StopEmission)>();
    CParticleProperty__StopEmissionAndDestroyImmediately = module.Offset(0x27DA20).RCast<decltype(CParticleProperty__StopEmissionAndDestroyImmediately)>();
    CParticleProperty__StopParticlesInvolving = module.Offset(0x27DB10).RCast<decltype(CParticleProperty__StopParticlesInvolving)>();
    CParticleProperty__StopParticlesNamed = module.Offset(0x27DC40).RCast<decltype(CParticleProperty__StopParticlesNamed)>();
    CParticleProperty__OnParticleSystemUpdated = module.Offset(0x27D110).RCast<decltype(CParticleProperty__OnParticleSystemUpdated)>();
    CParticleProperty__OnParticleSystemDeleted = module.Offset(0x27D080).RCast<decltype(CParticleProperty__OnParticleSystemDeleted)>();
    CParticleProperty__OwnerSetDormantTo = module.Offset(0x27D1C0).RCast<decltype(CParticleProperty__OwnerSetDormantTo)>();
    CParticleProperty__ReplaceParticleEffect = module.Offset(0x27D680).RCast<decltype(CParticleProperty__ReplaceParticleEffect)>();
    CParticleProperty__TransferOwnership = module.Offset(0x27DEA0).RCast<decltype(CParticleProperty__TransferOwnership)>();
    CParticleProperty__FindEffectByName = module.Offset(0x27C7E0).RCast<decltype(CParticleProperty__FindEffectByName)>();
    CParticleProperty__UpdateControlPoint = module.Offset(0x27E0E0).RCast<decltype(CParticleProperty__UpdateControlPoint)>();
    CParticleProperty__ComputeControlPointForSpawnCull = module.Offset(0x27C410).RCast<decltype(CParticleProperty__ComputeControlPointForSpawnCull)>();
    s_ParticleEffectName = module.Offset(0x644F20).RCast<decltype(s_ParticleEffectName)>();
})
