#pragma once

#include <cstddef>
#include <cstdint>

#include "basecombatweapon_shared.h"
#include "engine/ehandle.h"
#include "server/baseanimating.h"
#include "server/entityoutput.h"
#include "server/weapon_parse.h"

class CBaseCombatCharacter;
class IPhysicsConstraint;

class CBaseCombatWeapon : public CBaseAnimating
{
  public:
    ServerClass* GetServerClass() override = 0;                                                                              // 3
    int YouForgotToImplementOrDeclareServerClass() override = 0;                                                             // 4
    ServerDataMap* GetDataDescMap() override = 0;                                                                            // 5
    ScriptClassDesc_t* GetScriptDesc() override = 0;                                                                         // 6
    void PostNewModel() override = 0;                                                                                        // 8
    int UpdateTransmitState() override = 0;                                                                                  // 20
    const char* GetTracerType() override = 0;                                                                                // 22
    void Spawn() override = 0;                                                                                               // 23
    void Precache() override = 0;                                                                                            // 24
    void Activate() override = 0;                                                                                            // 29
    void SetParent(CBaseEntity* parent, int attachment) override = 0;                                                        // 31
    int ObjectCaps() override = 0;                                                                                           // 32
    DISABLE_NETWORK_VAR_FOR_DERIVED(m_nNextThinkTick);                                                                       // 45-47
    bool IsBaseCombatWeapon() const override = 0;                                                                            // 100
    const CBaseCombatWeapon* MyCombatWeaponPointerConst() const override = 0;                                                // 101
    CBaseCombatWeapon* MyCombatWeaponPointer() override = 0;                                                                 // 102
    void Use(CBaseEntity* activator, CBaseEntity* caller, int useType) override = 0;                                         // 109
    CBaseEntity* Respawn() override = 0;                                                                                     // 124
    bool FVisible(CBaseEntity* entity, int traceMask, CBaseEntity** blocker) override = 0;                                   // 146
    bool CreateVPhysics() override = 0;                                                                                      // 156
    CBasePlayer* HasPhysicsAttacker(float timeLimit) override = 0;                                                           // 168
    void PerformCustomPhysics(Vector3D* position, Vector3D* velocity, QAngle* angles, QAngle* angularVelocity) override = 0; // 174
    virtual void OnPickedUp(CBaseCombatCharacter* newOwner) = 0;                                                             // 274

    WEAPON_FILE_INFO_HANDLE GetWeaponFileInfoHandle() const
    {
        return m_weaponInfoFileHandle;
    }
    const FileWeaponInfo_Server& GetWpnData() const
    {
        return *GetFileWeaponInfoFromHandle_Server(GetWeaponFileInfoHandle());
    }
    CBaseCombatCharacter* GetWeaponOwner() const
    {
        return m_weaponOwner.Get();
    }
    bool IsReloading() const
    {
        return m_bInReload;
    }
    int GetWeaponActivity() const
    {
        return m_weaponActivity;
    }
    float GetWeaponIdleTime() const
    {
        return m_flTimeWeaponIdle;
    }
    void SetWeaponIdleTime(float time)
    {
        m_flTimeWeaponIdle = time;
    }
    std::uint32_t GetWeaponPrimaryClipCount() const
    {
        return m_ammoInClip;
    }
    std::uint32_t GetWeaponPrimaryAmmoCount() const
    {
        return m_ammoInStockpile;
    }
    WeaponActiveState_e GetActiveState() const
    {
        return m_ActiveState;
    }

    CHandle<CBaseCombatCharacter> m_weaponOwner;         // 0xEB8
    CHandle<CBaseCombatCharacter> m_weaponOwnerPrevious; // 0xEBC
    bool m_weaponOwnerPreviousWasNPC;                    // 0xEC0
    std::byte _padEC1[0x3];                              // 0xEC1
    float m_nextReadyTime;                               // 0xEC4
    float m_nextPrimaryAttackTime;                       // 0xEC8
    int m_iWorldModelIndex;                              // 0xECC
    int m_holsterModelIndex;                             // 0xED0
    int m_droppedModelIndex;                             // 0xED4
    WeaponActiveState_e m_ActiveState;                   // 0xED8
    std::uint32_t m_ammoInClip;                          // 0xEDC
    std::uint32_t m_ammoInStockpile;                     // 0xEE0
    int m_lifetimeShots;                                 // 0xEE4
    float m_flTimeWeaponIdle;                            // 0xEE8
    CHandle<CBaseEntity> m_physicsAttacker;              // 0xEEC
    int m_projectileModelIndex;                          // 0xEF0
    float m_lastPrimaryAttack;                           // 0xEF4
    float m_flNextEmptySoundTime;                        // 0xEF8
    int m_weaponActivity;                                // 0xEFC
    bool m_bRemoveable;                                  // 0xF00
    bool m_bInReload;                                    // 0xF01
    std::byte _padF02[0x2];                              // 0xF02
    int m_nIdealSequence;                                // 0xF04
    int m_IdealActivity;                                 // 0xF08
    int m_ownerMuzzleAttachment;                         // 0xF0C
    WEAPON_FILE_INFO_HANDLE m_weaponInfoFileHandle;      // 0xF10
    std::byte _padF12[0x6];                              // 0xF12
    IPhysicsConstraint* m_pConstraint;                   // 0xF18
    COutputEvent m_OnPlayerUse;                          // 0xF20
    COutputEvent m_OnPlayerPickup;                       // 0xF48
    COutputEvent m_OnNPCPickup;                          // 0xF70
    COutputEvent m_OnCacheInteraction;                   // 0xF98
};

static_assert(sizeof(CBaseCombatWeapon) == 0xFC0);
