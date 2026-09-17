#include "client/baseanimating.h"
#include "core/tier1.h"

CStudioHdr* (*s_GetModelPtr)(const C_BaseAnimating*);
float (*s_GetSequenceCycleRate)(C_BaseAnimating*, int);
float (*s_GetSequenceCycleRateForModel)(const C_BaseAnimating*, const CStudioHdr*, int);
bool (*s_IsValidSequence)(const C_BaseAnimating*, int);
bool (*s_IsSequenceLooping)(const C_BaseAnimating*, const CStudioHdr*, int);
float (*s_ClampCycle)(float, bool);
float (*s_SequenceDuration)(C_BaseAnimating*, int);
datamap_t* s_PredictedAnimEventDataMap;

datamap_t* PredictedAnimEventData::GetPredDescMap()
{
    return s_PredictedAnimEventDataMap;
}
const datamap_t* PredictedAnimEventData::GetPredDescMap() const
{
    return s_PredictedAnimEventDataMap;
}

CStudioHdr* C_BaseAnimating::GetModelPtr() const
{
    return s_GetModelPtr(this);
}
float C_BaseAnimating::GetSequenceCycleRate(int sequence)
{
    return s_GetSequenceCycleRate(this, sequence);
}
float C_BaseAnimating::GetSequenceCycleRateForModel(const CStudioHdr* hdr, int sequence) const
{
    return s_GetSequenceCycleRateForModel(this, hdr, sequence);
}
bool C_BaseAnimating::IsValidSequence(int sequence) const
{
    return s_IsValidSequence(this, sequence);
}
bool C_BaseAnimating::IsSequenceLooping(const CStudioHdr* hdr, int sequence) const
{
    return s_IsSequenceLooping(this, hdr, sequence);
}
float C_BaseAnimating::ClampCycle(float cycle, bool isLooping)
{
    return s_ClampCycle(cycle, isLooping);
}
float C_BaseAnimating::SequenceDuration(int sequence)
{
    return s_SequenceDuration(this, sequence);
}

ON_DLL_LOAD_CLIENT("client.dll", BaseAnimatingMethods, [](CModule module)
{
    s_GetModelPtr = module.Offset(0x88900).RCast<decltype(s_GetModelPtr)>();
    s_GetSequenceCycleRate = module.Offset(0xAB4B0).RCast<decltype(s_GetSequenceCycleRate)>();
    s_GetSequenceCycleRateForModel = module.Offset(0xAB530).RCast<decltype(s_GetSequenceCycleRateForModel)>();
    s_IsValidSequence = module.Offset(0xFA4F0).RCast<decltype(s_IsValidSequence)>();
    s_IsSequenceLooping = module.Offset(0xAC710).RCast<decltype(s_IsSequenceLooping)>();
    s_ClampCycle = module.Offset(0xAA260).RCast<decltype(s_ClampCycle)>();
    s_SequenceDuration = module.Offset(0xADA20).RCast<decltype(s_SequenceDuration)>();
    s_PredictedAnimEventDataMap = module.Offset(0xAF46E0).RCast<datamap_t*>();
})
