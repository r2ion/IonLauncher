#include "vscript/languages/squirrel_re/squirrel.h"
#include "client/origin.h"

ADD_SQFUNC("table< string, string >", NSGetFriendSubscriptionMap, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->newtable(sqvm);

	if (g_IDPartySubMap.empty())
		return SQRESULT_NOTNULL;

	for (std::pair<__int64, std::string> item : g_IDPartySubMap)
	{
		g_pSquirrel[context]->pushstring(sqvm, std::to_string(item.first).c_str(), -1);
		g_pSquirrel[context]->pushstring(sqvm, item.second.c_str(), -1);

		g_pSquirrel[context]->newslot(sqvm, -3, false);
	}

	return SQRESULT_NOTNULL;
}
