#include "engine/cl_main.h"
#include "cdll_int.h"
#include "client/input.h"
#include "common/netmessages.h"
#include "core/tier0.h"
#include "engine/cdll_int.h"
#include "engine/client/clientstate.h"
#include "engine/demo.h"
#include "engine/isplitscreen.h"
#include "engine/net.h"
#include "engine/r2engine.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"
#include "tier1/cvar.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

DECLARE_MODULE(EngineClient)


using CL_SendMove_t = void (*)();

CL_SendMove_t CL_SendMove;

float* interval_per_tick;
float* host_frametime_unbounded;
float* host_frametime;
float* host_frametime_stddeviation;
float* sv_cpu_percent;

static const ConVar* host_timescale;
static const ConVar* cl_cmdrate;
static const ConVar* cl_move_use_dt;

// this is kind of lazy
void InitialiseClientMovementConVars()
{
    host_timescale = g_pCVar->FindVar("host_timescale");
    cl_cmdrate = g_pCVar->FindVar("cl_cmdrate");
    cl_move_use_dt = g_pCVar->FindVar("cl_move_use_dt");
}

DECLARE_HOOK(CL_Move, engine.dll + 0x734C0, [](auto&, float, bool finalTick)
{
	CClientState* const client = GetBaseLocalClient();
    auto& timing = *client->GetClientStateExtended();
	const bool isActive = client->m_nSignonState == eSignonState::FULL;
	if (isActive != timing.m_bWasFullyConnected)
	{
		timing.m_flAccumulatedCommandFrameTime = 0.0f;
		timing.m_flAccumulatedHostFrameTime = 0.0f;
		timing.m_flLastMovementCall = isActive ? g_PlatFloatTime() : 0.0;
		timing.m_bWasFullyConnected = isActive;
		if (g_pInput)
			g_pInput->ResetExtraMouseSamples();
	}

	if (static_cast<int>(client->m_nSignonState) < static_cast<int>(eSignonState::CONNECTED))
		return;

	if (!Host_ShouldRun() || g_pDemoPlayer->IsPlayingBack())
		return;

	const int commandTick =
		client->m_pCurrentFrameSnapshot ? client->m_pCurrentFrameSnapshot->m_nCommandTick : -1;
	const int pendingCommandCount = client->m_nOutgoingCommandNumber - commandTick + 1;

	const bool vanillaCompatibility = g_pVanillaCompatibility->GetVanillaCompatibility();

	constexpr int maxNewCommands = 15;
	constexpr float maxFrameTime = 0.1f;


    CNetChan* const channel = client->m_NetChannel;
	const float hostTimeScale = host_timescale->GetFloat();
	const bool isTimeScaleDefault = hostTimeScale == 1.0f;
	const float netTime = static_cast<float>(*g_pNetTime);

	bool sendPacket = true;
	const bool packetIsDue = client->m_flNextCmdTime <= netTime;
	if (packetIsDue && (finalTick || pendingCommandCount >= maxNewCommands))
		sendPacket = channel->CanPacket();
	else if (pendingCommandCount < maxNewCommands || isTimeScaleDefault)
		sendPacket = false;

	if (isActive)
	{
		const double movementCallTime = g_PlatFloatTime();
		const float elapsedMovementCallTime = static_cast<float>(movementCallTime - timing.m_flLastMovementCall);
		const int outgoingCommandNumber = client->m_nOutgoingCommandNumber;
		const bool isPaused = client->IsPaused();
		const int nextCommandNumber = isPaused ? outgoingCommandNumber : outgoingCommandNumber + 1;

		if (!g_pSplitScreenMgr->IsDisconnecting(0))
		{
			float timeScale;
			float frameTime;
			float deltaTime;

			if (isPaused)
			{
				timeScale = 1.0f;
				frameTime = elapsedMovementCallTime;
				deltaTime = frameTime;
			}
			else
			{
				timeScale = hostTimeScale;
				frameTime = client->GetFrameTime() + timing.m_flAccumulatedCommandFrameTime;
				deltaTime = frameTime / timeScale;
			}

			const float hostFrameTime = *host_frametime + timing.m_flAccumulatedHostFrameTime;
			const bool useHostCadence = !isPaused && !vanillaCompatibility &&
				cl_move_use_dt->GetBool() && g_pEngineClient->IsMoveOneCmdPerClientFrameEnabled();
			const float commandCadence = useHostCadence ? hostFrameTime : deltaTime;
			const float minimumCommandFrameTime = vanillaCompatibility ? 0.005f : 0.001f;

			if (deltaTime > maxFrameTime)
				frameTime = timeScale * maxFrameTime;

			if (isTimeScaleDefault && (commandCadence < minimumCommandFrameTime || frameTime <= 0.0f))
			{
				if (!isPaused && frameTime > timing.m_flAccumulatedCommandFrameTime)
					g_ClientDLL->ExtraMouseSample(frameTime - timing.m_flAccumulatedCommandFrameTime);
				timing.m_flAccumulatedCommandFrameTime = frameTime;
				timing.m_flAccumulatedHostFrameTime = hostFrameTime;
				return;
			}

			timing.m_flAccumulatedCommandFrameTime = 0.0f;
			timing.m_flAccumulatedHostFrameTime = 0.0f;
			g_ClientDLL->CreateMove(nextCommandNumber, frameTime, !isPaused);
			client->m_nOutgoingCommandNumber = nextCommandNumber;
			if (g_pDemoRecorder->IsRecording())
				g_pDemoRecorder->RecordUserInput(nextCommandNumber);
		}

		if (sendPacket)
			CL_SendMove();
		else
			channel->SetChoked();

		timing.m_flLastMovementCall = movementCallTime;


	}

	if (sendPacket)
	{
		if (isActive)
        {
            CLC_ClientTick tickMessage;
            tickMessage.m_nDeltaTick = client->m_nDeltaTick;
            tickMessage.m_nStringTableTick = client->m_nStringTableAckTick;
            tickMessage.m_flFrameTime = *host_frametime_unbounded;
            tickMessage.m_flFrameTimeStdDeviation = *host_frametime_stddeviation;
            tickMessage.m_nServerCPU = static_cast<std::uint8_t>(*sv_cpu_percent * 100.0f);
            channel->SendNetMsg(tickMessage, false, false);
        }

		channel->SendDatagram(nullptr);

		const float commandPacketInterval = 1.0f / cl_cmdrate->GetFloat();
		const float maxPacketTimeAdjustment = std::max(*interval_per_tick, commandPacketInterval);
		const float delta = netTime - static_cast<float>(client->m_flNextCmdTime);
		const float packetTimeAdjustment = std::clamp(delta, 0.0f, maxPacketTimeAdjustment);
		client->m_flNextCmdTime =
			static_cast<double>(commandPacketInterval + netTime - packetTimeAdjustment);
	}
})


ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", R2EngineClient, (ConVar, ConCommand), [](CModule module)
{
	CL_SendMove = module.Offset(0x74F10).RCast<CL_SendMove_t>();
	interval_per_tick = module.Offset(0x7CB418).RCast<float*>();
	host_frametime = module.Offset(0x13158BA0).RCast<float*>();
	host_frametime_unbounded = module.Offset(0x13158BA4).RCast<float*>();
	host_frametime_stddeviation = module.Offset(0x13158BAC).RCast<float*>();
	sv_cpu_percent = module.Offset(0x130024C0).RCast<float*>();

	DISPATCH_MODULE(EngineClient)
})
