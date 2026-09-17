#include "studio.h"
#include "client/animation.h"
#include "core/tier1.h"

void (*s_StudioHdrInit)(CStudioHdr*, const studiohdr_t*, IMDLCache*);
void (*s_StudioHdrTerm)(CStudioHdr*);
const studiohdr_t* (*s_SeqStudioHdr)(CStudioHdr*, int);
const studiohdr_t* (*s_AnimStudioHdr)(CStudioHdr*, int);
const studiohdr_t* (*s_GroupStudioHdr)(CStudioHdr*, int);
const virtualmodel_t* (*s_ResetVModel)(const CStudioHdr*, const virtualmodel_t*);
bool (*s_SequencesAvailable)(const CStudioHdr*);
int (*s_GetVirtualNumSeq)(const CStudioHdr*);
mstudioseqdesc_t& (*s_Seqdesc)(CStudioHdr*, int);
int (*s_RelativeAnim)(const CStudioHdr*, int, int);
int (*s_RelativeSeq)(const CStudioHdr*, int, int);
int (*s_GetActivityListVersion)(CStudioHdr*);
void (*s_SetActivityListVersion)(CStudioHdr*, int);
int (*s_GetEventListVersion)(CStudioHdr*);
int (*s_GetNumAttachments)(const CStudioHdr*);
const mstudioattachment_t& (*s_Attachment)(CStudioHdr*, int);
int (*s_GetAttachmentBone)(CStudioHdr*, int);
int (*s_EntryNode)(CStudioHdr*, int);
int (*s_ExitNode)(CStudioHdr*, int);
const char* (*s_NodeName)(CStudioHdr*, int);
int (*s_GetTransition)(const CStudioHdr*, int, int);
int (*s_GetNumPoseParameters)(const CStudioHdr*);
const mstudioposeparamdesc_t& (*s_PoseParameter)(CStudioHdr*, int);
int (*s_GetSharedPoseParameter)(const CStudioHdr*, int, int);
int (*s_GetNumIKAutoplayLocks)(const CStudioHdr*);
void (*s_SetEventListVersion)(CStudioHdr*, int);
int (*s_GetAutoplayList)(const studiohdr_t*, unsigned short**);
const studiohdr_t* (*s_VirtualGroupStudioHdr)(const virtualgroup_t*);
CStudioHdr::CActivityToSequenceMapping* (*s_FindActivityMapping)(CStudioHdr*);
void (*s_ReleaseActivityMapping)(CStudioHdr::CActivityToSequenceMapping*);
void (*s_ResetUnusedActivityMappings)();
void (*s_InitializeActivityMapping)(CStudioHdr::CActivityToSequenceMapping*, CStudioHdr*);
void (*s_ReinitializeActivityMapping)(CStudioHdr::CActivityToSequenceMapping*, CStudioHdr*);
bool (*s_ValidateActivityMapping)(const CStudioHdr::CActivityToSequenceMapping*, const CStudioHdr*);
const CStudioHdr::CActivityToSequenceMapping::SequenceTuple* (*s_GetActivitySequences)(CStudioHdr::CActivityToSequenceMapping*, int, int*,
                                                                                              int*);
int (*s_SelectWeightedSequence)(CStudioHdr::CActivityToSequenceMapping*, CStudioHdr*, int, bool, const CUtlSymbol*, int, int);
unsigned int (*s_ActivityHashInt)(int);
void (*s_AppendSequences)(virtualmodel_t*, int, const studiohdr_t*);
void (*s_AppendAnimations)(virtualmodel_t*, int, const studiohdr_t*);
void (*s_AppendPoseParameters)(virtualmodel_t*, int, const studiohdr_t*);
void (*s_AppendBonemap)(virtualmodel_t*, int, const studiohdr_t*);
void (*s_AppendNodes)(virtualmodel_t*, int, const studiohdr_t*);
void (*s_AppendIKLocks)(virtualmodel_t*, int, const studiohdr_t*);
void (*s_AppendModels)(virtualmodel_t*, int, const studiohdr_t*);

