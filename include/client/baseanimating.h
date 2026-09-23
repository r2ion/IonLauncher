#pragma once

#include "client/animation.h"
#include "client/baseentity.h"
#include "client/bone_accessor.h"
#include "client/sequence_transitioner.h"
#include "datacache/imdlcache.h"
#include "engine/deferredtrace.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlvector.h"

class CStudioHdr;
class IBoneSetup;
class CBoneBitList;
class C_ClientRagdoll;
struct BoneList;
class CRagdoll;
class CIKContext;
class CBoneMergeCache;
class CJiggleBones;
class C_RopeKeyframe;

struct ClientModelRenderInfo_t;
struct DrawModelState_t;
struct KeyHitboxes
{
    int m_keyHitboxes[9];
    int m_keyHitboxesCacheModelIndex;
};

struct AnimatingData
{
    unsigned int nextHandle;
    unsigned int thisHandle;
    float animStartTime;
    float animStartCycle;
    float animPlaybackRate;
    bool animFrozen;
    int animModelIndex;
    int animSequence;
    int animSequenceParity;
    float m_flPoseParameters[24];
    bool clientSideRagdoll;
    Vector3D ragdollForce;
    float m_flEstIkOffset;
};

struct RagdollInfo_t
{
    bool m_bActive;
    float m_flSaveTime;
    int m_nNumBones;
    Vector3D m_rgBonePos[256];
    Quaternion m_rgBoneQuaternion[256];
};

struct CAttachmentData
{
    matrix3x4a_t m_AttachmentToWorld;
    QAngle m_angRotation;
    Vector3D m_vOriginVelocity;
    unsigned int m_nLastFramecount : 31;
    unsigned char m_bAnglesComputed : 1;
};

class C_BaseAnimating : public C_BaseEntity
{
  public:
    virtual unsigned int BuildTransformations(const CStudioHdr* hdr, Vector3D* positions, Quaternion* rotations, Vector3D* scales,
                                              const matrix3x4_t& cameraTransform, const BoneList* boneList, CBoneBitList& boneComputed) = 0; // 205
    virtual void UpdateIKLocks(float currentTime) = 0;                                                                                       // 206
    virtual void CalculateIKLocks(float currentTime) = 0;                                                                                    // 207
    virtual void DoInternalDrawModel(ClientModelRenderInfo_t* info, DrawModelState_t* state, matrix3x4_t* boneToWorld) = 0; // 208
    virtual const matrix3x4_t* GetHandIKOffset() const = 0;       // 209
    virtual void DoAnimationEventsHdr(const CStudioHdr* hdr) = 0; // 210
    virtual void FireEvent(const Vector3D& origin, const QAngle& angles, int event, const char* options, const float* cycleTime,
                           const int* animationLayerIndex) = 0; // 211
    virtual void AnimEventScriptCallback(const char* name) = 0; // 212
    virtual void StandardBlendingRules(const CStudioHdr* hdr, Vector3D* positions, Quaternion* rotations, Vector3D* scales, float currentTime,
                                       const BoneList* boneList) = 0;                                                                          // 213
    virtual void AccumulateLayers(IBoneSetup& boneSetup, Vector3D* positions, Quaternion* rotations, Vector3D* scales, float currentTime) = 0; // 214
    virtual Vector3D GetThirdPersonViewPosition() = 0;                                                                                         // 215
    virtual bool IsClientRagdoll() const = 0;                                                                                                  // 216
    virtual C_BaseAnimating* BecomeRagdollOnClient() = 0;                                                                                      // 217
    virtual C_ClientRagdoll* CreateClientRagdoll(bool restoring) = 0;                                                                          // 218
    virtual void SaveRagdollInfo(int boneCount, const matrix3x4_t& parentTransform, C_BoneAccessor& boneAccessor) = 0;                         // 219
    virtual bool RetrieveRagdollInfo(Vector3D* positions, Quaternion* rotations) = 0;                                                          // 220
    virtual void GetRagdollInitBoneArrays(matrix3x4_t* previousBones, matrix3x4_t* nextBones, matrix3x4_t* currentBones, float boneDt) = 0;    // 221
    virtual void ClearAnimationSoundsAndEffects(const CStudioHdr* hdr, int sequence, float cycleBegin) = 0;                                    // 222
    virtual void StudioFrameAdvance() = 0;                                                                                                     // 223
    virtual void UpdateClientSideAnimation() = 0;                                                                                              // 224
    virtual unsigned int ComputeClientSideAnimationFlags() = 0;                                                                                // 225
    virtual bool IsActivityFinished() = 0;            // 226
    virtual bool ForbidThreadedBoneSetup() const = 0; // 227
    virtual bool ShouldFlipViewModel() = 0; // 228
    virtual C_BaseAnimating* GetBoneSetupDependency() = 0;                                                // 229
    virtual void ClearPredictedAnimEvents() = 0;                                                          // 230
    virtual void ExecPredictedAnimEvent(int event, const char* options) = 0;                              // 231
    virtual void OnScriptAnimStart(int sequence) = 0;                                                     // 232
    virtual bool Weapon_ShouldSmartAmmoLockOn(C_BaseEntity* target, C_WeaponX* weapon, int lockType) = 0; // 233
    virtual void FormatViewModelAttachment(int attachment, matrix3x4_t& matrix) = 0;                      // 234
    virtual bool IsMenuModel() const = 0;                                                                 // 235
    virtual bool CalcAttachments() = 0; // 236
    virtual float LastBoneChangedTime() = 0; // 237
    virtual bool UpdateBlending(int flags, const RenderableInstance_t& instance) = 0; // 238

