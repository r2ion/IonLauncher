#pragma once

#include <cstddef>
#include <cstdint>
#include <xmmintrin.h>

#include "mathlib/vector.h"

class CBasePlayer;
class CUserCmd;
class ITraceListData;

inline constexpr char GAME_MOVEMENT_INTERFACE_VERSION[] = "GameMovement001";
using EntityHandle_t = std::uint32_t;

enum class MoveDataFlags_t : std::uint8_t
{
	FirstRunOfFunctions = 1 << 0,
	GameCodeMovedPlayer = 1 << 1,
	CommandPredicted = 1 << 2,
};

class CMoveData
{
public:
	MoveDataFlags_t m_Flags;
	std::uint8_t m_Pad0001[3];
	EntityHandle_t m_PlayerHandle;
	std::uint32_t m_ImpulseCommand;
	QAngle m_ViewAngles;
	std::uint32_t m_Buttons;
	std::uint32_t m_OldButtons;
	std::uint32_t m_ButtonsPressed;
	float m_ForwardMove;
	float m_SideMove;
	float m_UpMove;
	Vector3D m_ViewForward;
	Vector3D m_ViewRight;
	Vector3D m_ViewUp;
	Vector3D m_PlanarForward;
	Vector3D m_PlanarRight;
	Vector3D m_WishDirection;
	std::uint32_t m_PlayerSettingsIndex;
	float m_StandingHullHeight;
	float m_CrouchingHullHeight;
	float m_HullHalfWidth;
	float m_MaxSpeed;
	Vector3D m_Velocity;
	Vector3D m_PreviousVelocity;
	std::uint8_t m_Pad00A4[0xC];
	Vector3D m_OutputWishVelocity;
	Vector3D m_ConstraintCenter;
	float m_ConstraintRadius;
	float m_ConstraintWidth;
	float m_ConstraintSpeedFactor;
	std::uint8_t m_Pad00D4[4];
	Vector3D m_AbsOrigin;
	bool m_IsSprinting;
	std::uint8_t m_Pad00E5[3];
};

class IGameMovement
{
public:
	virtual ~IGameMovement() = default;
	virtual void ProcessMovement(CBasePlayer* pPlayer, CMoveData* pMoveData) = 0;
	virtual void Reset() = 0;
	virtual void StartTrackPredictionErrors(CBasePlayer* pPlayer) = 0;
	virtual void FinishTrackPredictionErrors(CBasePlayer* pPlayer) = 0;
	virtual void DiffPrint(const char* pFormat, ...) = 0;
	virtual const Vector3D& GetPlayerMins(bool ducked) const = 0;
	virtual bool IsMovingPlayerStuck() const = 0;
	virtual CBasePlayer* GetMovingPlayer() const = 0;
	virtual void UnblockPusher(CBasePlayer* pPlayer, CBasePlayer* pPusher) = 0;
	virtual void SetupMovementBounds(CBasePlayer* pPlayer) = 0;
	virtual void SetTraceListData(ITraceListData* pTraceListData) = 0;
	virtual void SetPlayerAndMoveData(CBasePlayer* pPlayer, CMoveData* pMoveData) = 0;
	virtual void PlayerMove() = 0;
	virtual std::uint32_t TryPlayerMove(Vector3D* pOrigin, Vector3D* pVelocity, float frameTime,
		void* pFirstTrace) = 0;
};

static_assert(sizeof(IGameMovement) == sizeof(void*));
