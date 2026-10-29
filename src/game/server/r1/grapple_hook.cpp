#include "game/server/baseentity.h"
#include "game/shared/r1/grapple_hook.h"
#include "tier0/hooks.h"

DECLARE_MODULE(ServerGrappleHooks)

//-----------------------------------------------------------------------------
// Purpose: Decides whether a grapple may move |mover|, see GetTitanGrappleRule
//-----------------------------------------------------------------------------
DECLARE_HOOK(CGrappleHook::CanMoveEntity, server.dll + 0x6101B0,
             [](auto& hook, CBaseEntity* mover, CBaseEntity* other, bool isSelfPull) -> bool
{
    switch (GetTitanGrappleRule(mover, other, isSelfPull))
    {
    case GrappleTitanRule_t::Deny:
        return false;
    case GrappleTitanRule_t::Allow:
        return true;
    default:
        return hook.Original(mover, other, isSelfPull);
    }
})

ON_DLL_LOAD("server.dll", ServerGrapple, [](CModule)
{
    DISPATCH_HOOK(ServerGrappleHooks, CGrappleHook::CanMoveEntity)
})
