#include "pakfile.h"

PakGlobalState_s* g_pakGlobalState;

ON_DLL_LOAD("rtech_game.dll", PakFile, (CModule module))
{
	g_pakGlobalState = module.Offset(0x43270).RCast<PakGlobalState_s*>();
}
