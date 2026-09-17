#pragma once

#include "client/icliententity.h"

#include "client/iclientmodelrenderable.h"

#include "client/collisionproperty.h"
#include "client/particleproperty.h"
#include "engine/ICollideable.h"
#include "engine/deferredtrace.h"
#include "engine/ehandle.h"
#include "gametrace.h"
#include "mathlib/color.h"
#include "predictableid.h"
#include "vscript/script_scope.h"

#include <cstddef>
#include <cstdint>

class ISave;
class IRestore;
class C_WeaponX;
class C_BaseCombatWeapon;
class C_TitanSoul;
class CParticleEffect;
struct ScriptClassDesc_t;
struct Quaternion;
struct ClientInterpolationSnapshot;
struct VisibleToLocalPlayerTrace;
struct FileWeaponInfo_t;

struct datamap_t;
class IPhysicsObject;
class C_BaseAnimating;
class C_BaseAnimatingOverlay;
class C_BaseCombatCharacter;
class C_Player;
class C_RopeKeyframe;
class C_ScriptProp;
class C_Beam;
class C_DynamicProp;

class C_BaseEntity;
class CScriptNetDataList;
class C_StatusEffectPlugin;
struct EntityFXData;
struct SingleSnapshotValues;
struct PredictedEntityStates;

enum BurstFireType : int
{
    BURST_FIRE_None = 0,
    BURST_FIRE_Start,
    BURST_FIRE_Middle,
    BURST_FIRE_End
};

struct ScriptOriginatedDamageInfo_Client
{
    int m_scriptDamageType = 0;
    int m_damageSourceIdentifier = -1;
    int m_reserved = -1;
};

struct FireBulletsInfo_Client_t
{
    FireBulletsInfo_Client_t() = default;
    FireBulletsInfo_Client_t(int shots, const Vector3D& source, const Vector3D& direction,
                            float spread = 0.0f, float distance = 8192.0f, bool primaryAttack = true)
        : m_iShots(shots), m_vecSrc(source), m_vecDirShooting(direction), m_spread(spread),
          m_flDistance(distance), m_bPrimaryAttack(primaryAttack)
    {
    }

    int m_iShots = 1;
    Vector3D m_vecSrc;
    Vector3D m_vecDirShooting;
    float m_spread = 0.0f;
    float m_flDistance = 8192.0f;
    int m_iTracerFreq = 4;
    int m_fireBulletFlags = 0;
    C_BaseEntity* m_pAttacker = nullptr;
    C_WeaponX* m_weapon = nullptr;
    const FileWeaponInfo_t* m_weaponInfo = nullptr;
    C_BaseEntity* m_pAdditionalIgnoreEnt = nullptr;
    bool m_bPrimaryAttack = true;
    bool m_bNetOptimized = false;
    bool m_bReflected = false;
    BurstFireType m_burstFire = BURST_FIRE_None;
    int m_fireDelay = 0;
    int m_iImpactEffectTableIndex = 0;
    int m_passedThicknessTotal = 0;
    int m_passedEntities = 0;
    unsigned int m_passThroughModCount[1]{};
    float m_currentDamageFactor = 1.0f;
    ScriptOriginatedDamageInfo_Client m_scriptDamageInfo;
};

using BASEPTR = void (C_BaseEntity::*)();
using ENTITYFUNCPTR = void (C_BaseEntity::*)(C_BaseEntity*);

struct CurrentFrameData
{
    Vector3D origin;
    short modelIndex;
    bool isRagdoll;
    Vector3D ragdollForce;
    Vector3D viewOffset;
    float animCycle;
    CHandle<C_WeaponX> weaponGettingSwitchedOut;
    bool showActiveWeapon3p;
};

struct LerpData
{
    const SingleSnapshotValues* lastSnap;
    const SingleSnapshotValues* currentSnap;
    const SingleSnapshotValues* futureSnap;
    const SingleSnapshotValues* snaps[3];
};

struct PredictionContext
{
    bool m_bActive;
    float m_creationTime;
    int m_nCreationCommandNumber;
    const char* m_pszCreationModule;
    int m_nCreationLineNumber;
    CHandle<C_BaseEntity> m_hServerEntity;
};

