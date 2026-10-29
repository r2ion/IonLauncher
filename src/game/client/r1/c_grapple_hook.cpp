#include "game/client/c_baseentity.h"
#include "game/shared/r1/grapple_hook.h"
#include "tier0/hooks.h"

DECLARE_MODULE(ClientGrappleHooks)

//-----------------------------------------------------------------------------
// Purpose: Decides whether a grapple may move |mover|, see GetTitanGrappleRule
//-----------------------------------------------------------------------------
DECLARE_HOOK(C_GrappleHook::CanMoveEntity, client.dll + 0x4F7A60,
             [](auto& hook, C_BaseEntity* mover, C_BaseEntity* other, bool isSelfPull) -> bool
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

ON_DLL_LOAD_CLIENT("client.dll", ClientGrapple, [](CModule)
{
    DISPATCH_HOOK(ClientGrappleHooks, C_GrappleHook::CanMoveEntity)
})
