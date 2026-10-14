#pragma once

class CBaseEntity;

class CLagCompensationManager
{
public:
    float GetNextSnapshotTime(const CBaseEntity* entity, float time);
};

extern CLagCompensationManager* g_pLagCompensationManager;
