#include "engine/r2engine.h"
#include "client/r2client.h"
#include "engine/netmessages.h"
#include "mods/autodownload/moddownloader.h"
#include "core/tier0.h"

AUTOHOOK_INIT()

AUTOHOOK(CClientState__ProcessConnectionlessPacket, engine.dll + 0x19F400, bool, __fastcall, (CClientState* self, netpacket_t* packet))
{
	bf_read msg(packet->data, packet->size);
	unsigned int header = msg.ReadLong();

	bool serverAuthingUs = false;
	char buff[512];
	int version;
	int notifyType;

	float serverNotifyTime;
	float clientNotifyTime;

	if (header == CONNECTIONLESS_HEADER)
	{
		char packetType = msg.ReadChar();

		switch(packetType)
		{
			case S2C_MODDOWNLOADINFO:
				return g_pModDownloader->RecvModInfoConnectionlessPacket(msg);
			case S2A_CUSTOMSERVERINFO:
				version = msg.ReadLong();
				if(version != CUSTOMSERVERINFO_VERSION)
					break;

				g_bReceivedServerInfo = true;

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

				g_bListeningforCustomServerInfoPacket = false;
				return true;
			case S2C_CLIENTNOTIFY:
				version = msg.ReadLong();
				spdlog::info("Received client notify packet, version {}", version);
				if(version != CLIENTNOTIFY_VERSION)
					break;

				notifyType = msg.ReadLong();
				serverNotifyTime = msg.ReadFloat();
				clientNotifyTime = Plat_FloatTime();

				g_bReceivedAuthNotify = true;

				switch(notifyType)
				{
					case NOTIFY_AUTHENTICATED:
					{
						char authToken[256];
						msg.ReadString(authToken, sizeof(authToken));
						g_pCVar->FindVar("serverfilter")->SetValue(authToken);
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

	return CClientState__ProcessConnectionlessPacket(self, packet);
}

ON_DLL_LOAD_RELIESON("engine.dll", ClientNetHooks, R2Engine, (CModule module))
{
	AUTOHOOK_DISPATCH();
}
