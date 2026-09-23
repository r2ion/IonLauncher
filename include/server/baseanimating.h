#pragma once

#include "server/baseentity.h"

class CStudioHdr;
class IBoneSetup;
struct Quaternion;
enum class eSmartAmmoLockType : int;
struct animevent_t;
struct matrix3x4_t;
struct RecordedAnimFrame;
struct RecordedAnimLayerFrame;

class CBaseAnimating : public CBaseEntity
{
  public:
    ServerClass* GetServerClass() override = 0; // 3
    ServerDataMap* GetDataDescMap() override = 0; // 5
    ScriptClassDesc_t* GetScriptDesc() override = 0; // 6
    void DrawDebugTextOverlays() override = 0; // 35
    int Restore(IRestore& restore) override = 0; // 37
    void OnRestore() override = 0; // 40
    const CBaseAnimating* GetBaseAnimatingConst() const override = 0; // 48
    CBaseAnimating* GetBaseAnimating() override = 0; // 49
    void GetVelocity(Vector3D* velocity, Vector3D* angularVelocity) override = 0; // 142
    virtual float GetIdealSpeed() const = 0; // 245
    virtual float GetIdealAccel() const = 0; // 246
    virtual void StudioFrameAdvance() = 0; // 247
    virtual bool IsActivityFinished() const = 0; // 248
    virtual float GetSequenceGroundSpeed(CStudioHdr* studioHdr, int sequence) = 0; // 249
    virtual bool Weapon_ShouldSmartAmmoLockOn(CBaseEntity* attacker, CWeaponX* weapon, eSmartAmmoLockType lockType) = 0; // 250
    virtual bool BecomeRagdollOnClient(const Vector3D& force) = 0; // 251
    virtual bool IsRagdoll() = 0; // 252
    virtual bool CanBecomeRagdoll() = 0; // 253
    virtual bool BecomeRagdoll(const CTakeDamageInfo& info, const Vector3D& force) = 0; // 254
    virtual void GetBoneTransform(int bone, matrix3x4_t& transform) = 0; // 255
    virtual void SetupBones(matrix3x4_t* boneToWorld, int boneMask, int boneCacheFlags) = 0; // 256
    virtual void DispatchAnimEvents(CBaseAnimating* eventHandler) = 0; // 257
    virtual void HandleAnimEvent(animevent_t* event) = 0; // 258
    virtual void AnimEventScriptCallback(const char* options) = 0; // 259
  protected:
    virtual void PopulatePoseParameters() = 0; // 260
    virtual void AccumulateLayers(IBoneSetup* boneSetup, Vector3D* positions, Quaternion* rotations, Vector3D* scales, float currentTime) = 0; // 261
  public:
    virtual bool GetAttachment(int attachment, matrix3x4_t& transform) = 0; // 262
    virtual void OnScriptAnimStart(int sequence) = 0; // 263
    virtual void OnRecordedAnimationEnded() = 0; // 264
    virtual void UpdateRecordedAnimation() = 0;  // 265
    virtual void ApplyRecordedAnimationLayers(const RecordedAnimFrame* frame, const RecordedAnimLayerFrame* layerFrames) = 0; // 266
    virtual void InitBoneControllers() = 0; // 267
    virtual Vector3D GetGroundSpeedVelocity() = 0; // 268
    virtual void InvalidateBoneCache() = 0; // 269
    virtual bool IsViewModel() = 0; // 270
    virtual bool ForbidThreadedBoneSetup() const = 0; // 271
    virtual void ClearPredictedAnimEvents() = 0; // 272
    virtual void ExecPredictedAnimEvent(int event, const char* options) = 0; // 273

    std::byte m_Reserved09E0[0x24];
    int32_t m_camoIndex;  // 0xA04
    int32_t m_decalIndex; // 0xA08
    std::byte m_Reserved0A0C[0x4AC];
};

static_assert(sizeof(CBaseAnimating) == 0xEB8);
static_assert(offsetof(CBaseAnimating, m_camoIndex) == 0xA04);
static_assert(offsetof(CBaseAnimating, m_decalIndex) == 0xA08);
