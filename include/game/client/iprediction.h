#pragma once

#include "game/client/icliententitylist.h"

#include <cstdint>

class CMoveData;
class QAngle;
class Vector3;
class C_BasePlayer;
class CUserCmd;

inline constexpr char VCLIENT_PREDICTION_INTERFACE_VERSION[] = "VClientPrediction001";

class IPrediction
{
public:
	virtual ~IPrediction() = default;
	virtual void Init() = 0;
	virtual void Shutdown() = 0;
	virtual void Update(bool receivedNewWorldUpdate, int startFrame, bool validFrame, int incomingAcknowledged,
		int outgoingCommand, int serverCommandsAcknowledged) = 0;
	virtual void BeginEntityPacketProcessing() = 0;
	virtual void PreEntityPacketReceived(std::uint32_t packetSequence, int commandsAcknowledged,
		int currentWorldUpdatePacket, int serverTicksElapsed) = 0;
	virtual void PostEntityPacketReceived(std::uint32_t packetSequence) = 0;
	virtual void PostNetworkDataReceived(std::uint32_t packetSequence, int commandsAcknowledged,
		bool receivedEntityUpdates) = 0;
	virtual void OnReceivedUncompressedPacket() = 0;

	virtual void GetViewOrigin(Vector3& origin) = 0;
	virtual void SetViewOrigin(Vector3& origin) = 0;
	virtual void GetViewAngles(QAngle& angles) = 0;
	virtual void SetViewAngles(QAngle& angles) = 0;
	virtual void GetLocalViewAngles(QAngle& angles) = 0;
	virtual void SetLocalViewAngles(QAngle& angles) = 0;

	virtual void ResetPredictionCounters() = 0;
	virtual void ShutdownPredictables() = 0;
	virtual void ShutdownPredictableEntity(int entityIndex) = 0;
	virtual bool IsPredictableEntity(int entityIndex) = 0;
	virtual int GetPredictableEntityHandles(ClientEntityHandle_t** ppEntityHandles) = 0;
	virtual bool HasPredictableEntityIndex(int entityIndex) = 0;
	virtual void RestoreOriginalEntityState() = 0;
	virtual int CollectPredictableEntityIndices(int** ppEntityIndices) = 0;
	virtual void ReinitPredictables(std::uint32_t predictionSlot) = 0;
	virtual int GetIncomingPacketNumber() = 0;
	virtual void SetupMove(C_BasePlayer* pPlayer, CUserCmd* pUserCmd, void* pMoveHelper, CMoveData* pMoveData) = 0;
	virtual void FinishMove(C_BasePlayer* pPlayer, CUserCmd* pUserCmd, CMoveData* pMoveData) = 0;
	virtual void SetIdealPitch(int splitScreenSlot, C_BasePlayer* pPlayer, const Vector3& origin,
		const QAngle& angles, const Vector3& viewHeight) = 0;
	virtual void SmoothViewOnMovingPlatform(std::uint32_t predictedFrame, int debugSlot,
		C_BasePlayer* pPlayer) = 0;
	virtual void UpdateInternal(int splitScreenSlot, bool receivedNewWorldUpdate, bool validFrame,
		int incomingAcknowledged, std::uint32_t outgoingCommand, int serverCommandsAcknowledged) = 0;
};

static_assert(sizeof(IPrediction) == sizeof(void*));