struct thinkfunc_client_t
{
    BASEPTR m_pfnThink;
    const char* m_iszContext;
    bool m_fireBeforeBaseThink;
    int m_nNextThinkTick;
    int m_nLastThinkTick;
};

struct PredictedEntityData
{
    bool predicted;
    PredictedEntityStates* states;
};

struct MinimapBaseEntityData_Client
{
    std::uint64_t visibilityDefaultFlag;
    std::uint64_t visibilityShowFlag;
    unsigned int flags;
    unsigned int zOrder;
    unsigned int customState;
    float objectScale;
};

class C_BaseEntity : public IClientEntity, public IClientModelRenderable
{
  public:
    struct AbsHistory
    {
        Vector3D origin;
        QAngle angles;
        float time;
    };

    virtual datamap_t* GetDataDescMap() = 0;                    // 13
    virtual int YouForgotToImplementOrDeclareClientClass() = 0; // 14
    virtual datamap_t* GetPredDescMap() = 0;                    // 15
    virtual const datamap_t* GetPredDescMapConst() const = 0;   // 16
    virtual ScriptClassDesc_t* GetScriptDesc() = 0;             // 17
    virtual void FireBullets(const FireBulletsInfo_Client_t& info) = 0; // 18
  private:
    virtual void UnknownEntity019() = 0;
    virtual void UnknownEntity020() = 0;

  public:
    virtual int GetTracerAttachment() = 0;         // 21
    virtual C_BaseEntity* GetTracerEntity() = 0;   // 22
    virtual const char* GetTracerType() const = 0; // 23
    virtual int GetArmorType() const = 0;          // 24
  private:
    virtual void UnknownEntity025() = 0;

  public:
    virtual void Spawn() = 0;             // 26
    virtual void SpawnClientEntity() = 0; // 27
    virtual void Precache() = 0;          // 28
    virtual void Activate() = 0;          // 29
  private:
    virtual void UnknownEntity030() = 0;

  public:
    virtual bool KeyValue(const char* key, const char* value) = 0;                               // 31
    virtual bool GetKeyValue(const char* key, char* value, int maxLength) = 0;                   // 32
    virtual int GetSpawnCounter() const = 0;                                                     // 33
    virtual bool Entity_Init(int entityIndex, int serialNumber, bool clientOnly) = 0;            // 34
    virtual const C_BaseAnimating* GetBaseAnimatingConst() const = 0;                            // 35
    virtual C_BaseAnimating* GetBaseAnimating() = 0;                                             // 36
    virtual const C_BaseAnimatingOverlay* GetBaseAnimatingOverlayConst() const = 0;              // 37
    virtual C_BaseAnimatingOverlay* GetBaseAnimatingOverlay() = 0;                               // 38
    virtual C_RopeKeyframe* GetRopeKeyframe() = 0;                                               // 39
    virtual void SetClassname(const char* classname) = 0;                                        // 40
    virtual bool IsFirstPersonProxy() = 0;                                                       // 41
    virtual bool IsViewModel() = 0;                                                              // 42
    virtual bool SkipsAnimationData() = 0;                                                       // 43
    virtual float GetIKHeightOffset() const = 0;                                                 // 44
    virtual int Classify() const = 0;                                                            // 45
    virtual void ReleaseNoDeleteSelf() = 0;                                                      // 46
    virtual void CalcRenderOriginAndAngles(Vector3D& origin, QAngle& angles) = 0;                // 47
    virtual bool IsTransparentPrimary() = 0;                                                     // 48
    virtual bool TestCollision(const Ray_t& ray, unsigned int contentsMask, trace_t& trace) = 0; // 49
    virtual bool TestHitboxes(const Ray_t& ray, unsigned int contentsMask, trace_t& trace) = 0;  // 50
    virtual C_BaseEntity* GetOwnerEntity() const = 0; // 51; BD940
    virtual C_BaseEntity* GetOwnerEntity() = 0;                  // 52; BD980
  public:
    virtual C_BaseEntity* GetViewModelOwner() = 0;                              // 53
    virtual float GetAttackDamageScale() const = 0;                             // 54
    virtual void MoveFieldsIntoLerpingStruct(float time) = 0;                   // 55
    virtual void PostDataUpdateThreaded(DataUpdateType_t updateType) = 0;       // 56
    virtual void SetDormant(bool dormant) = 0;                                  // 57
    virtual bool ShouldSavePhysics() = 0;                                       // 58
    virtual void OnSave() = 0;                                                  // 59
    virtual void OnRestore() = 0;                                               // 60
    virtual int ObjectCaps() = 0;                                               // 61
    virtual int Save(ISave& save) = 0;                                          // 62
    virtual int Restore(IRestore& restore) = 0;                                 // 63
    virtual bool CreateVPhysics() = 0;                                          // 64
    virtual void VPhysicsDestroyObject() = 0;                                   // 65
    virtual void VPhysicsUpdate(IPhysicsObject* physicsObject) = 0;             // 66
    virtual void VPhysicsShadowUpdate(IPhysicsObject* physicsObject) = 0;       // 67
    virtual int VPhysicsGetObjectList(IPhysicsObject** list, int maxCount) = 0; // 68
    virtual bool SetupBones_MayUseFootIk() = 0;                                 // 69
  private:
    virtual void UnknownEntity070() = 0;

