#include "cdll_int.h"
#include "common/netmessages.h"
#include "core/tier0.h"
#include "engine/client/clientstate.h"
#include "engine/demo.h"
#include "tier0/hooks.h"
#include "tier1/convar.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

DECLARE_MODULE(EngineClient)

// need to move this
struct CLCClientTickMessageData
{
	void* m_pVTable;
	std::int32_t m_nGroup;
	bool m_bReliable;
	std::uint8_t m_Padding000D[3];
	CNetChan* m_pNetChannel;
	INetMessageHandler* m_pMessageHandler;
	std::int32_t m_nDeltaTick;
	std::int32_t m_nStringTableTick;
	float m_flFrameTime;
	float m_flFrameTimeStdDeviation;
	std::uint8_t m_nServerCPU;
	std::uint8_t m_Padding0031[7];
};

using HostShouldRunFn = bool (*)();
using CClientStateIsPausedFn = bool (*)(CClientState*);
using CNetChanCanPacketFn = bool (*)(CNetChan*);
using CNetChanSendNetMsgFn = bool (*)(CNetChan*, INetMessage*, bool, bool);
using CNetChanSendDatagramFn = std::int64_t (*)(CNetChan*, bf_write*);
using CNetChanSetChokedFn = void (*)(CNetChan*);
using CLSendMoveFn = void (*)();

HostShouldRunFn HostShouldRun;
CClientStateIsPausedFn CClientStateIsPaused;
CNetChanCanPacketFn CNetChanCanPacket;
CNetChanSendNetMsgFn CNetChanSendNetMsg;
CNetChanSendDatagramFn CNetChanSendDatagram;
CNetChanSetChokedFn CNetChanSetChoked;
CLSendMoveFn CLSendMove;

IBaseClientDLL** s_ppClientDLL;
CDemoPlayer** s_ppDemoPlayer;
ConVar** s_ppHostTimescale;
ConVar** s_ppCmdRate;
void** s_ppSplitScreenManager;
void** s_ppCommandTracker;
double* s_pNetTime;
float* s_pIntervalPerTick;
float* s_pClientFrameTime;
float* s_pClientFrameTimeStdDeviation;
float* s_pServerCPUPercent;
void* s_pCLCClientTickVTable;
float s_lastMovementCall;
float s_commandTimeAccumulator;

char* g_pLocalPlayerUserID;
char* g_pLocalPlayerOriginToken;
GetBaseLocalClientType GetBaseLocalClient;
GetLocalPlayerIndexType GetLocalPlayerIndex;
CClientState__SendStringCmd_t CClientState__SendStringCmd;
CPlayer__IsMantling_t CPlayer__IsMantling;

bool IsLocalClientDisconnecting()
{
	void* manager = *s_ppSplitScreenManager;
	using IsDisconnectingFn = bool (*)(void*, int);
	auto isDisconnecting = reinterpret_cast<IsDisconnectingFn>((*reinterpret_cast<void***>(manager))[13]);
	return isDisconnecting(manager, 0);
}

void NotifyCommandCreated(int commandNumber)
{
	void* tracker = *s_ppCommandTracker;
	auto vtable = *reinterpret_cast<void***>(tracker);
	using IsEnabledFn = bool (*)(void*);
	using NotifyFn = void (*)(void*, int);

	if (reinterpret_cast<IsEnabledFn>(vtable[5])(tracker))
		reinterpret_cast<NotifyFn>(vtable[10])(tracker, commandNumber);
}

void SendClientTick(CClientState* client, CNetChan* channel)
{
	CLCClientTickMessageData tickMessage {};
	tickMessage.m_pVTable = s_pCLCClientTickVTable;
	tickMessage.m_nDeltaTick = client->m_nDeltaTick;
	tickMessage.m_nStringTableTick = client->m_nStringTableAckTick;
	tickMessage.m_flFrameTime = *s_pClientFrameTime;
	tickMessage.m_flFrameTimeStdDeviation = *s_pClientFrameTimeStdDeviation;
	tickMessage.m_nServerCPU = static_cast<std::uint8_t>(*s_pServerCPUPercent * 100.0f);

	CNetChanSendNetMsg(channel, reinterpret_cast<INetMessage*>(&tickMessage), false, false);
}

