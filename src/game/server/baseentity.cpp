#include "game/server/baseentity.h"

CBaseEntity* (*CBaseEntity__Instance)(int entityIndex);
const Vector3D& (*CBaseEntity__GetAbsVelocity)(CBaseEntity* entity);
void (*CBaseEntity__SetAbsVelocity)(CBaseEntity* entity, const Vector3D& velocity);
void (*CBaseEntity__NetworkStateChanged)(CBaseEntity* entity);
CBaseEntity_PrecacheModel_t CBaseEntity__PrecacheModel;
CBaseEntity_PrecacheScriptSound_t CBaseEntity__PrecacheScriptSound;

CBaseEntity* CBaseEntity::Instance(const int entityIndex)
{
    return CBaseEntity__Instance(entityIndex);
}

const Vector3D& CBaseEntity::GetAbsVelocity()
{
    return CBaseEntity__GetAbsVelocity(this);
}

void CBaseEntity::SetAbsVelocity(const Vector3D& velocity)
{
    CBaseEntity__SetAbsVelocity(this, velocity);
}

void CBaseEntity::NetworkStateChanged()
{
    CBaseEntity__NetworkStateChanged(this);
}

int CBaseEntity::PrecacheModel(const char* modelName)
{
    return CBaseEntity__PrecacheModel(modelName);
}

void CBaseEntity::PrecacheScriptSound(const char* soundName)
{
    CBaseEntity__PrecacheScriptSound(this, soundName);
}

ON_DLL_LOAD_RELIESON("server.dll", ServerHitscanTrace, ConVar, [](CModule module)
{
    CBaseEntity__Instance = module.Offset(0xFB820).RCast<decltype(CBaseEntity__Instance)>();
    CBaseEntity__GetAbsVelocity = module.Offset(0xA42F0).RCast<decltype(CBaseEntity__GetAbsVelocity)>();
    CBaseEntity__SetAbsVelocity = module.Offset(0x4328D0).RCast<decltype(CBaseEntity__SetAbsVelocity)>();
    CBaseEntity__NetworkStateChanged = module.Offset(0x5A4FD0).RCast<decltype(CBaseEntity__NetworkStateChanged)>();
    CBaseEntity__PrecacheModel = module.Offset(0x429550).RCast<CBaseEntity_PrecacheModel_t>();
    CBaseEntity__PrecacheScriptSound = module.Offset(0x6A8C70).RCast<CBaseEntity_PrecacheScriptSound_t>();
    // Ignore hitscan traces ending outside maximum map coordinates instead of terminating the server.
    module.Offset(0x43D4D8).NoOP(6);
})