  public:
    virtual const Vector3D& GetPrevAbsOrigin() const = 0;                                                          // 71
    virtual void Teleport(const Vector3D* position, const QAngle* angles, const Vector3D* velocity) = 0;           // 72
    virtual void PreNewModel() = 0;                                                                                // 73
    virtual void PostNewModel() = 0;                                                                               // 74
    virtual const Vector3D& GetBoundingMins() const = 0;                                                           // 75
    virtual const Vector3D& GetBoundingMaxs() const = 0;                                                           // 76
    virtual Vector3D Script_GetBoundingMins() const = 0;                                                           // 77
    virtual Vector3D Script_GetBoundingMaxs() const = 0;                                                           // 78
    virtual const Vector3D& WorldSpaceCenter() const = 0;                                                          // 79
    virtual void ComputeWorldSpaceSurroundingBox(Vector3D* mins, Vector3D* maxs) = 0;                              // 80
    virtual void GetVectors(Vector3D* forward, Vector3D* right, Vector3D* up) const = 0;                           // 81
    virtual SolidType_t GetSolid() const = 0;                                                                      // 82
    virtual int GetSolidFlags() const = 0;                                                                         // 83
    virtual bool GetAttachmentOrigin(int attachment, Vector3D& origin) = 0;                                        // 84
    virtual bool GetAttachmentVelocity(int attachment, Vector3D& originVelocity, Quaternion& angleVelocity) = 0;   // 85
    virtual bool GetHitboxAttachmentVelocity(int hitbox, Vector3D& originVelocity, Quaternion& angleVelocity) = 0; // 86
    virtual bool GetHitboxAttachment(int hitbox, Vector3D& origin, QAngle& angles) = 0;                            // 87
    virtual void InvalidateAttachments() = 0;                                                                      // 88
    virtual int GetTeamNumber() const = 0;                                                                         // 89
    virtual void ChangeTeam(int teamNumber) = 0;                                                                   // 90
  private:
    virtual void UnknownEntity091() = 0;
    virtual void UnknownEntity092() = 0;

  public:
    virtual bool CastsShadows() = 0;                                                                                                           // 93
    virtual void UpdatePartitionListEntry() = 0;                                                                                               // 94
    virtual bool InitializeAsClientEntity(const char* modelName, bool renderWithViewModels) = 0;                                               // 95
    virtual bool Simulate() = 0;                                                                                                               // 96
    virtual bool CanStandOn(C_BaseEntity* surface) const = 0;                                                                                  // 97
    virtual void GetAimEntOrigin(IClientEntity* entity, Vector3D* origin, QAngle* angles) = 0;                                                 // 98
    virtual bool ShouldRenderAlways() const = 0;                                                                                               // 99
    virtual int ComputeTranslucencyType() = 0;                                                                                                 // 100
    virtual bool ComputeSortPriority() = 0;                                                                                                    // 101
    virtual int GetCollideType() = 0;                                                                                                          // 102
    virtual void UpdateVisibility() = 0;                                                                                                       // 103
    virtual bool ShouldSuppressForSplitScreenPlayer(int splitScreenSlot) = 0;                                                                  // 104
    virtual bool IsImmuneToPhaseShiftInvisibility(C_Player* viewPlayer) const = 0;                                                             // 105
    virtual bool IsSelfAnimating() = 0;                                                                                                        // 106
    virtual void OnNewParticleEffect(const char* name, CParticleEffect* effect) = 0;                                                           // 107
    virtual void OnParticleEffectDeleted(CParticleEffect* effect) = 0;                                                                         // 108
    virtual void ResetIK() = 0;                                                                                                                // 109
    virtual int InterpolateFieldsInternal(float currentTime, const ClientInterpolationSnapshot* secondSnapshot, float secondSnapshotTime) = 0; // 110
  private:
    virtual void UnknownEntity111() = 0;

