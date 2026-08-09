#include "vscript/squirrel/squirrel.h"
#include "eos/eos_layer.h"
#include "engine/client/clientstate.h"
#include "engine/r2engine.h"
#include "tier1/cvar.h"

ADD_SQFUNC("string", NSGetLocalP2PEndpointAddress, "", "", ScriptContext::UI)
{
	auto& eosLayer = eos::EosLayer::Instance();
	if(!eosLayer.IsInitialized() || !eosLayer.IsReady())
	{
		spdlog::info("EOS layer not initialized or ready");
		g_pSquirrel[context]->pushstring(sqvm, "");
		return SQRESULT_NOTNULL;
	}

	eos::FakeIpLayer* ipLayer = eosLayer.GetFakeIpLayer();
	if (!ipLayer)
	{
		spdlog::warn("Failed to get FakeIpLayer");
		g_pSquirrel[context]->pushstring(sqvm, "");
		return SQRESULT_NOTNULL;
	}

	const eos::FakeEndpoint& endpoint = ipLayer->GetLocalEndpoint();
	if (!endpoint.IsValid())
	{
		spdlog::warn("Local P2P endpoint is not valid");
		g_pSquirrel[context]->pushstring(sqvm, "");
		return SQRESULT_NOTNULL;
	}

	char addrStr[INET6_ADDRSTRLEN] = {};
	inet_ntop(AF_INET6, &endpoint.address, addrStr, sizeof(addrStr));
	std::string result = fmt::format("{}", addrStr);

	g_pSquirrel[context]->pushstring(sqvm, result.c_str());
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("int", NSGetLocalP2PEndpointPort, "", "", ScriptContext::UI)
{
	auto& eosLayer = eos::EosLayer::Instance();

	if(!eosLayer.IsInitialized() || !eosLayer.IsReady())
	{
		g_pSquirrel[context]->pushinteger(sqvm, 0);
		return SQRESULT_NOTNULL;
	}

	eos::FakeIpLayer* ipLayer = eosLayer.GetFakeIpLayer();
	if (!ipLayer)
	{
		g_pSquirrel[context]->pushinteger(sqvm, 0);
		return SQRESULT_NOTNULL;
	}

	const eos::FakeEndpoint& endpoint = ipLayer->GetLocalEndpoint();
	if (!endpoint.IsValid())
	{
		g_pSquirrel[context]->pushinteger(sqvm, 0);
		return SQRESULT_NOTNULL;
	}

	g_pSquirrel[context]->pushinteger(sqvm, endpoint.port);
	return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSIsP2PConnection, "", "", ScriptContext::UI)
{
	CClientState* client = GetBaseLocalClient();

	if(g_pCVar->FindVar("ns_has_agreed_allow_eos")->GetInt() != 1)
	{
		g_pSquirrel[context]->pushbool(sqvm, false);
		return SQRESULT_NOTNULL;
	}

	std::string adrString = client->m_szServerAddress;

	if(adrString.find("[3ffe:") != std::string::npos)
	{
		g_pSquirrel[context]->pushbool(sqvm, true);
		return SQRESULT_NOTNULL;
	}

	if(g_pServerState && *g_pServerState >= ss_active)
	{
		g_pSquirrel[context]->pushbool(sqvm, true);
		return SQRESULT_NOTNULL;
	}

	g_pSquirrel[context]->pushbool(sqvm, false);
	return SQRESULT_NOTNULL;
}
