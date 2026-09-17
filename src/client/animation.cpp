#include "client/animation.h"
#include "core/tier1.h"

int (*s_ExtractBbox)(CStudioHdr* studioHdr, int sequence, Vector3D& mins, Vector3D& maxs);
void (*s_IndexModelSequences)(CStudioHdr* studioHdr);
void (*s_ResetActivityIndexes)(CStudioHdr* studioHdr);
void (*s_VerifySequenceIndex)(CStudioHdr* studioHdr);
int (*s_SelectWeightedSequence)(CStudioHdr* studioHdr, int activity, bool useModifiers, const CUtlSymbol* activityModifiers, int modifierCount,
                                       int currentSequence);
int (*s_SelectHeaviestSequence)(CStudioHdr* studioHdr, int activity);
void (*s_SetEventIndexForSequence)(mstudioseqdesc_t& sequence);
void (*s_BuildAllAnimationEventIndexes)(CStudioHdr* studioHdr);
void (*s_ResetEventIndexes)(CStudioHdr* studioHdr);
void (*s_GetEyePosition)(CStudioHdr* studioHdr, Vector3D& eyePosition);
int (*s_LookupActivity)(CStudioHdr* studioHdr, const char* label);
int (*s_LookupSequence)(CStudioHdr* studioHdr, const char* label);
void (*s_GetSequenceLinearMotion)(CStudioHdr* studioHdr, int sequence, const float poseParameters[], Vector3D* motion);
const char* (*s_GetSequenceName)(CStudioHdr* studioHdr, int sequence);
const char* (*s_GetSequenceActivityName)(CStudioHdr* studioHdr, int sequence);
int (*s_GetSequenceFlags)(CStudioHdr* studioHdr, int sequence);
int (*s_GetAnimationEvent)(CStudioHdr* studioHdr, int sequence, animevent_t* event, float start, float end, int index);
bool (*s_HasAnimationEventOfType)(CStudioHdr* studioHdr, int sequence, int type);
int (*s_FindTransitionSequence)(CStudioHdr* studioHdr, int currentSequence, int goalSequence, int* direction);
bool (*s_GotoSequence)(CStudioHdr* studioHdr, int currentSequence, float currentCycle, float currentRate, int goalSequence, int& nextSequence,
                              float& nextCycle, int& nextDirection);
void (*s_SetBodygroup)(CStudioHdr* studioHdr, int& body, int group, int value);
int (*s_GetBodygroup)(CStudioHdr* studioHdr, int body, int group);
const char* (*s_GetBodygroupName)(CStudioHdr* studioHdr, int group);
const char* (*s_GetBodygroupPartName)(CStudioHdr* studioHdr, int group, int part);
int (*s_FindBodygroupByName)(CStudioHdr* studioHdr, const char* name);
int (*s_GetBodygroupCount)(CStudioHdr* studioHdr, int group);
int (*s_GetSequenceActivity)(CStudioHdr* studioHdr, int sequence, int* weight);
void (*s_GetAttachmentLocalSpace)(CStudioHdr* studioHdr, int attachment, matrix3x4_t& localToWorld);
int (*s_FindHitboxSetByName)(CStudioHdr* studioHdr, const char* name);
const char* (*s_GetHitboxSetName)(CStudioHdr* studioHdr, int set);
int (*s_GetHitboxSetCount)(CStudioHdr* studioHdr);

int ExtractBbox(CStudioHdr* studioHdr, int sequence, Vector3D& mins, Vector3D& maxs)
{
    return s_ExtractBbox(studioHdr, sequence, mins, maxs);
}

void IndexModelSequences(CStudioHdr* studioHdr)
{
    s_IndexModelSequences(studioHdr);
}

void ResetActivityIndexes(CStudioHdr* studioHdr)
{
    s_ResetActivityIndexes(studioHdr);
}

void VerifySequenceIndex(CStudioHdr* studioHdr)
{
    s_VerifySequenceIndex(studioHdr);
}

int SelectWeightedSequence(CStudioHdr* studioHdr, int activity, bool useModifiers, const CUtlSymbol* activityModifiers, int modifierCount,
                           int currentSequence)
{
    return s_SelectWeightedSequence(studioHdr, activity, useModifiers, activityModifiers, modifierCount, currentSequence);
}

