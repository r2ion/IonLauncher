#include "game/server/r1/weapon_missile.h"

#include "tier0/hooks.h"
#include "vscript/ivscript.h"

DECLARE_MODULE(ServerMissileHooks)

DECLARE_HOOK(CMissile::InitMissileForRandomDriftBasic, server.dll + 0x6755E0,
             [](auto& hook, CMissile* missile, const Vector3D& launchOrigin, const Vector3D& launchDir) -> void
{
    hook.Original(missile, launchOrigin, launchDir);
    ApplyPredictedMissileRandomDrift<ScriptContext::SERVER>(missile->m_pathSettings);
})

ON_DLL_LOAD("server.dll", ServerMissile, [](CModule)
{
    DISPATCH_HOOK(ServerMissileHooks, CMissile::InitMissileForRandomDriftBasic)
})
