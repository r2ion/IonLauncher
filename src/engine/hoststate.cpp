#include "engine/hoststate.h"
#include "core/tier0.h"
#include "dedicated/dedicated.h"
#include "engine/r2engine.h"
#include "masterserver/masterserver.h"
#include "plugins/pluginmanager.h"
#include "server/auth/serverauthentication.h"
#include "server/serverpresence.h"
#include "shared/exploit_fixes/ns_limits.h"
#include "shared/playlist.h"
#include "vscript/squirrel/squirrel.h"
#include "tier0/vanilla.h"
#include "tier0/frametask.h"
#include "modsystem/moddownloader.h"
#include "eos/eos_layer.h"
#include "tier1/cvar.h"

DECLARE_MODULE(HostStateHooks)

CHostState* g_pHostState;

std::string sLastMode;

static ConVar* Cvar_hostport = nullptr;
static void(__fastcall* _Cmd_Exec_f)(const CCommand& arg, bool bOnlyIfExists, bool bUseWhitelists) = nullptr;

void ServerStartingOrChangingMap()
{
	ConVar* Cvar_mp_gamemode = g_pCVar->FindVar("mp_gamemode");
	g_pVanillaCompatibility->SetCompatabilityMode(VanillaCompatibility::CompatibilityMode::Northstar);

	g_pModDownloader->LoadServerModSchema();

	// directly call _Cmd_Exec_f to avoid weirdness with ; being in mp_gamemode potentially
	// if we ran exec {mp_gamemode} and mp_gamemode contained semicolons, this could be used to execute more commands
	char* commandBuf[1040]; // assumedly this is the size of CCommand since we don't have an actual constructor
	memset(commandBuf, 0, sizeof(commandBuf));
	CCommand tempCommand = *(CCommand*)&commandBuf;
	if (sLastMode.length() &&
		CCommand__Tokenize(tempCommand, fmt::format("exec server/cleanup_gamemode_{}", sLastMode).c_str(), cmd_source_t::kCommandSrcCode))
		_Cmd_Exec_f(tempCommand, false, false);

	memset(commandBuf, 0, sizeof(commandBuf));
	if (CCommand__Tokenize(
			tempCommand,
			fmt::format("exec server/setup_gamemode_{}", sLastMode = Cvar_mp_gamemode->GetString()).c_str(),
			cmd_source_t::kCommandSrcCode))
	{
		_Cmd_Exec_f(tempCommand, false, false);
	}

	Cbuf_Execute(); // exec everything right now

	// net_data_block_enabled is required for sp, force it if we're on an sp map
	// sucks for security but just how it be
	if (!strncmp(g_pHostState->m_levelName, "sp_", 3))
	{
		g_pCVar->FindVar("net_data_block_enabled")->SetValue(true);
		g_pServerAuthentication->m_bStartingLocalSPGame = true;
	}
	else
		g_pServerAuthentication->m_bStartingLocalSPGame = false;
}

DECLARE_HOOK(CHostState__State_NewGame, engine.dll + 0x16E7D0, [](auto& hook, CHostState* self)
{
	spdlog::info("HostState: NewGame");

	if (IsDedicatedServer())
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "exec autoexec_ns_dedicatedserver", cmd_source_t::kCommandSrcCode);
	else
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "exec autoexec_ns_listenserver", cmd_source_t::kCommandSrcCode);

	Cbuf_Execute();

	// need to do this to ensure we don't go to private match
	if (g_pServerAuthentication->m_bNeedLocalAuthForNewgame)
		R2::SetCurrentPlaylist("tdm");

	ServerStartingOrChangingMap();

	double dStartTime = g_PlatFloatTime();
	hook.Original(self);
	spdlog::info("loading took {}s", g_PlatFloatTime() - dStartTime);

	// setup server presence
	g_pServerPresence->CreatePresence();
	g_pServerPresence->SetMap(g_pHostState->m_levelName, true);
	g_pServerPresence->SetPlaylist(R2::GetCurrentPlaylistName());
	g_pServerPresence->SetPort(Cvar_hostport->GetInt());

	g_pServerAuthentication->m_bNeedLocalAuthForNewgame = false;
})

