#pragma once

#include "mathlib/vector.h"
#include "tier1/utlsymbol.h"

#include <cstddef>
#include <cstdint>

typedef unsigned short WEAPON_FILE_INFO_HANDLE;
#define INVALID_WEAPON_INFO_HANDLE 0xFFFF

#define MAX_WEAPON_STRING 80
#define MAX_WEAPON_MODS 32
#define MAX_WEAPON_MOD_GROUPS (MAX_WEAPON_MODS - 1)
#define MAX_WEAPON_MOD_ENTRIES 200
#define MAX_ENCODED_WEAPON_MOD_ENTRIES (static_cast<std::size_t>(UINT16_MAX) + 1)

#define WEAPON_MOD_ENTRY_FIRE_MODE 4
#define WEAPON_MOD_ENTRY_AIMASSIST_ADSPULL_WEAPONCLASS 44
#define WEAPON_MOD_ENTRY_DAMAGE_FLAGS 60
#define WEAPON_MOD_ENTRY_EXPLOSION_DAMAGE_FLAGS 61
#define WEAPON_MOD_ENTRY_AMMO_SUCK_BEHAVIOR 102
#define WEAPON_MOD_ENTRY_DAMAGE_FALLOFF_TYPE 135
#define WEAPON_MOD_ENTRY_VIEWKICK_SPRING 268
#define WEAPON_MOD_ENTRY_SMART_AMMO_HUD_TYPE 538
#define WEAPON_MOD_ENTRY_SMART_AMMO_HUD_LOCK_STYLE 539
#define WEAPON_MOD_ENTRY_SMART_AMMO_WEAPON_TYPE 540
#define WEAPON_MOD_ENTRY_SMART_AMMO_LOCK_TYPE 541

#define MAX_WEAPON_STRING_POOL 3072
#define MAX_WEAPON_MOD_PARSE_ENTRIES 0x2D6


enum class eWeaponFireMode : int
{
    automatic = 0,
    semiAutomatic = 1,
    offhand = 2,
    offhandInstant = 3,
    offhandHybrid = 4,
    offhandMelee = 5,
    offhandMeleeHybrid = 6,
};

enum class eWeaponDamageFalloffType : int
{
    linear = 0,
    inverse = 1,
};

enum class eSmartAmmoHudType : int
{
    none = 0,
    smartPistol = 1,
    rocketLauncher = 2,
    homingRockets = 3,
    shoulderRockets = 4,
    arcTool = 5,
    predatorCannon = 6,
};

enum class eSmartAmmoHudLockStyle : int
{
    defaultStyle = 0,
    pilotLauncher = 1,
    pilotPistol = 2,
    titanMissile = 3,
    titanTargetMissile = 4,
};

enum class eSmartAmmoWeaponType : int
{
    bullet = 0,
    homingMissile = 1,
};

enum class eSmartAmmoLockType : int
{
    none = 0,
    smallType = 1,
    large = 2,
    special = 3,
    anyType = 4,
};

enum class eWeaponHolsterType : int
{
    defaultHolster = 0,
    rifle = 1,
    pistol = 2,
    antiTitan = 3,
    _count = 4,
};

enum WeaponString_t : std::uint16_t
{
    WEAPSTR_EMPTY = 0,
    WEAPSTR_INVALID = 0xFFFF,
};

