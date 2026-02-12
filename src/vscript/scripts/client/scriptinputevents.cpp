#include "core/convar/convar.h"
#include "vscript/squirrel/squirrel.h"

DECLARE_MODULE(ScriptInputEventsHooks)

#define CInputSystem__PostEvent_SQFunc "CInputSystem__ProcessPostEvent"

#define CALL_INPUTSYS_SQ_FUNC(context) \
	if( g_pSquirrel[ScriptContext::context]->m_pSQVM && g_pSquirrel[ScriptContext::context]->m_pSQVM->sqvm ) \
	{ \
		HSQUIRRELVM sqvm = g_pSquirrel[ScriptContext::context]->m_pSQVM->sqvm; \
		SQObject functionobj {}; \
		if( g_pSquirrel[ScriptContext::context]->sq_getfunction(sqvm, CInputSystem__PostEvent_SQFunc, &functionobj, 0) == 0) \
			g_pSquirrel[ScriptContext::context]->AsyncCall( \
				CInputSystem__PostEvent_SQFunc, nType, nTick, nData, nData2, nData3); \
	}

// clang-format off
DECLARE_HOOK(CInputSystem__PostEvent, inputsystem.dll + 0x7EC0, ([](auto& hook, void* self, int nType, int nTick, int nData, int nData2, int nData3)
// clang-format on
{
	hook.Original(self, nType, nTick, nData, nData2, nData3);
	CALL_INPUTSYS_SQ_FUNC(CLIENT);
	CALL_INPUTSYS_SQ_FUNC(UI);
}))

ON_DLL_LOAD_RELIESON("inputsystem.dll", FastCallbacks, ConVar, [](CModule module)
{
	DISPATCH_MODULE(ScriptInputEventsHooks);
})
