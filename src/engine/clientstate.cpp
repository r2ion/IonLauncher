#include "engine/client/clientstate.h"
#include "tier0/hooks.h"

#include <algorithm>
#include <cmath>

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


CClientStateIsPaused_t CClientState__IsPaused;
CClientStateGetFrameTime_t CClientState__GetFrameTime;
CClientStateSendStringCmd_t CClientState__SendStringCmd;

void* pAccumulateClientTimeReturnAddress;

double CClientState::GetPreciseClientTime() const
{
    auto& clock = GetClientStateExtended()->m_PreciseClientTime;
    if (this != clock.owner)
        return static_cast<double>(m_clientTime);

    if (m_clientTime != clock.clientTime || m_NetChannel != clock.channel || m_nServerCount != clock.serverCount ||
        m_nSignonState != clock.signonState)
    {
        clock.owner = nullptr;
        clock.remainder = 0.0;
        return static_cast<double>(m_clientTime);
    }

    return static_cast<double>(m_clientTime) + clock.remainder;
}

DECLARE_HOOK(CClientState__Clear, engine.dll + 0x8C580, [](auto& hook, CClientState* self)
{
    auto& clock = self->GetClientStateExtended()->m_PreciseClientTime;
    if (self == clock.owner)
    {
        clock.owner = nullptr;
        clock.remainder = 0.0;
    }
    hook.Original(self);
})

DECLARE_HOOK(CClientState__SetClientTime, engine.dll + 0x91AD0, [](auto& hook, CClientState* self, float clientTime)
{
    auto& clock = self->GetClientStateExtended()->m_PreciseClientTime;
    double remainder = 0.0;

    if (hook.ReturnAddress() == pAccumulateClientTimeReturnAddress)
    {
        const double time = std::min(self->GetPreciseClientTime() + self->m_frameTime, static_cast<double>(self->m_flServerUptime));
        clientTime = static_cast<float>(time);
        if (std::isfinite(time))
            remainder = time - static_cast<double>(clientTime);
    }
    hook.Original(self, clientTime);
    clock.owner = self;
    clock.channel = self->m_NetChannel;
    clock.serverCount = self->m_nServerCount;
    clock.signonState = self->m_nSignonState;
    clock.clientTime = clientTime;
    clock.remainder = remainder;
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

    CClientState__IsPaused = module.Offset(0x8F520).RCast<CClientStateIsPaused_t>();
    CClientState__GetFrameTime = module.Offset(0x8E400).RCast<CClientStateGetFrameTime_t>();
    CClientState__SendStringCmd = module.Offset(0x91A10).RCast<CClientStateSendStringCmd_t>();
    pAccumulateClientTimeReturnAddress = module.Offset(0x15952B).RCast<void*>();
    DISPATCH_MODULE(EngineClientState)
})
