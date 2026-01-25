#include "dedicatedlogtoclient.h"
#include "engine/client.h"
#include "engine/r2engine.h"

void (*CGameClient__ClientPrintf)(CClient* pClient, const char* fmt, ...);

void DedicatedServerLogToClientSink::sink_it_(const spdlog::details::log_msg& msg)
{
	if (*g_pServerState == server_state_t::ss_dead)
		return;

	enum class eSendPrintsToClient
	{
		NONE = -1,
		FIRST,
		ALL
	};

	static const ConVar* Cvar_dedi_sendPrintsToClient = g_pCVar->FindVar("dedi_sendPrintsToClient");
	eSendPrintsToClient eSendPrints = static_cast<eSendPrintsToClient>(Cvar_dedi_sendPrintsToClient->GetInt());
	if (eSendPrints == eSendPrintsToClient::NONE)
		return;

	std::string payload(msg.payload.data(), msg.payload.size());
	std::string sLogMessage = fmt::format("[DEDICATED SERVER] [{}] {}", level_names[msg.level], payload);
	for (int i = 0; i < g_pGlobals->m_nMaxClients; i++)
	{
		CClient* pClient = &g_pClientArray[i];

		if (pClient->m_nSignonState >= eSignonState::CONNECTED)
		{
			CGameClient__ClientPrintf(pClient, sLogMessage.c_str());

			if (eSendPrints == eSendPrintsToClient::FIRST)
				break;
		}
	}
}

void DedicatedServerLogToClientSink::flush_() {}

ON_DLL_LOAD_DEDI("engine.dll", DedicatedServerLogToClient, [](CModule module)
{
	CGameClient__ClientPrintf = module.Offset(0x1016A0).RCast<void (*)(CClient*, const char*, ...)>();
})
