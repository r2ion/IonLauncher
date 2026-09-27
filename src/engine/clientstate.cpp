#include "engine/client/clientstate.h"
#include "tier0/hooks.h"

#include <algorithm>

DECLARE_MODULE(EngineClientState)

char* g_pLocalPlayerUserID;
char* g_pLocalPlayerOriginToken;
GetBaseLocalClientType GetBaseLocalClient;
GetLocalPlayerIndexType GetLocalPlayerIndex;

CClientStateExtended CClientState::sm_ClientStateExtended;

CClientStateExtended* CClientState::GetClientStateExtended() const
{
    return &sm_ClientStateExtended;
}

using CClientStateIsPausedFn = bool (*)(const CClientState*);
using CClientStateGetFrameTimeFn = float (*)(const CClientState*);
using CClientStateSendStringCmdFn = void (*)(CClientState*, const char*);

CClientStateIsPausedFn CClientState__IsPaused;
CClientStateGetFrameTimeFn CClientState__GetFrameTime;
CClientStateSendStringCmdFn CClientState__SendStringCmd;

void* pAccumulateClientTimeReturnAddress;
double flClientTimeRemainder;
float flLastClientTime;

DECLARE_HOOK(CClientState__SetClientTime, engine.dll + 0x91AD0, [](auto& hook, CClientState* self, float clientTime)
{
    if (self->m_clientTime != flLastClientTime)
        flClientTimeRemainder = 0.0;

    if (hook.ReturnAddress() == pAccumulateClientTimeReturnAddress)
    {
        const double time = std::min(static_cast<double>(self->m_clientTime) + flClientTimeRemainder + self->m_frameTime,
                                     static_cast<double>(self->m_flServerUptime));
        clientTime = static_cast<float>(time);
        flClientTimeRemainder = time - static_cast<double>(clientTime);
    }
    else
    {
        flClientTimeRemainder = 0.0;
    }

    flLastClientTime = clientTime;
    hook.Original(self, clientTime);
})

bool CClientState::IsPaused() const
{
    return CClientState__IsPaused(this);
}

float CClientState::GetFrameTime() const
{
    return CClientState__GetFrameTime(this);
}

void CClientState::SendStringCmd(const char* command)
{
    CClientState__SendStringCmd(this, command);
}

ON_DLL_LOAD_CLIENT("engine.dll", ClientStateMethods, [](CModule module)
{
    g_pLocalPlayerUserID = module.Offset(0x13F8E688).RCast<char*>();
    g_pLocalPlayerOriginToken = module.Offset(0x13979C80).RCast<char*>();
    GetBaseLocalClient = module.Offset(0x78200).RCast<GetBaseLocalClientType>();
    GetLocalPlayerIndex = module.Offset(0x52260).RCast<GetLocalPlayerIndexType>();

    CClientState__IsPaused = module.Offset(0x8F520).RCast<CClientStateIsPausedFn>();
    CClientState__GetFrameTime = module.Offset(0x8E400).RCast<CClientStateGetFrameTimeFn>();
    CClientState__SendStringCmd = module.Offset(0x91A10).RCast<CClientStateSendStringCmdFn>();
    pAccumulateClientTimeReturnAddress = module.Offset(0x15952B).RCast<void*>();
    DISPATCH_MODULE(EngineClientState)
})
