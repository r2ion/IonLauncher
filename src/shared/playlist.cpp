#include "playlist.h"
#include "core/convar/concommand.h"
#include "tier0/vanilla.h"
#include "vscript/squirrel/squirrel.h"
#include "engine/hoststate.h"
#include "engine/r2engine.h"
#include "server/serverpresence.h"
#include "dedicated/dedicated.h"

DECLARE_MODULE(PlaylistHooks)

ConVar* Cvar_ns_use_clc_SetPlaylistVarOverride;

// clang-format off
DECLARE_HOOK(clc_SetPlaylistVarOverride::Process, engine.dll + 0x222180,
[](auto& hook, void* a1, void* a2) -> char
// clang-format on
{
	// the private_match playlist on mp_lobby is the only situation where there should be any legitimate sending of this netmessage
	if (!Cvar_ns_use_clc_SetPlaylistVarOverride->GetBool() || strcmp(R2::GetCurrentPlaylistName(), "private_match") ||
		strcmp(g_pGlobals->m_pMapName, "mp_lobby"))
		return 1;

	return hook.Original(a1, a2);
})

// clang-format off
DECLARE_HOOK(SetCurrentPlaylist, engine.dll + 0x18EB20,
[](auto& hook, const char* pPlaylistName) -> bool
// clang-format on
{
	bool bSuccess = hook.Original(pPlaylistName);

	if (bSuccess)
	{
		spdlog::info("Set playlist to {}", R2::GetCurrentPlaylistName());
		g_pServerPresence->SetPlaylist(R2::GetCurrentPlaylistName());
	}

	return bSuccess;
})

// clang-format off
DECLARE_HOOK(SetPlaylistVarOverride, engine.dll + 0x18ED00,
[](auto& hook, const char* pVarName, const char* pValue)
// clang-format on
{
	if (strlen(pValue) >= 64)
		return;

	hook.Original(pVarName, pValue);
})

// clang-format off
DECLARE_HOOK(GetCurrentPlaylistVar, engine.dll + 0x18C680,
[](auto& hook, const char* pVarName, bool bUseOverrides) -> const char*
// clang-format on
{
	if (!bUseOverrides && !strcmp(pVarName, "max_players"))
		bUseOverrides = true;

	return hook.Original(pVarName, bUseOverrides);
})

// clang-format off
DECLARE_HOOK(GetCurrentGamemodeMaxPlayers, engine.dll + 0x18C430,
[](auto& hook) -> int
// clang-format on
{
	const char* pMaxPlayers = R2::GetCurrentPlaylistVar("max_players", 0);
	if (!pMaxPlayers)
		return hook.Original();

	int iMaxPlayers = atoi(pMaxPlayers);
	return iMaxPlayers;
})

void ConCommand_playlist(const CCommand& args)
{
	if (args.ArgC() < 2)
		return;

	R2::SetCurrentPlaylist(args.Arg(1));
}

void ConCommand_setplaylistvaroverride(const CCommand& args)
{
	if (args.ArgC() < 3)
		return;

	for (int i = 1; i < args.ArgC(); i += 2)
		R2::SetPlaylistVarOverride(args.Arg(i), args.Arg(i + 1));
}

ON_DLL_LOAD_RELIESON("engine.dll", PlaylistHooks, (ConCommand, ConVar), [](CModule module)
{
	DISPATCH_MODULE(PlaylistHooks)

	R2::GetCurrentPlaylistName = module.Offset(0x18C640).RCast<decltype(R2::GetCurrentPlaylistName)>();
	R2::SetCurrentPlaylist = module.Offset(0x18EB20).RCast<decltype(R2::SetCurrentPlaylist)>();
	R2::SetPlaylistVarOverride = module.Offset(0x18ED00).RCast<decltype(R2::SetPlaylistVarOverride)>();
	R2::GetCurrentPlaylistVar = module.Offset(0x18C680).RCast<decltype(R2::GetCurrentPlaylistVar)>();

	// playlist is the name of the command on respawn servers, but we already use setplaylist so can't get rid of it
	RegisterConCommand("playlist", ConCommand_playlist, "Sets the current playlist", FCVAR_NONE);
	RegisterConCommand("setplaylist", ConCommand_playlist, "Sets the current playlist", FCVAR_NONE);
	RegisterConCommand("setplaylistvaroverrides", ConCommand_setplaylistvaroverride, "sets a playlist var override", FCVAR_NONE);

	// note: clc_SetPlaylistVarOverride is pretty insecure, since it allows for entirely arbitrary playlist var overrides to be sent to the
	// server, this is somewhat restricted on custom servers to prevent it being done outside of private matches, but ideally it should be
	// disabled altogether, since the custom menus won't use it anyway this should only really be accepted if you want vanilla client
	// compatibility
	// sonny: This has been patched in vanilla for yonks and on Northstar I can't see what the issue is since Process is hooked to validate some stuff anyway
	// private matches are basically a no-mans-land for playlist var validation anyway.
	Cvar_ns_use_clc_SetPlaylistVarOverride = new ConVar(
		"ns_use_clc_SetPlaylistVarOverride", "1", FCVAR_GAMEDLL, "Whether the server should accept clc_SetPlaylistVarOverride messages");

	// patch to prevent clc_SetPlaylistVarOverride from being able to crash servers if we reach max overrides due to a call to Error (why is
	// this possible respawn, wtf) todo: add a warning for this
	module.Offset(0x18ED8D).Patch("C3");

	if( IsDedicatedServer() )
		module.Offset(0x18ED17).NoOP(6);
})
