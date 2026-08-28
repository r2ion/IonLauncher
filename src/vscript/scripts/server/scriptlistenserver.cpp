#include "vscript/languages/squirrel_re/squirrel.h"
bool g_bNSClearPlaylistOverrides = false;

ADD_SQFUNC(
	"void",
	NSMarkClearPlaylistOverrides,
	"",
	"Marks that the listen server should clear playlist overrides next mapspawn.",
	ScriptContext::UI)
{
	g_bNSClearPlaylistOverrides = true;
	return SQRESULT_NULL;
}

ADD_SQFUNC(
	"bool",
	NSShouldClearPlaylistOverrides,
	"",
	"Returns whether the listen server should clear playlist overrides.",
	ScriptContext::SERVER)
{
	g_pSquirrel[context]->pushbool(sqvm, g_bNSClearPlaylistOverrides);
	g_bNSClearPlaylistOverrides = false;
	return SQRESULT_NOTNULL;
}

