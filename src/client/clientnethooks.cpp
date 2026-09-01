#include "engine/r2engine.h"
#include "engine/client/clientstate.h"
#include "common/netmessages.h"
#include "modsystem/moddownloader.h"
#include "core/tier0.h"
#include "tier1/cvar.h"

DECLARE_MODULE(ClientNetHooks)

DECLARE_HOOK(CClientState__ProcessConnectionlessPacket, engine.dll + 0x19F400, [](auto& hook, CClientState* self, netpacket_t* packet) -> bool
{
	bf_read msg(packet->data, packet->size);
	unsigned int header = msg.ReadLong();

	bool serverAuthingUs = false;
	char buff[512];
	int version;
	int notifyType;


	if (header == CONNECTIONLESS_HEADER)
	{
		char packetType = msg.ReadChar();

		switch(packetType)
		{
			case S2C_MODDOWNLOADINFO:
				return false;
			case S2A_CUSTOMSERVERINFO:
				version = msg.ReadLong();
				if(version != CUSTOMSERVERINFO_VERSION)
					break;


				msg.ReadChar(); // marker
				if(!msg.ReadString(g_szLastServerInfoName, sizeof(g_szLastServerInfoName)))
					return false;

				msg.ReadString(buff, sizeof(buff)); // desc
				msg.ReadString(buff, sizeof(buff)); // map
				msg.ReadString(buff, sizeof(buff)); // playlist
				msg.ReadByte(); // reserved
				msg.ReadLong(); // player count
				msg.ReadLong(); // max players
				msg.ReadChar(); // d/l
				msg.ReadString(buff, sizeof(buff)); // region

				msg.ReadByte();
				serverAuthingUs = msg.ReadByte() != 0;

				if(serverAuthingUs && g_bNextServerAuthUs)
					g_bNextServerAllowingAuthUs = true;


				g_bReceivedServerInfo.store(true, std::memory_order_release);
				g_bListeningforCustomServerInfoPacket = false;
				return true;
			case S2C_CLIENTNOTIFY:
				version = msg.ReadLong();
				spdlog::info("Received client notify packet, version {}", version);
				if(version != CLIENTNOTIFY_VERSION)
					break;

				notifyType = msg.ReadLong();
				msg.ReadFloat();

				switch(notifyType)
				{
					case NOTIFY_AUTHENTICATED:
					{
						char authToken[256];
						if (!msg.ReadString(authToken, sizeof(authToken)))
							return false;

						g_pCVar->FindVar("serverfilter")->SetValue(authToken);
						g_bReceivedAuthNotify.store(true, std::memory_order_release);
						break;
					}
					default:
						break;
				}

				return true;
			default:
				break;
		}
	}

	return hook.Original(self, packet);
})

ON_DLL_LOAD_RELIESON("engine.dll", ClientNetHooks, R2Engine, [](CModule module)
{
	DISPATCH_MODULE(ClientNetHooks);
})
