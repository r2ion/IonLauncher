#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/ehandle.h"
#include "mathlib/mathlib.h"
#include "server/takedamageinfo.h"

class CPlayer;
struct surfacedata_t;

struct PushingEntState
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    EHANDLE ent;           // 0x08
    Vector3D origin;       // 0x0C
    QAngle angles;         // 0x18
    std::byte _pad24[0x4]; // 0x24
};

static_assert(sizeof(PushingEntState) == 0x28);

enum class GameMovementImpactEventType : std::int32_t;

struct GameMovementUtilCollection
{
    surfacedata_t* m_pSurfaceData;              // 0x00
    float m_surfaceFriction;                    // 0x08
    char m_chTextureType;                       // 0x0C
    std::byte _padD[0x3];                       // 0x0D
    std::int32_t m_surfaceProps;                // 0x10
    EHANDLE m_hCollisionEntity;                 // 0x14
    Vector3D m_vSurfaceNormal;                  // 0x18
    Vector3D m_vSurfaceContactPoint;            // 0x24
    GameMovementImpactEventType m_eImpactEvent; // 0x30
    std::byte _pad34[0x4];                      // 0x34
};

static_assert(sizeof(GameMovementUtilCollection) == 0x38);

struct Rodeo_PlayerData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    std::int32_t stage;                 // 0x08
    bool canRodeo;                      // 0x0C
    std::byte _padD[0x3];               // 0x0D
    std::int32_t rodeoCountParity;      // 0x10
    float startTime;                    // 0x14
    float endTime;                      // 0x18
    EHANDLE targetEnt;                  // 0x1C
    EHANDLE prevEnt;                    // 0x20
    float prevEntCooldown;              // 0x24
    std::int32_t pilot1pSequenceIndex;  // 0x28
    std::int32_t pilot3pSequenceIndex;  // 0x2C
    std::int32_t targetAttachmentIndex; // 0x30
    float rodeoStabilizeStrength;       // 0x34
    bool rodeoStabilizeViewFirstFrame;  // 0x38
    std::byte _pad39[0x3];              // 0x39
    matrix3x4_t lastPlayerToWorld;      // 0x3C
    Quaternion initialCameraCorrection; // 0x6C
    std::byte _pad7C[0x4];              // 0x7C
};

static_assert(sizeof(Rodeo_PlayerData) == 0x80);

struct ClassModValues
{
    float health;                      // 0x00
    float healthShield;                // 0x04
    float healthDoomed;                // 0x08
    float healthPerSegment;            // 0x0C
    float powerRegenRate;              // 0x10
    float dodgeDuration;               // 0x14
    float dodgeSpeed;                  // 0x18
    float dodgePowerDrain;             // 0x1C
    float smartAmmoLockTimeModifier;   // 0x20
    float wallrunAccelerateVertical;   // 0x24
    float wallrunAccelerateHorizontal; // 0x28
    float wallrunMaxSpeedHorizontal;   // 0x2C
    float wallrun_timeLimit;           // 0x30
    float wallrun_hangTimeLimit;       // 0x34
    bool wallrunAllowed;               // 0x38
    std::byte _pad39[0x3];             // 0x39
    std::int32_t wallrunAdsType;       // 0x3C
    bool doubleJumpAllowed;            // 0x40
    std::byte _pad41[0x3];             // 0x41
    float pitchMaxUp;                  // 0x44
    float pitchMaxDown;                // 0x48
    bool mantlePitchLeveling;          // 0x4C
    bool dodgeAllowed;                 // 0x4D
    bool sprintAllowed;                // 0x4E
    bool stealthSounds;                // 0x4F
    bool hoverEnabled;                 // 0x50
    std::byte _pad51[0x3];             // 0x51
    float grapple_power_regen_delay;   // 0x54
    float grapple_power_regen_rate;    // 0x58
    float slideFOVScale;               // 0x5C
    float slideFOVLerpInTime;          // 0x60
    float slideFOVLerpOutTime;         // 0x64
    float airSlowMoSpeed;              // 0x68
    std::int32_t sharedEnergyTotal;    // 0x6C
    float sharedEnergyRegenRate;       // 0x70
};

static_assert(sizeof(ClassModValues) == 0x74);

struct PerPosClassModValues
{
    float speed_;       // 0x00
    float sprintSpeed_; // 0x04
};

static_assert(sizeof(PerPosClassModValues) == 0x8);

