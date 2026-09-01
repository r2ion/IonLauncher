#include "common/netmessages.h"
#include "netchannel.h"

bool g_bListeningforCustomServerInfoPacket = false;
char g_szLastServerInfoName[256] = {0};
bool g_bNextServerAuthUs = false;
bool g_bNextServerAllowingAuthUs = false;

std::atomic_bool g_bReceivedAuthNotify = false;
std::atomic_bool g_bReceivedServerInfo = false;

DECLARE_MODULE(NetMessagesHooks)

DECLARE_HOOK(CClient__ConnectionStart, engine.dll + 0x1019C0, [](auto& hook, __int64 thisptr, CNetChan* chan) -> bool
{
	return hook.Original(thisptr, chan);
})

DECLARE_HOOK(CClientState__ConnectionStart, engine.dll + 0x8CB40, [](auto& hook, __int64 thisptr, CNetChan* chan) -> bool
{
	return hook.Original(thisptr, chan);
})

ON_DLL_LOAD_RELIESON("engine.dll", NetMessages, NetChan, [](CModule module)
{
	DISPATCH_MODULE(NetMessagesHooks);
})
