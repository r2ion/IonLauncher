#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/ehandle.h"
#include "server/basecombatcharacter.h"
#include "server/playerdata.h"
#include "server/playerlocaldata.h"
#include "server/playerstate.h"
#include "server/usercmd.h"
#include "tier1/utlvector.h"

class CAI_Squad;
class CCommandContext;
class CNavArea;
class IPhysicsObject;
class IPhysicsPlayerController;
struct AnimRecordingAssetHeader_s;

using AttachmentId_t = std::uint8_t;

enum PlayerConnectedState : std::int32_t
{
    PlayerConnected,
    PlayerDisconnecting,
    PlayerDisconnected,
};

class CPlayer : public CBaseCombatCharacter
{
  public:
    ServerClass* GetServerClass() override = 0;                                             // 3
    ServerDataMap* GetDataDescMap() override = 0;                                           // 5
    ScriptClassDesc_t* GetScriptDesc() override = 0;                                        // 6
    void Spawn() override = 0;                                                              // 23
    void Precache() override = 0;                                                           // 24
    int OnTakeDamage(const CTakeDamageInfo& info) override = 0;                             // 60
    int TakeHealth(float health, int damageType) override = 0;                              // 62
    void Event_Killed(const CTakeDamageInfo& info) override = 0;                            // 63
    void Event_KilledOther(CBaseEntity* victim, const CTakeDamageInfo& info) override = 0;  // 64
    bool ChangeTeam(int team) override = 0;                                                 // 104
    void Touch(CBaseEntity* other) override = 0;                                            // 111
    void DrawDebugGeometryOverlays() override = 0;                                          // 34
    void DrawDebugTextOverlays() override = 0;                                              // 35
    int Save(ISave& save) override = 0;                                                     // 36
    int Restore(IRestore& restore) override = 0;                                            // 37
    void OnRestore() override = 0;                                                          // 40
    bool IsPlayer() const override = 0;                                                     // 83
    bool IsTitan() const override = 0;                                                      // 84
    void PhysicsSimulate() override = 0;                                                    // 116
    void UpdateOnRemove() override = 0;                                                     // 117
    Vector3D EyePosition() override = 0;                                                    // 133
    Vector3D EarPosition() override = 0;                                                    // 134
    QAngle EyeAngles() override = 0;                                                        // 135
    QAngle LocalEyeAngles() override = 0;                                                   // 136
    Vector3D GetSmoothedVelocity() override = 0;                                            // 141
    void GetVelocity(Vector3D* velocity, Vector3D* angularVelocity) override = 0;           // 142
    float GetGravity() const override = 0;                                                  // 143
    const Vector3D& WorldSpaceCenter() const override = 0;                                  // 152
    void VPhysicsDestroyObject() override = 0;                                              // 157
    unsigned int PhysicsSolidMaskForEntity() const override = 0;                            // 169
    void HandleAnimEvent(animevent_t* event) override = 0;                                  // 258
    const impactdamagetable_t& GetPhysicsImpactDamageTable() override = 0;                  // 274
    QAngle BodyAngles() const override = 0;                                                 // 280
    Vector3D EyeDirection2D() override = 0;                                                 // 285
    Vector3D EyeDirection3D() override = 0;                                                 // 286
    float ScriptGetAttackSpreadAngle() override = 0;                                        // 288
    void ScriptGiveExistingWeapon(CBaseEntity* weapon) override = 0;                        // 289
    bool Weapon_Detach(CBaseCombatWeapon* weapon) override = 0;                             // 292
    bool Weapon_Switch(CBaseCombatWeapon* weapon) override = 0;                             // 293
    bool Weapon_IsPlaying3pEquipActivity() const override = 0;                              // 295
    bool Weapon_IsPlaying3pReloadActivity() const override = 0;                             // 296
    Vector3D Weapon_ShootPosition() override = 0;                                           // 297
    const char* GetWeaponClass() const override = 0;                                        // 298
    void Weapon_StartGestureAnim(int activity, float duration, bool autokill) override = 0; // 302
    void Weapon_EndGestureAnim(int activity, float fadeOut) override = 0;                   // 303
    void ScriptTakeWeapon(const char* weaponName) override = 0;                             // 304
    void ScriptTakeWeaponNow(const char* weaponName) override = 0;                          // 305
    Vector3D ScriptGetPlayerOrNPCViewVector() override = 0;                                 // 306
    Vector3D ScriptGetPlayerOrNPCViewForward() override = 0;                                // 307
    Vector3D ScriptGetPlayerOrNPCViewUp() override = 0;                                     // 308
    Vector3D ScriptGetPlayerOrNPCViewRight() override = 0;                                  // 309
    void SetOutOfBoundsDeadTime(float time) override = 0;                                   // 310
    float GetOutOfBoundsDeadTime() override = 0;                                            // 311
    int ScriptGiveWeapon(SQVM* vm) override = 0;                                            // 312
    void GrappleDetach() override = 0;                                                      // 314
    int OnTakeDamage_Alive(const CTakeDamageInfo& info) override = 0;                       // 315
    void Event_Dying() override = 0;                                                        // 320
    float GetHullWidth() const override = 0;                                                // 324
    float GetHullHeight() const override = 0;                                               // 325
    bool PlayerMelee_ExecutionStartAttacker(float duration) override = 0;                   // 336
    bool PlayerMelee_ExecutionStartTarget(CBaseEntity* attacker) override = 0;              // 337
    bool PlayerMelee_ExecutionEndAttacker() override = 0;                                   // 338
    bool PlayerMelee_ExecutionEndTarget() override = 0;                                     // 339
    void Event_LeechEnd() override = 0;                                                     // 342
    void ItemPostFrame();