struct WeaponModValues
{
    const char* printName;                                       // 0x000
    const char* shortPrintName;                                  // 0x008
    const char* description;                                     // 0x010
    eWeaponFireMode fireMode;                                    // 0x018
    float fireRate;                                              // 0x01C
    float fireRateMax;                                           // 0x020
    float fireRateMaxTimeSpeedup;                                // 0x024
    float fireRateMaxTimeCooldown;                               // 0x028
    bool fireRateMaxUseADS;                                      // 0x02C
    float fireDuration;                                          // 0x030
    bool netBulletFix;                                           // 0x034
    int burstFireCount;                                          // 0x038
    float burstFireDelay;                                        // 0x03C
    float redCrosshairRange;                                     // 0x040
    bool ui1_enable;                                             // 0x044
    bool ui2_enable;                                             // 0x045
    bool ui3_enable;                                             // 0x046
    bool ui4_enable;                                             // 0x047
    bool ui5_enable;                                             // 0x048
    bool ui6_enable;                                             // 0x049
    bool ui7_enable;                                             // 0x04A
    bool ui8_enable;                                             // 0x04B
    bool ui1_draw_cloaked;                                       // 0x04C
    bool ui2_draw_cloaked;                                       // 0x04D
    bool ui3_draw_cloaked;                                       // 0x04E
    bool ui4_draw_cloaked;                                       // 0x04F
    bool ui5_draw_cloaked;                                       // 0x050
    bool ui6_draw_cloaked;                                       // 0x051
    bool ui7_draw_cloaked;                                       // 0x052
    bool ui8_draw_cloaked;                                       // 0x053
    bool silenced;                                               // 0x054
    bool fastSwapTo;                                             // 0x055
    bool instant_swap_to;                                        // 0x056
    bool instant_swap_from;                                      // 0x057
    bool offhandSwitchForceDraw;                                 // 0x058
    bool primary_fire_does_not_block_sprint;                     // 0x059
    bool bypassSemiAutoHoldProtection;                           // 0x05A
    bool aimassist_disable_hipfire;                              // 0x05B
    bool aimassist_disable_ads;                                  // 0x05C
    bool aimassist_disable_hipfire_titansonly;                   // 0x05D
    bool aimassist_disable_ads_titansonly;                       // 0x05E
    bool aimassist_disable_hipfire_humansonly;                   // 0x05F
    bool aimassist_disable_ads_humansonly;                       // 0x060
    int aimassistADSPullWeaponClassIndex;                        // 0x064
    float aimassistADSPullZoomStart;                             // 0x068
    float aimassistADSPullZoomEnd;                               // 0x06C
    bool gamepadUseYawSpeedForPitchADS;                          // 0x070
    bool allowEmptyFire;                                         // 0x071
    bool allowHeadShots;                                         // 0x072
    bool npcUseAdsMoveSpeedScale;                                // 0x073
    float adsMoveSpeedScale;                                     // 0x074
    float move_speed_modifier;                                   // 0x078
    float move_speed_modifier_when_out_of_ammo;                  // 0x07C
    bool offhand_blocks_sprint;                                  // 0x080
    float adsFOVZoomFracStart;                                   // 0x084
    float adsFOVZoomFracEnd;                                     // 0x088
    bool attackButtonPressesMelee;                               // 0x08C
    bool attackButtonPressesADS;                                 // 0x08D
    int offhandDefaultInventorySlot;                             // 0x090
    int damage_flags;                                            // 0x094
    int explosion_damage_flags;                                  // 0x098
    int altFireAnimCount;                                        // 0x09C
    float zoomfracAutoattack;                                    // 0x0A0
    const char* activityModifier;                                // 0x0A8
    CUtlSymbol activityModifierSymbol;                           // 0x0B0
    Vector3D viewmodel_offset_hip;                               // 0x0B4
    Vector3D viewmodel_offset_ads;                               // 0x0C0
    float meleeLungeTime;                                        // 0x0CC
    float meleeLungeTargetRange;                                 // 0x0D0
    float meleeLungeTargetAngle;                                 // 0x0D4
    bool meleeCanHitHumanSized;                                  // 0x0D8
    bool meleeCanHitTitans;                                      // 0x0D9
    int meleeDamage;                                             // 0x0DC
    int meleeDamageHeavyArmor;                                   // 0x0E0
    float meleeRange;                                            // 0x0E4
    float meleeAngle;                                            // 0x0E8
    const char* melee_rumble_on_hit;                             // 0x0F0
    const char* melee_rumble_on_hit_partial;                     // 0x0F8
    float meleeFreezelookOnHit;                                  // 0x100
    float meleeAttackAnimTime;                                   // 0x104
    float meleeRaiseRecoveryAnimTimeNormal;                      // 0x108
    float meleeRaiseRecoveryAnimTimeQuick;                       // 0x10C
    const char* melee_sound_attack_1p;                           // 0x110
    const char* melee_sound_attack_3p;                           // 0x118
    int melee_anim_1p_number;                                    // 0x120
    const char* meleeAnim3p;                                     // 0x128
    const char* fire_rumble;                                     // 0x130
    bool offhandInteruptsWeaponAnims;                            // 0x138
    bool offhandHoldEnabled;                                     // 0x139
    bool hideHolsteredSidearmWhenActive;                         // 0x13A
    eWeaponHolsterType holsterType;                              // 0x13C
    bool offhandTransitionHasAttachDetachAnimEvents;             // 0x140
    float boltHitSize;                                           // 0x144
    float boltHitSizeGrow1Time;                                  // 0x148
    float boltHitSizeGrow1Size;                                  // 0x14C
    float boltHitSizeGrow2Time;                                  // 0x150
    float boltHitSizeGrow2Size;                                  // 0x154
    float boltHitSizeGrowFinalLerpTime;                          // 0x158
    float boltHitSizeGrowFinalSize;                              // 0x15C
    float boltBounceFrac;                                        // 0x160
    bool boltGravityEnabled;                                     // 0x164
    int projectilesPerShot;                                      // 0x168
    int ammo_suck_behavior;                                      // 0x16C
    int ammo_clip_size;                                          // 0x170
    int ammoStockpileMax;                                        // 0x174
    int ammoDefaultTotal;                                        // 0x178
    int ammoClipReloadMax;                                       // 0x17C
    int ammoPerShot;                                             // 0x180
    int ammo_min_to_fire;                                        // 0x184
    float ammo_clip_random_loss_on_npc_drop;                     // 0x188
    int ammo_clip_random_loss_on_npc_drop_chunksize;             // 0x18C
    bool ammoMinToFireAutoreloads;                               // 0x190
    int ammoSizeSegmentedReload;                                 // 0x194
    bool ammo_no_remove_from_clip;                               // 0x198
    bool ammo_no_remove_from_stockpile;                          // 0x199
    bool ammoDisplayAsClips;                                     // 0x19A
    bool ammoDrainsToEmptyOnFire;                                // 0x19B
    const char* ammoDisplay;                                     // 0x1A0
    bool destroy_on_all_ammo_take;                               // 0x1A8
    float lowAmmoFraction;                                       // 0x1AC
    int lifetimeShotsDefault;                                    // 0x1B0
    float chanceForBonusLastShotInClip;                          // 0x1B4
    float regenAmmoRefillRate;                                   // 0x1B8
    float regenAmmoRefillStartDelay;                             // 0x1BC
    float regenAmmoStockPileMaxFraction;                         // 0x1C0
    float regen_ammo_stockpile_drain_rate_when_charging;         // 0x1C4
    bool regenAmmoWhileFiring;                                   // 0x1C8
    int regenAmmoSoundRangeStart1;                               // 0x1CC
    int regenAmmoSoundRangeStart2;                               // 0x1D0
    int regenAmmoSoundRangeStart3;                               // 0x1D4
    const char* regenAmmoSoundRangeName1;                        // 0x1D8
    const char* regenAmmoSoundRangeName2;                        // 0x1E0
    const char* regenAmmoSoundRangeName3;                        // 0x1E8
    int sharedEnergyCost;                                        // 0x1F0
    int sharedEnergyChargeCost;                                  // 0x1F4
    eWeaponDamageFalloffType damageFalloffType;                  // 0x1F8
    float damageNearDistance;                                    // 0x1FC
    float damageFarDistance;                                     // 0x200
    float damageVeryFarDistance;                                 // 0x204
    float damageInverseDistance;                                 // 0x208
    int damageNearValue;                                         // 0x20C
    int damage_near_value_titanarmor;                            // 0x210
    int damageFarValue;                                          // 0x214
    int damage_far_value_titanarmor;                             // 0x218
    int damageVeryFarValue;                                      // 0x21C
    int damage_very_far_value_titanarmor;                        // 0x220
    int damageRodeo;                                             // 0x224
    int damageAdditionalBullets;                                 // 0x228
    int damage_additional_bullets_titanarmor;                    // 0x22C
    float damage_headshot_scale;                                 // 0x230
    float damage_heavyarmor_nontitan_scale;                      // 0x234
    int passThroughDepth;                                        // 0x238
    float passThroughDamagePreservedScale;                       // 0x23C
    int explosionDamage;                                         // 0x240
    int explosionDamageHeavyArmor;                               // 0x244
    int npcExplosionDamage;                                      // 0x248
    int npcExplosionDamageHeavyArmor;                            // 0x24C
    float impulse_force;                                         // 0x250
    float impulse_force_explosions;                              // 0x254
    bool criticalHit;                                            // 0x258
    float critical_hit_damage_scale;                             // 0x25C
    bool titanarmor_critical_hit_required;                       // 0x260
    float explosion_inner_radius;                                // 0x264
    float explosionradius;                                       // 0x268
    bool explosionDamagesOwner;                                  // 0x26C
    float grenadeBounceVelFracShallow;                           // 0x270
    float grenadeBounceVelFracSharp;                             // 0x274
    float grenadeBounceVelFracAlongNormal;                       // 0x278
    float grenadeBounceRandomness;                               // 0x27C
    float grenadeBounceExtraVerticalRandomness;                  // 0x280
    float grenadeRollVelFracPerSecond;                           // 0x284
    float grenadeRadiusVertical;                                 // 0x288
    float grenadeRadiusHorizontal;                               // 0x28C
    float grenadeFuseTime;                                       // 0x290
    float grenadeIgnitionTime;                                   // 0x294
    bool grenadeOrientToVelocity;                                // 0x298
    bool grenadeArcIndicatorShowFromHip;                         // 0x299
    int grenadeArcIndicatorBounceCount;                          // 0x29C
    float grenade_death_drop_velocity_scale;                     // 0x2A0
    float grenade_death_drop_velocity_extraUp;                   // 0x2A4
    float projectile_inherit_owner_velocity_scale;               // 0x2A8
    float projectile_first_person_offset_fraction;               // 0x2AC
    float projectile_launch_speed;                               // 0x2B0
    float projectile_launch_pitch_offset;                        // 0x2B4
    float projectile_gravity_scale;                              // 0x2B8
    int projectile_max_deployed;                                 // 0x2BC
    float explosion_shake_radius;                                // 0x2C0
    float explosion_shake_amplitude;                             // 0x2C4
    float explosion_shake_frequency;                             // 0x2C8
    float explosion_shake_duration;                              // 0x2CC
    float reloadTime;                                            // 0x2D0
    float reloadTime_late1;                                      // 0x2D4
    float reloadTime_late2;                                      // 0x2D8
    float reloadTime_late3;                                      // 0x2DC
    float reloadTime_late4;                                      // 0x2E0
    float reloadTime_late5;                                      // 0x2E4
    float reloadEmptyTime;                                       // 0x2E8
    float reloadEmptyTime_late1;                                 // 0x2EC
    float reloadEmptyTime_late2;                                 // 0x2F0
    float reloadEmptyTime_late3;                                 // 0x2F4
    float reloadEmptyTime_late4;                                 // 0x2F8
    float reloadEmptyTime_late5;                                 // 0x2FC
    float reloadSegmentTime_Loop;                                // 0x300
    float reloadSegmentTime_End;                                 // 0x304
    float reloadSegmentEmptyTime_End;                            // 0x308
    bool reloadIsSegmented;                                      // 0x30C
    bool reloadNoAutoIfADSPressed;                               // 0x30D
    bool reloadAltAnim;                                          // 0x30E
    float rechamberTime;                                         // 0x310
    float vortex_drain;                                          // 0x314
    bool enable_highlight_networking_on_creation;                // 0x318
    float chargeTime;                                            // 0x31C
    float chargeCooldownTime;                                    // 0x320
    float chargeCooldownDelay;                                   // 0x324
    bool chargeIsTriggeredByADS;                                 // 0x328
    int chargeLevels;                                            // 0x32C
    bool chargeRequireInput;                                     // 0x330
    bool chargeAllowMidwayCharge;                                // 0x331
    bool charge_allow_melee;                                     // 0x332
    bool chargeEndForcesFire;                                    // 0x333
    bool chargeMaintainedUntilFired;                             // 0x334
    bool chargeRemainFullWhenFired;                              // 0x335
    const char* charge_sound_1p;                                 // 0x338
    const char* charge_sound_3p;                                 // 0x340
    const char* chargeFullSound1P;                               // 0x348
    const char* chargeFullSound3P;                               // 0x350
    const char* chargeDrainSound1P;                              // 0x358
    const char* chargeDrainSound3P;                              // 0x360
    bool chargeSoundStopWhenFull;                                // 0x368
    bool chargeSoundSeekToChargeFraction;                        // 0x369
    bool chargeDrainSoundStopWhenEmpty;                          // 0x36A
    bool chargeDrainSoundSeekToChargeFraction;                   // 0x36B
    const char* chargeEffectAttachment;                          // 0x370
    const char* chargeEffect2Attachment;                         // 0x378
    bool chargeEffectShowDuringDrain;                            // 0x380
    int charge_rumble_min;                                       // 0x384
    int charge_rumble_max;                                       // 0x388
    float coreDuration;                                          // 0x38C
    float coreBuildTime;                                         // 0x390
    float sustainedDischargeDuration;                            // 0x394
    bool sustainedDischargeRequireInput;                         // 0x398
    bool sustainedDischargeAllowMelee;                           // 0x399
    float sustainedDischargePulseFrequency;                      // 0x39C
    bool sustainedDischargeWantPulseCallbacks;                   // 0x3A0
    bool sustainedLaserEnabled;                                  // 0x3A1
    float sustainedLaserRadius;                                  // 0x3A4
    int sustainedLaserRadialIterations;                          // 0x3A8
    int sustainedLaserRadialStep;                                // 0x3AC
    float sustainedLaserRange;                                   // 0x3B0
    const char* sustainedLaserAttachment;                        // 0x3B8
    bool sustainedLaserEffectLoops;                              // 0x3C0
    bool sustainedLaserImpactEffectLoops;                        // 0x3C1
    float sustainedLaserImpactDistance;                          // 0x3C4
    float holsterTime;                                           // 0x3C8
    float deploy_time;                                           // 0x3CC
    float deployfirst_time;                                      // 0x3D0
    float deploycatch_time;                                      // 0x3D4
    float lowerTime;                                             // 0x3D8
    float raiseTime;                                             // 0x3DC
    float raiseFromSprintTime;                                   // 0x3E0
    float sprintCycleTime;                                       // 0x3E4
    bool sprintFractionalAnims;                                  // 0x3E8
    float tossHoldSprintCycleTime;                               // 0x3EC
    float tossTime;                                              // 0x3F0
    float tossOverheadTime;                                      // 0x3F4
    float tossPulloutTime;                                       // 0x3F8
    float cooldownTime;                                          // 0x3FC
    int viewkick_spring;                                         // 0x400
    float viewkickPitchBase;                                     // 0x404
    float viewkickPitchRandom;                                   // 0x408
    float viewkick_pitch_random_innerexclude;                    // 0x40C
    float viewkickPitchSoftScale;                                // 0x410
    float viewkickPitchHardScale;                                // 0x414
    float viewkickYawBase;                                       // 0x418
    float viewkickYawRandom;                                     // 0x41C
    float viewkick_yaw_random_innerexclude;                      // 0x420
    float viewkickYawSoftScale;                                  // 0x424
    float viewkickYawHardScale;                                  // 0x428
    float viewkickRollBase;                                      // 0x42C
    float viewkickRollRandomMin;                                 // 0x430
    float viewkickRollRandomMax;                                 // 0x434
    float viewkickRollSoftScale;                                 // 0x438
    float viewkickRollHardScale;                                 // 0x43C
    float viewkick_hipfire_weaponFraction;                       // 0x440
    float viewkick_hipfire_weaponFraction_vmScale;               // 0x444
    float viewkick_ads_weaponFraction;                           // 0x448
    float viewkick_ads_weaponFraction_vmScale;                   // 0x44C
    float viewkickScaleFirstShot_hipfire;                        // 0x450
    float viewkickScaleFirstShot_ADS;                            // 0x454
    float viewkickScaleMin_hipfire;                              // 0x458
    float viewkickScaleMax_hipfire;                              // 0x45C
    float viewkickScaleMin_ADS;                                  // 0x460
    float viewkickScaleMax_ADS;                                  // 0x464
    float viewkickScaleValuePerShot;                             // 0x468
    float viewkick_scale_valueLerpStart;                         // 0x46C
    float viewkick_scale_valueLerpEnd;                           // 0x470
    float viewkickScaleValueDecayDelay;                          // 0x474
    float viewkickScaleValueDecayRate;                           // 0x478
    float viewkick_duck_scale;                                   // 0x47C
    float viewkick_hover_scale;                                  // 0x480
    float viewkick_move_scale;                                   // 0x484
    float viewkick_air_scale_ads;                                // 0x488
    float viewkickPermPitchBase;                                 // 0x48C
    float viewkickPermPitchRandom;                               // 0x490
    float viewkick_perm_pitch_random_innerexclude;               // 0x494
    float viewkickPermYawBase;                                   // 0x498
    float viewkickPermYawRandom;                                 // 0x49C
    float viewkick_perm_yaw_random_innerexclude;                 // 0x4A0
    float cooldownViewkickPitchBase;                             // 0x4A4
    float cooldownViewkickPitchRandom;                           // 0x4A8
    float cooldown_viewkick_pitch_random_innerexclude;           // 0x4AC
    float cooldownViewkickYawBase;                               // 0x4B0
    float cooldownViewkickYawRandom;                             // 0x4B4
    float cooldown_viewkick_yaw_random_innerexclude;             // 0x4B8
    float cooldownViewkickSoftScale;                             // 0x4BC
    float cooldownViewkickHardScale;                             // 0x4C0
    float cooldownViewkickADSScale;                              // 0x4C4
    float viewdrift_hipfire_stand_scale_pitch;                   // 0x4C8
    float viewdrift_hipfire_crouch_scale_pitch;                  // 0x4CC
    float viewdrift_hipfire_air_scale_pitch;                     // 0x4D0
    float viewdrift_hipfire_speed_pitch;                         // 0x4D4
    float viewdrift_hipfire_stand_scale_yaw;                     // 0x4D8
    float viewdrift_hipfire_crouch_scale_yaw;                    // 0x4DC
    float viewdrift_hipfire_air_scale_yaw;                       // 0x4E0
    float viewdrift_hipfire_speed_yaw;                           // 0x4E4
    float viewdrift_ads_stand_scale_pitch;                       // 0x4E8
    float viewdrift_ads_crouch_scale_pitch;                      // 0x4EC
    float viewdrift_ads_air_scale_pitch;                         // 0x4F0
    float viewdrift_ads_speed_pitch;                             // 0x4F4
    float viewdrift_ads_stand_scale_yaw;                         // 0x4F8
    float viewdrift_ads_crouch_scale_yaw;                        // 0x4FC
    float viewdrift_ads_air_scale_yaw;                           // 0x500
    float viewdrift_ads_speed_yaw;                               // 0x504
    float spreadStandHip;                                        // 0x508
    float spreadStandHipRun;                                     // 0x50C
    float spreadStandHipSprint;                                  // 0x510
    float spreadStandADS;                                        // 0x514
    float spreadCrouchHip;                                       // 0x518
    float spreadCrouchADS;                                       // 0x51C
    float spreadAirHip;                                          // 0x520
    float spreadAirADS;                                          // 0x524
    float spreadWallRunning;                                     // 0x528
    float spreadWallHanging;                                     // 0x52C
    float spread_lerp_speed;                                     // 0x530
    float npcAttackConeAngle;                                    // 0x534
    float spreadKickOnFireStandHip;                              // 0x538
    float spreadKickOnFireStandADS;                              // 0x53C
    float spreadKickOnFireCrouchHip;                             // 0x540
    float spreadKickOnFireCrouchADS;                             // 0x544
    float spreadKickOnFireAirHip;                                // 0x548
    float spreadKickOnFireAirADS;                                // 0x54C
    float spreadMaxKickStandHip;                                 // 0x550
    float spreadMaxKickStandADS;                                 // 0x554
    float spreadMaxKickCrouchHip;                                // 0x558
    float spreadMaxKickCrouchADS;                                // 0x55C
    float spreadMaxKickAirHip;                                   // 0x560
    float spreadMaxKickAirADS;                                   // 0x564
    float spreadDecayRate;                                       // 0x568
    float spreadDecayDelay;                                      // 0x56C
    float spreadTimetoMax;                                       // 0x570
    int impactEffectTableIndex;                                  // 0x574
    int bounceEffectTableIndex;                                  // 0x578
    bool looping_sounds;                                         // 0x57C
    const char* readymessage;                                    // 0x580
    const char* readyhint;                                       // 0x588
    const char* fire_sound_1;                                    // 0x590
    const char* fire_sound_2;                                    // 0x598
    const char* fire_sound_3;                                    // 0x5A0
    const char* fire_sound_first_shot;                           // 0x5A8
    const char* fire_sound_1_npc;                                // 0x5B0
    const char* fire_sound_2_npc;                                // 0x5B8
    const char* fire_sound_3_npc;                                // 0x5C0
    const char* fire_sound_first_shot_npc;                       // 0x5C8
    const char* fire_sound_1_player_1p;                          // 0x5D0
    const char* fire_sound_2_player_1p;                          // 0x5D8
    const char* fire_sound_3_player_1p;                          // 0x5E0
    const char* fire_sound_first_shot_player_1p;                 // 0x5E8
    const char* fire_sound_1_player_3p;                          // 0x5F0
    const char* fire_sound_2_player_3p;                          // 0x5F8
    const char* fire_sound_3_player_3p;                          // 0x600
    const char* fire_sound_first_shot_player_3p;                 // 0x608
    const char* burst_or_looping_fire_sound_start;               // 0x610
    const char* burst_or_looping_fire_sound_middle;              // 0x618
    const char* burst_or_looping_fire_sound_end;                 // 0x620
    const char* burst_or_looping_fire_sound_start_npc;           // 0x628
    const char* burst_or_looping_fire_sound_middle_npc;          // 0x630
    const char* burst_or_looping_fire_sound_end_npc;             // 0x638
    const char* burst_or_looping_fire_sound_start_1p;            // 0x640
    const char* burst_or_looping_fire_sound_middle_1p;           // 0x648
    const char* burst_or_looping_fire_sound_end_1p;              // 0x650
    const char* burst_or_looping_fire_sound_start_3p;            // 0x658
    const char* burst_or_looping_fire_sound_middle_3p;           // 0x660
    const char* burst_or_looping_fire_sound_end_3p;              // 0x668
    const char* idleSound_Player_1P;                             // 0x670
    const char* low_ammo_sound_name_1;                           // 0x678
    const char* low_ammo_sound_name_2;                           // 0x680
    const char* low_ammo_sound_name_3;                           // 0x688
    const char* low_ammo_sound_name_4;                           // 0x690
    const char* low_ammo_sound_name_5;                           // 0x698
    const char* low_ammo_sound_name_6;                           // 0x6A0
    const char* low_ammo_sound_name_7;                           // 0x6A8
    const char* low_ammo_sound_name_8;                           // 0x6B0
    const char* low_ammo_sound_name_9;                           // 0x6B8
    const char* low_ammo_sound_name_10;                          // 0x6C0
    const char* low_ammo_sound_name_11;                          // 0x6C8
    const char* low_ammo_sound_name_12;                          // 0x6D0
    const char* low_ammo_sound_name_13;                          // 0x6D8
    const char* low_ammo_sound_name_14;                          // 0x6E0
    const char* low_ammo_sound_name_15;                          // 0x6E8
    int low_ammo_sound_range_start_1;                            // 0x6F0
    int low_ammo_sound_range_start_2;                            // 0x6F4
    int low_ammo_sound_range_start_3;                            // 0x6F8
    const char* low_ammo_sound_range_name_1;                     // 0x700
    const char* low_ammo_sound_range_name_2;                     // 0x708
    const char* low_ammo_sound_range_name_3;                     // 0x710
    int body_skin;                                               // 0x718
    int bodygroup1_set;                                          // 0x71C
    int bodygroup2_set;                                          // 0x720
    int bodygroup3_set;                                          // 0x724
    int bodygroup4_set;                                          // 0x728
    int bodygroup5_set;                                          // 0x72C
    int bodygroup6_set;                                          // 0x730
    int bodygroup7_set;                                          // 0x734
    int bodygroup8_set;                                          // 0x738
    int bodygroup9_set;                                          // 0x73C
    int bodygroup10_set;                                         // 0x740
    int bodygroup_ads_scope_set;                                 // 0x744
    int bodygroup_ammo_index_count;                              // 0x748
    int npcMinBurst;                                             // 0x74C
    int npcMaxBurst;                                             // 0x750
    int npcBurstSecondary;                                       // 0x754
    bool npcMissFastPlayer;                                      // 0x758
    bool npcFullAutoVsHeavyArmor;                                // 0x759
    bool npcUseStrictMuzzleDir;                                  // 0x75A
    bool npcAimAtFeet;                                           // 0x75B
    bool npcAimAtFeetVsHeavyArmor;                               // 0x75C
    bool npcVortexBlock;                                         // 0x75D
    bool npcSelfExplosionSafety;                                 // 0x75E
    bool npcDangerousToNormalArmor;                              // 0x75F
    bool npcDangerousToHeavyArmor;                               // 0x760
    bool npcSuppressLSPAllowed;                                  // 0x761
    bool npcClearChargeIfNotFired;                               // 0x762
    float npc_rest_time_between_bursts_min;                      // 0x764
    float npc_rest_time_between_bursts_max;                      // 0x768
    float npcChargeTimeMin;                                      // 0x76C
    float npcChargeTimeMax;                                      // 0x770
    float npcMinRange;                                           // 0x774
    float npcMaxRange;                                           // 0x778
    float npcMinRangeSecondary;                                  // 0x77C
    float npcMaxRangeSecondary;                                  // 0x780
    float npcDamageNearDistance;                                 // 0x784
    float npcDamageFarDistance;                                  // 0x788
    float npcDamageVeryFarDistance;                              // 0x78C
    int npcDamageNearValue;                                      // 0x790
    int npc_damage_near_value_titanarmor;                        // 0x794
    int npcDamageFarValue;                                       // 0x798
    int npc_damage_far_value_titanarmor;                         // 0x79C
    int npcDamageVeryFarValue;                                   // 0x7A0
    int npc_damage_very_far_value_titanarmor;                    // 0x7A4
    float npcMinEngageRange;                                     // 0x7A8
    float npcMaxEngageRange;                                     // 0x7AC
    float npcMinEngageRange_heavyArmor;                          // 0x7B0
    float npcMaxEngageRange_heavyArmor;                          // 0x7B4
    float npc_rest_time_between_bursts_expedite;                 // 0x7B8
    float npcRestTimeSecondary;                                  // 0x7BC
    float npcPreFireDelay;                                       // 0x7C0
    float npcPreFireDelayInterval;                               // 0x7C4
    float npc_lead_time_scale;                                   // 0x7C8
    float npc_lead_time_min_dist;                                // 0x7CC
    float npc_lead_time_max_dist;                                // 0x7D0
    float npc_directed_fire_ang_limit_cos;                       // 0x7D4
    float npcFireAtEnemyDefenseTime;                             // 0x7D8
    int npcReloadEnabled;                                        // 0x7DC
    float npcAccuracyMultiplier_heavyArmor;                      // 0x7E0
    float npcAccuracyMultiplier_pilot;                           // 0x7E4
    float npcAccuracyMultiplier_npc;                             // 0x7E8
    float npcUseShortDuration;                                   // 0x7EC
    float npcUseLongDuration;                                    // 0x7F0
    float npcUseMinDamage;                                       // 0x7F4
    float npcUseMaxDamage;                                       // 0x7F8
    float npcUseMinProjectileDamage;                             // 0x7FC
    float npcSpreadConeFocusTime;                                // 0x800
    float npcSpreadDefocusedConeMultiplier;                      // 0x804
    float npcSpreadPatternFocusTime;                             // 0x808
    float npcSpreadPatternNotInFOVTime;                          // 0x80C
    float npcSpreadPatternNotInFOVFactor;                        // 0x810
    float proficiency_poor_spreadscale;                          // 0x814
    float proficiency_poor_bias;                                 // 0x818
    float proficiency_poor_additional_rest;                      // 0x81C
    float proficiency_average_spreadscale;                       // 0x820
    float proficiency_average_bias;                              // 0x824
    float proficiency_average_additional_rest;                   // 0x828
    float proficiency_good_spreadscale;                          // 0x82C
    float proficiency_good_bias;                                 // 0x830
    float proficiency_good_additional_rest;                      // 0x834
    float proficiency_very_good_spreadscale;                     // 0x838
    float proficiency_very_good_bias;                            // 0x83C
    float proficiency_very_good_additional_rest;                 // 0x840
    float proficiency_perfect_spreadscale;                       // 0x844
    float proficiency_perfect_bias;                              // 0x848
    float proficiency_perfect_additional_rest;                   // 0x84C
    float smartAmmoSearchAngle;                                  // 0x850
    float smartAmmoSearchDistance;                               // 0x854
    float smartAmmoNewTargetDelay;                               // 0x858
    bool smartAmmoApplyNewTargetDelayToFirstTarget;              // 0x85C
    float smartAmmoTargetMaxLocksNormal;                         // 0x860
    float smartAmmoTargetMaxLocksHeavy;                          // 0x864
    float smartAmmoTargetNPCLockFactor;                          // 0x868
    float smartAmmoAlertNPCFraction;                             // 0x86C
    int smartAmmoMaxTargets;                                     // 0x870
    int smartAmmoMaxTargetedBurst;                               // 0x874
    bool smartAmmoAltLockStyle;                                  // 0x878
    bool smartAmmoAllowAdsLock;                                  // 0x879
    bool smartAmmoAllowHipFireLock;                              // 0x87A
    bool smartAmmoAllowSearchWhileFiring;                        // 0x87B
    bool smartAmmoAllowSearchWhileInactive;                      // 0x87C
    bool smartAmmoOnlySearchOnCharge;                            // 0x87D
    bool smartAmmoAlwaysDoBurst;                                 // 0x87E
    bool smartAmmoTrackCloakedTargets;                           // 0x87F
    float smartAmmoTargetingTimeMin;                             // 0x880
    float smartAmmoTargetingTimeMax;                             // 0x884
    float smartAmmoTargetingTimeMinNpc;                          // 0x888
    float smartAmmoTargetingTimeMaxNpc;                          // 0x88C
    float smartAmmoTargetingTimeModifierCloaked;                 // 0x890
    float smartAmmoTargetingTimeModifierProjectile;              // 0x894
    float smartAmmoTargetingTimeModifierProjectileOwner;         // 0x898
    float smartAmmoUnlockDebounceTime;                           // 0x89C
    bool smartAmmoDrawAcquisitionLines;                          // 0x8A0
    eSmartAmmoHudType smartAmmoHudType;                          // 0x8A4
    eSmartAmmoHudLockStyle smartAmmoHudLockStyle;                // 0x8A8
    eSmartAmmoWeaponType smartAmmoWeaponType;                    // 0x8AC
    eSmartAmmoLockType smartAmmoLockType;                        // 0x8B0
    const char* smartAmmoTargetConfirmedSound;                   // 0x8B8
    const char* smartAmmoTargetConfirmingSound;                  // 0x8C0
    const char* smartAmmoTargetFoundSound;                       // 0x8C8
    const char* smartAmmoTargetLostSound;                        // 0x8D0
    const char* smartAmmoLoopingSoundAcquiring;                  // 0x8D8
    const char* smartAmmoLoopingSoundLocked;                     // 0x8E0
    bool smartAmmoSearchNPCs;                                    // 0x8E8
    bool smartAmmoSearchPlayers;                                 // 0x8E9
    bool smartAmmoStickToFullyLockedTargets;                     // 0x8EA
    float smartAmmoHoldAndResetAfterAllLocks;                    // 0x8EC
    float smartAmmoActiveShotTime;                               // 0x8F0
    bool smartAmmoActiveShotOnFirstLockOnly;                     // 0x8F4
    float smartAmmoActiveShotDamageMultiplier;                   // 0x8F8
    bool smartAmmoPlayerTargetsMustBeTracked;                    // 0x8FC
    bool smartAmmoNpcTargetsMustBeTracked;                       // 0x8FD
    bool smartAmmoOtherTargetsMustBeTracked;                     // 0x8FE
    bool smartAmmoTrackedTargetsCheckVisibility;                 // 0x8FF
    int smartAmmoMaxTrackersPerTarget;                           // 0x900
    bool smartAmmoTrackerStatusEffects;                          // 0x904
    const char* smartAmmoLockEffectAttachment;                   // 0x908
    const char* smartAmmoLockEffect2Attachment;                  // 0x910
    bool smartAmmoSearchFriendlyTeam;                            // 0x918
    bool smartAmmoSearchEnemyTeam;                               // 0x919
    bool smartAmmoSearchNeutralTeam;                             // 0x91A
    bool smartAmmoSearchPhaseShift;                              // 0x91B
    float zoomTimeIn;                                            // 0x91C
    float zoomTimeOut;                                           // 0x920
    float zoomToggleLerpTime;                                    // 0x924
    float zoomFOV;                                               // 0x928
    float zoomToggleFOV;                                         // 0x92C
    float zoomFOVViewmodel;                                      // 0x930
    float zoomScopeFracStart;                                    // 0x934
    float zoomScopeFracEnd;                                      // 0x938
    float zoomAngleShiftPitch;                                   // 0x93C
    float zoomAngleShiftYaw;                                     // 0x940
    float dof_nearDepthStart;                                    // 0x944
    float dof_nearDepthEnd;                                      // 0x948
    float dof_zoom_nearDepthStart;                               // 0x94C
    float dof_zoom_nearDepthEnd;                                 // 0x950
    float dof_zoom_focusArea_Horizontal;                         // 0x954
    float dof_zoom_focusArea_Top;                                // 0x958
    float dof_zoom_focusArea_Bottom;                             // 0x95C
    int anim_alt_idleAttack;                                     // 0x960
    float minimapRevealDistance;                                 // 0x964
    bool breaks_cloak;                                           // 0x968
    int rui_crosshair_index;                                     // 0x96C
    int activeCrosshairCount;                                    // 0x970
    int ordnanceCrosshairAlwaysOnStartIndex;                     // 0x974
    bool crosshair_force_sprint_fade_disabled;                   // 0x978
    bool projectileVisibleToSmartAmmo;                           // 0x979
    bool projectile_do_predict_impact_effects;                   // 0x97A
    bool is_burn_mod;                                            // 0x97B
    float projectileDriftWindiness;                              // 0x97C
    float projectileDriftIntensity;                              // 0x980
    float projectile_straight_time_min;                          // 0x984
    float projectile_straight_time_max;                          // 0x988
    float projectileStraightRadiusMin;                           // 0x98C
    float projectileStraightRadiusMax;                           // 0x990
    float projectileLifeTime;                                    // 0x994
    float projectile_damage_reduction_per_bounce;                // 0x998
    float projectileSpeedReductionFactor;                        // 0x99C
    float projectileCollideWithOwnerGraceTime;                   // 0x9A0
    int projectileRicochetMaxCount;                              // 0x9A4
    bool projectileDamagesOwner;                                 // 0x9A8
    bool projectile_collide_with_owner;                          // 0x9A9
    bool projectileAirburstOnDeath;                              // 0x9AA
    const char* projectileDeathSound;                            // 0x9B0
    const char* projectileFlightSound;                           // 0x9B8
    bool projectile_killreplay_enabled;                          // 0x9C0
    float projectile_chasecamDistanceMax;                        // 0x9C4
    float projectile_chasecamMaxOrbitDepth;                      // 0x9C8
    float projectile_chasecamMaxPitchUp;                         // 0x9CC
    float projectile_chasecamOffsetUp;                           // 0x9D0
    float projectile_chasecamOffsetRight;                        // 0x9D4
    float projectile_chasecamOffsetForward;                      // 0x9D8
    const char* challeng_req;                                    // 0x9E0
    int challengeTier;                                           // 0x9E8
    bool loadoutSelectable;                                      // 0x9EC
    const char* loadoutType;                                     // 0x9F0
    const char* loadoutParentRef;                                // 0x9F8
    const char* loadoutChildRef;                                 // 0xA00
    const char* modPrintName;                                    // 0xA08
    const char* modShortPrintName;                               // 0xA10
    const char* modDescription;                                  // 0xA18
    float statDamage;                                            // 0xA20
    float statAccuracy;                                          // 0xA24
    float statRange;                                             // 0xA28
    float stat_rof;                                              // 0xA2C
    const char* menu_image;                                      // 0xA30
    const char* menu_icon;                                       // 0xA38
    const char* menu_alt_icon;                                   // 0xA40
    const char* hud_icon;                                        // 0xA48
    bool neverDrop;                                              // 0xA50
    bool destroyOnDrop;                                          // 0xA51
    bool clearFXOnNewViewModel;                                  // 0xA52
    bool custom_bool_0;                                          // 0xA53
    bool custom_bool_1;                                          // 0xA54
    bool custom_bool_2;                                          // 0xA55
    bool custom_bool_3;                                          // 0xA56
    bool custom_bool_4;                                          // 0xA57
    bool custom_bool_5;                                          // 0xA58
    bool custom_bool_6;                                          // 0xA59
    bool custom_bool_7;                                          // 0xA5A
    int custom_int_0;                                            // 0xA5C
    int custom_int_1;                                            // 0xA60
    int custom_int_2;                                            // 0xA64
    int custom_int_3;                                            // 0xA68
    int custom_int_4;                                            // 0xA6C
    int custom_int_5;                                            // 0xA70
    int custom_int_6;                                            // 0xA74
    int custom_int_7;                                            // 0xA78
    float custom_float_0;                                        // 0xA7C
    float custom_float_1;                                        // 0xA80
    float custom_float_2;                                        // 0xA84
    float custom_float_3;                                        // 0xA88
    float custom_float_4;                                        // 0xA8C
    float custom_float_5;                                        // 0xA90
    float custom_float_6;                                        // 0xA94
    float custom_float_7;                                        // 0xA98
    bool reload_enabled;                                         // 0xA9C
    const char* sound_dryfire;                                   // 0xAA0
    const char* sound_pickup;                                    // 0xAA8
    const char* sound_weaponReady;                               // 0xAB0
    bool meleeRespectNextAttackTime;                             // 0xAB8
    bool canAttackWhenDead;                                      // 0xAB9
    bool showPreModdedTracer;                                    // 0xABA
    bool threatScopeEnabled;                                     // 0xABB
    float threatScopeZoomFracStart;                              // 0xABC
    float threatScopeZoomFracEnd;                                // 0xAC0
    const char* threatScopeBoundsTagName1;                       // 0xAC8
    const char* threatScopeBoundsTagName2;                       // 0xAD0
    float threatScopeBoundsWidth;                                // 0xAD8
    float threatScopeBoundsHeight;                               // 0xADC
    bool threat_scope_fadeWithDistance;                          // 0xAE0
    float ignitionDistance;                                      // 0xAE4
    float preIgnitionSpeed;                                      // 0xAE8
    int preIgnitionDamage;                                       // 0xAEC
    int pre_ignition_damage_titanarmor;                          // 0xAF0
    int preIgnitionNpcDamage;                                    // 0xAF4
    int pre_ignition_npc_damage_titanarmor;                      // 0xAF8
    const char* preIgnitionFlightSound;                          // 0xB00
    const char* preIgnitionTrailEffect;                          // 0xB08
    const char* ignitionSound;                                   // 0xB10
    const char* ignitionEffect;                                  // 0xB18
    bool grapple_weapon;                                         // 0xB20
    float grapple_power_required;                                // 0xB24
    float grapple_power_use_rate;                                // 0xB28
    float grapple_maxLength;                                     // 0xB2C
    float grapple_maxLengthVert;                                 // 0xB30
    WeaponString_t viewModel;                                    // 0xB34
    WeaponString_t playermodel;                                  // 0xB36
    WeaponString_t holsterModel;                                 // 0xB38
    const char* impact_effect_table;                             // 0xB40
    const char* bounce_effect_table;                             // 0xB48
    const char* sustained_laser_new_surface_impact_effect_table; // 0xB50
    const char* pre_ignition_impact_effect_table;                // 0xB58
    const char* tracerEffect;                                    // 0xB60
    const char* tracerEffectFirstPerson;                         // 0xB68
    const char* projectile_trail_effect_0;                       // 0xB70
    const char* projectile_trail_effect_1;                       // 0xB78
    const char* projectile_trail_effect_2;                       // 0xB80
    const char* projectile_trail_effect_3;                       // 0xB88
    const char* projectile_trail_effect_4;                       // 0xB90
    const char* fx_muzzle_flash_view;                            // 0xB98
    const char* fx_muzzle_flash_world;                           // 0xBA0
    const char* fx_muzzle_flash_attach;                          // 0xBA8
    const char* fx_muzzle_flash_attach_scoped;                   // 0xBB0
    const char* fx_muzzle_flash2_view;                           // 0xBB8
    const char* fx_muzzle_flash2_world;                          // 0xBC0
    const char* fx_muzzle_flash2_attach;                         // 0xBC8
    const char* fx_muzzle_flash2_attach_scoped;                  // 0xBD0
    const char* fx_shell_eject_view;                             // 0xBD8
    const char* fx_shell_eject_world;                            // 0xBE0
    const char* fx_shell_eject_attach;                           // 0xBE8
    const char* fx_shell_eject_attach_scoped;                    // 0xBF0
    const char* fx_shell_eject2_view;                            // 0xBF8
    const char* fx_shell_eject2_world;                           // 0xC00
    const char* fx_shell_eject2_attach;                          // 0xC08
    const char* fx_shell_eject2_attach_scoped;                   // 0xC10
    const char* grenadeArcIndicatorEffect;                       // 0xC18
    const char* grenadeArcIndicatorEffectFirst;                  // 0xC20
    const char* grenadeArcImpactIndicatorEffect;                 // 0xC28
    const char* sustainedLaserEffect1p;                          // 0xC30
    const char* sustainedLaserEffect3p;                          // 0xC38
    const char* sustainedLaserImpactEffect;                      // 0xC40
    const char* chargeEffect1P;                                  // 0xC48
    const char* chargeEffect3P;                                  // 0xC50
    const char* chargeEffect21P;                                 // 0xC58
    const char* chargeEffect23P;                                 // 0xC60
    const char* smartAmmoLockEffect1P;                           // 0xC68
    const char* smartAmmoLockEffect3P;                           // 0xC70
    const char* smartAmmoLockEffect21P;                          // 0xC78
    const char* smartAmmoLockEffect23P;                          // 0xC80
    const char* vortex_absorb_effect;                            // 0xC88
    const char* vortex_absorb_effect_third_person;               // 0xC90
    const char* vortex_impact_effect;                            // 0xC98
};

