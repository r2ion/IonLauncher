#pragma once

#include <cstddef>
#include "mathlib/vector.h"
#include "mathlib/mathlib.h"

class C_BaseAnimating;
struct datamap_t;
struct studiohdr_t;
class CStudioHdr;
class IBoneSetup;
class CIKContext;
class C_SequenceTransitionerLayer
{
  public:
    virtual datamap_t* GetPredDescMap();
    virtual const datamap_t* GetPredDescMap() const;

    C_SequenceTransitionerLayer();
    void Reset();
    void SetActive() { m_sequenceTransitionerLayerActive = true; }
    void SetInactive() { m_sequenceTransitionerLayerActive = false; }
    bool IsActive() const { return m_sequenceTransitionerLayerActive; }
    int GetModelIndex() const { return m_sequenceTransitionerLayerModelIndex; }
    int GetSequence() const { return m_sequenceTransitionerLayerSequence; }
    float GetWeight() const { return m_weight; }
    void SetModelIndex(int index) { m_sequenceTransitionerLayerModelIndex = index; }
    void SetSequence(int sequence) { m_sequenceTransitionerLayerSequence = sequence; }
    float GetFadeOutWeight(float currentTime) const;

    bool m_isCurrent;
    C_BaseAnimating* m_owner;
    bool m_sequenceTransitionerLayerActive;
    int m_sequenceTransitionerLayerModelIndex;
    float m_sequenceTransitionerLayerStartCycle;
    int m_sequenceTransitionerLayerSequence;
    float m_weight;
    float m_sequenceTransitionerLayerPlaybackRate;
    float m_sequenceTransitionerLayerStartTime;
    float m_sequenceTransitionerLayerFadeOutDuration;
};

class C_SequenceTransitioner
{
  public:
    virtual datamap_t* GetPredDescMap();
    virtual const datamap_t* GetPredDescMap() const;
    C_SequenceTransitioner();
    C_BaseAnimating* m_ownerBaseAnimating;
    enum SequenceTransitionerTargetLayer
    {
        STTL_BASE_ANIMATING_LAYER,
        STTL_BASE_ANIMATING_OVERLAY_LAYER_0,
        STTL_BASE_ANIMATING_OVERLAY_LAYER_1,
        STTL_BASE_ANIMATING_OVERLAY_LAYER_2,
        STTL_BASE_ANIMATING_OVERLAY_LAYER_3,
        STTL_BASE_ANIMATING_OVERLAY_LAYER_4,
        STTL_BASE_ANIMATING_OVERLAY_LAYER_5,
        STTL_BASE_ANIMATING_OVERLAY_LAYER_6,
        STTL_BASE_ANIMATING_OVERLAY_LAYER_7,
        STTL_MAX
    } m_targetLayer;

    void SequenceTransitioner_Init(C_BaseAnimating* owner, SequenceTransitionerTargetLayer layer);
    void CheckForSequenceChange();
    void SequenceTransitioner_UpdateCurrent(float currentTime);
    void CalcWeights(float currentTime);
    void AccumulateSequenceTransitions(IBoneSetup& boneSetup, Vector3D* positions, Quaternion* rotations,
                                      Vector3D* scales, CIKContext* ikContext);
    void RemoveAll();

  private:
    void Remove(const CStudioHdr* hdr, int index);

  public:
    C_SequenceTransitionerLayer m_current;
    C_SequenceTransitionerLayer m_sequenceTransitionerLayers[6];
    unsigned int m_sequenceTransitionerLayerCount;
    int m_prevSequenceParity;
    const studiohdr_t* m_prevModel;
    float m_prevUpdateTime;
    int m_animViewEntityThirdPersonParity;
};
