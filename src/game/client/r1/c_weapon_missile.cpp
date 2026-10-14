#include "game/client/r1/c_weapon_missile.h"

#include "tier0/hooks.h"
#include "vscript/ivscript.h"

DECLARE_MODULE(ClientMissileHooks)

DECLARE_HOOK(C_Missile::InitMissileForRandomDriftBasic, client.dll + 0x590AE0,
             [](auto& hook, C_Missile* missile, const Vector3D& launchOrigin, const Vector3D& launchDir) -> void
{
    hook.Original(missile, launchOrigin, launchDir);
    ApplyPredictedMissileRandomDrift<ScriptContext::CLIENT>(missile->m_pathSettings);
})

ON_DLL_LOAD_CLIENT("client.dll", ClientMissile, [](CModule)
{
    DISPATCH_HOOK(ClientMissileHooks, C_Missile::InitMissileForRandomDriftBasic)
})
