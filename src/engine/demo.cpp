#include "engine/demo.h"

CDemoPlayer* s_ClientDemoPlayer;

ON_DLL_LOAD_RELIESON("engine.dll", Demo, ConVar, [](CModule module)
{
	s_ClientDemoPlayer = module.Offset(0xFD15A90).RCast<CDemoPlayer*>();
})