    CStudioHdr* GetModelPtr() const;
    int GetSequence() const { return m_currentFrameBaseAnimating.animSequence; }
    float GetCycle() const { return m_currentFrame.animCycle; }
    float GetPlaybackRate() const { return m_currentFrameBaseAnimating.animPlaybackRate; }
    int GetHitboxSet() const { return m_nHitboxSet; }
    bool IsSequenceFinished() const { return m_bSequenceFinished; }
    bool IsSequenceLooping() const { return m_bSequenceLoops; }
    bool IsRagdoll() const { return m_currentFrameBaseAnimating.clientSideRagdoll; }
    float GetSequenceCycleRate(int sequence);
    float GetSequenceCycleRateForModel(const CStudioHdr* hdr, int sequence) const;
    bool IsValidSequence(int sequence) const;
    bool IsSequenceLooping(const CStudioHdr* hdr, int sequence) const;
    static float ClampCycle(float cycle, bool isLooping);
    float SequenceDuration(int sequence);
    float SequenceDuration() { return SequenceDuration(GetSequence()); }
    bool IsModelScaled() const { return m_flModelScale != 1.0f; }
    bool IsModelScaleFractional() const { return m_flModelScale < 1.0f; }
    const C_BoneAccessor& GetBoneAccessor() const { return m_BoneAccessor; }
    C_BoneAccessor& GetBoneAccessor() { return m_BoneAccessor; }
    bool GetAttachment(const char* name, Vector3D& origin)
    {
        return GetAttachmentOrigin(LookupAttachment(name), origin);
    }
    bool GetAttachment(const char* name, Vector3D& origin, QAngle& angles)
    {
        return GetAttachmentOriginAngles(LookupAttachment(name), origin, angles);
    }
    using IClientRenderable::GetAttachment;

