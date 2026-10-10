#include "client/animation.h"
#include "core/tier1.h"

InterpolatePoseParameters_t Animation_InterpolatePoseParameters;
CalculateCycleForAnimationStateAtTime_t Animation_CalculateCycleForAnimationStateAtTime;
Animation_ExtractBbox_t Animation_ExtractBbox;
Animation_IndexModelSequences_t Animation_IndexModelSequences;
Animation_ResetActivityIndexes_t Animation_ResetActivityIndexes;
Animation_VerifySequenceIndex_t Animation_VerifySequenceIndex;
Animation_SelectWeightedSequence_t Animation_SelectWeightedSequence;
Animation_SelectHeaviestSequence_t Animation_SelectHeaviestSequence;
Animation_SetEventIndexForSequence_t Animation_SetEventIndexForSequence;
Animation_BuildAllAnimationEventIndexes_t Animation_BuildAllAnimationEventIndexes;
Animation_ResetEventIndexes_t Animation_ResetEventIndexes;
Animation_GetEyePosition_t Animation_GetEyePosition;
Animation_LookupActivity_t Animation_LookupActivity;
Animation_LookupSequence_t Animation_LookupSequence;
Animation_GetSequenceLinearMotion_t Animation_GetSequenceLinearMotion;
Animation_GetSequenceName_t Animation_GetSequenceName;
Animation_GetSequenceActivityName_t Animation_GetSequenceActivityName;
Animation_GetSequenceFlags_t Animation_GetSequenceFlags;
Animation_GetAnimationEvent_t Animation_GetAnimationEvent;
Animation_HasAnimationEventOfType_t Animation_HasAnimationEventOfType;
Animation_FindTransitionSequence_t Animation_FindTransitionSequence;
Animation_GotoSequence_t Animation_GotoSequence;
Animation_SetBodygroup_t Animation_SetBodygroup;
Animation_GetBodygroup_t Animation_GetBodygroup;
Animation_GetBodygroupName_t Animation_GetBodygroupName;
Animation_GetBodygroupPartName_t Animation_GetBodygroupPartName;
Animation_FindBodygroupByName_t Animation_FindBodygroupByName;
Animation_GetBodygroupCount_t Animation_GetBodygroupCount;
Animation_GetSequenceActivity_t Animation_GetSequenceActivity;
Animation_GetAttachmentLocalSpace_t Animation_GetAttachmentLocalSpace;
Animation_FindHitboxSetByName_t Animation_FindHitboxSetByName;
Animation_GetHitboxSetName_t Animation_GetHitboxSetName;
Animation_GetHitboxSetCount_t Animation_GetHitboxSetCount;

void InterpolatePoseParameters(float fraction, int count, const float* first, const float* second, const bool* looping, float* result)
{
    Animation_InterpolatePoseParameters(fraction, count, first, second, looping, result);
}

float CalculateCycleForAnimationStateAtTime(C_BaseAnimating* entity, float time, float startCycle, float startTime, float playbackRate,
                                           unsigned int sequence, bool frozen, int* result)
{
    return Animation_CalculateCycleForAnimationStateAtTime(entity, time, startCycle, startTime, playbackRate, sequence, frozen, result);
}

int ExtractBbox(CStudioHdr* studioHdr, int sequence, Vector3D& mins, Vector3D& maxs)
{
    return Animation_ExtractBbox(studioHdr, sequence, mins, maxs);
}

void IndexModelSequences(CStudioHdr* studioHdr)
{
    Animation_IndexModelSequences(studioHdr);
}

void ResetActivityIndexes(CStudioHdr* studioHdr)
{
    Animation_ResetActivityIndexes(studioHdr);
}

void VerifySequenceIndex(CStudioHdr* studioHdr)
{
    Animation_VerifySequenceIndex(studioHdr);
}

int SelectWeightedSequence(CStudioHdr* studioHdr, int activity, bool useModifiers, const CUtlSymbol* activityModifiers, int modifierCount,
                           int currentSequence)
{
    return Animation_SelectWeightedSequence(studioHdr, activity, useModifiers, activityModifiers, modifierCount, currentSequence);
}

int SelectHeaviestSequence(CStudioHdr* studioHdr, int activity)
{
    return Animation_SelectHeaviestSequence(studioHdr, activity);
}

void SetEventIndexForSequence(mstudioseqdesc_t& sequence)
{
    Animation_SetEventIndexForSequence(sequence);
}

