#include "game/server/player_lagcompensation.h"
#include "game/server/baseentity.h"
#include "mathlib/vector.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"
#include "util/utils.h"

DECLARE_MODULE(LagCompensationHooks)

CLagCompensationManager* g_pLagCompensationManager;
static float (*CLagCompensationManager__GetNextSnapshotTime)(CLagCompensationManager*, const CBaseEntity*, float);
static ConVar* Cvar_ns_skip_historical_hitbox_check;
static bool g_bReachedLagCompensationRayGeometry;

float CLagCompensationManager::GetNextSnapshotTime(const CBaseEntity* entity, float time)
{
    return CLagCompensationManager__GetNextSnapshotTime(this, entity, time);
}

DECLARE_HOOK(CalcDistanceSqrToLineSegment, server.dll + 0x6EF440,
             [](auto& hook, const Vector3D* point, const Vector3D* segmentStart, const Vector3D* segmentEnd, float* outFraction) -> float
{
    g_bReachedLagCompensationRayGeometry = true;

    return hook.Original(point, segmentStart, segmentEnd, outFraction);
})

DECLARE_HOOK(CLagCompensationManager::ShotShapeAllowsEntity, server.dll + 0x5C4A30,
             [](auto& hook, CLagCompensationManager* manager, CBaseEntity* candidate, const CCheckTransmitInfo* transmitInfo,
                unsigned int attackerMask) -> bool
{
    const bool previousReachedRayGeometry = g_bReachedLagCompensationRayGeometry;
    g_bReachedLagCompensationRayGeometry = false;
    const ScopeGuard restoreRayGeometry([previousReachedRayGeometry] { g_bReachedLagCompensationRayGeometry = previousReachedRayGeometry; });

    const bool nativeResult = hook.Original(manager, candidate, transmitInfo, attackerMask);
    const bool reachedRayGeometry = g_bReachedLagCompensationRayGeometry;
    return nativeResult || (Cvar_ns_skip_historical_hitbox_check->GetBool() && reachedRayGeometry && candidate && candidate->IsPlayer());
})

ON_DLL_LOAD_RELIESON("server.dll", LagCompensation, ConVar, [](CModule module)
{
    g_pLagCompensationManager = module.Offset(0x14E6830).RCast<CLagCompensationManager*>();
    CLagCompensationManager__GetNextSnapshotTime = module.Offset(0x5BD0D0).RCast<decltype(CLagCompensationManager__GetNextSnapshotTime)>();
    Cvar_ns_skip_historical_hitbox_check =
        new ConVar("ns_skip_historical_hitbox_check", "1", FCVAR_GAMEDLL,
                   "Improves hitreg against faster targets, minor performance impact when enabled.", true, 0.0f, true, 1.0f, nullptr);

    DISPATCH_MODULE(LagCompensationHooks);
})
