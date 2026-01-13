#include "vscript/squirrel/squirrel.h"
#include "engine/r2engine.h"
#include "engine/netmessages.h"
#include "masterserver/masterserver.h"
#include "client/r2client.h"
#include "core/tier0.h"
#include "client/connect.h"

ADD_SQFUNC("void", NSRequestServerInfo, "string ip, int port, bool requestMods, bool serverAuthUs", "", ScriptContext::UI)
{
	const SQChar* ip = g_pSquirrel[context]->getstring(sqvm, 1);
	SQInteger port = g_pSquirrel[context]->getinteger(sqvm, 2);
	bool requestMods = g_pSquirrel[context]->getbool(sqvm, 3);
	bool serverAuthUs = g_pSquirrel[context]->getbool(sqvm, 4);

	g_bReceivedServerInfo = false;
	g_bReceivedAuthNotify = false;

	std::string address = fmt::format("[{}]:{}", ip, port);
	netadr_t addr = CNetAdr();


	if (std::strchr(ip, ':') != nullptr)
    {
        // IPv6
        in6_addr ipv6Addr{};
        if (InetPtonA(AF_INET6, ip, &ipv6Addr) != 1)
        {
            spdlog::error("NSRequestServerInfo: invalid IPv6 address '{}'", ip);
            return SQRESULT_NULL;
        }

        // assumes you have an overload/ctor like CNetAdr(const in6_addr&, uint16_t)
        addr = CNetAdr();
		addr.SetIP(&ipv6Addr);
		addr.SetType(netadrtype_t::NA_IP);
		addr.SetPort(static_cast<uint16_t>(port));

		char dummyPackBuf[32];
		bf_write dummyPack(dummyPackBuf, sizeof(dummyPackBuf));
		dummyPack.WriteLong(CONNECTIONLESS_HEADER);
		NET_SendPacket(nullptr, NS_CLIENT, &addr, dummyPack.GetData(), dummyPack.GetNumBytesWritten(), nullptr, false, 0, true);
    }
	else
	{
		std::string address = fmt::format("[::ffff:{}]:{}", ip, port);
		addr = CNetAdr(address.c_str());
	}

	g_bNextServerAllowingAuthUs = false;
	g_bNextServerAuthUs = false;

	char buffer[256];
	bf_write msg(buffer, sizeof(buffer));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_REQUESTCUSTOMSERVERINFO);
	msg.WriteLong(CUSTOMSERVERINFO_VERSION);
	msg.WriteByte(requestMods);
	msg.WriteLong(MODDOWNLOADINFO_VERSION);

	if(serverAuthUs)
	{
		g_bNextServerAuthUs = true;
		msg.WriteByte(1);
		msg.WriteString(g_pLocalPlayerUserID);
		msg.WriteString(g_pMasterServerManager->m_sOwnClientAuthToken);
	}
	else
	{
		msg.WriteByte(0);
	}

	g_bListeningforCustomServerInfoPacket = true;

	NET_SendPacket(nullptr, NS_CLIENT, &addr, msg.GetData(), msg.GetNumBytesWritten(), nullptr, false, 0, true);

	return SQRESULT_NULL;
}

ADD_SQFUNC("bool", NSIsServerAuthingUs, "", "Returns true if the last requested server is authenticating us.", ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, g_bNextServerAllowingAuthUs);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("string", NSGetNameFromServerInfo, "", "Returns the name from the last received server info packet.", ScriptContext::UI)
{
	g_pSquirrel[context]->pushstring(sqvm, g_szLastServerInfoName);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSReceivedAuthNotify, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, g_bReceivedAuthNotify);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSReceivedServerInfo, "", "", ScriptContext::UI)
{
	g_pSquirrel[context]->pushbool(sqvm, g_bReceivedServerInfo);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSMarkConnectingToServer, "bool state", "Skips the retry logic and marks that we're connecting to the server.", ScriptContext::UI)
{
	g_bConnectingToServer = g_pSquirrel[context]->getbool(sqvm, 1);
	return SQRESULT_NULL;
}