int SelectHeaviestSequence(CStudioHdr* studioHdr, int activity)
{
    return s_SelectHeaviestSequence(studioHdr, activity);
}

void SetEventIndexForSequence(mstudioseqdesc_t& sequence)
{
    s_SetEventIndexForSequence(sequence);
}

void BuildAllAnimationEventIndexes(CStudioHdr* studioHdr)
{
    s_BuildAllAnimationEventIndexes(studioHdr);
}

void ResetEventIndexes(CStudioHdr* studioHdr)
{
    s_ResetEventIndexes(studioHdr);
}

void GetEyePosition(CStudioHdr* studioHdr, Vector3D& eyePosition)
{
    s_GetEyePosition(studioHdr, eyePosition);
}

int LookupActivity(CStudioHdr* studioHdr, const char* label)
{
    return s_LookupActivity(studioHdr, label);
}

int LookupSequence(CStudioHdr* studioHdr, const char* label)
{
    return s_LookupSequence(studioHdr, label);
}

void GetSequenceLinearMotion(CStudioHdr* studioHdr, int sequence, const float poseParameters[], Vector3D* motion)
{
    s_GetSequenceLinearMotion(studioHdr, sequence, poseParameters, motion);
}

const char* GetSequenceName(CStudioHdr* studioHdr, int sequence)
{
    return s_GetSequenceName(studioHdr, sequence);
}

const char* GetSequenceActivityName(CStudioHdr* studioHdr, int sequence)
{
    return s_GetSequenceActivityName(studioHdr, sequence);
}

int GetSequenceFlags(CStudioHdr* studioHdr, int sequence)
{
    return s_GetSequenceFlags(studioHdr, sequence);
}

int GetAnimationEvent(CStudioHdr* studioHdr, int sequence, animevent_t* event, float start, float end, int index)
{
    return s_GetAnimationEvent(studioHdr, sequence, event, start, end, index);
}

bool HasAnimationEventOfType(CStudioHdr* studioHdr, int sequence, int type)
{
    return s_HasAnimationEventOfType(studioHdr, sequence, type);
}

int FindTransitionSequence(CStudioHdr* studioHdr, int currentSequence, int goalSequence, int* direction)
{
    return s_FindTransitionSequence(studioHdr, currentSequence, goalSequence, direction);
}

bool GotoSequence(CStudioHdr* studioHdr, int currentSequence, float currentCycle, float currentRate, int goalSequence, int& nextSequence,
                  float& nextCycle, int& nextDirection)
{
    return s_GotoSequence(studioHdr, currentSequence, currentCycle, currentRate, goalSequence, nextSequence, nextCycle, nextDirection);
}

void SetBodygroup(CStudioHdr* studioHdr, int& body, int group, int value)
{
    s_SetBodygroup(studioHdr, body, group, value);
}

int GetBodygroup(CStudioHdr* studioHdr, int body, int group)
{
    return s_GetBodygroup(studioHdr, body, group);
}

const char* GetBodygroupName(CStudioHdr* studioHdr, int group)
{
    return s_GetBodygroupName(studioHdr, group);
}

const char* GetBodygroupPartName(CStudioHdr* studioHdr, int group, int part)
{
    return s_GetBodygroupPartName(studioHdr, group, part);
}

int FindBodygroupByName(CStudioHdr* studioHdr, const char* name)
{
    return s_FindBodygroupByName(studioHdr, name);
}

int GetBodygroupCount(CStudioHdr* studioHdr, int group)
{
    return s_GetBodygroupCount(studioHdr, group);
}

int GetSequenceActivity(CStudioHdr* studioHdr, int sequence, int* weight)
{
    return s_GetSequenceActivity(studioHdr, sequence, weight);
}

void GetAttachmentLocalSpace(CStudioHdr* studioHdr, int attachment, matrix3x4_t& localToWorld)
{
    s_GetAttachmentLocalSpace(studioHdr, attachment, localToWorld);
}

int FindHitboxSetByName(CStudioHdr* studioHdr, const char* name)
{
    return s_FindHitboxSetByName(studioHdr, name);
}

