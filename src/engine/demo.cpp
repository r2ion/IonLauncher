#include "engine/demo.h"

CDemoPlayer* g_pDemoPlayer;
IDemoRecorder* g_pDemoRecorder;

ON_DLL_LOAD_RELIESON("engine.dll", Demo, ConVar, [](CModule module)
{
	g_pDemoPlayer = module.Offset(0xFD15A90).RCast<CDemoPlayer*>();
	g_pDemoRecorder = module.Offset(0x763D80).RCast<IDemoRecorder*>();
})