CStudioHdr::CStudioHdr() : m_nFrameUnlockCounter(0), m_pFrameUnlockCounter(&m_nFrameUnlockCounter)
{
    Init(nullptr, nullptr);
}

CStudioHdr::CStudioHdr(const studiohdr_t* studioHdr, IMDLCache* mdlcache) : m_nFrameUnlockCounter(0), m_pFrameUnlockCounter(&m_nFrameUnlockCounter)
{
    Init(studioHdr, mdlcache);
}

CStudioHdr::~CStudioHdr()
{
    Term();
}
void CStudioHdr::Init(const studiohdr_t* studioHdr, IMDLCache* mdlcache)
{
    s_StudioHdrInit(this, studioHdr, mdlcache);
}
void CStudioHdr::Term()
{
    s_StudioHdrTerm(this);
}
const studiohdr_t* CStudioHdr::pSeqStudioHdr(int sequence)
{
    return s_SeqStudioHdr(this, sequence);
}
const studiohdr_t* CStudioHdr::pAnimStudioHdr(int animation)
{
    return s_AnimStudioHdr(this, animation);
}
const studiohdr_t* CStudioHdr::GroupStudioHdr(int group)
{
    return s_GroupStudioHdr(this, group);
}
const virtualmodel_t* CStudioHdr::ResetVModel(const virtualmodel_t* virtualModel) const
{
    return s_ResetVModel(this, virtualModel);
}
mstudiobone_t* CStudioHdr::pBone(int bone) const
{
    return reinterpret_cast<mstudiobone_t*>(reinterpret_cast<std::uint8_t*>(const_cast<studiohdr_t*>(m_pStudioHdr)) + m_pStudioHdr->boneindex) + bone;
}
bool CStudioHdr::SequencesAvailable() const
{
    return s_SequencesAvailable(this);
}
int CStudioHdr::GetNumSeq() const
{
    return m_pVModel ? s_GetVirtualNumSeq(this) : m_pStudioHdr->numlocalseq;
}
mstudioseqdesc_t& CStudioHdr::pSeqdesc(int sequence)
{
    return s_Seqdesc(this, sequence);
}
int CStudioHdr::iRelativeAnim(int baseSequence, int relativeAnimation) const
{
    return m_pVModel ? s_RelativeAnim(this, baseSequence, relativeAnimation) : relativeAnimation;
}
int CStudioHdr::iRelativeSeq(int baseSequence, int relativeSequence) const
{
    return s_RelativeSeq(this, baseSequence, relativeSequence);
}
int CStudioHdr::GetActivityListVersion()
{
    return s_GetActivityListVersion(this);
}
void CStudioHdr::SetActivityListVersion(int version)
{
    s_SetActivityListVersion(this, version);
}
int CStudioHdr::GetEventListVersion()
{
    return s_GetEventListVersion(this);
}
void CStudioHdr::SetEventListVersion(int version)
{
    s_SetEventListVersion(this, version);
}
int CStudioHdr::GetNumAttachments() const
{
    return s_GetNumAttachments(this);
}
const mstudioattachment_t& CStudioHdr::pAttachment(int attachment)
{
    return s_Attachment(this, attachment);
}
int CStudioHdr::GetAttachmentBone(int attachment)
{
    return s_GetAttachmentBone(this, attachment);
}
int CStudioHdr::EntryNode(int sequence)
{
    return s_EntryNode(this, sequence);
}
int CStudioHdr::ExitNode(int sequence)
{
    return s_ExitNode(this, sequence);
}
const char* CStudioHdr::pszNodeName(int node)
{
    return s_NodeName(this, node);
}
int CStudioHdr::GetTransition(int from, int to) const
{
    return s_GetTransition(this, from, to);
}
int CStudioHdr::GetNumPoseParameters() const
{
    return s_GetNumPoseParameters(this);
}
const mstudioposeparamdesc_t& CStudioHdr::pPoseParameter(int parameter)
{
    return s_PoseParameter(this, parameter);
}
int CStudioHdr::GetSharedPoseParameter(int sequence, int localPose) const
{
    return s_GetSharedPoseParameter(this, sequence, localPose);
}
int CStudioHdr::GetNumIKAutoplayLocks() const
{
    return s_GetNumIKAutoplayLocks(this);
}
const mstudioiklock_t& CStudioHdr::pIKAutoplayLock(int lock)
{
    const studiohdr_t* hdr = m_pStudioHdr;
    if (m_pVModel)
    {
        const virtualgeneric_t& entry = m_pVModel->m_iklock[lock];
        hdr = GroupStudioHdr(entry.group);
        lock = entry.index;
    }
    return *(reinterpret_cast<const mstudioiklock_t*>(reinterpret_cast<const std::uint8_t*>(hdr) + hdr->localikautoplaylockindex) + lock);
}
int CStudioHdr::GetAutoplayList(unsigned short** sequences) const
{
    return s_GetAutoplayList(m_pStudioHdr, sequences);
}