void BuildAllAnimationEventIndexes(CStudioHdr* studioHdr)
{
    Animation_BuildAllAnimationEventIndexes(studioHdr);
}

void ResetEventIndexes(CStudioHdr* studioHdr)
{
    Animation_ResetEventIndexes(studioHdr);
}

void GetEyePosition(CStudioHdr* studioHdr, Vector3D& eyePosition)
{
    Animation_GetEyePosition(studioHdr, eyePosition);
}

int LookupActivity(CStudioHdr* studioHdr, const char* label)
{
    return Animation_LookupActivity(studioHdr, label);
}

int LookupSequence(CStudioHdr* studioHdr, const char* label)
{
    return Animation_LookupSequence(studioHdr, label);
}

void GetSequenceLinearMotion(CStudioHdr* studioHdr, int sequence, const float poseParameters[], Vector3D* motion)
{
    Animation_GetSequenceLinearMotion(studioHdr, sequence, poseParameters, motion);
}

const char* GetSequenceName(CStudioHdr* studioHdr, int sequence)
{
    return Animation_GetSequenceName(studioHdr, sequence);
}

const char* GetSequenceActivityName(CStudioHdr* studioHdr, int sequence)
{
    return Animation_GetSequenceActivityName(studioHdr, sequence);
}

int GetSequenceFlags(CStudioHdr* studioHdr, int sequence)
{
    return Animation_GetSequenceFlags(studioHdr, sequence);
}

int GetAnimationEvent(CStudioHdr* studioHdr, int sequence, animevent_t* event, float start, float end, int index)
{
    return Animation_GetAnimationEvent(studioHdr, sequence, event, start, end, index);
}

bool HasAnimationEventOfType(CStudioHdr* studioHdr, int sequence, int type)
{
    return Animation_HasAnimationEventOfType(studioHdr, sequence, type);
}

int FindTransitionSequence(CStudioHdr* studioHdr, int currentSequence, int goalSequence, int* direction)
{
    return Animation_FindTransitionSequence(studioHdr, currentSequence, goalSequence, direction);
}

bool GotoSequence(CStudioHdr* studioHdr, int currentSequence, float currentCycle, float currentRate, int goalSequence, int& nextSequence,
                  float& nextCycle, int& nextDirection)
{
    return Animation_GotoSequence(studioHdr, currentSequence, currentCycle, currentRate, goalSequence, nextSequence, nextCycle, nextDirection);
}

void SetBodygroup(CStudioHdr* studioHdr, int& body, int group, int value)
{
    Animation_SetBodygroup(studioHdr, body, group, value);
}

int GetBodygroup(CStudioHdr* studioHdr, int body, int group)
{
    return Animation_GetBodygroup(studioHdr, body, group);
}

const char* GetBodygroupName(CStudioHdr* studioHdr, int group)
{
    return Animation_GetBodygroupName(studioHdr, group);
}

const char* GetBodygroupPartName(CStudioHdr* studioHdr, int group, int part)
{
    return Animation_GetBodygroupPartName(studioHdr, group, part);
}

int FindBodygroupByName(CStudioHdr* studioHdr, const char* name)
{
    return Animation_FindBodygroupByName(studioHdr, name);
}

int GetBodygroupCount(CStudioHdr* studioHdr, int group)
{
    return Animation_GetBodygroupCount(studioHdr, group);
}

int GetSequenceActivity(CStudioHdr* studioHdr, int sequence, int* weight)
{
    return Animation_GetSequenceActivity(studioHdr, sequence, weight);
}

void GetAttachmentLocalSpace(CStudioHdr* studioHdr, int attachment, matrix3x4_t& localToWorld)
{
    Animation_GetAttachmentLocalSpace(studioHdr, attachment, localToWorld);
}

int FindHitboxSetByName(CStudioHdr* studioHdr, const char* name)
{
    return Animation_FindHitboxSetByName(studioHdr, name);
}

const char* GetHitboxSetName(CStudioHdr* studioHdr, int set)
{
    return Animation_GetHitboxSetName(studioHdr, set);
}

int GetHitboxSetCount(CStudioHdr* studioHdr)
{
    return Animation_GetHitboxSetCount(studioHdr);
}

int SelectWeightedSequence(CStudioHdr* studioHdr, int activity, int currentSequence)
{
    return SelectWeightedSequence(studioHdr, activity, false, nullptr, 0, currentSequence);
}

int GetNumBodyGroups(CStudioHdr* studioHdr)
{
    return studioHdr ? studioHdr->numbodyparts() : 0;
}

