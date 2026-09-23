#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/ehandle.h"
#include "mathlib/color.h"
#include "mathlib/mathlib.h"

class CSkyCamera;

enum ForceStance : std::int32_t;

struct audioparams_t
{
  public:
    void* __vftable;              // 0x00
    Vector3D localSound[8];       // 0x08
    std::int32_t soundscapeIndex; // 0x68
    std::int32_t localBits;       // 0x6C
    std::int32_t entIndex;        // 0x70
    std::byte _pad74[0x4];        // 0x74
};

static_assert(sizeof(audioparams_t) == 0x78);

struct fogparams_t
{
  public:
    void* __vftable;       // 0x00
    float botAlt;          // 0x08
    float topAlt;          // 0x0C
    float halfDistBot;     // 0x10
    float halfDistTop;     // 0x14
    float distColorStr;    // 0x18
    float dirColorStr;     // 0x1C
    float distOffset;      // 0x20
    float densityScale;    // 0x24
    float halfAngleDeg;    // 0x28
    float HDRColorScale;   // 0x2C
    color32 distColor;     // 0x30
    color32 dirColor;      // 0x34
    Vector3D direction;    // 0x38
    float minFadeTime;     // 0x44
    bool forceOntoSky;     // 0x48
    bool enable;           // 0x49
    std::byte _pad4A[0x2]; // 0x4A
    std::int32_t id;       // 0x4C
};

static_assert(sizeof(fogparams_t) == 0x50);

struct fogplayerparamsstate_t
{
  public:
    void* __vftable;       // 0x00
    bool enable;           // 0x08
    std::byte _pad9[0x3];  // 0x09
    float botAlt;          // 0x0C
    float topAlt;          // 0x10
    float halfDistBot;     // 0x14
    float halfDistTop;     // 0x18
    float distOffset;      // 0x1C
    float densityScale;    // 0x20
    float halfAngleDeg;    // 0x24
    float distColorStr;    // 0x28
    float dirColorStr;     // 0x2C
    float HDRColorScale;   // 0x30
    float minFadeTime;     // 0x34
    bool forceOntoSky;     // 0x38
    color32 distColor;     // 0x39
    color32 dirColor;      // 0x3D
    std::byte _pad41[0x3]; // 0x41
    Vector3D direction;    // 0x44
    std::int32_t id;       // 0x50
    std::byte _pad54[0x4]; // 0x54
};

static_assert(sizeof(fogplayerparamsstate_t) == 0x58);

struct fogplayerparams_t
{
  public:
    void* __vftable;                 // 0x00
    EHANDLE m_hCtrl;                 // 0x08
    float m_flTransitionStartTime;   // 0x0C
    fogplayerparamsstate_t m_Old;    // 0x10
    fogplayerparamsstate_t m_New;    // 0x68
    fogplayerparamsstate_t m_OldSky; // 0xC0
    fogplayerparamsstate_t m_NewSky; // 0x118
};

static_assert(sizeof(fogplayerparams_t) == 0x170);

struct sky3dparams_t
{
  public:
    void* __vftable;       // 0x00
    std::int32_t scale;    // 0x08
    std::int32_t cellNum;  // 0x0C
    bool useWorldFog;      // 0x10
    std::byte _pad11[0x7]; // 0x11
    fogparams_t fog;       // 0x18
};

static_assert(sizeof(sky3dparams_t) == 0x68);

