#pragma once

#include "game/shared/ehandle.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"

class C_BaseAnimating;
struct datamap_t;

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

struct PredictedAnimEventData_Client
{
    virtual datamap_t* GetPredDescMap();
    virtual const datamap_t* GetPredDescMapConst() const;
    float m_predictedAnimEventTimes[8];
    int m_predictedAnimEventIndices[8];
    int m_predictedAnimEventCount;
    CHandle<C_BaseAnimating> m_predictedAnimEventTarget;
    int m_predictedAnimEventSequence;
    int m_predictedAnimEventModel;
    float m_predictedAnimEventsReadyToFireTime;
};

