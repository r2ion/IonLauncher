#include "tier1/convar.h"
#include "tier1/cvar.h"
#include "engine/r2engine.h"
#include "server/r2server.h"
#include "engine/client/client.h"
DECLARE_MODULE(ServerAllTalkHooks)

size_t __fastcall ShouldAllowAlltalk()
{
	// this needs to return a 64 bit integer where 0 = true and 1 = false
	static ConVar* Cvar_sv_alltalk = g_pCVar->FindVar("sv_alltalk");
	if (Cvar_sv_alltalk->GetBool())
		return 0;

	// lobby should default to alltalk, otherwise don't allow it
	return strcmp(g_pGlobals->m_pMapName, "mp_lobby");
}

CClient* AdjustShiftedThisPointer(CClient* shiftedPointer)
{
	/* Original function called method "CClient::ExecuteStringCommand" with an optimization
	 * that shifted the 'this' pointer with 8 bytes.
	 * Since this has been inlined with "CClient::ProcessStringCmd" as of S2, the shifting
	 * happens directly to anything calling this function. */
	char* pShifted = reinterpret_cast<char*>(shiftedPointer) - 8;
	return reinterpret_cast<CClient*>(pShifted);
}

DECLARE_HOOK(CClient_ProcessVoiceData, engine.dll + 0x104560, [](auto& hook, CClient* client, void* pMsg) -> bool
{
	auto pAdj = AdjustShiftedThisPointer(client);
	auto extended = pAdj->GetClientExtended();
	if (!extended)
	{
		spdlog::error("failed to get extended");
		return hook.Original(client, pMsg);
	}
	if (extended->IsClientCommsBanned())
	{
		return true;
	}
	return hook.Original(client, pMsg);
})

ADD_SQFUNC("void", NSSetVoiceCommsBanned, "entity ent, bool value", "", ScriptContext::SERVER)
{
	auto pPlayer = g_pSquirrel[context]->template getentity<CPlayer>(sqvm, 1);

	bool value = g_pSquirrel[context]->getbool(sqvm, 2);
	if (!pPlayer)
	{
		spdlog::error("bad ent");
		g_pSquirrel[context]->raiseerror(sqvm, "bad ent");
		return SQRESULT_ERROR;
	}
	int32_t client_idx = (pPlayer->m_nPlayerIndex) - 1;
	if (client_idx < 0 || client_idx > 32)
	{
		spdlog::error("bad client index: {}",client_idx);
		g_pSquirrel[context]->raiseerror(sqvm, "bad ent index");
		return SQRESULT_ERROR;
	}
	auto client = &g_pClientArray[client_idx];
	if (!client)
	{
		spdlog::error("bad client{}", client_idx);
		g_pSquirrel[context]->raiseerror(sqvm, "bad client");
		return SQRESULT_ERROR;
	}
	auto extend = client->GetClientExtended();
	if (!extend)
	{
		spdlog::error("bad client{}", client_idx);
		g_pSquirrel[context]->raiseerror(sqvm, "bad extend");
		return SQRESULT_ERROR;
	}
	extend->SetClientIsCommsBanned(value);
	return SQRESULT_NOTNULL;
}


ADD_SQFUNC("bool", NSIsVoiceCommsBanned, "entity ent", "", ScriptContext::SERVER)
{
	auto pPlayer = g_pSquirrel[context]->template getentity<CPlayer>(sqvm, 1);
	if (!pPlayer)
	{
		spdlog::error("bad ent");
		g_pSquirrel[context]->raiseerror(sqvm, "bad ent");
		return SQRESULT_ERROR;
	}
	int32_t client_idx = (pPlayer->m_nPlayerIndex) - 1;
	if (client_idx < 0 || client_idx > 32)
	{
		spdlog::error("bad client index: {}", client_idx);
		g_pSquirrel[context]->raiseerror(sqvm, "bad ent index");
		return SQRESULT_ERROR;
	}
	auto client = &g_pClientArray[client_idx];
	if (!client)
	{
		spdlog::error("bad client{}", client_idx);
		g_pSquirrel[context]->raiseerror(sqvm, "bad client");
		return SQRESULT_ERROR;
	}
	auto extend = client->GetClientExtended();
	if (!extend)
	{
		spdlog::error("bad client{}", client_idx);
		g_pSquirrel[context]->raiseerror(sqvm, "bad extend");
		return SQRESULT_ERROR;
	}
	g_pSquirrel[context]->pushbool(sqvm, extend->IsClientCommsBanned());
	return SQRESULT_NOTNULL;
}



ON_DLL_LOAD_RELIESON("engine.dll", ServerAllTalk, ConVar, [](CModule module)
{
	ServerAllTalkHooks.DispatchForModule("engine.dll");
	// replace strcmp function called in CClient::ProcessVoiceData with our own code that calls ShouldAllowAllTalk
	CMemory base = module.Offset(0x1085FA);

	base.Patch("48 B8"); // mov rax, 64 bit int
	// (uint8_t*)&ShouldAllowAlltalk doesn't work for some reason? need to make it a uint64 first
	uint64_t pShouldAllowAllTalk = reinterpret_cast<uint64_t>(ShouldAllowAlltalk);
	base.Offset(0x2).Patch((uint8_t*)&pShouldAllowAllTalk, 8);
	base.Offset(0xA).Patch("FF D0"); // call rax

	// nop until compare (test eax, eax)
	base.Offset(0xC).NoOP(0x7);
})
