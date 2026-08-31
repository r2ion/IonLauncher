#include "engine/client/clientstate.h"
#include "tier0/callbacks.h"

using CClientStateIsPausedFn = bool (*)(const CClientState*);
using CClientStateGetFrameTimeFn = float (*)(const CClientState*);

static CClientStateIsPausedFn s_CClientStateIsPaused;
static CClientStateGetFrameTimeFn s_CClientStateGetFrameTime;

bool CClientState::IsPaused() const
{
	return s_CClientStateIsPaused(this);
}

float CClientState::GetFrameTime() const
{
	return s_CClientStateGetFrameTime(this);
}

ON_DLL_LOAD_CLIENT("engine.dll", ClientStateMethods, [](CModule module)
{
	s_CClientStateIsPaused = module.Offset(0x8F520).RCast<CClientStateIsPausedFn>();
	s_CClientStateGetFrameTime = module.Offset(0x8E400).RCast<CClientStateGetFrameTimeFn>();
})