    Vector3D m_animClientInitialPos;    // 0xB38
    Quaternion m_animClientInitialRot;  // 0xB44
    float m_animClientInitialBeginTime; // 0xB54
    int m_groundEffectTableIdx;         // 0xB58
    bool m_cycleChanged;                // 0xB5C
    bool m_bAnimationChanged;
    bool m_bSequenceChanged;
    bool m_bCanSkipAnimDetails;
    int m_animNetworkFlags;   // 0xB60
    bool m_networkAnimActive; // 0xB64
    bool m_oldNetworkAnimActive;
    bool m_animActive;
    bool m_animCollisionEnabled;
    bool m_animInitialCorrection;
    bool m_animPaused;
    AnimRelativeData_Client m_networkedAnimRelativeData; // 0xB6C
    AnimRelativeData_Client m_animRelativeData;          // 0xBE4
    CHandle<C_BaseEntity> m_syncingWithEntity;           // 0xC5C
    PredictedAnimEventData m_predictedAnimEventData;     // 0xC60
    CRagdoll* m_pRagdoll;                                // 0xCC0
    CHandle<C_BaseEntity> m_pClientsideRagdoll;
    int m_nRagdollImpactFXTableId;
    float m_flSkyScaleStartValue; // 0xCD0
    float m_flSkyScaleEndValue;
    float m_flSkyScaleStartTime;
    float m_flSkyScaleEndTime;
    bool m_bClientSkyScale;
    float m_flClientSkyScaleStartValue;
    float m_flClientSkyScaleEndValue;
    float m_flClientSkyScaleStartTime;
    float m_flClientSkyScaleEndTime;
    int m_nHitboxSet;
    C_SequenceTransitioner m_SequenceTransitioner; // 0xCF8
    KeyHitboxes m_keyHitboxes[4];                  // 0xEB0
    float m_correctionFinishTime;                  // 0xF50
    Vector3D m_originCorrectionOffset;
    Vector3D m_originCorrectionRate;
    Quaternion m_quatCorrectionOffset;
    int m_prevSequence; // 0xF7C
    int m_nSkin;
    int m_nBody;
    int m_camoIndex;
    int m_decalIndex;
    int m_nResetEventsParity; // 0xF90
    int m_prevResetEventsParity;
    CIKContext* m_pIk; // 0xF98
    int m_ikPrevSequence;
    float m_flEstIkOffset;
    int m_weaponHandAttachment;
    bool m_bStoreRagdollInfo;
    RagdollInfo_t* m_pRagdollInfo; // 0xFB0
    int m_nForceBone;
    unsigned int m_iMostRecentModelBoneCounter;
    unsigned int m_iMostRecentBoneSetupRequest;
    std::uint64_t m_lastSetupDuration; // 0xFC8
    unsigned int m_iMostRecentNonThreadedBoneSetupRequest; // 0xFD0
    C_BaseAnimating* m_pNextForThreadedBoneSetup;          // 0xFD8
    unsigned int m_iMostRecentAccumBoneCounter;
    int m_iPrevAccumulatedBoneMask;
    int m_iAccumulatedBoneMask;
    C_BoneAccessor m_BoneAccessor; // 0xFF0
    bool m_settingUpBones;         // 0x1010
    volatile bool m_allowBoneSetup;
    unsigned int m_ClientSideAnimationListHandle;
    bool m_bCanUseFastPath; // 0x1018
    bool m_bCanUseFastPathFromServer;
    float m_flGroundSpeed;
    bool m_bSequenceFinished; // 0x1020
    bool m_useLockedAnimDeltaYaw;
    float m_lockedAnimDeltaYaw;
    bool m_bSequenceLoops;
    float m_flModelScale;
    AnimatingData m_currentFrameBaseAnimating;               // 0x1030
    int m_restoreSequence;                                   // 0x10C8
    CUtlLinkedList<C_RopeKeyframe*, unsigned short> m_Ropes; // 0x10D0
    float m_flPrevEventCycle;                                // 0x1100
    float m_prevEventProcessTime;
    int m_prevEventSequence;
    int m_numPoseParameters;
    bool m_bClientSideAnimation;
    Vector3D m_vecPreRagdollMins;
    Vector3D m_vecPreRagdollMaxs;
    bool m_builtRagdoll; // 0x112C
    bool m_bIsStaticProp;
    float m_cockpitOpenTime; // 0x1130
    float m_cockpitCloseTime;
    const char* m_footStepCustomType;
    AutoDeferredTraceHandle_Client_s m_footstepTrace; // 0x1140
    int m_footstepAttachmentL;
    int m_footstepAttachmentR;
    int m_footstepAttachmentFL;
    int m_footstepAttachmentFR;
    float m_flOldCycle;
    float m_prevClientCycle;
    float m_prevClientAnimTime;
    bool m_prevAnimIsActive; // 0x1160
    bool m_killRagdoll;
    std::byte m_alignment1162;
    unsigned char m_bTeamChanged : 1; // 0x1163
    unsigned char m_bCheckBodyGroupChange : 1;
    unsigned char m_bBodyGroupChanged : 1;
    unsigned char m_bBodyGroupTransferred : 1;
    unsigned char m_bDoBodyGroupChangeScriptCB : 1;
    unsigned char m_bDoModelChangeScriptCB : 1;
    int m_nScriptCBForBodyGroup; // 0x1164
    int m_iPrevTeam;
    int m_nPrevBody;
    int m_nPrevSkin;
    float m_flOldModelScale;
    int m_nOldSequence;
    CBoneMergeCache* m_pBoneMergeCache; // 0x1180
    CUtlVector<matrix3x4a_t, CUtlMemoryAligned<matrix3x4a_t, 16>> m_CachedBoneData;
    float m_flLastBoneSetupTime; // 0x11A8
    CJiggleBones* m_pJiggleBones;
    bool m_isJiggleBonesEnabled;
    CUtlVector<CAttachmentData> m_Attachments;       // 0x11C0
    CUtlVector<CAttachmentData> m_hitboxAttachments; // 0x11E0
    bool m_hitboxAttachmentsAreValid;
    bool m_bInitModelEffects;
    CStudioHdr* m_pStudioHdr; // 0x1208
    MDLHandle_t m_hStudioHdr;
    Vector3D m_vecRenderOriginOverride;
    int m_fireAttachmentSmartAmmoIndex; // 0x1220
    int m_fireAttachmentChestFocusIndex;
    int m_fireAttachmentModelIndex;
};