    const char* GetNetName() const
    {
        return m_szNetname;
    }
    std::uint64_t GetPlatformUserId() const
    {
        return m_platformUserId;
    }
    bool IsZooming() const
    {
        return m_bZooming;
    }
    bool IsConnected() const
    {
        return m_iConnected != PlayerDisconnected;
    }
    bool IsDisconnecting() const
    {
        return m_iConnected == PlayerDisconnecting;
    }

    char m_szNetname[256];                                        // 0x1498
    bool m_bZooming;                                              // 0x1598
    bool m_zoomToggleOn;                                          // 0x1599
    std::byte _pad159A[0x2];                                      // 0x159A
    float m_zoomBaseFrac;                                         // 0x159C
    float m_zoomBaseTime;                                         // 0x15A0
    float m_zoomFullStartTime;                                    // 0x15A4
    std::uint32_t m_physicsSolidMask;                             // 0x15A8
    AttachmentId_t m_rightHandAttachment;                         // 0x15AC
    AttachmentId_t m_leftHandAttachment;                          // 0x15AD
    AttachmentId_t m_headAttachment;                              // 0x15AE
    AttachmentId_t m_chestAttachment;                             // 0x15AF
    CPlayerLocalData m_Local;                                     // 0x15B0
    fogplayerparams_t m_PlayerFog;                                // 0x19D8
    CUtlVector<EHANDLE> m_hTriggerTonemapList;                    // 0x1B48
    EHANDLE m_hColorCorrectionCtrl;                               // 0x1B68
    std::byte _pad1B6C[0x4];                                      // 0x1B6C
    CUtlVector<EHANDLE> m_hTriggerSoundscapeList;                 // 0x1B70
    CPlayerState pl;                                              // 0x1B90
    Rodeo_PlayerData m_rodeo;                                     // 0x1C10
    bool m_hasBadReputation;                                      // 0x1C90
    char m_communityName[64];                                     // 0x1C91
    char m_communityClanTag[16];                                  // 0x1CD1
    char m_factionName[16];                                       // 0x1CE1
    char m_hardwareIcon[16];                                      // 0x1CF1
    bool m_happyHourActive;                                       // 0x1D01
    std::byte _pad1D02[0x6];                                      // 0x1D02
    std::uint64_t m_platformUserId;                               // 0x1D08
    std::int32_t m_classModsActive;                               // 0x1D10
    std::int32_t m_classModsActiveOld;                            // 0x1D14
    ClassModValues m_classModValues;                              // 0x1D18
    std::int32_t m_posClassModsActive[4];                         // 0x1D8C
    std::int32_t m_posClassModsActiveOld[4];                      // 0x1D9C
    PerPosClassModValues m_perPosValues[4];                       // 0x1DAC
    bool m_passives[128];                                         // 0x1DCC
    std::int32_t m_communityId;                                   // 0x1E4C
    std::int32_t m_nButtons;                                      // 0x1E50
    std::int32_t m_afButtonPressed;                               // 0x1E54
    std::int32_t m_afButtonReleased;                              // 0x1E58
    std::int32_t m_afButtonLast;                                  // 0x1E5C
    std::int32_t m_afButtonDisabled;                              // 0x1E60
    std::int32_t m_afButtonForced;                                // 0x1E64
    float m_forwardMove;                                          // 0x1E68
    float m_sideMove;                                             // 0x1E6C
    float m_prevForwardMove;                                      // 0x1E70
    float m_prevSideMove;                                         // 0x1E74
    bool m_bLagCompensation;                                      // 0x1E78
    bool m_bPredictWeapons;                                       // 0x1E79
    bool m_bPredictionEnabled;                                    // 0x1E7A
    bool m_wantedToMatchmake;                                     // 0x1E7B
    EHANDLE m_skyCamera;                                          // 0x1E7C
    EHANDLE m_titanSoulBeingRodeoed;                              // 0x1E80
    EHANDLE m_entitySyncingWithMe;                                // 0x1E84
    std::int32_t m_playerFlags;                                   // 0x1E88
    bool m_hasMic;                                                // 0x1E8C
    bool m_inPartyChat;                                           // 0x1E8D
    std::byte _pad1E8E[0x2];                                      // 0x1E8E
    float m_playerMoveSpeedScale;                                 // 0x1E90
    std::int32_t m_gestureSequences[4];                           // 0x1E94
    float m_gestureStartTimes[4];                                 // 0x1EA4
    float m_gestureBlendInDuration[4];                            // 0x1EB4
    float m_gestureBlendOutDuration[4];                           // 0x1EC4
    float m_gestureFadeOutStartTime[4];                           // 0x1ED4
    float m_gestureFadeOutDuration[4];                            // 0x1EE4
    std::int32_t m_gestureAutoKillBitfield;                       // 0x1EF4
    bool m_bDropEnabled;                                          // 0x1EF8
    bool m_bDuckEnabled;                                          // 0x1EF9
    std::byte _pad1EFA[0x2];                                      // 0x1EFA
    std::int32_t m_iRespawnFrames;                                // 0x1EFC
    std::int32_t m_afPhysicsFlags;                                // 0x1F00
    EHANDLE m_remoteTurret;                                       // 0x1F04
    float m_flTimeLastTouchedGround;                              // 0x1F08
    float m_flTimeLastJumped;                                     // 0x1F0C
    float m_flTimeLastLanded;                                     // 0x1F10
    Vector3D m_upDirWhenLastTouchedGround;                        // 0x1F14
    bool m_bHasJumpedSinceTouchedGround;                          // 0x1F20
    std::byte _pad1F21[0x3];                                      // 0x1F21
    float m_holdToUseTimeLeft;                                    // 0x1F24
    float m_fTimeLastHurt;                                        // 0x1F28
    float m_fLastAimBotCheckTime;                                 // 0x1F2C
    Vector3D m_accumDamageImpulseVel;                             // 0x1F30
    float m_accumDamageImpulseTime;                               // 0x1F3C
    float m_damageImpulseNoDecelEndTime;                          // 0x1F40
    EHANDLE m_hDmgEntity;                                         // 0x1F44
    float m_DmgTake;                                              // 0x1F48
    std::int32_t m_bitsDamageType;                                // 0x1F4C
    std::int32_t m_bitsHUDDamage;                                 // 0x1F50
    float m_xpRate;                                               // 0x1F54
    float m_flDeathTime;                                          // 0x1F58
    float m_flDeathAnimTime;                                      // 0x1F5C
    bool m_frozen;                                                // 0x1F60
    bool m_stressAnimation;                                       // 0x1F61
    std::byte _pad1F62[0x2];                                      // 0x1F62
    std::int32_t m_iObserverMode;                                 // 0x1F64
    std::int32_t m_iObserverLastMode;                             // 0x1F68
    EHANDLE m_hObserverTarget;                                    // 0x1F6C
    Vector3D m_observerModeStaticPosition;                        // 0x1F70
    QAngle m_observerModeStaticAngles;                            // 0x1F7C
    bool m_isValidChaseObserverTarget;                            // 0x1F88
    std::byte _pad1F89[0x3];                                      // 0x1F89
    std::int32_t m_vphysicsCollisionState;                        // 0x1F8C
    bool m_bHasVPhysicsCollision;                                 // 0x1F90
    std::byte _pad1F91[0x3];                                      // 0x1F91
    float m_fNextSuicideTime;                                     // 0x1F94
    std::int32_t m_iSuicideCustomKillFlags;                       // 0x1F98
    std::int32_t m_preNoClipPhysicsFlags;                         // 0x1F9C
    EHANDLE m_hTonemapController;                                 // 0x1FA0
    std::int32_t m_activeBurnCardIndex;                           // 0x1FA4
    CUtlVector<CCommandContext> m_CommandContext;                 // 0x1FA8
    IPhysicsPlayerController* m_pPhysicsController;               // 0x1FC8
    IPhysicsObject* m_pShadowStand;                               // 0x1FD0
    IPhysicsObject* m_pShadowCrouch;                              // 0x1FD8
    Vector3D m_oldOrigin;                                         // 0x1FE0
    Vector3D m_vecSmoothedVelocity;                               // 0x1FEC
    bool m_bTouchedPhysObject;                                    // 0x1FF8
    bool m_bPhysicsWasFrozen;                                     // 0x1FF9
    std::byte _pad1FFA[0x2];                                      // 0x1FFA
    std::int32_t m_iTargetVolume;                                 // 0x1FFC
    float m_flDuckTime;                                           // 0x2000
    float m_flDuckJumpTime;                                       // 0x2004
    bool m_VDU;                                                   // 0x2008
    bool m_fInitHUD;                                              // 0x2009
    bool m_fGameHUDInitialized;                                   // 0x200A
    bool m_fWeapon;                                               // 0x200B
    std::int32_t m_iUpdateTime;                                   // 0x200C
    PlayerConnectedState m_iConnected;                            // 0x2010
    std::int32_t m_iPlayerLocked;                                 // 0x2014
    std::int32_t m_gameStats[12];                                 // 0x2018
    EHANDLE m_firstPersonProxy;                                   // 0x2048
    EHANDLE m_predictedFirstPersonProxy;                          // 0x204C
    EHANDLE m_grappleHook;                                        // 0x2050
    EHANDLE m_petTitan;                                           // 0x2054
    std::int32_t m_petTitanMode;                                  // 0x2058
    std::int32_t m_xp;                                            // 0x205C
    std::int32_t m_generation;                                    // 0x2060
    std::int32_t m_rank;                                          // 0x2064
    std::int32_t m_serverForceIncreasePlayerListGenerationParity; // 0x2068
    bool m_isPlayingRanked;                                       // 0x206C
    std::byte _pad206D[0x3];                                      // 0x206D
    float m_skill_mu;                                             // 0x2070
    EHANDLE m_hardpointEntity;                                    // 0x2074
    float m_nextTitanRespawnAvailable;                            // 0x2078
    bool m_activeViewmodelModifiers[25];                          // 0x207C
    bool m_activeViewmodelModifiersChanged;                       // 0x2095
    std::byte _pad2096[0x2];                                      // 0x2096
    EHANDLE m_hViewModel;                                         // 0x2098
    std::byte _pad209C[0x4];                                      // 0x209C
    SV_CUserCmd m_LastCmd;                                        // 0x20A0
    SV_CUserCmd* m_pCurrentCommand;                               // 0x21D8
    float m_flStepSoundTime;                                      // 0x21E0
    float m_flStepSoundReduceTime;                                // 0x21E4
    EHANDLE m_hThirdPersonEnt;                                    // 0x21E8
    std::byte _pad21EC[0x4];                                      // 0x21EC
    ThirdPersonViewData m_thirdPerson;                            // 0x21F0
    std::int32_t m_duckState;                                     // 0x2250
    Vector3D m_StandHullMin;                                      // 0x2254
    Vector3D m_StandHullMax;                                      // 0x2260
    Vector3D m_DuckHullMin;                                       // 0x226C
    Vector3D m_DuckHullMax;                                       // 0x2278
    Vector3D m_upDir;                                             // 0x2284
    Vector3D m_upDirPredicted;                                    // 0x2290
    Vector3D m_lastWallRunStartPos;                               // 0x229C
    float m_wallRunStartTime;                                     // 0x22A8
    float m_wallRunClearTime;                                     // 0x22AC
    std::int32_t m_wallRunCount;                                  // 0x22B0
    bool m_wallRunWeak;                                           // 0x22B4
    std::byte _pad22B5[0x3];                                      // 0x22B5
    float m_wallRunPushAwayTime;                                  // 0x22B8
    float m_wallrunFrictionScale;                                 // 0x22BC
    float m_groundFrictionScale;                                  // 0x22C0
    float m_wallrunRetryTime;                                     // 0x22C4
    Vector3D m_wallrunRetryPos;                                   // 0x22C8
    Vector3D m_wallrunRetryNormal;                                // 0x22D4
    bool m_wallHanging;                                           // 0x22E0
    std::byte _pad22E1[0x3];                                      // 0x22E1
    float m_wallHangStartTime;                                    // 0x22E4
    float m_wallHangTime;                                         // 0x22E8
    std::int32_t m_traversalType;                                 // 0x22EC
    std::int32_t m_traversalState;                                // 0x22F0
    Vector3D m_traversalBegin;                                    // 0x22F4
    Vector3D m_traversalMid;                                      // 0x2300
    Vector3D m_traversalEnd;                                      // 0x230C
    float m_traversalMidFrac;                                     // 0x2318
    Vector3D m_traversalForwardDir;                               // 0x231C
    Vector3D m_traversalRefPos;                                   // 0x2328
    float m_traversalProgress;                                    // 0x2334
    float m_traversalStartTime;                                   // 0x2338
    float m_traversalHandAppearTime;                              // 0x233C
    float m_traversalReleaseTime;                                 // 0x2340
    float m_traversalBlendOutStartTime;                           // 0x2344
    Vector3D m_traversalBlendOutStartOffset;                      // 0x2348
    float m_traversalYawDelta;                                    // 0x2354
    std::int32_t m_traversalYawPoseParameter;                     // 0x2358
    float m_wallDangleJumpOffTime;                                // 0x235C
    bool m_wallDangleMayHangHere;                                 // 0x2360
    bool m_wallDangleForceFallOff;                                // 0x2361
    bool m_wallDangleLastPushedForward;                           // 0x2362
    std::byte _pad2363[0x1];                                      // 0x2363
    std::int32_t m_wallDangleDisableWeapon;                       // 0x2364
    float m_wallDangleClimbProgressFloor;                         // 0x2368
    float m_prevMoveYaw;                                          // 0x236C
    float m_sprintTiltVel;                                        // 0x2370
    std::int32_t m_sprintTiltPoseParameter;                       // 0x2374
    std::int32_t m_sprintFracPoseParameter;                       // 0x2378
    std::byte _pad237C[0x4];                                      // 0x237C
    GrappleData m_grapple;                                        // 0x2380
    bool m_grappleActive;                                         // 0x23E8
    bool m_grappleNeedWindowCheck;                                // 0x23E9
    std::byte _pad23EA[0x2];                                      // 0x23EA
    EHANDLE m_grappleNextWindowHint;                              // 0x23EC
    bool m_sliding;                                               // 0x23F0
    bool m_slideLongJumpAllowed;                                  // 0x23F1
    std::byte _pad23F2[0x2];                                      // 0x23F2
    float m_lastSlideTime;                                        // 0x23F4
    float m_lastSlideBoost;                                       // 0x23F8
    EHANDLE m_activeZipline;                                      // 0x23FC
    bool m_ziplineReverse;                                        // 0x2400
    std::byte _pad2401[0x3];                                      // 0x2401
    EHANDLE m_lastZipline;                                        // 0x2404
    float m_useLastZiplineCooldown;                               // 0x2408
    bool m_ziplineValid3pWeaponLayerAnim;                         // 0x240C
    std::byte _pad240D[0x3];                                      // 0x240D
    std::int32_t m_ziplineState;                                  // 0x2410
    std::byte _pad2414[0x4];                                      // 0x2414
    PlayerZiplineData m_zipline;                                  // 0x2418
    Player_OperatorData m_operator;                               // 0x2468
    Player_ViewOffsetEntityData m_viewOffsetEntity;               // 0x24E8
    Player_AnimViewEntityData m_animViewEntity;                   // 0x2500
    bool m_highSpeedViewmodelAnims;                               // 0x2580
    std::byte _pad2581[0x3];                                      // 0x2581
    std::int32_t m_gravityGrenadeStatusEffect;                    // 0x2584
    float m_onSlopeTime;                                          // 0x2588
    Vector3D m_lastWallNormal;                                    // 0x258C
    bool m_dodgingInAir;                                          // 0x2598
    bool m_dodging;                                               // 0x2599
    std::byte _pad259A[0x2];                                      // 0x259A
    float m_lastDodgeTime;                                        // 0x259C
    float m_airSpeed;                                             // 0x25A0
    float m_airAcceleration;                                      // 0x25A4
    bool m_iSpawnParity;                                          // 0x25A8
    bool m_boosting;                                              // 0x25A9
    bool m_repeatedBoost;                                         // 0x25AA
    std::byte _pad25AB[0x1];                                      // 0x25AB
    float m_boostMeter;                                           // 0x25AC
    bool m_jetpack;                                               // 0x25B0
    bool m_gliding;                                               // 0x25B1
    std::byte _pad25B2[0x2];                                      // 0x25B2
    float m_glideMeter;                                           // 0x25B4
    float m_glideRechargeDelayAccumulator;                        // 0x25B8
    bool m_hovering;                                              // 0x25BC
    bool m_climbing;                                              // 0x25BD
    bool m_isPerformingBoostAction;                               // 0x25BE
    std::byte _pad25BF[0x1];                                      // 0x25BF
    float m_lastJumpHeight;                                       // 0x25C0
    std::int32_t m_numPingsUsed;                                  // 0x25C4
    std::int32_t m_numPingsAvailable;                             // 0x25C8
    float m_lastPingTime;                                         // 0x25CC
    float m_pingGroupStartTime;                                   // 0x25D0
    std::int32_t m_pingGroupAccumulator;                          // 0x25D4
    const char* m_lastBodySound1p;                                // 0x25D8
    const char* m_lastBodySound3p;                                // 0x25E0
    const char* m_lastFinishSound1p;                              // 0x25E8
    const char* m_lastFinishSound3p;                              // 0x25F0
    const char* m_primedSound1p;                                  // 0x25F8
    const char* m_primedSound3p;                                  // 0x2600
    CurrentData_Player m_currentFramePlayer;                      // 0x2608
    CurrentData_LocalPlayer m_currentFrameLocalPlayer;            // 0x2628
    std::int32_t m_nImpulse;                                      // 0x2688
    float m_flFlashTime;                                          // 0x268C
    float m_flForwardMove;                                        // 0x2690
    float m_flSideMove;                                           // 0x2694
    std::int32_t m_nNumCrateHudHints;                             // 0x2698
    bool m_needStuckCheck;                                        // 0x269C
    std::byte _pad269D[0x3];                                      // 0x269D
    float m_totalFrameTime;                                       // 0x26A0
    float m_joinFrameTime;                                        // 0x26A4
    std::int32_t m_lastUCmdSimulationTicks;                       // 0x26A8
    float m_lastUCmdSimulationRemainderTime;                      // 0x26AC
    float m_totalExtraClientCmdTimeAttempted;                     // 0x26B0
    bool m_bGamePaused;                                           // 0x26B4
    bool m_bPlayerUnderwater;                                     // 0x26B5
    std::byte _pad26B6[0x2];                                      // 0x26B6
    EHANDLE m_hPlayerViewEntity;                                  // 0x26B8
    bool m_bShouldDrawPlayerWhileUsingViewEntity;                 // 0x26BC
    std::byte _pad26BD[0x3];                                      // 0x26BD
    EHANDLE m_hConstraintEntity;                                  // 0x26C0
    Vector3D m_vecConstraintCenter;                               // 0x26C4
    float m_flConstraintRadius;                                   // 0x26D0
    float m_flConstraintWidth;                                    // 0x26D4
    float m_flConstraintSpeedFactor;                              // 0x26D8
    bool m_bConstraintPastRadius;                                 // 0x26DC
    std::byte _pad26DD[0x3];                                      // 0x26DD
    float m_lastActiveTime;                                       // 0x26E0
    float m_flLaggedMovementValue;                                // 0x26E4
    float m_lastMoveInputTime;                                    // 0x26E8
    Vector3D m_vNewVPhysicsPosition;                              // 0x26EC
    Vector3D m_vNewVPhysicsVelocity;                              // 0x26F8
    Vector3D m_vNewVPhysicsWishVel;                               // 0x2704
    Vector3D m_vecPreviouslyPredictedOrigin;                      // 0x2710
    std::int32_t m_nBodyPitchPoseParam;                           // 0x271C
    CNavArea* m_lastNavArea;                                      // 0x2720
    char m_szNetworkIDString[64];                                 // 0x2728
    CAI_Squad* m_squad;                                           // 0x2768
    const char* m_SquadName;                                      // 0x2770
    GameMovementUtilCollection m_gameMovementUtil;                // 0x2778
    float m_flTimeAllSuitDevicesOff;                              // 0x27B0
    bool m_bIsStickySprinting;                                    // 0x27B4
    std::byte _pad27B5[0x3];                                      // 0x27B5
    float m_fStickySprintMinTime;                                 // 0x27B8
    bool m_bPlayedSprintStartEffects;                             // 0x27BC
    std::byte _pad27BD[0x3];                                      // 0x27BD
    std::int32_t m_autoSprintForced;                              // 0x27C0
    bool m_fIsSprinting;                                          // 0x27C4
    bool m_fIsWalking;                                            // 0x27C5
    std::byte _pad27C6[0x2];                                      // 0x27C6
    float m_useHeldTime;                                          // 0x27C8
    float m_sprintStartedTime;                                    // 0x27CC
    float m_sprintStartedFrac;                                    // 0x27D0
    float m_sprintEndedTime;                                      // 0x27D4
    float m_sprintEndedFrac;                                      // 0x27D8
    float m_stickySprintStartTime;                                // 0x27DC
    bool m_bSinglePlayerGameEnding;                               // 0x27E0
    std::byte _pad27E1[0x3];                                      // 0x27E1
    std::int32_t m_ubEFNoInterpParity;                            // 0x27E4
    bool m_viewConeActive;                                        // 0x27E8
    bool m_viewConeParented;                                      // 0x27E9
    std::byte _pad27EA[0x2];                                      // 0x27EA
    std::int32_t m_viewConeParity;                                // 0x27EC
    std::int32_t m_lastViewConeParityTick;                        // 0x27F0
    float m_viewConeLerpTime;                                     // 0x27F4
    bool m_viewConeSpecificEnabled;                               // 0x27F8
    std::byte _pad27F9[0x3];                                      // 0x27F9
    Vector3D m_viewConeSpecific;                                  // 0x27FC
    Vector3D m_viewConeRelativeAngleMin;                          // 0x2808
    Vector3D m_viewConeRelativeAngleMax;                          // 0x2814
    EHANDLE m_hReservedSpawnPoint;                                // 0x2820
    EHANDLE m_hLastSpawnPoint;                                    // 0x2824
    bool m_autoKickDisabled;                                      // 0x2828
    std::byte _pad2829[0x3];                                      // 0x2829
    Vector3D m_movementCollisionNormal;                           // 0x282C
    Vector3D m_groundNormal;                                      // 0x2838
    EHANDLE m_stuckCharacter;                                     // 0x2844
    char m_title[32];                                             // 0x2848
    bool sentHUDScriptChecksum;                                   // 0x2868
    bool m_bIsFullyConnected;                                     // 0x2869
    std::byte _pad286A[0x2];                                      // 0x286A
    CTakeDamageInfo m_lastDeathInfo;                              // 0x286C
    std::byte _pad28E4[0x4];                                      // 0x28E4
    PlayerMelee_PlayerData m_melee;                               // 0x28E8
    EHANDLE m_lungeTargetEntity;                                  // 0x2910
    bool m_isLungingToPosition;                                   // 0x2914
    std::byte _pad2915[0x3];                                      // 0x2915
    Vector3D m_lungeTargetPosition;                               // 0x2918
    Vector3D m_lungeStartPositionOffset;                          // 0x2924
    Vector3D m_lungeStartPositionOffset_notLagCompensated;        // 0x2930
    Vector3D m_lungeEndPositionOffset;                            // 0x293C
    float m_lungeStartTime;                                       // 0x2948
    float m_lungeEndTime;                                         // 0x294C
    bool m_lungeCanFly;                                           // 0x2950
    bool m_lungeLockPitch;                                        // 0x2951
    std::byte _pad2952[0x2];                                      // 0x2952
    float m_lungeStartPitch;                                      // 0x2954
    float m_lungeSmoothTime;                                      // 0x2958
    float m_lungeMaxTime;                                         // 0x295C
    float m_lungeMaxEndSpeed;                                     // 0x2960
    bool m_useCredit;                                             // 0x2964
    std::byte _pad2965[0x3];                                      // 0x2965
    CBaseEntity* m_smartAmmoNextTarget;                           // 0x2968
    CBaseEntity* m_smartAmmoPrevTarget;                           // 0x2970
    float m_smartAmmoHighestLocksOnMeFractionValues[4];           // 0x2978
    EHANDLE m_smartAmmoHighestLocksOnMeEntities[4];               // 0x2988
    float m_smartAmmoPreviousHighestLockOnMeFractionValue;        // 0x2998
    float m_smartAmmoPendingHighestLocksOnMeFractionValues[4];    // 0x299C
    std::byte _pad29AC[0x4];                                      // 0x29AC
    CBaseEntity* m_smartAmmoPendingHighestLocksOnMeEntities[4];   // 0x29B0
    bool m_smartAmmoRemoveFromTargetList;                         // 0x29D0
    std::byte _pad29D1[0x3];                                      // 0x29D1
    std::int32_t m_delayedFlinchEvents;                           // 0x29D4
    std::uint64_t m_delayedFlinchEventCount;                      // 0x29D8
    char m_extraWeaponModNames[512];                              // 0x29E0
    const char* m_extraWeaponModNamesArray[8];                    // 0x2BE0
    std::uint64_t m_extraWeaponModNameCount;                      // 0x2C20
    CAI_Squad* m_pPlayerAISquad;                                  // 0x2C28
    float m_flAreaCaptureScoreAccumulator;                        // 0x2C30
    float m_flCapPointScoreRate;                                  // 0x2C34
    float m_flConnectionTime;                                     // 0x2C38
    float m_fullyConnectedTime;                                   // 0x2C3C
    float m_connectedForDurationCallback_duration;                // 0x2C40
    float m_flLastForcedChangeTeamTime;                           // 0x2C44
    std::int32_t m_iBalanceScore;                                 // 0x2C48
    std::byte _pad2C4C[0x4];                                      // 0x2C4C
    void* m_PlayerAnimState;                                      // 0x2C50
    Vector3D m_vWorldSpaceCenterHolder;                           // 0x2C58
    Vector3D m_vPrevGroundNormal;                                 // 0x2C64
    std::int32_t m_threadedPostProcessJob;                        // 0x2C70
    std::byte _pad2C74[0x4];                                      // 0x2C74
    CPlayerShared m_Shared;                                       // 0x2C78
    StatusEffectTimedData m_statusEffectsTimedPlayerNV[10];       // 0x2D18
    StatusEffectEndlessData m_statusEffectsEndlessPlayerNV[10];   // 0x2E08
    std::int32_t m_pilotClassIndex;                               // 0x2EA8
    std::int32_t m_latestCommandRun;                              // 0x2EAC
    PushingEntState m_nearbyPushers[12];                          // 0x2EB0
    std::int32_t m_nearbyPusherCount;                             // 0x3090
    PushHistoryEntry m_pushHistory[16];                           // 0x3094
    std::int32_t m_pushHistoryEntryIndex;                         // 0x3194
    float m_baseVelocityLastServerTime;                           // 0x3198
    Vector3D m_pushedThisFrame;                                   // 0x319C
    Vector3D m_pushedThisSnapshotAccum;                           // 0x31A8
    std::int32_t m_pushedFixedPointOffset[3];                     // 0x31B4
    float m_lastCommandContextWarnTime;                           // 0x31C0
    Vector3D m_pushAwayFromTopAcceleration;                       // 0x31C4
    float m_trackedState[52];                                     // 0x31D0
    std::int32_t m_prevTrackedState;                              // 0x32A0
    Vector3D m_prevTrackedStatePos;                               // 0x32A4
    AnimRecordingAssetHeader_s* m_recordingAnim;                  // 0x32B0
    void* m_animRecordFile;                                       // 0x32B8
    std::int32_t m_animRecordButtons;                             // 0x32C0
    Vector3D m_prevAbsOrigin;                                     // 0x32C4
    bool m_sendMovementCallbacks;                                 // 0x32D0
    bool m_sendInputCallbacks;                                    // 0x32D1
    std::byte _pad32D2[0x2];                                      // 0x32D2
    PredictableServerEvent m_predictableServerEvents[16];         // 0x32D4
    std::int32_t m_predictableServerEventCount;                   // 0x3454
    std::int32_t m_predictableServerEventAcked;                   // 0x3458
    EHANDLE m_playerScriptNetDataGlobal;                          // 0x345C
    EHANDLE m_playerScriptNetDataExclusive;                       // 0x3460
    std::byte _pad3464[0x4];                                      // 0x3464
};

static_assert(sizeof(CPlayer) == 0x3468);

extern CPlayer*(__fastcall* UTIL_PlayerByIndex)(int playerIndex);
