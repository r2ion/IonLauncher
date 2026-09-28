#include "cdll_int.h"
#include "client/input.h"
#include "common/netmessages.h"
#include "core/tier0.h"
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


using CLSendMoveFn = void (*)();

CLSendMoveFn CL_SendMove;

ConVar* host_timescale;
ConVar* cl_cmdrate;
float* s_pIntervalPerTick;
float* s_pClientFrameTime;
float* s_pClientFrameTimeStdDeviation;
float* s_pServerCPUPercent;
double s_lastMovementCall;
float s_LastFrameTime;
bool s_bWasFullyConnected;


void SendClientTick(CClientState* client, CNetChan* channel)
{
	CLC_ClientTick tickMessage;
	tickMessage.m_nDeltaTick = client->m_nDeltaTick;
	tickMessage.m_nStringTableTick = client->m_nStringTableAckTick;
	tickMessage.m_flFrameTime = *s_pClientFrameTime;
	tickMessage.m_flFrameTimeStdDeviation = *s_pClientFrameTimeStdDeviation;
	tickMessage.m_nServerCPU = static_cast<std::uint8_t>(*s_pServerCPUPercent * 100.0f);

	channel->SendNetMsg(tickMessage, false, false);
}

DECLARE_HOOK(CL_Move, engine.dll + 0x734C0, [](auto&, float, bool finalTick)
{
	CClientState* const client = GetBaseLocalClient();
	const bool isActive = client->m_nSignonState == eSignonState::FULL;
	if (isActive != s_bWasFullyConnected)
	{
		s_LastFrameTime = 0.0f;
		s_lastMovementCall = isActive ? g_PlatFloatTime() : 0.0;
		s_bWasFullyConnected = isActive;
		if (g_pInput)
			g_pInput->ResetExtraMouseSamples();
	}

	if (static_cast<int>(client->m_nSignonState) < static_cast<int>(eSignonState::CONNECTED))
		return;

	if (!Host_ShouldRun() || g_pDemoPlayer->IsPlayingBack())
		return;

    if (!cl_cmdrate)
        cl_cmdrate = g_pCVar->FindVar("cl_cmdrate");

    if (!host_timescale)
        host_timescale = g_pCVar->FindVar("host_timescale");

	const int commandTick =
		client->m_pCurrentFrameSnapshot ? client->m_pCurrentFrameSnapshot->m_nCommandTick : -1;
	const int pendingCommandCount = client->m_nOutgoingCommandNumber - commandTick + 1;

	float minimumCommandFrameTime;

	// this really fucking pisses me off
	if(g_pVanillaCompatibility->GetVanillaCompatibility())
		minimumCommandFrameTime = 0.005f; // we will speedhack on vanilla if we don't do this
	else
		minimumCommandFrameTime = 0.001f; // need this for listen servers to work properly, smooth to around ~1000 fps

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
		const float elapsedMovementCallTime = static_cast<float>(movementCallTime - s_lastMovementCall);
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
				frameTime = client->GetFrameTime() + s_LastFrameTime;
				deltaTime = frameTime / timeScale;
			}

			if (deltaTime > maxFrameTime)
				frameTime = timeScale * maxFrameTime;

			if (isTimeScaleDefault && deltaTime < minimumCommandFrameTime)
			{
				if (!isPaused && frameTime > s_LastFrameTime)
					g_ClientDLL->ExtraMouseSample(frameTime - s_LastFrameTime);
				s_LastFrameTime = frameTime;
				return;
			}

			s_LastFrameTime = 0.0f;
			g_ClientDLL->CreateMove(nextCommandNumber, frameTime, !isPaused);
			client->m_nOutgoingCommandNumber = nextCommandNumber;
			if (g_pDemoRecorder->IsRecording())
				g_pDemoRecorder->RecordUserInput(nextCommandNumber);
		}

		if (sendPacket)
			CL_SendMove();
		else
			channel->SetChoked();

		s_lastMovementCall = movementCallTime;


	}

	if (sendPacket)
	{
		if (isActive)
			SendClientTick(client, channel);

		channel->SendDatagram(nullptr);

		const float commandPacketInterval = 1.0f / cl_cmdrate->GetFloat();
		const float maxPacketTimeAdjustment = std::max(*s_pIntervalPerTick, commandPacketInterval);
		const float delta = netTime - static_cast<float>(client->m_flNextCmdTime);
		const float packetTimeAdjustment = std::clamp(delta, 0.0f, maxPacketTimeAdjustment);
		client->m_flNextCmdTime =
			static_cast<double>(commandPacketInterval + netTime - packetTimeAdjustment);
	}
})


ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", R2EngineClient, (ConVar, ConCommand), [](CModule module)
{
	CL_SendMove = module.Offset(0x74F10).RCast<CLSendMoveFn>();
	host_timescale = g_pCVar->FindVar("host_timescale");
	cl_cmdrate = g_pCVar->FindVar("cl_cmdrate");
	s_pIntervalPerTick = module.Offset(0x7CB418).RCast<float*>();
	s_pClientFrameTime = module.Offset(0x13158BA4).RCast<float*>();
	s_pClientFrameTimeStdDeviation = module.Offset(0x13158BAC).RCast<float*>();
	s_pServerCPUPercent = module.Offset(0x130024C0).RCast<float*>();

	DISPATCH_MODULE(EngineClient)
})
