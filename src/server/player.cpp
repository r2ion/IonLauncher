#include "player.h"

CPlayer*(__fastcall* UTIL_PlayerByIndex)(int playerIndex);

ON_DLL_LOAD("server.dll", CPlayer, (CModule module))
{
	UTIL_PlayerByIndex = module.Offset(0x26AA10).RCast<CPlayer*(__fastcall*)(int)>();
}
