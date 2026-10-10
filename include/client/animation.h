#pragma once

#include "engine/ehandle.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "studio.h"
#include "tier1/utlsymbol.h"
#include <cstddef>

class C_BaseAnimating;
struct datamap_t;

#define ACTIVITY_NOT_AVAILABLE -1
#define NOMOTION 99999

struct animevent_t
{
    union
    {
        int event;
        std::uint16_t event_newsystem;
    };
    const char* options;
    float cycle;
    float eventtime;
    int type;
    C_BaseAnimating* pSource;

    int Event() const { return (type & 0x400) ? event_newsystem : event; }
    void SetEvent(int value)
    {
        if (type & 0x400)
            event_newsystem = static_cast<std::uint16_t>(value);
        else
            event = value;
    }
};

int ExtractBbox(CStudioHdr* studioHdr, int sequence, Vector3D& mins, Vector3D& maxs);
void IndexModelSequences(CStudioHdr* studioHdr);
void ResetActivityIndexes(CStudioHdr* studioHdr);
void VerifySequenceIndex(CStudioHdr* studioHdr);
int SelectWeightedSequence(CStudioHdr* studioHdr, int activity, int currentSequence = -1);
int SelectWeightedSequence(CStudioHdr* studioHdr, int activity, bool useModifiers,
    const CUtlSymbol* activityModifiers, int modifierCount, int currentSequence = -1);
int SelectHeaviestSequence(CStudioHdr* studioHdr, int activity);
void SetEventIndexForSequence(mstudioseqdesc_t& sequence);
void BuildAllAnimationEventIndexes(CStudioHdr* studioHdr);
void ResetEventIndexes(CStudioHdr* studioHdr);
void GetEyePosition(CStudioHdr* studioHdr, Vector3D& eyePosition);
int LookupActivity(CStudioHdr* studioHdr, const char* label);
int LookupSequence(CStudioHdr* studioHdr, const char* label);
void GetSequenceLinearMotion(CStudioHdr* studioHdr, int sequence, const float poseParameters[], Vector3D* motion);
const char* GetSequenceName(CStudioHdr* studioHdr, int sequence);
const char* GetSequenceActivityName(CStudioHdr* studioHdr, int sequence);
int GetSequenceFlags(CStudioHdr* studioHdr, int sequence);
int GetAnimationEvent(CStudioHdr* studioHdr, int sequence, animevent_t* event, float start, float end, int index);
bool HasAnimationEventOfType(CStudioHdr* studioHdr, int sequence, int type);
int FindTransitionSequence(CStudioHdr* studioHdr, int currentSequence, int goalSequence, int* direction);
bool GotoSequence(CStudioHdr* studioHdr, int currentSequence, float currentCycle, float currentRate,
    int goalSequence, int& nextSequence, float& nextCycle, int& nextDirection);
void SetBodygroup(CStudioHdr* studioHdr, int& body, int group, int value);
int GetBodygroup(CStudioHdr* studioHdr, int body, int group);
const char* GetBodygroupName(CStudioHdr* studioHdr, int group);
const char* GetBodygroupPartName(CStudioHdr* studioHdr, int group, int part);
int FindBodygroupByName(CStudioHdr* studioHdr, const char* name);
int GetBodygroupCount(CStudioHdr* studioHdr, int group);
int GetNumBodyGroups(CStudioHdr* studioHdr);
int GetSequenceActivity(CStudioHdr* studioHdr, int sequence, int* weight = nullptr);
void GetAttachmentLocalSpace(CStudioHdr* studioHdr, int attachment, matrix3x4_t& localToWorld);
int FindHitboxSetByName(CStudioHdr* studioHdr, const char* name);
const char* GetHitboxSetName(CStudioHdr* studioHdr, int set);
int GetHitboxSetCount(CStudioHdr* studioHdr);
void InterpolatePoseParameters(float fraction, int count, const float* first, const float* second, const bool* looping, float* result);
float CalculateCycleForAnimationStateAtTime(C_BaseAnimating* entity, float time, float startCycle, float startTime, float playbackRate,
                                           unsigned int sequence, bool frozen, int* result);

