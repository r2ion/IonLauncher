#include "engine/demo.h"
#include "vscript/squirrel/squirrel.h"

ADD_SQFUNC("bool", Demo_IsPlayingBack, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, s_ClientDemoPlayer->IsPlayingBack());
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", Demo_TogglePause, "", "", ScriptContext::UI)
{
	if (s_ClientDemoPlayer->IsPlaybackPaused())
	{
		s_ClientDemoPlayer->ResumePlayback();
	}
	else
	{
		s_ClientDemoPlayer->PausePlayback(0);
	}

	return SQRESULT_NULL;
}

ADD_SQFUNC("int", Demo_GetPlaybackTick, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushinteger(sqvm, s_ClientDemoPlayer->GetPlaybackTick());

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("int", Demo_GetTotalTicks, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushinteger(sqvm, s_ClientDemoPlayer->GetTotalTicks());

	return SQRESULT_NOTNULL;
}

std::vector<char*> buf;

ADD_SQFUNC("array<string>", Demo_GetDemoFiles, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->newarray(sqvm, 0);

	for (char* item : buf)
		delete item;

	buf.clear();

	for (const auto& entry : fs::directory_iterator("./r2"))
	{
		if (entry.path().extension() == ".dem")
		{
			char* buff_entry = new char[255];
			strcpy(buff_entry, entry.path().filename().string().c_str());
			buf.push_back(buff_entry);

			g_pSquirrel[context]->pushstring(sqvm, buff_entry);
			g_pSquirrel[context]->arrayappend(sqvm, -2);
		}
	}

	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", Demo_StopPlayback, "", "", ScriptContext::UI)
{
	Cbuf_AddText(Cbuf_GetCurrentPlayer(), "stopdemo", cmd_source_t::kCommandSrcCode);
	Cbuf_Execute();
	return SQRESULT_NULL;
}