DECLARE_HOOK(CHostState__State_LoadGame, engine.dll + 0x16E730, [](auto& hook, CHostState* self)
{
	// singleplayer server starting
	// useless in 99% of cases but without it things could potentially break very much

	spdlog::info("HostState: LoadGame");

	if (IsDedicatedServer())
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "exec autoexec_ns_dedicatedserver", cmd_source_t::kCommandSrcCode);
	else
		Cbuf_AddText(Cbuf_GetCurrentPlayer(), "exec autoexec_ns_listenserver", cmd_source_t::kCommandSrcCode);

	Cbuf_Execute();

	// this is normally done in ServerStartingOrChangingMap(), but seemingly the map name isn't set at this point
	g_pCVar->FindVar("net_data_block_enabled")->SetValue(true);
	g_pServerAuthentication->m_bStartingLocalSPGame = true;

	double dStartTime = g_PlatFloatTime();
	hook.Original(self);
	spdlog::info("loading took {}s", g_PlatFloatTime() - dStartTime);

	// no server presence, can't do it because no map name in hoststate
	// and also not super important for sp saves really

	g_pServerAuthentication->m_bNeedLocalAuthForNewgame = false;
})

DECLARE_HOOK(CHostState__State_ChangeLevelMP, engine.dll + 0x16E520, [](auto& hook, CHostState* self)
{
	spdlog::info("HostState: ChangeLevelMP");

	ServerStartingOrChangingMap();

	double dStartTime = g_PlatFloatTime();
	hook.Original(self);
	spdlog::info("loading took {}s", g_PlatFloatTime() - dStartTime);

	g_pServerPresence->SetMap(g_pHostState->m_levelName);
})

DECLARE_HOOK(CHostState__State_GameShutdown, engine.dll + 0x16E640, [](auto& hook, CHostState* self)
{
	spdlog::info("HostState: GameShutdown");

	g_pServerPresence->DestroyPresence();

	hook.Original(self);

	// run gamemode cleanup cfg now instead of when we start next map
	if (sLastMode.length())
	{
		char* commandBuf[1040]; // assumedly this is the size of CCommand since we don't have an actual constructor
		memset(commandBuf, 0, sizeof(commandBuf));
		CCommand tempCommand = *(CCommand*)&commandBuf;
		if (CCommand__Tokenize(
				tempCommand, fmt::format("exec server/cleanup_gamemode_{}", sLastMode).c_str(), cmd_source_t::kCommandSrcCode))
		{
			_Cmd_Exec_f(tempCommand, false, false);
			Cbuf_Execute();
		}

		sLastMode.clear();
	}

	auto& layer = eos::EosLayer::Instance();
	if(layer.GetFakeIpLayer() != nullptr)
		layer.GetFakeIpLayer()->Clear();
})

DECLARE_HOOK(CHostState__FrameUpdate, engine.dll + 0x16DB00, [](auto& hook, CHostState* self, double flCurrentTime, float flFrameTime)
{
	hook.Original(self, flCurrentTime, flFrameTime);
	RunFrameTasks();

	if (*g_pServerState == server_state_t::ss_active)
	{
		// update server presence
		g_pServerPresence->RunFrame(flCurrentTime);

		// update limits for frame
		g_pServerLimits->RunFrame(flCurrentTime, flFrameTime);
	}

	// Run Squirrel message buffer
	if (g_pSquirrel[ScriptContext::UI]->m_pSQVM != nullptr && g_pSquirrel[ScriptContext::UI]->m_pSQVM->sqvm != nullptr)
		g_pSquirrel[ScriptContext::UI]->ProcessMessageBuffer();

	if (g_pSquirrel[ScriptContext::CLIENT]->m_pSQVM != nullptr && g_pSquirrel[ScriptContext::CLIENT]->m_pSQVM->sqvm != nullptr)
		g_pSquirrel[ScriptContext::CLIENT]->ProcessMessageBuffer();

	if (g_pSquirrel[ScriptContext::SERVER]->m_pSQVM != nullptr && g_pSquirrel[ScriptContext::SERVER]->m_pSQVM->sqvm != nullptr)
		g_pSquirrel[ScriptContext::SERVER]->ProcessMessageBuffer();

	g_pPluginManager->RunFrame();
})

ON_DLL_LOAD_RELIESON("engine.dll", HostState, ConVar, [](CModule module)
{
	DISPATCH_MODULE(HostStateHooks)
	Cvar_hostport = module.Offset(0x13FA6070).RCast<decltype(Cvar_hostport)>();
	_Cmd_Exec_f = module.Offset(0x1232C0).RCast<decltype(_Cmd_Exec_f)>();

	g_pHostState = module.Offset(0x7CF180).RCast<CHostState*>();
})