struct ThirdPersonViewData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    Vector3D m_thirdPersonEntViewOffset;             // 0x08
    bool m_thirdPersonEntPitchIsFreelook;            // 0x14
    bool m_thirdPersonEntYawIsFreelook;              // 0x15
    bool m_thirdPersonEntUseFixedDist;               // 0x16
    bool m_thirdPersonEntPushedInByGeo;              // 0x17
    bool m_thirdPersonEntDrawViewmodel;              // 0x18
    std::byte _pad19[0x3];                           // 0x19
    float m_thirdPersonEntBlendTotalDuration;        // 0x1C
    float m_thirdPersonEntBlendEaseInDuration;       // 0x20
    float m_thirdPersonEntBlendEaseOutDuration;      // 0x24
    float m_thirdPersonEntFixedPitch;                // 0x28
    float m_thirdPersonEntFixedYaw;                  // 0x2C
    float m_thirdPersonEntFixedDist;                 // 0x30
    float m_thirdPersonEntMinYaw;                    // 0x34
    float m_thirdPersonEntMaxYaw;                    // 0x38
    float m_thirdPersonEntMinPitch;                  // 0x3C
    float m_thirdPersonEntMaxPitch;                  // 0x40
    float m_thirdPersonEntSpringToCenterRate;        // 0x44
    float m_thirdPersonEntLookaheadLowerEntSpeed;    // 0x48
    float m_thirdPersonEntLookaheadUpperEntSpeed;    // 0x4C
    float m_thirdPersonEntLookaheadMaxAngle;         // 0x50
    float m_thirdPersonEntLookaheadLerpAheadRate;    // 0x54
    float m_thirdPersonEntLookaheadLerpToCenterRate; // 0x58
    std::byte _pad5C[0x4];                           // 0x5C
};

static_assert(sizeof(ThirdPersonViewData) == 0x60);

struct GrappleData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    Vector3D m_grappleVel;               // 0x08
    Vector3D m_grapplePoints[4];         // 0x14
    std::int32_t m_grapplePointCount;    // 0x44
    bool m_grappleAttached;              // 0x48
    bool m_grapplePulling;               // 0x49
    bool m_grappleRetracting;            // 0x4A
    bool m_grappleForcedRetracting;      // 0x4B
    float m_grappleUsedPower;            // 0x4C
    float m_grapplePullTime;             // 0x50
    float m_grappleAttachTime;           // 0x54
    float m_grappleDetachTime;           // 0x58
    EHANDLE m_grappleMeleeTarget;        // 0x5C
    bool m_grappleHasGoodVelocity;       // 0x60
    std::byte _pad61[0x3];               // 0x61
    float m_grappleLastGoodVelocityTime; // 0x64
};

static_assert(sizeof(GrappleData) == 0x68);

struct PlayerZiplineData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    bool m_ziplineReenableWeapons;            // 0x08
    std::byte _pad9[0x3];                     // 0x09
    float m_mountingZiplineDuration;          // 0x0C
    float m_mountingZiplineAlpha;             // 0x10
    float m_ziplineStartTime;                 // 0x14
    float m_ziplineEndTime;                   // 0x18
    Vector3D m_mountingZiplineSourcePosition; // 0x1C
    Vector3D m_mountingZiplineSourceVelocity; // 0x28
    Vector3D m_mountingZiplineTargetPosition; // 0x34
    Vector3D m_ziplineUsePosition;            // 0x40
    std::byte _pad4C[0x4];                    // 0x4C
};

static_assert(sizeof(PlayerZiplineData) == 0x50);

struct Player_OperatorData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    bool diving;                          // 0x08
    bool cameraEnabled;                   // 0x09
    std::byte _padA[0x2];                 // 0x0A
    float minPitch;                       // 0x0C
    float maxPitch;                       // 0x10
    float followDistance;                 // 0x14
    float followHeight;                   // 0x18
    bool shootFromPlayer;                 // 0x1C
    std::byte _pad1D[0x3];                // 0x1D
    float smoothDuration;                 // 0x20
    float smoothFollowDistanceStartTime;  // 0x24
    float smoothFollowDistanceStartValue; // 0x28
    float smoothFollowDistanceEndValue;   // 0x2C
    float smoothFollowHeightStartTime;    // 0x30
    float smoothFollowHeightStartValue;   // 0x34
    float smoothFollowHeightEndValue;     // 0x38
    float smoothMinPitchStartTime;        // 0x3C
    float smoothMinPitchStartValue;       // 0x40
    float smoothMinPitchEndValue;         // 0x44
    float smoothMaxPitchStartTime;        // 0x48
    float smoothMaxPitchStartValue;       // 0x4C
    float smoothMaxPitchEndValue;         // 0x50
    bool forceDefaultFloorHeight;         // 0x54
    std::byte _pad55[0x3];                // 0x55
    float defaultFloorHeight;             // 0x58
    bool ignoreWorldForMovement;          // 0x5C
    bool ignoreWorldForFloorTrace;        // 0x5D
    std::byte _pad5E[0x2];                // 0x5E
    float moveGridSizeScale;              // 0x60
    float moveFloorHeightOffset;          // 0x64
    bool jumpIsDodge;                     // 0x68
    std::byte _pad69[0x3];                // 0x69
    float jumpAcceleration;               // 0x6C
    float jumpMaxSpeed;                   // 0x70
    float hoverAcceleration;              // 0x74
    float hoverMaxSpeed;                  // 0x78
    std::byte _pad7C[0x4];                // 0x7C
};

static_assert(sizeof(Player_OperatorData) == 0x80);

