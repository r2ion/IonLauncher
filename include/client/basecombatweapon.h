#pragma once

#include "basecombatweapon_shared.h"
#include "client/baseanimating.h"
#include "client/weapon_parse.h"
#include "engine/ehandle.h"
#include "vscript/ivscript.h"

#include <cstddef>
#include <cstdint>

class C_BaseCombatCharacter;
class C_CombatWeaponClone;
class IPhysicsConstraint;

class C_BaseCombatWeapon : public C_BaseAnimating
{
  public:
    virtual void OnPickedUp(C_BaseCombatCharacter* newOwner) = 0; // 239

    WEAPON_FILE_INFO_HANDLE GetWeaponFileInfoHandle() const
    {
        return m_weaponInfoFileHandle;
    }
    const FileWeaponInfo_Client& GetWpnData() const
    {
        return *GetFileWeaponInfoFromHandle_Client(GetWeaponFileInfoHandle());
    }
    C_BaseCombatCharacter* GetWeaponOwner() const
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
    unsigned int GetWeaponPrimaryClipCount() const
    {
        return m_ammoInClip;
    }
    unsigned int GetWeaponPrimaryAmmoCount() const
    {
        return m_ammoInStockpile;
    }
    WeaponActiveState_e GetActiveState() const
    {
        return m_ActiveState;
    }

    ScriptStringOrNull GetPrintName() const;
    ScriptStringOrNull GetWeaponDescription() const;
    void Script_SetDroppedModel(const char* modelName);
    int ScriptLookupWorldModelAttachment(const char* attachmentName);
    int ScriptLookupViewModelAttachment(const char* attachmentName);

    CHandle<C_BaseCombatCharacter> m_oldWeaponOwner; // 0x1230
    CHandle<C_BaseCombatCharacter> m_weaponOwner;
    float m_nextReadyTime;
    float m_nextPrimaryAttackTime;
    int m_iWorldModelIndex; // 0x1240
    int m_holsterModelIndex;
    int m_droppedModelIndex;
    WeaponActiveState_e m_ActiveState;
    unsigned int m_ammoInClip; // 0x1250
    unsigned int m_ammoInStockpile;
    int m_lifetimeShots;
    float m_flTimeWeaponIdle;
    int m_projectileModelIndex; // 0x1260
    float m_lastPrimaryAttack;
    float m_flNextEmptySoundTime;
    int m_weaponActivity;
    bool m_bRemoveable; // 0x1270
    bool m_bInReload;
    int m_nIdealSequence;
    int m_IdealActivity;
    int m_ownerMuzzleAttachment;
    WEAPON_FILE_INFO_HANDLE m_weaponInfoFileHandle; // 0x1280
    IPhysicsConstraint* m_pConstraint;              // 0x1288
    Vector3D m_cachedAttachmentOrigin;              // 0x1290
    QAngle m_cachedAttachmentAngles;
    WeaponActiveState_e m_OldActiveState;    // 0x12A8
    C_CombatWeaponClone* m_pWorldModelClone; // 0x12B0
};