  public:
    virtual bool DidEntityTeleport() = 0;                                   // 112
    virtual bool IsSubModel() = 0;                                          // 113
    virtual int DrawBrushModel(const CViewSetup* view, int flags) = 0;      // 114
    virtual float GetTextureAnimationStartTime() = 0;                       // 115
    virtual void TextureAnimationWrapped() = 0;                             // 116
    virtual void SetNextClientThink(float nextThinkTime) = 0;               // 117
    virtual void SetHealth(int health) = 0;                                 // 118
    virtual C_TitanSoul* GetTitanSoul() const = 0;                          // 119
    virtual C_BaseCombatCharacter* TitanSoul_GetTitan() = 0;                // 120
    virtual bool IsPredictedProjectile() const = 0;                         // 121
    virtual void UpdateOnRemove() = 0;                                      // 122
    virtual void SUB_Remove() = 0;                                          // 123
    virtual CBaseHandle GetEntForDecal() = 0;                               // 124
    virtual C_Player* GetPredictionOwner() = 0;                             // 125
    virtual void InitPredictable(C_Player* owner, const char* context) = 0; // 126
    virtual void SetPredictable(bool predictable) = 0;                      // 127
    virtual bool PredictionErrorShouldResetLatchedForAllPredictables() = 0; // 128
    virtual bool PredictionIsPhysicallySimulated() = 0;                     // 129
    virtual void FindNonWorldTimeEntities() = 0;                            // 130
    virtual void OnPostRestoreData() = 0;                                   // 131
    virtual bool IsMechanical() const = 0;                                  // 132
    virtual bool HasGibModel() const = 0;                                   // 133
    virtual void DispatchImpactEffects(C_BaseEntity* hitEntity, const Vector3D& start, const Vector3D& end, const Vector3D& normal, short surfaceProp,
                                       int staticPropId, int damageType, int impactEffectTable, C_BaseEntity* otherEntity,
                                       unsigned int impactEffectFlags) = 0;         // 134
    virtual bool ShouldPredict() = 0;                                               // 135
    virtual void Think() = 0;                                                       // 136
    virtual bool PreRender() = 0;                                                   // 137
    virtual const char* GetClassname() const = 0;                                   // 138
    virtual bool IsCurrentlyTouching() const = 0;                                   // 139
    virtual void StartTouch(C_BaseEntity* other) = 0;                               // 140
    virtual void Touch(C_BaseEntity* other, const trace_t* trace) = 0;              // 141
    virtual void EndTouch(C_BaseEntity* other) = 0;                                 // 142
    virtual unsigned int PhysicsSolidMaskForEntity() const = 0;                     // 143
    virtual void ResolveFlyCollisionCustom(trace_t& trace, Vector3D& velocity) = 0; // 144
    virtual void PhysicsSimulate() = 0;                                             // 145
    virtual bool IsFuncBrush() const = 0;                                           // 146
    virtual bool IsMoving() = 0;                                                    // 147
    virtual bool ShouldRegenerateOriginFromCellBits() const = 0;                    // 148
    virtual bool IsPlayer() = 0;                                                    // 149
    virtual bool IsTitan() const = 0;                                               // 150
    virtual bool IsTitanSoul() = 0;                                                 // 151
    virtual bool IsOperator() const = 0;                                            // 152
    virtual bool IsHuman() const = 0;                                               // 153
    virtual bool IsZipline() const = 0;                                             // 154
    virtual bool IsBreakableGlass() = 0;                                            // 155
    virtual bool IsHologram() const = 0;                                            // 156
    virtual bool IsPlayerDecoy() = 0;                                               // 157
    virtual bool IsBaseCombatCharacter() = 0;                                       // 158
    virtual bool IsGrenade() = 0;                                                   // 159
    virtual C_BaseCombatCharacter* MyCombatCharacterPointer() = 0;                  // 160
    virtual C_Player* MyPlayerPointer() = 0;                                        // 161
    virtual C_ScriptProp* MyScriptPropPointer() = 0;                                // 162
    virtual C_Beam* MyBeamPointer() = 0;                                            // 163
    virtual bool IsNPC() = 0;                                                       // 164
    virtual bool IsPointCamera() = 0;                                               // 165
    virtual bool IsTurret() = 0;                                                    // 166
    virtual bool DisallowsGrapple() = 0;                                            // 167
    virtual C_DynamicProp* MyDynamicPropPointer() = 0;                              // 168
    virtual bool IsSprite() = 0;                                                    // 169
    virtual bool IsProp() = 0;                                                      // 170
    virtual bool IsWeaponX() const = 0;                                             // 171
    virtual bool IsCombatWeaponClone() = 0;                                         // 172
    virtual const C_WeaponX* MyWeaponXPointerConst() const = 0;                     // 173
    virtual C_WeaponX* MyWeaponXPointer() = 0;                                      // 174
    virtual const C_BaseCombatWeapon* MyCombatWeaponPointerConst() const = 0;       // 175
    virtual C_BaseCombatWeapon* MyCombatWeaponPointer() = 0;                        // 176
  private:
    virtual void UnknownEntity177() = 0;

