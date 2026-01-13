#include "netmessages.h"
#include "netchannel.h"

bool g_bListeningforCustomServerInfoPacket = false;
char g_szLastServerInfoName[256] = {0};
bool g_bNextServerAuthUs = false;
bool g_bNextServerAllowingAuthUs = false;

bool g_bReceivedAuthNotify = false;
bool g_bReceivedServerInfo = false;

AUTOHOOK_INIT()

// clang-format off
AUTOHOOK(CClient__ConnectionStart, engine.dll + 0x1019C0, bool, __fastcall, (__int64 thisptr, CNetChan* chan))
// clang-format on
{
	return CClient__ConnectionStart(thisptr, chan);
}

// clang-format off
AUTOHOOK(CClientState__ConnectionStart, engine.dll + 0x8CB40, bool, __fastcall, (__int64 thisptr, CNetChan* chan))
// clang-format on
{
	return CClientState__ConnectionStart(thisptr, chan);
}

ON_DLL_LOAD_RELIESON("engine.dll", NetMessages, NetChan, (CModule module))
{
	AUTOHOOK_DISPATCH();
}