class CPlayerLocalData
{
  public:
    void* __vftable;                                   // 0x00
    std::int32_t m_iHideHUD;                           // 0x08
    Vector3D m_vecOverViewpoint;                       // 0x0C
    bool m_duckToggleOn;                               // 0x18
    std::byte _pad19[0x3];                             // 0x19
    ForceStance m_forceStance;                         // 0x1C
    std::int32_t m_nDuckTransitionTimeMsecs;           // 0x20
    std::int32_t m_superJumpsUsed;                     // 0x24
    bool m_jumpedOffRodeo;                             // 0x28
    std::byte _pad29[0x3];                             // 0x29
    float m_flSuitPower;                               // 0x2C
    float m_flSuitJumpPower;                           // 0x30
    float m_flSuitGrapplePower;                        // 0x34
    std::int32_t m_nStepside;                          // 0x38
    float m_flFallVelocity;                            // 0x3C
    std::int32_t m_nOldButtons;                        // 0x40
    float m_oldForwardMove;                            // 0x44
    CSkyCamera* m_pOldSkyCamera;                       // 0x48
    float m_accelScale;                                // 0x50
    float m_powerRegenRateScale;                       // 0x54
    float m_dodgePowerDelayScale;                      // 0x58
    bool m_bDrawViewmodel;                             // 0x5C
    std::byte _pad5D[0x3];                             // 0x5D
    float m_flStepSize;                                // 0x60
    bool m_bAllowAutoMovement;                         // 0x64
    std::byte _pad65[0x3];                             // 0x65
    float m_airSlowMoFrac;                             // 0x68
    std::int32_t predictableFlags;                     // 0x6C
    std::int32_t m_bitsActiveDevices;                  // 0x70
    EHANDLE m_hSkyCamera;                              // 0x74
    sky3dparams_t m_skybox3d;                          // 0x78
    fogplayerparams_t m_PlayerFog;                     // 0xE0
    fogparams_t m_fog;                                 // 0x250
    audioparams_t m_audio;                             // 0x2A0
    float m_animNearZ;                                 // 0x318
    Vector3D m_airMoveBlockPlanes[2];                  // 0x31C
    float m_airMoveBlockPlaneTime;                     // 0x334
    std::int32_t m_airMoveBlockPlaneCount;             // 0x338
    float m_queuedMeleePressTime;                      // 0x33C
    float m_queuedGrappleMeleeTime;                    // 0x340
    bool m_queuedMeleeAttackAnimEvent;                 // 0x344
    bool m_disableMeleeUntilRelease;                   // 0x345
    std::byte _pad346[0x2];                            // 0x346
    float m_meleePressTime;                            // 0x348
    std::int32_t m_meleeDisabledCounter;               // 0x34C
    EHANDLE lastAttacker;                              // 0x350
    std::int32_t attackedCount;                        // 0x354
    std::int32_t m_trackedChildProjectileCount;        // 0x358
    bool m_oneHandedWeaponUsage;                       // 0x35C
    bool m_prevOneHandedWeaponUsage;                   // 0x35D
    std::byte _pad35E[0x2];                            // 0x35E
    float m_flCockpitEntryTime;                        // 0x360
    float m_ejectStartTime;                            // 0x364
    float m_disembarkStartTime;                        // 0x368
    float m_hotDropImpactTime;                         // 0x36C
    float m_outOfBoundsDeadTime;                       // 0x370
    std::int32_t m_objectiveIndex;                     // 0x374
    EHANDLE m_objectiveEntity;                         // 0x378
    float m_objectiveEndTime;                          // 0x37C
    std::int32_t m_cinematicEventFlags;                // 0x380
    bool m_forcedDialogueOnly;                         // 0x384
    std::byte _pad385[0x3];                            // 0x385
    float m_titanBuildTime;                            // 0x388
    float m_titanBubbleShieldTime;                     // 0x38C
    bool m_titanEmbarkEnabled;                         // 0x390
    bool m_titanDisembarkEnabled;                      // 0x391
    std::byte _pad392[0x2];                            // 0x392
    std::int32_t m_voicePackIndex;                     // 0x394
    float m_playerAnimUpdateTime;                      // 0x398
    float m_playerAnimLastAimTurnTime;                 // 0x39C
    float m_playerAnimCurrentFeetYaw;                  // 0x3A0
    float m_playerAnimEstimateYaw;                     // 0x3A4
    float m_playerAnimGoalFeetYaw;                     // 0x3A8
    bool m_playerAnimJumping;                          // 0x3AC
    std::byte _pad3AD[0x3];                            // 0x3AD
    float m_playerAnimJumpStartTime;                   // 0x3B0
    bool m_playerAnimFirstJumpFrame;                   // 0x3B4
    bool m_playerAnimDodging;                          // 0x3B5
    std::byte _pad3B6[0x2];                            // 0x3B6
    float m_playerLandStartTime;                       // 0x3B8
    std::int32_t m_playerAnimJumpActivity;             // 0x3BC
    Vector3D m_playerAnimLastWallRunNormal;            // 0x3C0
    bool m_playerAnimLanding;                          // 0x3CC
    bool m_playerAnimShouldLand;                       // 0x3CD
    std::byte _pad3CE[0x2];                            // 0x3CE
    float m_playerAnimLandStartTime;                   // 0x3D0
    bool m_playerAnimInAirWalk;                        // 0x3D4
    std::byte _pad3D5[0x3];                            // 0x3D5
    float m_playerAnimPrevFrameSequenceMotionYaw;      // 0x3D8
    float m_playerAnimMovementPlaybackRate;            // 0x3DC
    float m_fake_playerAnimUpdateTime;                 // 0x3E0
    float m_fake_playerAnimLastAimTurnTime;            // 0x3E4
    float m_fake_playerAnimCurrentFeetYaw;             // 0x3E8
    float m_fake_playerAnimEstimateYaw;                // 0x3EC
    float m_fake_playerAnimGoalFeetYaw;                // 0x3F0
    bool m_fake_playerAnimJumping;                     // 0x3F4
    std::byte _pad3F5[0x3];                            // 0x3F5
    float m_fake_playerAnimJumpStartTime;              // 0x3F8
    bool m_fake_playerAnimFirstJumpFrame;              // 0x3FC
    bool m_fake_playerAnimDodging;                     // 0x3FD
    std::byte _pad3FE[0x2];                            // 0x3FE
    float m_fake_playerLandStartTime;                  // 0x400
    std::int32_t m_fake_playerAnimJumpActivity;        // 0x404
    Vector3D m_fake_playerAnimLastWallRunNormal;       // 0x408
    bool m_fake_playerAnimLanding;                     // 0x414
    bool m_fake_playerAnimShouldLand;                  // 0x415
    std::byte _pad416[0x2];                            // 0x416
    float m_fake_playerAnimLandStartTime;              // 0x418
    bool m_fake_playerAnimInAirWalk;                   // 0x41C
    std::byte _pad41D[0x3];                            // 0x41D
    float m_fake_playerAnimPrevFrameSequenceMotionYaw; // 0x420
    float m_fake_playerAnimMovementPlaybackRate;       // 0x424
};

static_assert(sizeof(CPlayerLocalData) == 0x428);
