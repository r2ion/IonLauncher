#include "vscript/languages/squirrel_re/squirrel.h"
#include "engine/client/clientstate.h"
ADD_SQFUNC("int", NSGetSignonState, "", "Returns the current signon state of the client.", ScriptContext::CLIENT | ScriptContext::UI)
{
	CClientState* client = GetBaseLocalClient();
	if (!client)
	{
		g_pSquirrel[context]->pushinteger(sqvm, -1);
		return SQRESULT_NOTNULL;
	}

	g_pSquirrel[context]->pushinteger(sqvm, static_cast<int>(client->m_nSignonState));
	return SQRESULT_NOTNULL;
}
