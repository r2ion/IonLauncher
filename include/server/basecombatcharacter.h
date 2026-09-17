#pragma once

#include "server/baseanimatingoverlay.h"

struct impactdamagetable_t;
struct SQVM;

class CBaseCombatCharacter : public CBaseAnimatingOverlay
{
  public:
    ServerClass* GetServerClass() override = 0; // 3
    ServerDataMap* GetDataDescMap() override = 0; // 5
    ScriptClassDesc_t* GetScriptDesc() override = 0; // 6
    void Spawn() override = 0; // 23
    int OnTakeDamage(const CTakeDamageInfo& info) override = 0; // 60
    int TakeHealth(float health, int damageType) override = 0; // 62
    void Event_Killed(const CTakeDamageInfo& info) override = 0; // 63
    int Restore(IRestore& restore) override = 0; // 37
    const CBaseCombatCharacter* MyCombatCharacterPointerConst() const override = 0; // 68
    CBaseCombatCharacter* MyCombatCharacterPointer() override = 0; // 69
    void PhysicsSimulate() override = 0; // 116
    void UpdateOnRemove() override = 0; // 117
    void HandleAnimEvent(animevent_t* event) override = 0; // 258
    virtual const impactdamagetable_t& GetPhysicsImpactDamageTable() = 0; // 274
    virtual bool FInViewCone(const Vector3D& point) = 0; // 275
    virtual bool FInViewConeEntity(CBaseEntity* entity) = 0; // 276
    virtual bool FInAimCone(const Vector3D& point) = 0; // 277
    virtual bool FInAimConeEntity(CBaseEntity* entity) = 0; // 278
  protected:
    virtual void ReservedEntitySlot279() = 0; // 279
  public:
    virtual QAngle BodyAngles() const = 0; // 280
    virtual Vector3D BodyDirection2D() const = 0; // 281
    virtual Vector3D BodyDirection3D() const = 0; // 282
    virtual Vector3D HeadDirection2D() = 0; // 283
    virtual Vector3D HeadDirection3D() = 0; // 284
    virtual Vector3D EyeDirection2D() = 0; // 285
    virtual Vector3D EyeDirection3D() = 0; // 286
    virtual int NPC_TranslateActivity(int activity) = 0; // 287
    virtual float ScriptGetAttackSpreadAngle() = 0; // 288
    virtual void ScriptGiveExistingWeapon(CBaseEntity* weapon) = 0; // 289
  protected:
    virtual void ReservedEntitySlot290() = 0; // 290
    virtual void ReservedEntitySlot291() = 0; // 291
  public:
    virtual bool Weapon_Detach(CBaseCombatWeapon* weapon) = 0; // 292
    virtual bool Weapon_Switch(CBaseCombatWeapon* weapon) = 0; // 293
    virtual bool Weapon_CanSwitchTo(CBaseCombatWeapon* weapon, bool checkDeploy) = 0; // 294
    virtual bool Weapon_IsPlaying3pEquipActivity() const = 0; // 295
    virtual bool Weapon_IsPlaying3pReloadActivity() const = 0; // 296
    virtual Vector3D Weapon_ShootPosition() = 0; // 297
    virtual const char* GetWeaponClass() const = 0; // 298
    virtual void SetActiveWeapon(CBaseCombatWeapon* weapon) = 0; // 299
    virtual void OnChangeActiveWeapon(CBaseCombatWeapon* oldWeapon, CBaseCombatWeapon* newWeapon) = 0; // 300
  protected:
    virtual void ReservedEntitySlot301() = 0; // 301
  public:
    virtual void Weapon_StartGestureAnim(int activity, float duration, bool autokill) = 0; // 302
    virtual void Weapon_EndGestureAnim(int activity, float fadeOut) = 0; // 303
    virtual void ScriptTakeWeapon(const char* weaponName) = 0; // 304
    virtual void ScriptTakeWeaponNow(const char* weaponName) = 0; // 305
    virtual Vector3D ScriptGetPlayerOrNPCViewVector() = 0; // 306
    virtual Vector3D ScriptGetPlayerOrNPCViewForward() = 0; // 307
    virtual Vector3D ScriptGetPlayerOrNPCViewUp() = 0; // 308
    virtual Vector3D ScriptGetPlayerOrNPCViewRight() = 0; // 309
    virtual void SetOutOfBoundsDeadTime(float time) = 0; // 310
    virtual float GetOutOfBoundsDeadTime() = 0; // 311
    virtual int ScriptGiveWeapon(SQVM* vm) = 0; // 312
  protected:
    virtual void ReservedEntitySlot313() = 0; // 313
  public:
    virtual void GrappleDetach() = 0; // 314
    virtual int OnTakeDamage_Alive(const CTakeDamageInfo& info) = 0; // 315
    virtual int OnTakeDamage_Dying(const CTakeDamageInfo& info) = 0; // 316
    virtual int OnTakeDamage_Dead(const CTakeDamageInfo& info) = 0; // 317
    virtual Vector3D CalcDamageForceVector(const CTakeDamageInfo& info) = 0; // 318
  protected:
    virtual void ReservedEntitySlot319() = 0; // 319
  public:
    virtual void Event_Dying() = 0; // 320
  protected:
    virtual void ReservedEntitySlot321() = 0; // 321
  public:
    virtual CBaseEntity* CheckTraceHullAttackFromTo(const Vector3D& start, const Vector3D& end, const Vector3D& mins, const Vector3D& maxs, float damage, int damageType, float forceScale, bool damageAnyNPC) = 0; // 322
    virtual CBaseEntity* CheckTraceHullAttack(float distance, const Vector3D& mins, const Vector3D& maxs, float damage, int damageType, float forceScale, bool damageAnyNPC) = 0; // 323
    virtual float GetHullWidth() const = 0; // 324
    virtual float GetHullHeight() const = 0; // 325
  protected:
    virtual void ReservedEntitySlot326() = 0; // 326
    virtual void ReservedEntitySlot327() = 0; // 327
    virtual void ReservedEntitySlot328() = 0; // 328
    virtual void ReservedEntitySlot329() = 0; // 329
    virtual void ReservedEntitySlot330() = 0; // 330
    virtual void ReservedEntitySlot331() = 0; // 331
    virtual void ReservedEntitySlot332() = 0; // 332
    virtual void ReservedEntitySlot333() = 0; // 333
    virtual void ReservedEntitySlot334() = 0; // 334
    virtual void ReservedEntitySlot335() = 0; // 335
  public:
    virtual bool PlayerMelee_ExecutionStartAttacker(float duration) = 0; // 336
    virtual bool PlayerMelee_ExecutionStartTarget(CBaseEntity* attacker) = 0; // 337
    virtual bool PlayerMelee_ExecutionEndAttacker() = 0; // 338
    virtual bool PlayerMelee_ExecutionEndTarget() = 0; // 339
  protected:
    virtual void ReservedEntitySlot340() = 0; // 340
  public:
    virtual void Event_LeechStart() = 0; // 341
    virtual void Event_LeechEnd() = 0; // 342

    CBaseEntity* GetActiveWeapon();
    bool IsWeaponHolstered();
    void HolsterWeapon();

    std::byte m_Reserved1040[0x314];
    int32_t m_selectedOffhand;                    // 0x1354
    int32_t m_selectedOffhandPendingHybridAction; // 0x1358
    char _unk_0x135c[92];
    int32_t m_titanSoul; // 0x13B8
    std::byte m_Reserved13BC[0xDC];
};

static_assert(sizeof(CBaseCombatCharacter) == 0x1498);
static_assert(offsetof(CBaseCombatCharacter, m_selectedOffhand) == 0x1354);
static_assert(offsetof(CBaseCombatCharacter, m_selectedOffhandPendingHybridAction) == 0x1358);
static_assert(offsetof(CBaseCombatCharacter, m_titanSoul) == 0x13B8);
