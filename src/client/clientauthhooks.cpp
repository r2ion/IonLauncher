#include "client/r2client.h"
#include "core/convar/convar.h"
#include "tier0/vanilla.h"
#include "masterserver/masterserver.h"

ConVar* Cvar_ns_has_agreed_to_send_token;
char* pDummy3P = const_cast<char*>("Protocol 3: Protect the Pilot");

DECLARE_MODULE(ClientAuthHooks)

DECLARE_HOOK(Host_GetNucleusToken, engine.dll + 0x183760, [](auto& hook) -> char*
{
	if (g_pCVar->FindVar("serverfilter")->GetBool() && g_pMasterServerManager->m_sOwnClientAuthToken[0])
		return pDummy3P;

	return hook.Original();
})

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", ClientAuthHooks, ConVar, [](CModule module)
{
	DISPATCH_MODULE(ClientAuthHooks)

	// this cvar will save to cfg once initially agreed with
	Cvar_ns_has_agreed_to_send_token = new ConVar(
		"ns_has_agreed_to_send_token",
		"0",
		FCVAR_ARCHIVE_PLAYERPROFILE,
		"whether the user has agreed to send their origin token to the northstar masterserver");
})