const char* GetHitboxSetName(CStudioHdr* studioHdr, int set)
{
    return s_GetHitboxSetName(studioHdr, set);
}

int GetHitboxSetCount(CStudioHdr* studioHdr)
{
    return s_GetHitboxSetCount(studioHdr);
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
    s_ExtractBbox = module.Offset(0xA2FC0).RCast<decltype(s_ExtractBbox)>();
    s_IndexModelSequences = module.Offset(0xA4170).RCast<decltype(s_IndexModelSequences)>();
    s_ResetActivityIndexes = module.Offset(0xA4650).RCast<decltype(s_ResetActivityIndexes)>();
    s_VerifySequenceIndex = module.Offset(0xA4EC0).RCast<decltype(s_VerifySequenceIndex)>();
    s_SelectWeightedSequence = module.Offset(0xA4860).RCast<decltype(s_SelectWeightedSequence)>();
    s_SelectHeaviestSequence = module.Offset(0xA4710).RCast<decltype(s_SelectHeaviestSequence)>();
    s_SetEventIndexForSequence = module.Offset(0xA4E00).RCast<decltype(s_SetEventIndexForSequence)>();
    s_BuildAllAnimationEventIndexes = module.Offset(0xA2DB0).RCast<decltype(s_BuildAllAnimationEventIndexes)>();
    s_ResetEventIndexes = module.Offset(0xA4670).RCast<decltype(s_ResetEventIndexes)>();
    s_GetEyePosition = module.Offset(0xA3780).RCast<decltype(s_GetEyePosition)>();
    s_LookupActivity = module.Offset(0xA4410).RCast<decltype(s_LookupActivity)>();
    s_LookupSequence = module.Offset(0xA4500).RCast<decltype(s_LookupSequence)>();
    s_GetSequenceLinearMotion = module.Offset(0xA3B60).RCast<decltype(s_GetSequenceLinearMotion)>();
    s_GetSequenceName = module.Offset(0xA3D10).RCast<decltype(s_GetSequenceName)>();
    s_GetSequenceActivityName = module.Offset(0xA39A0).RCast<decltype(s_GetSequenceActivityName)>();
    s_GetSequenceFlags = module.Offset(0xA3AF0).RCast<decltype(s_GetSequenceFlags)>();
    s_GetAnimationEvent = module.Offset(0xA3430).RCast<decltype(s_GetAnimationEvent)>();
    s_HasAnimationEventOfType = module.Offset(0xA4060).RCast<decltype(s_HasAnimationEventOfType)>();
    s_FindTransitionSequence = module.Offset(0xA3240).RCast<decltype(s_FindTransitionSequence)>();
    s_GotoSequence = module.Offset(0xA3DC0).RCast<decltype(s_GotoSequence)>();
    s_SetBodygroup = module.Offset(0xA4DB0).RCast<decltype(s_SetBodygroup)>();
    s_GetBodygroup = module.Offset(0xA3610).RCast<decltype(s_GetBodygroup)>();
    s_GetBodygroupName = module.Offset(0xA36B0).RCast<decltype(s_GetBodygroupName)>();
    s_GetBodygroupPartName = module.Offset(0xA36F0).RCast<decltype(s_GetBodygroupPartName)>();
    s_FindBodygroupByName = module.Offset(0xA3110).RCast<decltype(s_FindBodygroupByName)>();
    s_GetBodygroupCount = module.Offset(0xA3680).RCast<decltype(s_GetBodygroupCount)>();
    s_GetSequenceActivity = module.Offset(0xA38E0).RCast<decltype(s_GetSequenceActivity)>();
    s_GetAttachmentLocalSpace = module.Offset(0xA35E0).RCast<decltype(s_GetAttachmentLocalSpace)>();
    s_FindHitboxSetByName = module.Offset(0xA31B0).RCast<decltype(s_FindHitboxSetByName)>();
    s_GetHitboxSetName = module.Offset(0xA37D0).RCast<decltype(s_GetHitboxSetName)>();
    s_GetHitboxSetCount = module.Offset(0xA37B0).RCast<decltype(s_GetHitboxSetCount)>();
})