struct Player_ViewOffsetEntityData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    EHANDLE viewOffsetEntityHandle; // 0x08
    float lerpInDuration;           // 0x0C
    float lerpOutDuration;          // 0x10
    bool stabilizePlayerEyeAngles;  // 0x14
    std::byte _pad15[0x3];          // 0x15
};

static_assert(sizeof(Player_ViewOffsetEntityData) == 0x18);

struct Player_AnimViewEntityData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    EHANDLE animViewEntityHandle;                               // 0x08
    float animViewEntityAngleLerpInDuration;                    // 0x0C
    float animViewEntityOriginLerpInDuration;                   // 0x10
    float animViewEntityLerpOutDuration;                        // 0x14
    bool animViewEntityStabilizePlayerEyeAngles;                // 0x18
    std::byte _pad19[0x3];                                      // 0x19
    std::int32_t animViewEntityThirdPersonCameraParity;         // 0x1C
    std::int32_t animViewEntityThirdPersonCameraAttachment[6];  // 0x20
    std::int32_t animViewEntityNumThirdPersonCameraAttachments; // 0x38
    bool animViewEntityThirdPersonCameraVisibilityChecks;       // 0x3C
    std::byte _pad3D[0x3];                                      // 0x3D
    std::int32_t animViewEntityParity;                          // 0x40
    std::int32_t lastAnimViewEntityParity;                      // 0x44
    Vector3D animViewEntityCameraPosition;                      // 0x48
    QAngle animViewEntityCameraAngles;                          // 0x54
    float animViewEntityBlendStartTime;                         // 0x60
    Vector3D animViewEntityBlendStartEyePosition;               // 0x64
    QAngle animViewEntityBlendStartEyeAngles;                   // 0x70
    std::byte _pad7C[0x4];                                      // 0x7C
};

static_assert(sizeof(Player_AnimViewEntityData) == 0x80);

struct CurrentData_Player
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    float m_flHullHeight;          // 0x08
    float m_traversalAnimProgress; // 0x0C
    float m_sprintTiltFrac;        // 0x10
    QAngle m_angEyeAngles;         // 0x14
};

static_assert(sizeof(CurrentData_Player) == 0x20);

struct CurrentData_LocalPlayer
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    QAngle m_viewConeAngleMin;        // 0x08
    QAngle m_viewConeAngleMax;        // 0x14
    Vector3D m_stepSmoothingOffset;   // 0x20
    QAngle m_vecPunchBase_Angle;      // 0x2C
    QAngle m_vecPunchBase_AngleVel;   // 0x38
    QAngle m_vecPunchWeapon_Angle;    // 0x44
    QAngle m_vecPunchWeapon_AngleVel; // 0x50
    std::byte _pad5C[0x4];            // 0x5C
};

static_assert(sizeof(CurrentData_LocalPlayer) == 0x60);

struct PlayerMelee_PlayerData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    bool attackActive;                 // 0x08
    bool attackRecoveryShouldBeQuick;  // 0x09
    std::byte _padA[0x2];              // 0x0A
    float attackStartTime;             // 0x0C
    EHANDLE attackHitEntity;           // 0x10
    float attackHitEntityTime;         // 0x14
    float attackLastHitNonWorldEntity; // 0x18
    std::int32_t scriptedState;        // 0x1C
    bool pendingMeleePress;            // 0x20
    std::byte _pad21[0x7];             // 0x21
};

static_assert(sizeof(PlayerMelee_PlayerData) == 0x28);

struct CPlayerShared
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    std::int32_t m_nPlayerCond;      // 0x08
    bool m_bLoadoutUnavailable;      // 0x0C
    std::byte _padD[0x3];            // 0x0D
    float m_flCondExpireTimeLeft[2]; // 0x10
    CPlayer* m_pOuter;               // 0x18
    float m_flNextCritUpdate;        // 0x20
    float m_flTauntRemoveTime;       // 0x24
    CTakeDamageInfo m_damageInfo;    // 0x28
};

static_assert(sizeof(CPlayerShared) == 0xA0);

struct StatusEffectTimedData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    std::int32_t seComboVars; // 0x08
    float seTimeEnd;          // 0x0C
    float seEaseOut;          // 0x10
    std::byte _pad14[0x4];    // 0x14
};

static_assert(sizeof(StatusEffectTimedData) == 0x18);

struct StatusEffectEndlessData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    std::int32_t seComboVars; // 0x08
    std::byte _padC[0x4];     // 0x0C
};

static_assert(sizeof(StatusEffectEndlessData) == 0x10);

struct PushHistoryEntry
{
    float time;      // 0x00
    Vector3D pushed; // 0x04
};

static_assert(sizeof(PushHistoryEntry) == 0x10);

struct PredictableServerEvent
{
    std::int32_t type;               // 0x00
    float deadlineTime;              // 0x04
    std::int32_t fullSizeOfUnion[4]; // 0x08
};

static_assert(sizeof(PredictableServerEvent) == 0x18);