ON_DLL_LOAD_CLIENT("client.dll", AnimationHelpers, [](CModule module)
{
    Animation_InterpolatePoseParameters = module.Offset(0xF4220).RCast<InterpolatePoseParameters_t>();
    Animation_CalculateCycleForAnimationStateAtTime = module.Offset(0xAAAA0).RCast<CalculateCycleForAnimationStateAtTime_t>();
    Animation_ExtractBbox = module.Offset(0xA2FC0).RCast<decltype(Animation_ExtractBbox)>();
    Animation_IndexModelSequences = module.Offset(0xA4170).RCast<decltype(Animation_IndexModelSequences)>();
    Animation_ResetActivityIndexes = module.Offset(0xA4650).RCast<decltype(Animation_ResetActivityIndexes)>();
    Animation_VerifySequenceIndex = module.Offset(0xA4EC0).RCast<decltype(Animation_VerifySequenceIndex)>();
    Animation_SelectWeightedSequence = module.Offset(0xA4860).RCast<decltype(Animation_SelectWeightedSequence)>();
    Animation_SelectHeaviestSequence = module.Offset(0xA4710).RCast<decltype(Animation_SelectHeaviestSequence)>();
    Animation_SetEventIndexForSequence = module.Offset(0xA4E00).RCast<decltype(Animation_SetEventIndexForSequence)>();
    Animation_BuildAllAnimationEventIndexes = module.Offset(0xA2DB0).RCast<decltype(Animation_BuildAllAnimationEventIndexes)>();
    Animation_ResetEventIndexes = module.Offset(0xA4670).RCast<decltype(Animation_ResetEventIndexes)>();
    Animation_GetEyePosition = module.Offset(0xA3780).RCast<decltype(Animation_GetEyePosition)>();
    Animation_LookupActivity = module.Offset(0xA4410).RCast<decltype(Animation_LookupActivity)>();
    Animation_LookupSequence = module.Offset(0xA4500).RCast<decltype(Animation_LookupSequence)>();
    Animation_GetSequenceLinearMotion = module.Offset(0xA3B60).RCast<decltype(Animation_GetSequenceLinearMotion)>();
    Animation_GetSequenceName = module.Offset(0xA3D10).RCast<decltype(Animation_GetSequenceName)>();
    Animation_GetSequenceActivityName = module.Offset(0xA39A0).RCast<decltype(Animation_GetSequenceActivityName)>();
    Animation_GetSequenceFlags = module.Offset(0xA3AF0).RCast<decltype(Animation_GetSequenceFlags)>();
    Animation_GetAnimationEvent = module.Offset(0xA3430).RCast<decltype(Animation_GetAnimationEvent)>();
    Animation_HasAnimationEventOfType = module.Offset(0xA4060).RCast<decltype(Animation_HasAnimationEventOfType)>();
    Animation_FindTransitionSequence = module.Offset(0xA3240).RCast<decltype(Animation_FindTransitionSequence)>();
    Animation_GotoSequence = module.Offset(0xA3DC0).RCast<decltype(Animation_GotoSequence)>();
    Animation_SetBodygroup = module.Offset(0xA4DB0).RCast<decltype(Animation_SetBodygroup)>();
    Animation_GetBodygroup = module.Offset(0xA3610).RCast<decltype(Animation_GetBodygroup)>();
    Animation_GetBodygroupName = module.Offset(0xA36B0).RCast<decltype(Animation_GetBodygroupName)>();
    Animation_GetBodygroupPartName = module.Offset(0xA36F0).RCast<decltype(Animation_GetBodygroupPartName)>();
    Animation_FindBodygroupByName = module.Offset(0xA3110).RCast<decltype(Animation_FindBodygroupByName)>();
    Animation_GetBodygroupCount = module.Offset(0xA3680).RCast<decltype(Animation_GetBodygroupCount)>();
    Animation_GetSequenceActivity = module.Offset(0xA38E0).RCast<decltype(Animation_GetSequenceActivity)>();
    Animation_GetAttachmentLocalSpace = module.Offset(0xA35E0).RCast<decltype(Animation_GetAttachmentLocalSpace)>();
    Animation_FindHitboxSetByName = module.Offset(0xA31B0).RCast<decltype(Animation_FindHitboxSetByName)>();
    Animation_GetHitboxSetName = module.Offset(0xA37D0).RCast<decltype(Animation_GetHitboxSetName)>();
    Animation_GetHitboxSetCount = module.Offset(0xA37B0).RCast<decltype(Animation_GetHitboxSetCount)>();
})
