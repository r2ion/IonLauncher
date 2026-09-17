#pragma once

#include <cstddef>
#include <cstdint>

#include "server/iserverentity.h"
#include "mathlib/vector.h"
#include "server/variant_t.h"
#include "server/datamap.h"
#include "gametrace.h"
#include "networkvar.h"

class CBaseAnimating;
class CBaseAnimatingOverlay;
class CBaseCombatCharacter;
class CBaseCombatWeapon;
class CWeaponX;
class CRopeKeyframe;
class CSkyCamera;
class CBeam;
class CSprite;
class IPhysicsObject;
class ISave;
class IRestore;
class CTakeDamageInfo;
class CBaseTrigger;
struct Ray_t;
struct gamevcollisionevent_t;
struct ServerClass;
struct ScriptClassDesc_t;

class CBaseEntity : public IServerEntity
{
  public:
    virtual ServerClass* GetServerClass() = 0; // 3
  protected:
    virtual void ReservedEntitySlot004() = 0; // 4
  public:
    virtual ServerDataMap* GetDataDescMap() = 0; // 5
    virtual ScriptClassDesc_t* GetScriptDesc() = 0; // 6
    virtual void PreNewModel() = 0; // 7
    virtual void PostNewModel() = 0; // 8
  protected:
    virtual void ReservedEntitySlot009() = 0; // 9
  public:
    virtual bool TestCollision(const Ray_t& ray, unsigned int contentsMask, trace_t& trace) = 0; // 10
    virtual bool TestHitboxes(const Ray_t& ray, unsigned int contentsMask, trace_t& trace) = 0; // 11
    virtual void ComputeWorldSpaceSurroundingBox(Vector3D* worldMins, Vector3D* worldMaxs) = 0; // 12
  protected:
    virtual void ReservedEntitySlot013() = 0; // 13
  public:
    virtual bool ShouldCollideByVelocity(const Vector3D& velocity) = 0; // 14
    virtual bool ShouldCollide(const CBaseEntity* other, int collisionGroup, int contentsMask) = 0; // 15
    virtual void SetOwnerEntity(CBaseEntity* owner) = 0; // 16
    virtual const CBaseEntity* GetOwnerEntityConst() const = 0; // 17
    virtual CBaseEntity* GetOwnerEntity() = 0; // 18
  protected:
    virtual void ReservedEntitySlot019() = 0; // 19
    virtual void ReservedEntitySlot020() = 0; // 20
    virtual void ReservedEntitySlot021() = 0; // 21
  public:
    virtual const char* GetTracerType() = 0; // 22
    virtual void Spawn() = 0; // 23
    virtual void Precache() = 0; // 24
    virtual void PostConstructor(const char* className) = 0; // 25
    virtual void PostClientActive() = 0; // 26
    virtual bool KeyValue(const char* key, const char* value) = 0; // 27
    virtual bool GetKeyValue(const char* key, char* value, int maxLength) = 0; // 28
    virtual void Activate() = 0; // 29
    virtual CBaseEntity* GetViewModelOwner() = 0; // 30
    virtual void SetParent(CBaseEntity* parent, int attachment) = 0; // 31
    virtual int ObjectCaps() = 0; // 32
    virtual bool AcceptInput(const char* inputName, CBaseEntity* activator, CBaseEntity* caller, variant_t value, int outputID) = 0; // 33
    virtual void DrawDebugGeometryOverlays() = 0; // 34
    virtual void DrawDebugTextOverlays() = 0; // 35
    virtual int Save(ISave& save) = 0; // 36
    virtual int Restore(IRestore& restore) = 0; // 37
    virtual bool ShouldSavePhysics() = 0; // 38
    virtual void OnSave() = 0; // 39
    virtual void OnRestore() = 0; // 40
  protected:
    virtual void ReservedEntitySlot041() = 0; // 41
  public:
    virtual int RequiredEdictIndex() = 0; // 42
    virtual void MoveDone() = 0; // 43
    virtual void Think() = 0; // 44
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_nNextThinkTick ); // 45-47
    virtual const CBaseAnimating* GetBaseAnimatingConst() const = 0; // 48
    virtual CBaseAnimating* GetBaseAnimating() = 0; // 49
    virtual const CBaseAnimatingOverlay* GetBaseAnimatingOverlayConst() const = 0; // 50
    virtual CBaseAnimatingOverlay* GetBaseAnimatingOverlay() = 0; // 51
    virtual CRopeKeyframe* GetRopeKeyframe() = 0; // 52
    virtual CSkyCamera* GetAsSkyCamera() = 0; // 53
    virtual int LookupAttachment(const char* name) = 0; // 54
    virtual bool IsFirstPersonProxy() const = 0; // 55
    virtual const CBaseTrigger* MyTriggerPointerConst() const = 0; // 56
    virtual CBaseTrigger* MyTriggerPointer() = 0; // 57
  protected:
    virtual void ReservedEntitySlot058() = 0; // 58
    virtual void ReservedEntitySlot059() = 0; // 59
  public:
    virtual int OnTakeDamage(const CTakeDamageInfo& info) = 0; // 60
    virtual void AdjustDamageDirection(Vector3D& direction) = 0; // 61
    virtual int TakeHealth(float health, int damageType) = 0; // 62
    virtual void Event_Killed(const CTakeDamageInfo& info) = 0; // 63
    virtual void Event_KilledOther(CBaseEntity* victim, const CTakeDamageInfo& info) = 0; // 64
  protected:
    virtual void ReservedEntitySlot065() = 0; // 65
    virtual void ReservedEntitySlot066() = 0; // 66
  public:
    virtual bool IsNPC() const = 0; // 67
    virtual const CBaseCombatCharacter* MyCombatCharacterPointerConst() const = 0; // 68
    virtual CBaseCombatCharacter* MyCombatCharacterPointer() = 0; // 69
    virtual const CWeaponX* MyWeaponXPointerConst() const = 0; // 70
    virtual CWeaponX* MyWeaponXPointer() = 0; // 71
  protected:
    virtual void ReservedEntitySlot072() = 0; // 72
    virtual void ReservedEntitySlot073() = 0; // 73
  public:
    virtual CBeam* MyBeamPointer() = 0; // 74
    virtual CSprite* MySpritePointer() = 0; // 75
    virtual float GetDelay() = 0; // 76
    virtual bool IsMoving() = 0; // 77
    virtual bool IsFuncBrush() const = 0; // 78
    virtual bool DisallowsGrapple() const = 0; // 79
    virtual bool IsMechanical() const = 0; // 80
  protected:
    virtual void ReservedEntitySlot081() = 0; // 81
  public:
    virtual bool HasTarget(const char* targetName) = 0; // 82
    virtual bool IsPlayer() const = 0; // 83
    virtual bool IsTitan() const = 0; // 84
    virtual bool IsTitanSoul() const = 0; // 85
    virtual bool IsOperator() const = 0; // 86
    virtual bool IsHuman() const = 0; // 87
  protected:
    virtual void ReservedEntitySlot088() = 0; // 88
  public:
    virtual bool IsRopeZipline() const = 0; // 89
    virtual bool IsBreakableGlass() const = 0; // 90
    virtual bool IsHologram() const = 0; // 91
    virtual bool IsPlayerDecoy() const = 0; // 92
    virtual bool IsNetClient() const = 0; // 93
  protected:
    virtual void ReservedEntitySlot094() = 0; // 94
  public:
    virtual bool IsProjectile() const = 0; // 95
    virtual bool IsGrenade() const = 0; // 96
    virtual bool IsGrapplingHook() const = 0; // 97
    virtual bool IsTurret() const = 0; // 98
  protected:
    virtual void ReservedEntitySlot099() = 0; // 99
  public:
    virtual bool IsBaseCombatWeapon() const = 0; // 100
    virtual const CBaseCombatWeapon* MyCombatWeaponPointerConst() const = 0; // 101
    virtual CBaseCombatWeapon* MyCombatWeaponPointer() = 0; // 102
  protected:
    virtual void ReservedEntitySlot103() = 0; // 103
  public:
    virtual bool ChangeTeam(int team) = 0; // 104
  protected:
    virtual void ReservedEntitySlot105() = 0; // 105
  public:
    virtual bool CanStandOn(CBaseEntity* surface) const = 0; // 106
    virtual const CBaseEntity* GetEnemyConst() const = 0; // 107
    virtual CBaseEntity* GetEnemy() = 0; // 108
  protected:
    virtual void ReservedEntitySlot109() = 0; // 109
  public:
    virtual void StartTouch(CBaseEntity* other) = 0; // 110
    virtual void Touch(CBaseEntity* other) = 0; // 111
    virtual void EndTouch(CBaseEntity* other) = 0; // 112
  protected:
    virtual void ReservedEntitySlot113() = 0; // 113
  public:
    virtual void Blocked(CBaseEntity* other) = 0; // 114
  protected:
    virtual void ReservedEntitySlot115() = 0; // 115
  public:
    virtual void PhysicsSimulate() = 0; // 116
    virtual void UpdateOnRemove() = 0; // 117
  protected:
    virtual void ReservedEntitySlot118() = 0; // 118
    virtual void ReservedEntitySlot119() = 0; // 119
    virtual void ReservedEntitySlot120() = 0; // 120
    virtual void ReservedEntitySlot121() = 0; // 121
    virtual void ReservedEntitySlot122() = 0; // 122
    virtual void ReservedEntitySlot123() = 0; // 123
    virtual void ReservedEntitySlot124() = 0; // 124
    virtual void ReservedEntitySlot125() = 0; // 125
    virtual void ReservedEntitySlot126() = 0; // 126
    virtual void ReservedEntitySlot127() = 0; // 127
    virtual void ReservedEntitySlot128() = 0; // 128
    virtual void ReservedEntitySlot129() = 0; // 129
    virtual void ReservedEntitySlot130() = 0; // 130
    virtual void ReservedEntitySlot131() = 0; // 131
    virtual void ReservedEntitySlot132() = 0; // 132
  public:
    virtual Vector3D EyePosition() = 0; // 133
    virtual Vector3D EarPosition() = 0; // 134
    virtual QAngle EyeAngles() = 0; // 135
    virtual QAngle LocalEyeAngles() = 0; // 136
  protected:
    virtual void ReservedEntitySlot137() = 0; // 137
    virtual void ReservedEntitySlot138() = 0; // 138
    virtual void ReservedEntitySlot139() = 0; // 139
  public:
    virtual void GetVectors(Vector3D* forward, Vector3D* right, Vector3D* up) const = 0; // 140
    virtual Vector3D GetSmoothedVelocity() = 0; // 141
    virtual void GetVelocity(Vector3D* velocity, Vector3D* angularVelocity) = 0; // 142
    virtual float GetGravity() const = 0; // 143
    virtual float GetFriction() const = 0; // 144
    virtual bool FVisiblePosition(const Vector3D& target, int traceMask, CBaseEntity** blocker) = 0; // 145
    virtual bool FVisible(CBaseEntity* entity, int traceMask, CBaseEntity** blocker) = 0; // 146
    virtual bool FVisibleFromPosition(const Vector3D& source, const Vector3D& target, int traceMask, CBaseEntity** blocker) = 0; // 147
  protected:
    virtual void ReservedEntitySlot148() = 0; // 148
  public:
    virtual void GetGroundVelocityToApply(Vector3D& velocity) = 0; // 149
    virtual Vector3D Script_GetBoundingMins() = 0; // 150
    virtual Vector3D Script_GetBoundingMaxs() = 0; // 151
    virtual const Vector3D& WorldSpaceCenter() const = 0; // 152
  protected:
    virtual void ReservedEntitySlot153() = 0; // 153
    virtual void ReservedEntitySlot154() = 0; // 154
    virtual void ReservedEntitySlot155() = 0; // 155
    virtual void ReservedEntitySlot156() = 0; // 156
  public:
    virtual void VPhysicsDestroyObject() = 0; // 157
    virtual void VPhysicsSwapObject(IPhysicsObject* physics) = 0; // 158
    virtual void VPhysicsUpdate(IPhysicsObject* physics) = 0; // 159
    virtual int VPhysicsTakeDamage(const CTakeDamageInfo& info) = 0; // 160
  protected:
    virtual void ReservedEntitySlot161() = 0; // 161
    virtual void ReservedEntitySlot162() = 0; // 162
  public:
    virtual void VPhysicsCollision(int index, gamevcollisionevent_t* event) = 0; // 163
  protected:
    virtual void ReservedEntitySlot164() = 0; // 164
  public:
    virtual void UpdatePhysicsShadowToCurrentPosition(float deltaTime) = 0; // 165
    virtual int VPhysicsGetObjectList(IPhysicsObject** objects, int maxCount) = 0; // 166
  protected:
    virtual void ReservedEntitySlot167() = 0; // 167
    virtual void ReservedEntitySlot168() = 0; // 168
  public:
    virtual unsigned int PhysicsSolidMaskForEntity() const = 0; // 169
  protected:
    virtual void ReservedEntitySlot170() = 0; // 170
    virtual void ReservedEntitySlot171() = 0; // 171
    virtual void ReservedEntitySlot172() = 0; // 172
  public:
    virtual void ResolveFlyCollisionCustom(trace_t& trace, Vector3D& velocity) = 0; // 173
  protected:
    virtual void ReservedEntitySlot174() = 0; // 174
    virtual void ReservedEntitySlot175() = 0; // 175
    virtual void ReservedEntitySlot176() = 0; // 176
    virtual void ReservedEntitySlot177() = 0; // 177
  public:
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_nameVisibilityFlags ); // 178-180
  protected:
    virtual void ReservedEntitySlot181() = 0; // 181
  public:
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_cloakEndTime ); // 182-184
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_cloakFadeInEndTime ); // 185-187
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_cloakFadeOutStartTime ); // 188-190
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_cloakFadeInDuration ); // 191-193
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_cloakFlickerAmount ); // 194-196
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_cloakFlickerEndTime ); // 197-199
    virtual bool IsCloaked(bool includeFlicker) = 0; // 200
    virtual bool DoesInheritParentCloak() = 0; // 201
  protected:
    virtual void ReservedEntitySlot202() = 0; // 202
  public:
    virtual bool IsPhaseShifted() = 0; // 203
  protected:
    virtual void ReservedEntitySlot204() = 0; // 204
  public:
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_fFlags ); // 205-207
    NETWORK_VAR_CHANGE_CALLBACKS( m_hGroundEntity ); // 208-209
    NETWORK_VAR_CHANGE_CALLBACKS( m_vecBaseVelocity ); // 210-211
    NETWORK_VAR_CHANGE_CALLBACKS( m_vecAbsVelocity ); // 212-213
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_flFriction ); // 214-216
    NETWORK_VAR_CHANGE_CALLBACKS( m_vecVelocity ); // 217-218
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_iMaxHealth ); // 219-221
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_iHealth ); // 222-224
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_bClientSideRagdoll ); // 225-227
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_lifeState ); // 228-230
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_takedamage ); // 231-233
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_deathVelocity ); // 234-236
    DISABLE_NETWORK_VAR_FOR_DERIVED( m_flMaxspeed ); // 237-239
    NETWORK_VAR_CHANGE_CALLBACKS( m_vecViewOffset ); // 240-241
  protected:
    virtual void ReservedEntitySlot242() = 0; // 242
    virtual void ReservedEntitySlot243() = 0; // 243
    virtual void ReservedEntitySlot244() = 0; // 244
  public:


    std::byte m_Reserved0010[0x48];
    uint32_t m_nPlayerIndex; // 0x58
    char _unk_0x5c[572];
    int32_t m_fFlags; // 0x298
    char _unk_0x29c[376];
    int32_t m_hGroundEntity; // 0x414
    char _unk_0x418[120];
    Vector3D m_vecAbsOrigin; // 0x490
    char _unk_0x49c[52];
    int32_t m_iMaxHealth; // 0x4D0
    int32_t m_iHealth;    // 0x4D4
    char _unk_0x4d8[25];
    std::uint8_t m_lifeState; // 0x4F1
    char _unk_0x4f2[26];
    float m_flMaxspeed; // 0x50C
    std::byte m_Reserved0510[0x4D0];
};

static_assert(sizeof(CBaseEntity) == 0x9E0);
static_assert(alignof(CBaseEntity) == 0x8);
static_assert(offsetof(CBaseEntity, m_nPlayerIndex) == 0x58);
static_assert(offsetof(CBaseEntity, m_fFlags) == 0x298);
static_assert(offsetof(CBaseEntity, m_hGroundEntity) == 0x414);
static_assert(offsetof(CBaseEntity, m_vecAbsOrigin) == 0x490);
static_assert(offsetof(CBaseEntity, m_iHealth) == 0x4D4);
static_assert(offsetof(CBaseEntity, m_lifeState) == 0x4F1);
static_assert(offsetof(CBaseEntity, m_flMaxspeed) == 0x50C);
