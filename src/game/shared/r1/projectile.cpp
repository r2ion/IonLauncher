#include "game/shared/r1/projectile.h"
#include "engine/server/sv_main.h"
#include "gametrace.h"
#include "tier0/hooks.h"

#include <algorithm>

DECLARE_MODULE(ProjectileHooks)

class CLagCompensationManager;

CLagCompensationManager* g_pLagCompensationManager;
float (*CLagCompensationManager__GetNextSnapshotTime)(CLagCompensationManager*, const CBaseEntity*, float);
const float* fl_pProjectileLagCompensationEndTime;

DECLARE_HOOK(CProjectile::SimulateLagCompensation, server.dll + 0x63A7A0,
[](auto& hook, CProjectile* projectile, float startTime, float endTime, trace_t* trace) -> bool
{
    const CBaseEntity* const owner = projectile->GetOwnerEntityConst();

    const float targetTime = *fl_pProjectileLagCompensationEndTime;
    for (;;)
    {
        if (!hook.Original(projectile, startTime, endTime, trace))
            return false;

        const float simulatedTime = g_pGlobals->curtime;
        if (simulatedTime >= targetTime - 0.001f)
            return true;
        if (simulatedTime <= startTime)
            return false;

        startTime = simulatedTime;
        endTime = CLagCompensationManager__GetNextSnapshotTime(g_pLagCompensationManager, owner, startTime);
        if (endTime <= startTime)
            endTime = targetTime;
        else
            endTime = std::min(endTime, targetTime);
    }
})

ON_DLL_LOAD("server.dll", Projectile, [](CModule module)
{
    g_pLagCompensationManager = module.Offset(0x14E6830).RCast<CLagCompensationManager*>();
    CLagCompensationManager__GetNextSnapshotTime = module.Offset(0x5BD0D0).RCast<decltype(CLagCompensationManager__GetNextSnapshotTime)>();
    fl_pProjectileLagCompensationEndTime = module.Offset(0x1504350).RCast<const float*>();
    DISPATCH_HOOK(ProjectileHooks, CProjectile::SimulateLagCompensation)
})
