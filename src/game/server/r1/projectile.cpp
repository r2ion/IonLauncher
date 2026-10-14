#include "game/server/r1/projectile.h"
#include "game/server/player_lagcompensation.h"
#include "engine/server/sv_main.h"
#include "gametrace.h"
#include "tier0/hooks.h"

#include <algorithm>

DECLARE_MODULE(ProjectileHooks)

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
        endTime = g_pLagCompensationManager->GetNextSnapshotTime(owner, startTime);
        if (endTime <= startTime)
            endTime = targetTime;
        else
            endTime = std::min(endTime, targetTime);
    }
})

ON_DLL_LOAD_RELIESON("server.dll", Projectile, LagCompensation, [](CModule module)
{
    fl_pProjectileLagCompensationEndTime = module.Offset(0x1504350).RCast<const float*>();
    DISPATCH_HOOK(ProjectileHooks, CProjectile::SimulateLagCompensation)
})