struct WeaponSway_Transform
{
    Vector3D translate;
    QAngle rotate;
};

struct WeaponSway_Move
{
    WeaponSway_Transform forward;
    WeaponSway_Transform back;
    WeaponSway_Transform left;
    WeaponSway_Transform right;
    WeaponSway_Transform up;
    WeaponSway_Transform down;
};

struct WeaponSway_Turn
{
    WeaponSway_Transform left;
    WeaponSway_Transform right;
    WeaponSway_Transform up;
    WeaponSway_Transform down;
};

struct WeaponSway_Clamp
{
    char rotateAttachName[MAX_WEAPON_STRING];
    float rotateAttachBlendTime;
    Vector3D translateMin;
    Vector3D translateMax;
    QAngle rotateMin;
    QAngle rotateMax;
    float translateGain;
    float rotateGain;
    float bobCycleTime;
    float bobMinSpeed;
    float bobMaxSpeed;
    float bobVertDist;
    float bobHorzDist;
    float bobPitch;
    float bobYaw;
    float bobRoll;
    float bobGain;
    WeaponSway_Move move;
    WeaponSway_Turn turn;
};

struct WeaponSway_Spec
{
    WeaponSway_Clamp unzoomed;
    WeaponSway_Clamp zoomed;
    bool zoomedSway;
};

