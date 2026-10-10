#include "client/sequence_transitioner.h"
#include "core/tier1.h"

datamap_t* g_SequenceTransitionerPredMap;
datamap_t* g_SequenceTransitionerLayerPredMap;
void (*C_SequenceTransitioner__SequenceTransitioner_Init)(C_SequenceTransitioner*, C_BaseAnimating*, C_SequenceTransitioner::SequenceTransitionerTargetLayer);
void (*C_SequenceTransitioner__CheckForSequenceChange)(C_SequenceTransitioner*);
void (*C_SequenceTransitioner__SequenceTransitioner_UpdateCurrent)(C_SequenceTransitioner*, float);
void (*C_SequenceTransitioner__CalcWeights)(C_SequenceTransitioner*, float);
void (*C_SequenceTransitioner__AccumulateSequenceTransitions)(C_SequenceTransitioner*, IBoneSetup&, Vector3D*, Quaternion*, Vector3D*, CIKContext*);
void (*C_SequenceTransitioner__Remove)(C_SequenceTransitioner*, const CStudioHdr*, int);
void (*C_SequenceTransitioner__RemoveAll)(C_SequenceTransitioner*);
float (*C_SequenceTransitionerLayer__GetFadeOutWeight)(const C_SequenceTransitionerLayer*, float);

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
      m_prevUpdateTime(-1.0f), m_animViewEntityThirdPersonParity(0)
{
}

datamap_t* C_SequenceTransitionerLayer::GetPredDescMap()
{
    return g_SequenceTransitionerLayerPredMap;
}
const datamap_t* C_SequenceTransitionerLayer::GetPredDescMap() const
{
    return g_SequenceTransitionerLayerPredMap;
}
datamap_t* C_SequenceTransitioner::GetPredDescMap()
{
    return g_SequenceTransitionerPredMap;
}
const datamap_t* C_SequenceTransitioner::GetPredDescMap() const
{
    return g_SequenceTransitionerPredMap;
}

float C_SequenceTransitionerLayer::GetFadeOutWeight(float currentTime) const
{
    return C_SequenceTransitionerLayer__GetFadeOutWeight(this, currentTime);
}

void C_SequenceTransitioner::SequenceTransitioner_Init(C_BaseAnimating* owner, SequenceTransitionerTargetLayer layer)
{
    C_SequenceTransitioner__SequenceTransitioner_Init(this, owner, layer);
}

void C_SequenceTransitioner::CheckForSequenceChange()
{
    C_SequenceTransitioner__CheckForSequenceChange(this);
}
void C_SequenceTransitioner::SequenceTransitioner_UpdateCurrent(float currentTime)
{
    C_SequenceTransitioner__SequenceTransitioner_UpdateCurrent(this, currentTime);
}
void C_SequenceTransitioner::CalcWeights(float currentTime)
{
    C_SequenceTransitioner__CalcWeights(this, currentTime);
}
void C_SequenceTransitioner::RemoveAll()
{
    C_SequenceTransitioner__RemoveAll(this);
}
void C_SequenceTransitioner::Remove(const CStudioHdr* hdr, int index)
{
    C_SequenceTransitioner__Remove(this, hdr, index);
}

void C_SequenceTransitioner::AccumulateSequenceTransitions(IBoneSetup& boneSetup, Vector3D* positions, Quaternion* rotations, Vector3D* scales,
                                                           CIKContext* ikContext)
{
    C_SequenceTransitioner__AccumulateSequenceTransitions(this, boneSetup, positions, rotations, scales, ikContext);
}

ON_DLL_LOAD_CLIENT("client.dll", SequenceTransitionerMethods, [](CModule module)
{
    g_SequenceTransitionerPredMap = module.Offset(0xB26F50).RCast<datamap_t*>();
    g_SequenceTransitionerLayerPredMap = module.Offset(0xB26F10).RCast<datamap_t*>();
    C_SequenceTransitioner__SequenceTransitioner_Init = module.Offset(0x31ED50).RCast<decltype(C_SequenceTransitioner__SequenceTransitioner_Init)>();
    C_SequenceTransitioner__CheckForSequenceChange = module.Offset(0x31E390).RCast<decltype(C_SequenceTransitioner__CheckForSequenceChange)>();
    C_SequenceTransitioner__SequenceTransitioner_UpdateCurrent = module.Offset(0x31EE60).RCast<decltype(C_SequenceTransitioner__SequenceTransitioner_UpdateCurrent)>();
    C_SequenceTransitioner__CalcWeights = module.Offset(0x31E260).RCast<decltype(C_SequenceTransitioner__CalcWeights)>();
    C_SequenceTransitioner__AccumulateSequenceTransitions = module.Offset(0x31DF50).RCast<decltype(C_SequenceTransitioner__AccumulateSequenceTransitions)>();
    C_SequenceTransitioner__Remove = module.Offset(0x31E680).RCast<decltype(C_SequenceTransitioner__Remove)>();
    C_SequenceTransitioner__RemoveAll = module.Offset(0x31EA30).RCast<decltype(C_SequenceTransitioner__RemoveAll)>();
    C_SequenceTransitionerLayer__GetFadeOutWeight = module.Offset(0x31EB30).RCast<decltype(C_SequenceTransitionerLayer__GetFadeOutWeight)>();
})
