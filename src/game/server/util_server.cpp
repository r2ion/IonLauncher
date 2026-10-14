#include "game/server/util_server.h"

using UTIL_PlayerByIndex_t = CPlayer* (__fastcall*)(int playerIndex);
static UTIL_PlayerByIndex_t g_pfnUTIL_PlayerByIndex;

CPlayer* UTIL_PlayerByIndex(const int playerIndex)
{
    return g_pfnUTIL_PlayerByIndex(playerIndex);
}

ON_DLL_LOAD("server.dll", ServerUtilities, [](CModule module)
{
    g_pfnUTIL_PlayerByIndex = module.Offset(0x26AA10).RCast<UTIL_PlayerByIndex_t>();
})
