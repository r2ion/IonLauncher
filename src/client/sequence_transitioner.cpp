#include "client/sequence_transitioner.h"
#include "core/tier1.h"

datamap_t* s_SequenceTransitionerPredMap;
datamap_t* s_SequenceTransitionerLayerPredMap;
void (*s_SequenceTransitionerInit)(C_SequenceTransitioner*, C_BaseAnimating*, C_SequenceTransitioner::SequenceTransitionerTargetLayer);
void (*s_CheckForSequenceChange)(C_SequenceTransitioner*);
void (*s_UpdateCurrent)(C_SequenceTransitioner*, float);
void (*s_CalcWeights)(C_SequenceTransitioner*, float);
void (*s_AccumulateSequenceTransitions)(C_SequenceTransitioner*, IBoneSetup&, Vector3D*, Quaternion*, Vector3D*, CIKContext*);
void (*s_RemoveSequenceTransition)(C_SequenceTransitioner*, const CStudioHdr*, int);
void (*s_RemoveAllSequenceTransitions)(C_SequenceTransitioner*);
float (*s_GetFadeOutWeight)(const C_SequenceTransitionerLayer*, float);

C_SequenceTransitionerLayer::C_SequenceTransitionerLayer() : m_isCurrent(false), m_owner(nullptr)
{
    Reset();
}

void C_SequenceTransitionerLayer::Reset()
{
    m_sequenceTransitionerLayerActive = false;
    m_sequenceTransitionerLayerModelIndex = 0;
    m_sequenceTransitionerLayerStartCycle = 0.0f;
    m_sequenceTransitionerLayerSequence = 0;
    m_weight = 0.0f;
    m_sequenceTransitionerLayerPlaybackRate = 0.0f;
    m_sequenceTransitionerLayerStartTime = 0.0f;
    m_sequenceTransitionerLayerFadeOutDuration = 0.0f;
}

C_SequenceTransitioner::C_SequenceTransitioner()
    : m_ownerBaseAnimating(nullptr), m_targetLayer(STTL_MAX), m_sequenceTransitionerLayerCount(0), m_prevSequenceParity(15), m_prevModel(nullptr),
      m_prevUpdateTime(-1.0f)
{
}

datamap_t* C_SequenceTransitionerLayer::GetPredDescMap()
{
    return s_SequenceTransitionerLayerPredMap;
}
const datamap_t* C_SequenceTransitionerLayer::GetPredDescMap() const
{
    return s_SequenceTransitionerLayerPredMap;
}
datamap_t* C_SequenceTransitioner::GetPredDescMap()
{
    return s_SequenceTransitionerPredMap;
}
const datamap_t* C_SequenceTransitioner::GetPredDescMap() const
{
    return s_SequenceTransitionerPredMap;
}

float C_SequenceTransitionerLayer::GetFadeOutWeight(float currentTime) const
{
    return s_GetFadeOutWeight(this, currentTime);
}

void C_SequenceTransitioner::SequenceTransitioner_Init(C_BaseAnimating* owner, SequenceTransitionerTargetLayer layer)
{
    s_SequenceTransitionerInit(this, owner, layer);
}

void C_SequenceTransitioner::CheckForSequenceChange()
{
    s_CheckForSequenceChange(this);
}
void C_SequenceTransitioner::SequenceTransitioner_UpdateCurrent(float currentTime)
{
    s_UpdateCurrent(this, currentTime);
}
void C_SequenceTransitioner::CalcWeights(float currentTime)
{
    s_CalcWeights(this, currentTime);
}
void C_SequenceTransitioner::RemoveAll()
{
    s_RemoveAllSequenceTransitions(this);
}
void C_SequenceTransitioner::Remove(const CStudioHdr* hdr, int index)
{
    s_RemoveSequenceTransition(this, hdr, index);
}

void C_SequenceTransitioner::AccumulateSequenceTransitions(IBoneSetup& boneSetup, Vector3D* positions, Quaternion* rotations, Vector3D* scales,
                                                           CIKContext* ikContext)
{
    s_AccumulateSequenceTransitions(this, boneSetup, positions, rotations, scales, ikContext);
}

ON_DLL_LOAD_CLIENT("client.dll", SequenceTransitionerMethods, [](CModule module)
{
    s_SequenceTransitionerPredMap = module.Offset(0xB26F50).RCast<datamap_t*>();
    s_SequenceTransitionerLayerPredMap = module.Offset(0xB26F10).RCast<datamap_t*>();
    s_SequenceTransitionerInit = module.Offset(0x31ED50).RCast<decltype(s_SequenceTransitionerInit)>();
    s_CheckForSequenceChange = module.Offset(0x31E390).RCast<decltype(s_CheckForSequenceChange)>();
    s_UpdateCurrent = module.Offset(0x31EE60).RCast<decltype(s_UpdateCurrent)>();
    s_CalcWeights = module.Offset(0x31E260).RCast<decltype(s_CalcWeights)>();
    s_AccumulateSequenceTransitions = module.Offset(0x31DF50).RCast<decltype(s_AccumulateSequenceTransitions)>();
    s_RemoveSequenceTransition = module.Offset(0x31E680).RCast<decltype(s_RemoveSequenceTransition)>();
    s_RemoveAllSequenceTransitions = module.Offset(0x31EA30).RCast<decltype(s_RemoveAllSequenceTransitions)>();
    s_GetFadeOutWeight = module.Offset(0x31EB30).RCast<decltype(s_GetFadeOutWeight)>();
})