const studiohdr_t* virtualgroup_t::GetStudioHdr() const
{
    return s_VirtualGroupStudioHdr(this);
}
void virtualmodel_t::AppendSequences(int group, const studiohdr_t* hdr)
{
    s_AppendSequences(this, group, hdr);
}
void virtualmodel_t::AppendAnimations(int group, const studiohdr_t* hdr)
{
    s_AppendAnimations(this, group, hdr);
}
void virtualmodel_t::AppendPoseParameters(int group, const studiohdr_t* hdr)
{
    s_AppendPoseParameters(this, group, hdr);
}
void virtualmodel_t::AppendBonemap(int group, const studiohdr_t* hdr)
{
    s_AppendBonemap(this, group, hdr);
}
void virtualmodel_t::AppendNodes(int group, const studiohdr_t* hdr)
{
    s_AppendNodes(this, group, hdr);
}
void virtualmodel_t::AppendIKLocks(int group, const studiohdr_t* hdr)
{
    s_AppendIKLocks(this, group, hdr);
}
void virtualmodel_t::AppendModels(int group, const studiohdr_t* hdr)
{
    s_AppendModels(this, group, hdr);
}

CStudioHdr::CActivityToSequenceMapping::CActivityToSequenceMapping()
    : m_pSequenceTuples(nullptr), m_iSequenceTuplesCount(0), m_ActToSeqHash(32, 0, 0), m_pStudioHdr(nullptr), m_expectedVModel(nullptr)
{
}
CStudioHdr::CActivityToSequenceMapping::~CActivityToSequenceMapping()
{
    if (m_pSequenceTuples)
    {
        for (unsigned int i = 0; i < m_iSequenceTuplesCount; ++i)
            delete[] m_pSequenceTuples[i].pActivityModifiers;
        delete[] m_pSequenceTuples;
    }
}
unsigned int CStudioHdr::CActivityToSequenceMapping::HashValueType::HashFuncs::operator()(const HashValueType& value) const
{
    return s_ActivityHashInt(value.activityIdx);
}
const CStudioHdr::CActivityToSequenceMapping::SequenceTuple* CStudioHdr::CActivityToSequenceMapping::GetSequences(int activity, int* sequenceCount,
                                                                                                                  int* totalWeight)
{
    return s_GetActivitySequences(this, activity, sequenceCount, totalWeight);
}
int CStudioHdr::CActivityToSequenceMapping::NumSequencesForActivity(int activity)
{
    int count = 0;
    int weight = 0;
    GetSequences(activity, &count, &weight);
    return count;
}
CStudioHdr::CActivityToSequenceMapping* CStudioHdr::CActivityToSequenceMapping::FindMapping(CStudioHdr* hdr)
{
    return s_FindActivityMapping(hdr);
}
void CStudioHdr::CActivityToSequenceMapping::ReleaseMapping(CActivityToSequenceMapping* mapping)
{
    s_ReleaseActivityMapping(mapping);
}
void CStudioHdr::CActivityToSequenceMapping::ResetUnusedMappings()
{
    s_ResetUnusedActivityMappings();
}
void CStudioHdr::CActivityToSequenceMapping::Initialize(CStudioHdr* hdr)
{
    s_InitializeActivityMapping(this, hdr);
}
void CStudioHdr::CActivityToSequenceMapping::Reinitialize(CStudioHdr* hdr)
{
    s_ReinitializeActivityMapping(this, hdr);
}
bool CStudioHdr::CActivityToSequenceMapping::ValidateAgainst(const CStudioHdr* hdr) const
{
    return s_ValidateActivityMapping(this, hdr);
}
void CStudioHdr::CActivityToSequenceMapping::SetValidationPair(const CStudioHdr* hdr)
{
    m_pStudioHdr = hdr->GetRenderHdr();
    m_expectedVModel = hdr->GetVirtualModel();
}
int CStudioHdr::CActivityToSequenceMapping::SelectWeightedSequence(CStudioHdr* hdr, int activity, bool useModifiers, const CUtlSymbol* modifiers,
                                                                   int modifierCount, int currentSequence)
{
    return s_SelectWeightedSequence(this, hdr, activity, useModifiers, modifiers, modifierCount, currentSequence);
}
CStudioHdr::CActivityToSequenceMapping* CStudioHdr::GetActivityToSequenceMapping()
{
    if (!m_pActivityToSequence)
        m_pActivityToSequence = CActivityToSequenceMapping::FindMapping(this);
    return m_pActivityToSequence;
}
int CStudioHdr::SelectWeightedSequence(int activity, int currentSequence)
{
    return SelectWeightedSequence(activity, false, nullptr, 0, currentSequence);
}
int CStudioHdr::SelectWeightedSequence(int activity, bool useModifiers, const CUtlSymbol* modifiers, int modifierCount, int currentSequence)
{
    return GetActivityToSequenceMapping()->SelectWeightedSequence(this, activity, useModifiers, modifiers, modifierCount, currentSequence);
}
int CStudioHdr::SelectWeightedSequenceFromModifiers(int activity, const CUtlSymbol* modifiers, int modifierCount)
{
    return SelectWeightedSequence(activity, true, modifiers, modifierCount, -1);
}
bool CStudioHdr::HaveSequenceForActivity(int activity)
{
    if (!SequencesAvailable())
        return false;
    VerifySequenceIndex(this);
    if (GetNumSeq() == 1)
        return GetSequenceActivity(this, 0) == activity;
    CActivityToSequenceMapping* mapping = GetActivityToSequenceMapping();
    if (!mapping->ValidateAgainst(this))
        mapping->Reinitialize(this);
    return mapping->NumSequencesForActivity(activity) > 0;
}
void CStudioHdr::ReinitializeSequenceMapping()
{
    if (SequencesAvailable() && GetNumSeq() > 1)
        GetActivityToSequenceMapping()->Reinitialize(this);
}