using InterpolatePoseParameters_t = decltype(&InterpolatePoseParameters);
using CalculateCycleForAnimationStateAtTime_t = decltype(&CalculateCycleForAnimationStateAtTime);
using Animation_ExtractBbox_t = decltype(&ExtractBbox);
using Animation_IndexModelSequences_t = decltype(&IndexModelSequences);
using Animation_ResetActivityIndexes_t = decltype(&ResetActivityIndexes);
using Animation_VerifySequenceIndex_t = decltype(&VerifySequenceIndex);
using Animation_SelectWeightedSequence_t = int (*)(CStudioHdr*, int, bool, const CUtlSymbol*, int, int);
using Animation_SelectHeaviestSequence_t = decltype(&SelectHeaviestSequence);
using Animation_SetEventIndexForSequence_t = decltype(&SetEventIndexForSequence);
using Animation_BuildAllAnimationEventIndexes_t = decltype(&BuildAllAnimationEventIndexes);
using Animation_ResetEventIndexes_t = decltype(&ResetEventIndexes);
using Animation_GetEyePosition_t = decltype(&GetEyePosition);
using Animation_LookupActivity_t = decltype(&LookupActivity);
using Animation_LookupSequence_t = decltype(&LookupSequence);
using Animation_GetSequenceLinearMotion_t = decltype(&GetSequenceLinearMotion);
using Animation_GetSequenceName_t = decltype(&GetSequenceName);
using Animation_GetSequenceActivityName_t = decltype(&GetSequenceActivityName);
using Animation_GetSequenceFlags_t = decltype(&GetSequenceFlags);
using Animation_GetAnimationEvent_t = decltype(&GetAnimationEvent);
using Animation_HasAnimationEventOfType_t = decltype(&HasAnimationEventOfType);
using Animation_FindTransitionSequence_t = decltype(&FindTransitionSequence);
using Animation_GotoSequence_t = decltype(&GotoSequence);
using Animation_SetBodygroup_t = decltype(&SetBodygroup);
using Animation_GetBodygroup_t = decltype(&GetBodygroup);
using Animation_GetBodygroupName_t = decltype(&GetBodygroupName);
using Animation_GetBodygroupPartName_t = decltype(&GetBodygroupPartName);
using Animation_FindBodygroupByName_t = decltype(&FindBodygroupByName);
using Animation_GetBodygroupCount_t = decltype(&GetBodygroupCount);
using Animation_GetSequenceActivity_t = decltype(&GetSequenceActivity);
using Animation_GetAttachmentLocalSpace_t = decltype(&GetAttachmentLocalSpace);
using Animation_FindHitboxSetByName_t = decltype(&FindHitboxSetByName);
using Animation_GetHitboxSetName_t = decltype(&GetHitboxSetName);
using Animation_GetHitboxSetCount_t = decltype(&GetHitboxSetCount);

struct AnimRelativeData_Client
{
    Vector3D m_animInitialPos;
    Vector3D m_animInitialVel;
    Quaternion m_animInitialRot;
    Vector3D m_animInitialCorrectPos;
    Quaternion m_animInitialCorrectRot;
    Vector3D m_animEntityToRefOffset;
    Quaternion m_animEntityToRefRotation;
    float m_animBlendBeginTime;
    float m_animBlendEndTime;
    int m_animScriptSequence;
    int m_animScriptModel;
    bool m_animIgnoreParentRot;
    enum ScriptAnimMotionMode
    {
        SCRIPT_ANIM_USE_ROOT_MOTION,
        SCRIPT_ANIM_USE_ANIMATED_REF_ATTACHMENT
    } m_animMotionMode;
};

struct PredictedAnimEventData
{
    virtual datamap_t* GetPredDescMap();
    virtual const datamap_t* GetPredDescMap() const;
    float m_predictedAnimEventTimes[8];
    int m_predictedAnimEventIndices[8];
    int m_predictedAnimEventCount;
    CHandle<C_BaseAnimating> m_predictedAnimEventTarget;
    int m_predictedAnimEventSequence;
    int m_predictedAnimEventModel;
    float m_predictedAnimEventsReadyToFireTime;
};