  public:
    virtual bool IsProjectile() = 0;                                                                       // 178
    virtual bool IsGrapplingHook() = 0;                                                                    // 179
    virtual Vector3D EyePosition() = 0;                                                                    // 180
    virtual Vector3D PositionBetweenEyes() = 0;                                                            // 181
    virtual QAngle EyeAngles() = 0;                                                                        // 182
    virtual QAngle LocalEyeAngles() = 0;                                                                   // 183
    virtual Vector3D EarPosition() = 0;                                                                    // 184
    virtual bool ShouldCollideByVelocity(const Vector3D& velocity) const = 0;                              // 185
    virtual bool ShouldCollide(const C_BaseEntity* other, int collisionGroup, int contentsMask) const = 0; // 186
    virtual float GetGravity() const = 0;                                                                  // 187
    virtual void GetGroundVelocityToApply(Vector3D& velocity) = 0;                                         // 188
    virtual bool ShouldInterpolate() = 0;                                                                  // 189
    virtual void OnMoveParentRendered() = 0;                                                               // 190
    virtual bool IsCloaked(bool includeFade) = 0;                                                          // 191
    virtual bool DoesInheritParentCloak() = 0;                                                             // 192
    virtual bool ShouldPushPhasedEntities() const = 0;                                                     // 193
    virtual bool IsPhaseShifted() const = 0;                                                               // 194
    virtual const char* GetTitleForUI() const = 0;                                                         // 195
    virtual bool OnPredictedEntityRemove(bool isBeingRemoved, C_BaseEntity* predicted) = 0;                // 196
  private:
    virtual void UnknownEntity197() = 0;
    virtual void UnknownEntity198() = 0;
    virtual void UnknownEntity199() = 0;

  public:
    virtual int GetStudioBody() const = 0;                                                                                  // 200
    virtual void OnPositionChanged() = 0;                                                                                   // 201
    virtual void PerformCustomPhysics(Vector3D* position, Vector3D* velocity, QAngle* angles, QAngle* angularVelocity) = 0; // 202
    virtual bool CollisionExplodesMissiles() const = 0;                                                                     // 203
    virtual VisibleToLocalPlayerTrace* GetVisibileToPlayerLocalTrace() = 0;                                                 // 204

