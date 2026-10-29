#include "game/shared/r1/grapple_hook.h"

#include "tier0/hooks.h"
#include "tier1/convar.h"

ConVar* Cvar_ion_custom_titan_grapple;

bool IsCustomTitanGrappleEnabled()
{
    return Cvar_ion_custom_titan_grapple && Cvar_ion_custom_titan_grapple->GetBool();
}

ON_DLL_LOAD_RELIESON("engine.dll", CustomTitanGrappleConVar, ConVar, [](CModule)
{
    Cvar_ion_custom_titan_grapple =
        new ConVar("ion_custom_titan_grapple", "0", FCVAR_GAMEDLL | FCVAR_REPLICATED,
                   "Titan grapples pull the Titan towards the grappled entity instead of pulling the entity towards the Titan.", true, 0.0f, true, 1.0f,
                   nullptr);
})
