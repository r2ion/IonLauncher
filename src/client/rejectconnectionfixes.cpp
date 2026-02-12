#include "engine/r2engine.h"

DECLARE_MODULE(RejectConnectionFixesHooks)

// this is called from  when our connection is rejected, this is the only case we're hooking this for
DECLARE_HOOK(COM_ExplainDisconnection, engine.dll + 0x1342F0, [](auto& hook, bool a1, const char* fmt, ...)
{
	char buf[4096] = {};

	if (hook.HasVarArgs())
	{
		va_list copied;
		va_copy(copied, *hook.VarArgs());
		vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, copied);
		va_end(copied);
	}
	else
	{
		_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s", fmt ? fmt : "");
	}

	if (fmt && !strncmp(fmt, "Connection rejected: ", 21))
	{
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "disconnect", cmd_source_t::kCommandSrcCode);
	}

	hook.Original(a1, "%s", buf);
})

ON_DLL_LOAD_CLIENT("engine.dll", RejectConnectionFixes, [](CModule module)
{
	DISPATCH_MODULE(RejectConnectionFixesHooks)
})
