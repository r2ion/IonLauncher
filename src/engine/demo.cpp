#include "engine/demo.h"

CDemoPlayer* s_ClientDemoPlayer;

AUTOHOOK_INIT()

ON_DLL_LOAD_RELIESON("engine.dll", Demo, ConVar, (CModule module))
{
	AUTOHOOK_DISPATCH()

	s_ClientDemoPlayer = module.Offset(0xFD15A90).RCast<CDemoPlayer*>();
}
