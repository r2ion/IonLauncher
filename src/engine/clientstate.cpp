#include "engine/client/clientstate.h"
#include "tier0/callbacks.h"

CClientStateExtended CClientState::sm_ClientStateExtended;

CClientStateExtended* CClientState::GetClientStateExtended() const
{
    return &sm_ClientStateExtended;
}

using CClientStateIsPausedFn = bool (*)(const CClientState*);
using CClientStateGetFrameTimeFn = float (*)(const CClientState*);
using CClientStateSendStringCmdFn = void (*)(CClientState*, const char*);

static CClientStateIsPausedFn s_CClientStateIsPaused;
static CClientStateGetFrameTimeFn s_CClientStateGetFrameTime;
static CClientStateSendStringCmdFn s_CClientStateSendStringCmd;

bool CClientState::IsPaused() const
{
	return s_CClientStateIsPaused(this);
}

float CClientState::GetFrameTime() const
{
	return s_CClientStateGetFrameTime(this);
}

void CClientState::SendStringCmd(const char* command)
{
	s_CClientStateSendStringCmd(this, command);
}

ON_DLL_LOAD_CLIENT("engine.dll", ClientStateMethods, [](CModule module)
{
	s_CClientStateIsPaused = module.Offset(0x8F520).RCast<CClientStateIsPausedFn>();
	s_CClientStateGetFrameTime = module.Offset(0x8E400).RCast<CClientStateGetFrameTimeFn>();
	s_CClientStateSendStringCmd = module.Offset(0x91A10).RCast<CClientStateSendStringCmdFn>();
})
