#pragma once

#include "client/baseanimatingoverlay.h"

class C_WeaponX;
class C_GrappleHook;
struct GrappleData_Client;

class C_BaseCombatCharacter : public C_BaseAnimatingOverlay
{
    virtual const C_BaseCombatCharacter* MyCombatCharacterPointerConst() const = 0; // 239
    virtual float ScriptGetAttackSpreadAngle() = 0;                                 // 240
    virtual bool Weapon_Switch(C_WeaponX* weapon) = 0;                                 // 241
    virtual bool Weapon_CanSwitchTo(C_WeaponX* weapon, bool checkAmmo) = 0;            // 242
    virtual bool Weapon_IsPlaying3pEquipActivity() = 0;                                // 243
    virtual bool Weapon_IsPlaying3pReloadActivity() = 0;                               // 244
    virtual const char* GetWeaponClass() const = 0;                                    // 245
    virtual void OnChangeActiveWeapon(C_WeaponX* oldWeapon, C_WeaponX* newWeapon) = 0; // 246
    virtual void UpdateWeaponPoseParameterValues() = 0;                         // 247
    virtual void Weapon_StartGestureAnim(int activity, float duration, bool autokill) = 0; // 248
    virtual void Weapon_EndGestureAnim(int activity, float blendOut) = 0;       // 249
    virtual void GrappleDetach() = 0;                                           // 250
    virtual bool IsGrappleActive() const = 0;                                   // 251
    virtual GrappleData_Client* GetGrappleData() = 0;                           // 252
    virtual C_GrappleHook* GetGrappleHook() = 0;                                // 253
    virtual float GetHullWidth() const = 0;                                     // 254
    virtual float GetHullHeight() const = 0;                                    // 255
    virtual bool PlayerMelee_ExecutionStartAttacker(float expectedEndTime) = 0; // 256
    virtual bool PlayerMelee_ExecutionStartTarget(C_BaseEntity* attacker) = 0;  // 257
    virtual bool PlayerMelee_ExecutionEndAttacker() = 0;                        // 258
    virtual bool PlayerMelee_ExecutionEndTarget() = 0;                          // 259
    virtual Vector3D ScriptGetPlayerOrNPCViewVector() = 0;                      // 260
    virtual Vector3D ScriptGetPlayerOrNPCViewForward() = 0;                     // 261
    virtual Vector3D ScriptGetPlayerOrNPCViewUp() = 0;                          // 262
    virtual Vector3D ScriptGetPlayerOrNPCViewRight() = 0;                       // 263
    virtual void PrintInventory() = 0;                                          // 264
};