    // Source SDK accessors over the retail network/prediction state. Mutators
    // that require transform invalidation or networking remain native APIs.
    int GetHealth() const { return m_iHealth; }
    int GetMaxHealth() const { return m_iMaxHealth; }
    int GetFlags() const { return m_fFlags; }
    std::uint64_t GetEFlags() const { return m_iEFlags; }
    bool IsEFlagSet(std::uint64_t flags) const { return (m_iEFlags & flags) != 0; }
    int GetEffects() const { return m_fEffects; }
    bool IsEffectActive(int effects) const { return (m_fEffects & effects) != 0; }
    const Vector3D& GetLocalOrigin() const { return m_localOrigin; }
    const QAngle& GetLocalAngles() const { return m_localAngles; }
    const Vector3D& GetLocalVelocity() const { return m_vecVelocity; }
    const QAngle& GetLocalAngularVelocity() const { return m_vecAngVelocity; }
    const Vector3D& GetBaseVelocity() const { return m_vecBaseVelocity; }
    C_BaseEntity* GetMoveParent() const { return m_pMoveParent.Get(); }
    C_BaseEntity* FirstMoveChild() const { return m_pMoveChild.Get(); }
    C_BaseEntity* NextMovePeer() const { return m_pMovePeer.Get(); }
    C_BaseEntity* GetGroundEntity() const { return m_hGroundEntity.Get(); }
    int GetCollisionGroup() const { return m_CollisionGroup; }
    CCollisionProperty* CollisionProp() { return &m_Collision; }
    const CCollisionProperty* CollisionProp() const { return &m_Collision; }
    CParticleProperty* ParticleProp() { return &m_Particles; }
    const CParticleProperty* ParticleProp() const { return &m_Particles; }
    IPhysicsObject* VPhysicsGetObject() const { return m_pPhysicsObject; }
    bool IsClientOnly() const { return m_bClientOnly; }
    bool GetPredictable() const { return m_bPredictable; }
    bool IsPredictionEligible() const { return m_bPredictionEligible; }
    float GetAnimTime() const { return m_flAnimTime; }
    float GetFriction() const { return m_flFriction; }
    const char* GetEntityName() const { return m_iName; }
    const char* GetModelName() const { return m_ModelName; }
    const char* GetSignifierName() const { return m_iSignifierName; }
    const color32& GetRenderColor() const { return m_clrRender; }
    unsigned char GetRenderMode() const { return m_nRenderMode; }
    unsigned char GetRenderFX() const { return m_nRenderFX; }
    int GetSpawnFlags() const { return m_spawnflags; }
    bool HasSpawnFlags(int flags) const { return (m_spawnflags & flags) != 0; }