struct WeaponMod
{
    WeaponString_t modName;
    std::uint16_t firstEntry;
    std::uint16_t entryCount;
};

enum class WeaponModValueType : std::uint8_t
{
    Invalid = 0,
    Integer = 1,
    Float = 2,
    Boolean = 3,
    String = 4,
    Asset = 5,
    Vector = 6,
    WeaponString = 7,
    Special = 8,
};

enum WeaponModParseFlags : std::uint8_t
{
    WMPF_NONE = 0,
    WMPF_IMPACT_EFFECT_TABLE = 1 << 2,
    WMPF_PARTICLE_SYSTEM = 1 << 3,
};

enum WeaponModEntryType : std::uint16_t
{
    WMET_INVALID = 0,
};

struct WeaponModEntry_t
{
    WeaponModEntryType entryType;
    std::uint16_t entryOperator;
    union
    {
        int intValue;
        float floatValue;
        bool boolValue;
        WeaponString_t stringValue;
        float vectorValue[3];
        std::byte value[12];
    };

    bool HasValue() const
    {
        return entryOperator != 0;
    }
};

struct WeaponModParseTableEntry
{
    const char* parseName;
    const char* defaultString;
    union
    {
        int defaultInt;
        float defaultFloat;
    };
    std::uint32_t reserved0;
    bool defaultBool;
    WeaponModValueType parseType;
    std::uint8_t parseFlags;
    std::byte reserved1;
    WeaponModEntryType sanityCheckType;
    std::uint16_t structOffset;
};

struct WeaponModAssemblyItem_t
{
    WeaponModEntry_t* entry = nullptr;
    WeaponModAssemblyItem_t* next = nullptr;
    std::byte reserved0[8]{};
    bool setBaseValue = false;
    bool remove = false;
    std::byte reserved1[6]{};
};

static_assert(sizeof(WeaponModValues) == 0xCA0);
static_assert(sizeof(WeaponMod) == 0x6);
static_assert(sizeof(WeaponModEntry_t) == 0x10);
static_assert(sizeof(WeaponModParseTableEntry) == 0x20);
static_assert(sizeof(WeaponModAssemblyItem_t) == 0x20);