DECLARE_HOOK(CL_Move, engine.dll + 0x734C0, [](auto&, float, bool finalTick)
{
	CClientState* const client = GetBaseLocalClient();
	if (static_cast<int>(client->m_nSignonState) < static_cast<int>(eSignonState::CONNECTED))
	{
		s_commandTimeAccumulator = 0.0f;
		return;
	}

	if (!HostShouldRun() || (*s_ppDemoPlayer)->IsPlayingBack())
		return;

	const int commandTick =
		client->m_pCurrentFrameSnapshot ? client->m_pCurrentFrameSnapshot->m_nCommandTick : -1;
	const int pendingCommandCount = client->m_nOutgoingCommandNumber - commandTick + 1;

	constexpr int maxNewCommands = 15;
	constexpr float commandFrameTime = 0.005f;
	constexpr float maxFrameTime = 0.1f;

	CNetChan* const channel = client->m_NetChannel;
	const float hostTimeScale = (*s_ppHostTimescale)->GetFloat();
	const bool isTimeScaleDefault = hostTimeScale == 1.0f;
	const float netTime = static_cast<float>(*s_pNetTime);

	bool sendPacket = true;
	const bool packetIsDue = client->m_flNextCmdTime <= netTime;
	if (packetIsDue && (finalTick || pendingCommandCount >= maxNewCommands))
		sendPacket = CNetChanCanPacket(channel);
	else if (pendingCommandCount < maxNewCommands || isTimeScaleDefault)
		sendPacket = false;

	const bool isActive = client->m_nSignonState == eSignonState::FULL;
	if (isActive)
	{
		const float movementCallTime = static_cast<float>(g_PlatFloatTime());
		const float elapsedMovementCallTime = movementCallTime - s_lastMovementCall;
		s_lastMovementCall = movementCallTime;
		const int outgoingCommandNumber = client->m_nOutgoingCommandNumber;
		const bool isPaused = CClientStateIsPaused(client);
		const int nextCommandNumber = isPaused ? outgoingCommandNumber : outgoingCommandNumber + 1;

		if (!IsLocalClientDisconnecting())
		{
			IBaseClientDLL* const clientDLL = *s_ppClientDLL;
			const float timeScale = isPaused ? 1.0f : hostTimeScale;
			const float sourceFrameTime = isPaused ? elapsedMovementCallTime : *s_pClientFrameTime;
			const float deltaTime = sourceFrameTime / timeScale;

			s_commandTimeAccumulator =
				std::min(s_commandTimeAccumulator + deltaTime, maxFrameTime);

			if (isTimeScaleDefault && s_commandTimeAccumulator < commandFrameTime)
				return;

			float commandDeltaTime;
			if (isTimeScaleDefault && deltaTime < commandFrameTime)
			{
				commandDeltaTime = commandFrameTime;
				s_commandTimeAccumulator -= commandFrameTime;
			}
			else
			{
				commandDeltaTime = s_commandTimeAccumulator;
				s_commandTimeAccumulator = 0.0f;
			}

			const float frameTime = timeScale * commandDeltaTime;
			clientDLL->CreateMove(nextCommandNumber, frameTime, !isPaused);
			client->m_nOutgoingCommandNumber = nextCommandNumber;
			NotifyCommandCreated(nextCommandNumber);
		}

		if (sendPacket)
			CLSendMove();
		else
			CNetChanSetChoked(channel);


	}

	if (sendPacket)
	{
		if (isActive)
			SendClientTick(client, channel);

		CNetChanSendDatagram(channel, nullptr);

		const float commandPacketInterval = 1.0f / (*s_ppCmdRate)->GetFloat();
		const float maxPacketTimeAdjustment = std::max(*s_pIntervalPerTick, commandPacketInterval);
		const float delta = netTime - static_cast<float>(client->m_flNextCmdTime);
		const float packetTimeAdjustment = std::clamp(delta, 0.0f, maxPacketTimeAdjustment);
		client->m_flNextCmdTime =
			static_cast<double>(commandPacketInterval + netTime - packetTimeAdjustment);
	}
})

ON_DLL_LOAD("client.dll", R2Client, [](CModule module)
{
	CPlayer__IsMantling = module.Offset(0x9E0B0).RCast<CPlayer__IsMantling_t>();
})

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", R2EngineClient, ConCommand, [](CModule module)
{
	g_pLocalPlayerUserID = module.Offset(0x13F8E688).RCast<char*>();
	g_pLocalPlayerOriginToken = module.Offset(0x13979C80).RCast<char*>();
	GetBaseLocalClient = module.Offset(0x78200).RCast<GetBaseLocalClientType>();
	CClientState__SendStringCmd = module.Offset(0x91A10).RCast<CClientState__SendStringCmd_t>();
	GetLocalPlayerIndex = module.Offset(0x52260).RCast<GetLocalPlayerIndexType>();

	HostShouldRun = module.Offset(0x157B40).RCast<HostShouldRunFn>();
	CClientStateIsPaused = module.Offset(0x8F520).RCast<CClientStateIsPausedFn>();
	CNetChanCanPacket = module.Offset(0x20F620).RCast<CNetChanCanPacketFn>();
	CNetChanSendNetMsg = module.Offset(0x213270).RCast<CNetChanSendNetMsgFn>();
	CNetChanSendDatagram = module.Offset(0x212CD0).RCast<CNetChanSendDatagramFn>();
	CNetChanSetChoked = module.Offset(0x213760).RCast<CNetChanSetChokedFn>();
	CLSendMove = module.Offset(0x74F10).RCast<CLSendMoveFn>();

	s_ppClientDLL = module.Offset(0xF849AA8).RCast<IBaseClientDLL**>();
	s_ppDemoPlayer = module.Offset(0xFD15608).RCast<CDemoPlayer**>();
	s_ppHostTimescale = module.Offset(0x1315A2A8).RCast<ConVar**>();
	s_ppCmdRate = module.Offset(0xFDA5AC8).RCast<ConVar**>();
	s_ppSplitScreenManager = module.Offset(0x7A6490).RCast<void**>();
	s_ppCommandTracker = module.Offset(0xFD14FB8).RCast<void**>();
	s_pNetTime = module.Offset(0x13FA2DE0).RCast<double*>();
	s_pIntervalPerTick = module.Offset(0x7CB418).RCast<float*>();
	s_pClientFrameTime = module.Offset(0x13158BA4).RCast<float*>();
	s_pClientFrameTimeStdDeviation = module.Offset(0x13158BAC).RCast<float*>();
	s_pServerCPUPercent = module.Offset(0x130024C0).RCast<float*>();
	s_pCLCClientTickVTable = module.Offset(0x5D8C88).RCast<void*>();

	DISPATCH_MODULE(EngineClient)
})