    int m_entIndex; // 0x30
    unsigned short m_EntClientFlags;
    const char* m_iClassname;
    const model_t* m_model;
    color32 m_clrRender;
    bool m_thinkNextFrame;
    std::uint64_t m_iEFlags;
    CScriptScope m_ScriptScope; // 0x58
    HSCRIPT__* m_hScriptInstance;
    const char* m_iszScriptId;
    bool m_heavyweightEnt;
    int m_fFlags;
    CurrentFrameData m_currentFrame; // 0x90
    LerpData m_lerpData;             // 0xC8
    CHandle<C_BaseEntity> m_pMoveParent;
    CHandle<C_BaseEntity> m_pPreRestoreDataMoveParent;
    int m_preRestoreDataParentAttachment;
    CHandle<C_Player> m_bossPlayer;
    QAngle m_vecAngVelocity;
    QAngle m_angAbsRotation;
    Vector3D m_vecAbsVelocity;
    Vector3D m_vecAbsOrigin;
    Vector3D m_localOrigin;
    QAngle m_localAngles;
    int m_shieldHealth;
    int m_shieldHealthMax;
    int m_oldShieldHealth;
    bool m_wasAliveLastUpdate;
    float m_timeOfDeathNoticedOnClient;
    bool m_shouldDeleteSelf;
    bool m_releaseInitiated;
    color32 m_lastNetworkedColor;
    unsigned char m_lastNetworkedRenderMode;
    int m_forceVisibilityBits;
    int m_forceInvisibilityBits;
    bool m_forceShadowVisibility;
    bool m_markedRecvPropChanged;
    bool m_moveParentChanged;
    bool m_inWater;
    bool m_noDrawCloak;
    float m_cloakEndTime;
    float m_cloakFadeInEndTime;
    float m_cloakFadeOutStartTime;
    float m_cloakFadeInDuration;
    float m_cloakFlickerAmount;
    float m_cloakFlickerEndTime;
    float m_cloakFadeOutDuration;
    bool m_highlightIsNetworked;
    Vector3D m_highlightParams[16];
    unsigned int m_highlightFunctionBits[8];
    unsigned int m_highlightFunctionBitsPrev[8];
    unsigned int m_highlightPlayerVisibilityBits[8];
    unsigned int m_highlightPlayerVisibilityBitsPrev[8];
    float m_highlightServerFadeBases[2];
    float m_highlightServerFadeStartTimes[2];
    float m_highlightServerFadeEndTimes[2];
    float m_highlightClientFadeBases[2];
    float m_highlightClientFadeStartTimes[2];
    float m_highlightClientFadeEndTimes[2];
    float m_highlightEndTime;
    float m_nextCheckHighlightChangedTime;
    float m_highlightFadeInTime;
    float m_highlightFadeOutTime;
    float m_highlightNearFadeDist;
    float m_highlightFarFadeDist;
    float m_highlightServerFadeEndTimesPrev[2];
    int m_highlightServerContextID;
    int m_highlightServerContextIDPrev;
    int m_highlightCurrentContextID;
    unsigned int m_highlightTeamBits;
    unsigned int m_highlightTeamBitsPrev;
    unsigned int m_highlightFlags;
    int m_highlightVisibilityType;
    AutoDeferredTraceHandle_Client_s m_highlightEntTrace;
    bool m_highlightIsVisible;
    unsigned int m_networkedFlags;
    unsigned int m_prevNetworkedFlags;
    Vector3D m_renderOriginTemp;
    int m_cellBits;
    int m_cellWidth;
    Vector3D m_vecPrevAbsOrigin;
    float m_flGravity;
    float m_flProxyRandomValue;
    Vector3D m_vecBaseVelocity;
    CHandle<C_BaseEntity> m_hGroundEntity;
    int m_iHealth;
    float m_flMaxspeed;
    int m_visibilityFlags;
    bool m_scriptVisible;
    int m_fEffects;
    int m_iTeamNum;
    int m_passThroughFlags;
    int m_passThroughThickness;
    float m_passThroughDirection;
    Vector3D m_deathVelocity;
    Vector3D m_vecVelocity;
    QAngle m_angNetworkAngles;
    float m_flFriction;
    bool m_bIsActiveChild;
    CHandle<C_BaseEntity> m_hOwnerEntity;
    bool m_bRenderWithViewModels;
    unsigned char m_nRenderFX;
    EntityFXData* m_pModelFX;
    bool m_bForceModelFXUpdate;
    unsigned char m_nRenderMode;
    unsigned char m_MoveType;
    unsigned char m_MoveCollide;
    float m_flAnimTime;
    CCollisionProperty m_Collision; // 0x3F8
    QAngle m_renderAnglesTemp;
    AbsHistory m_lastCalculatedAbs;
    bool m_lastCalculatedAbsValid;
    bool m_shouldInterpolateOrigin;
    bool m_shouldInterpolateAngles;
    bool m_bDormant;
    bool m_bCanUseBrushModelFastPath;
    bool m_bDoDestroyCallback;
    bool m_bPrevAbsOriginValid;
    bool m_collectingInvalidateFlags;
    int m_collectedInvalidateFlags;
    int m_nNextThinkTick;
    int m_iMaxHealth;
    const char* m_iSignifierName;
    bool m_bClientOnly;
    char m_iName[260];
    int m_scriptNameIndex;
    int m_instanceNameIndex;
    char m_scriptName[64];
    char m_instanceName[64];
    const char* m_holdUsePrompt;
    const char* m_pressUsePrompt;
    float m_nextVelocitySample;
    Vector3D m_velocitySamples[5];
    BASEPTR m_pfnThink;       // 0x698
    ENTITYFUNCPTR m_pfnTouch; // 0x6B0
    char m_lifeState;
    bool m_bFakeReleased;
    float m_flOldAnimTime;
    int m_oldVisibilityFlags;
    bool m_oldScriptVisible;
    bool m_hasRenderable;
    ClientRenderHandle_t m_renderHandle;
    unsigned int m_VisibilityBits;
    unsigned int m_ShadowVisibilityBits;
    int m_nLastThinkTick;
    bool m_forceVisibleInPhaseShift;
    unsigned char m_baseTakeDamage;
    unsigned int m_invulnerableToDamageCount;
    bool m_passDamageToParent;
    float m_flSpeed;
    C_PredictableId m_PredictableID;
    PredictionContext* m_pPredictionContext;
    int touchStamp;
    bool m_bEnabledInToolView;
    unsigned int m_ToolHandle;
    IPhysicsObject* m_pPhysicsObject;
    bool m_bPredictionEligible;
    int m_nSimulationTick;
    CUtlVector<thinkfunc_client_t> m_aThinkFunctions;
    int m_iCurrentThinkContext;
    int m_spawnflags;
    float m_entitySpawnTime;
    bool m_bDormantPredictable;
    int m_nIncomingPacketEntityBecameDormant;
    float m_flLastMessageTime;
    ModelInstanceHandle_t m_ModelInstance;
    ClientThinkHandle_t m_hThink;
    float m_oldTime;
    float m_newTime;
    float m_attachmentLerpStartTime;
    float m_attachmentLerpEndTime;
    Vector3D m_attachmentLerpStartOrigin;
    QAngle m_attachmentLerpStartAngles;
    int m_parentAttachmentType;
    int m_parentAttachmentIndex;
    int m_parentAttachmentHitbox;
    int m_parentAttachmentModel;
    bool m_bPredictable;
    float m_fadeDist;
    int m_nSplitUserPlayerPredictionSlot;
    CHandle<C_BaseEntity> m_pMoveChild;
    CHandle<C_BaseEntity> m_pMovePeer;
    CHandle<C_BaseEntity> m_pMovePrevPeer;
    CHandle<C_BaseEntity> m_hOldMoveParent;
    bool m_OldMoveParentExisted;
    bool m_MoveParentExisted;
    bool m_wasLocalViewPlayer;
    const char* m_ModelName;
    CParticleProperty m_Particles; // 0x7C0
    float m_flGroundChangeTime;
    QAngle m_vecOldAngRotation;
    matrix3x4_t m_rgflCoordinateFrame;
    int m_CollisionGroup;
    int m_contents;
    PredictedEntityData m_savedPredictionStates;
    unsigned char m_debugOverlays;
    int m_DataChangeEventRef;
    CHandle<C_BaseEntity> m_dissolveEffectEntityHandle;
    int m_fDataObjectTypes;
    int m_nCreationTick;
    int m_usableType;
    int m_usablePriority;
    float m_usableRadius;
    float m_usableFOV;
    float m_usePromptSize;
    unsigned short m_ListEntry[4];
    int m_spottedByTeams;
    MinimapBaseEntityData_Client m_minimapData; // 0x888
    int m_nameVisibilityFlags;
    std::uint64_t m_hitboxAttachmentChildCount;
    bool m_bMinimapEntity;
    bool m_bIsBlurred;
    float m_lastHistoryFixupForParentTime;
    float m_lastHistoryFixupForUnparentTime;
    C_BaseEntity* m_healthChangedNext;
    C_BaseEntity* m_healthChangedPrev;
    int m_iPrevHealth;
    bool m_bDoDeathCallback;
    bool m_bDoHealthChangeCallback;
    bool m_requestUpdateVisibility;
    matrix3x4_t m_prevMatrix;
    int m_childPusherMoveHandlerCount;
    CScriptNetDataList* m_scriptNetDataList;
    C_StatusEffectPlugin* m_statusEffectPlugin;
    int m_notifyCreationArrayIndex;
    bool m_debugBrokenInterpolation;
    bool m_clientSidePhysicsUpdate;
    std::byte m_reserved092E;
    bool m_areEntityLinksNetworked;
    int m_entitiesLinkedFromMeCount;
    int m_entitiesLinkedToMeCount;
    CHandle<C_BaseEntity> m_entitiesLinkedFromMe[64]; // 0x938
    CHandle<C_BaseEntity> m_entitiesLinkedToMe[64];   // 0xA38
};
