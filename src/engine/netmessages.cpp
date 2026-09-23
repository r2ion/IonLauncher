#include "common/netmessages.h"
#include "engine/client/client.h"
#include "engine/client/clientstate.h"
#include "tier0/vanilla.h"
#include "util/utils.h"

bool g_bListeningforCustomServerInfoPacket = false;
char g_szLastServerInfoName[256] = {0};
bool g_bNextServerAuthUs = false;
bool g_bNextServerAllowingAuthUs = false;
bool g_bSendingClientSignonCapability = false;

std::atomic_bool g_bReceivedAuthNotify = false;
std::atomic_bool g_bReceivedServerInfo = false;


DECLARE_MODULE(NetMessagesHooks)

DECLARE_HOOK(NET_SignonState::ReadFromBuffer, engine.dll + 0x222630, [](auto& hook, NET_SignonState* message, bf_read* buffer) -> bool
{
	if (g_pVanillaCompatibility->GetVanillaCompatibility())
		return hook.Original(message, buffer);

	return message->NET_SignonState::ReadFromBuffer(buffer);
})

DECLARE_HOOK(NET_SignonState::WriteToBuffer, engine.dll + 0x22B0E0, [](auto& hook, NET_SignonState* message, bf_write* buffer) -> bool
{
	if (g_pVanillaCompatibility->GetVanillaCompatibility())
		return hook.Original(message, buffer);

	if (!g_bSendingClientSignonCapability)
		return message->NET_SignonState::WriteToBuffer(buffer);

	const bool previousFlag = message->m_bSendPlaylists;
	message->m_bIsExtendedClient = true;
	const ScopeGuard restoreFlag([message, previousFlag] { message->m_bSendPlaylists = previousFlag; });
	return message->NET_SignonState::WriteToBuffer(buffer);
})

DECLARE_HOOK(CClientState::ProcessSignonStateInternal, engine.dll + 0x91D20,
             [](auto& hook,
                CClientState* clientState,
                eSignonState state,
                int serverCount,
                NET_SignonState* message) -> bool
{
	if (g_pVanillaCompatibility->GetVanillaCompatibility() || state != eSignonState::CONNECTED)
		return hook.Original(clientState, state, serverCount, message);

	const bool wasSendingClientSignonCapability = g_bSendingClientSignonCapability;
	g_bSendingClientSignonCapability = true;
	const ScopeGuard restoreSendingState(
	    [wasSendingClientSignonCapability] { g_bSendingClientSignonCapability = wasSendingClientSignonCapability; });
	return hook.Original(clientState, state, serverCount, message);
})

DECLARE_HOOK(CClient::ProcessSignonState, engine.dll + 0x104390,
             [](auto& hook, IClientMessageHandler* handler, NET_SignonState* message) -> bool
{
	if (g_pVanillaCompatibility->GetVanillaCompatibility())
		return hook.Original(handler, message);

	if (message->m_nSignonState == eSignonState::CONNECTED)
	{
		CClient* client = static_cast<CClient*>(handler);
		client->GetClientExtended()->SetIsExtendedClient(message->m_bIsExtendedClient);
	}

	return hook.Original(handler, message);
})

ON_DLL_LOAD("engine.dll", NetMessages, [](CModule module)
{
	DISPATCH_MODULE(NetMessagesHooks);
})
