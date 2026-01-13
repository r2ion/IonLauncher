#include "client/r2client.h"
#include "core/convar/convar.h"
#include "core/vanilla.h"
#include "masterserver/masterserver.h"

ConVar* Cvar_ns_has_agreed_to_send_token;
char* pDummy3P = const_cast<char*>("Protocol 3: Protect the Pilot");

static char* (*__fastcall o_pAuth3PToken)() = nullptr;
static char* __fastcall h_Auth3PToken()
{
	if (g_pCVar->FindVar("serverfilter")->GetBool() && g_pMasterServerManager->m_sOwnClientAuthToken[0])
		return pDummy3P;

	return o_pAuth3PToken();
}

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", ClientAuthHooks, ConVar, (CModule module))
{
	o_pAuth3PToken = module.Offset(0x183760).RCast<decltype(o_pAuth3PToken)>();
	HookAttach(&(PVOID&)o_pAuth3PToken, (PVOID)h_Auth3PToken);

	// this cvar will save to cfg once initially agreed with
	Cvar_ns_has_agreed_to_send_token = new ConVar(
		"ns_has_agreed_to_send_token",
		"0",
		FCVAR_ARCHIVE_PLAYERPROFILE,
		"whether the user has agreed to send their origin token to the northstar masterserver");
}