ON_DLL_LOAD_CLIENT("client.dll", StudioHdrMethods, [](CModule module)
{
    s_StudioHdrInit = module.Offset(0x331560).RCast<decltype(s_StudioHdrInit)>();
    s_StudioHdrTerm = module.Offset(0x334D30).RCast<decltype(s_StudioHdrTerm)>();
    s_SeqStudioHdr = module.Offset(0x335940).RCast<decltype(s_SeqStudioHdr)>();
    s_AnimStudioHdr = module.Offset(0x335480).RCast<decltype(s_AnimStudioHdr)>();
    s_GroupStudioHdr = module.Offset(0x330F70).RCast<decltype(s_GroupStudioHdr)>();
    s_ResetVModel = module.Offset(0x334180).RCast<decltype(s_ResetVModel)>();
    s_SequencesAvailable = module.Offset(0x3347D0).RCast<decltype(s_SequencesAvailable)>();
    s_GetVirtualNumSeq = module.Offset(0x330CF0).RCast<decltype(s_GetVirtualNumSeq)>();
    s_Seqdesc = module.Offset(0xA5030).RCast<decltype(s_Seqdesc)>();
    s_RelativeAnim = module.Offset(0x335330).RCast<decltype(s_RelativeAnim)>();
    s_RelativeSeq = module.Offset(0x3353B0).RCast<decltype(s_RelativeSeq)>();
    s_GetActivityListVersion = module.Offset(0x330800).RCast<decltype(s_GetActivityListVersion)>();
    s_SetActivityListVersion = module.Offset(0x334850).RCast<decltype(s_SetActivityListVersion)>();
    s_GetEventListVersion = module.Offset(0x330A50).RCast<decltype(s_GetEventListVersion)>();
    s_GetNumAttachments = module.Offset(0x330C00).RCast<decltype(s_GetNumAttachments)>();
    s_Attachment = module.Offset(0x335590).RCast<decltype(s_Attachment)>();
    s_GetAttachmentBone = module.Offset(0x330A10).RCast<decltype(s_GetAttachmentBone)>();
    s_EntryNode = module.Offset(0x32FFA0).RCast<decltype(s_EntryNode)>();
    s_ExitNode = module.Offset(0x3300D0).RCast<decltype(s_ExitNode)>();
    s_NodeName = module.Offset(0x335B20).RCast<decltype(s_NodeName)>();
    s_GetTransition = module.Offset(0x330EB0).RCast<decltype(s_GetTransition)>();
    s_GetNumPoseParameters = module.Offset(0x330C70).RCast<decltype(s_GetNumPoseParameters)>();
    s_PoseParameter = module.Offset(0x3357A0).RCast<decltype(s_PoseParameter)>();
    s_GetSharedPoseParameter = module.Offset(0x330E00).RCast<decltype(s_GetSharedPoseParameter)>();
    s_GetNumIKAutoplayLocks = module.Offset(0x330C20).RCast<decltype(s_GetNumIKAutoplayLocks)>();
    s_SetEventListVersion = module.Offset(0x334AA0).RCast<decltype(s_SetEventListVersion)>();
    s_GetAutoplayList = module.Offset(0x335C70).RCast<decltype(s_GetAutoplayList)>();
    s_VirtualGroupStudioHdr = module.Offset(0x335C90).RCast<decltype(s_VirtualGroupStudioHdr)>();
    s_FindActivityMapping = module.Offset(0x3304B0).RCast<decltype(s_FindActivityMapping)>();
    s_ReleaseActivityMapping = module.Offset(0x3331E0).RCast<decltype(s_ReleaseActivityMapping)>();
    s_ResetUnusedActivityMappings = module.Offset(0x3340F0).RCast<decltype(s_ResetUnusedActivityMappings)>();
    s_InitializeActivityMapping = module.Offset(0x331640).RCast<decltype(s_InitializeActivityMapping)>();
    s_ReinitializeActivityMapping = module.Offset(0x333170).RCast<decltype(s_ReinitializeActivityMapping)>();
    s_ValidateActivityMapping = module.Offset(0x335300).RCast<decltype(s_ValidateActivityMapping)>();
    s_GetActivitySequences = module.Offset(0x330D20).RCast<decltype(s_GetActivitySequences)>();
    s_SelectWeightedSequence = module.Offset(0xA4960).RCast<decltype(s_SelectWeightedSequence)>();
    s_ActivityHashInt = module.Offset(0x73ADF0).RCast<decltype(s_ActivityHashInt)>();
})

ON_DLL_LOAD_CLIENT("datacache.dll", VirtualModelMethods, [](CModule module)
{
    s_AppendSequences = module.Offset(0x6B690).RCast<decltype(s_AppendSequences)>();
    s_AppendAnimations = module.Offset(0x6A220).RCast<decltype(s_AppendAnimations)>();
    s_AppendPoseParameters = module.Offset(0x6B240).RCast<decltype(s_AppendPoseParameters)>();
    s_AppendBonemap = module.Offset(0x6A6B0).RCast<decltype(s_AppendBonemap)>();
    s_AppendNodes = module.Offset(0x6AF00).RCast<decltype(s_AppendNodes)>();
    s_AppendIKLocks = module.Offset(0x6A930).RCast<decltype(s_AppendIKLocks)>();
    s_AppendModels = module.Offset(0x6AC20).RCast<decltype(s_AppendModels)>();
})
