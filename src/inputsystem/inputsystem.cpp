#include "inputsystem/inputsystem.h"
#include "inputsystem/ButtonCode.h"
#include "engine/lowlatency.h"
#include "game/client/crouchkick.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"
#include "vscript/languages/squirrel_re/squirrel.h"

#include <cstdint>

DECLARE_MODULE(ScriptInputEventsHooks)

using CInputSystem_PostEvent_t = void (*)(CInputSystem*, InputEventType_t, int, int, int, int);
static CInputSystem_PostEvent_t CInputSystem__PostEvent;
static ConVar* Cvar_ion_use_ps4_icons;

void CInputSystem::PostEvent(InputEventType_t type, int tick, int data, int data2, int data3)
{
    CInputSystem__PostEvent(this, type, tick, data, data2, data3);
}

template <ScriptContext context>
void Script_InputPostEvent_Callback(int nType, int nTick, int nData, int nData2, int nData3)
{
    auto* squirrel = g_pSquirrel[context];
    if (squirrel->m_pSQVM && squirrel->m_pSQVM->sqvm)
        squirrel->AsyncCall("CInputSystem__ProcessPostEvent", nType, nTick, nData, nData2, nData3);
}

DECLARE_HOOK(CInputSystem::PostEvent, inputsystem.dll + 0x7EC0,
             [](auto& hook, CInputSystem* self, InputEventType_t nType, int nTick, int nData, int nData2, int nData3)
{
    if ((nType == IE_ButtonPressed || nType == IE_ButtonDoubleClicked) && nData == MOUSE_LEFT)
        LowLatency_MarkInputEvent();

    if (!CFKPostEvent(self, nType, nTick, nData, nData2, nData3))
    {
        Script_InputPostEvent_Callback<ScriptContext::CLIENT>(nType, nTick, nData, nData2, nData3);
        Script_InputPostEvent_Callback<ScriptContext::UI>(nType, nTick, nData, nData2, nData3);
        hook.Original(self, nType, nTick, nData, nData2, nData3);
    }
})

DECLARE_HOOK(CInputSystem::DetermineControllerType, inputsystem.dll + 0x7420,
             [](auto& hook, CInputSystem* self) -> std::uint32_t
{
    if (Cvar_ion_use_ps4_icons && Cvar_ion_use_ps4_icons->GetBool())
        return 3;
    return hook.Original(self);
})

ON_DLL_LOAD_CLIENT("inputsystem.dll", FastCallbacks, [](CModule module)
{
    DISPATCH_MODULE(ScriptInputEventsHooks);
    CInputSystem__PostEvent = HookSys::GetOriginalFunction<CInputSystem_PostEvent_t>(HookSys::FindHook("CInputSystem::PostEvent"));
})

ON_DLL_LOAD_CLIENT_RELIESON("inputsystem.dll", ScriptInputEvents, ConVar, [](CModule module)
{
    Cvar_ion_use_ps4_icons = new ConVar("ion_use_ps4_icons", "0", FCVAR_ARCHIVE_PLAYERPROFILE, "Use PS4 icons for controller");
})
