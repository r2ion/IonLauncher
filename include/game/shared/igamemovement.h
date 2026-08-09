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
	Vector3 m_ViewForward;
	Vector3 m_ViewRight;
	Vector3 m_ViewUp;
	Vector3 m_PlanarForward;
	Vector3 m_PlanarRight;
	Vector3 m_WishDirection;
	std::uint32_t m_PlayerSettingsIndex;
	float m_StandingHullHeight;
	float m_CrouchingHullHeight;
	float m_HullHalfWidth;
	float m_MaxSpeed;
	Vector3 m_Velocity;
	Vector3 m_PreviousVelocity;
	std::uint8_t m_Pad00A4[0xC];
	Vector3 m_OutputWishVelocity;
	Vector3 m_ConstraintCenter;
	float m_ConstraintRadius;
	float m_ConstraintWidth;
	float m_ConstraintSpeedFactor;
	std::uint8_t m_Pad00D4[4];
	Vector3 m_AbsOrigin;
	bool m_IsSprinting;
	std::uint8_t m_Pad00E5[3];
};

static_assert(sizeof(EntityHandle_t) == 0x4);
static_assert(sizeof(MoveDataFlags_t) == 0x1);
static_assert(sizeof(CMoveData) == 0xE8);
static_assert(offsetof(CMoveData, m_ViewAngles) == 0xC);
static_assert(offsetof(CMoveData, m_ForwardMove) == 0x24);
static_assert(offsetof(CMoveData, m_ViewForward) == 0x30);
static_assert(offsetof(CMoveData, m_WishDirection) == 0x6C);
static_assert(offsetof(CMoveData, m_PlayerSettingsIndex) == 0x78);
static_assert(offsetof(CMoveData, m_MaxSpeed) == 0x88);
static_assert(offsetof(CMoveData, m_Velocity) == 0x8C);
static_assert(offsetof(CMoveData, m_PreviousVelocity) == 0x98);
static_assert(offsetof(CMoveData, m_OutputWishVelocity) == 0xB0);
static_assert(offsetof(CMoveData, m_ConstraintCenter) == 0xBC);
static_assert(offsetof(CMoveData, m_AbsOrigin) == 0xD8);
static_assert(offsetof(CMoveData, m_IsSprinting) == 0xE4);

class IGameMovement
{
public:
	virtual ~IGameMovement() = default;
	virtual void ProcessMovement(CBasePlayer* pPlayer, CMoveData* pMoveData) = 0;
	virtual void Reset() = 0;
	virtual void StartTrackPredictionErrors(CBasePlayer* pPlayer) = 0;
	virtual void FinishTrackPredictionErrors(CBasePlayer* pPlayer) = 0;
	virtual void DiffPrint(const char* pFormat, ...) = 0;
	virtual const Vector3& GetPlayerMins(bool ducked) const = 0;
	virtual bool IsMovingPlayerStuck() const = 0;
	virtual CBasePlayer* GetMovingPlayer() const = 0;
	virtual void UnblockPusher(CBasePlayer* pPlayer, CBasePlayer* pPusher) = 0;
	virtual void SetupMovementBounds(CBasePlayer* pPlayer) = 0;
	virtual void SetTraceListData(ITraceListData* pTraceListData) = 0;
	virtual void SetPlayerAndMoveData(CBasePlayer* pPlayer, CMoveData* pMoveData) = 0;
	virtual void PlayerMove() = 0;
	virtual std::uint32_t TryPlayerMove(Vector3* pOrigin, Vector3* pVelocity, float frameTime,
		void* pFirstTrace) = 0;
};

static_assert(sizeof(IGameMovement) == sizeof(void*));
