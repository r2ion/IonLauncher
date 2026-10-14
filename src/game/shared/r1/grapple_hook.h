#pragma once

#include <cstddef>
#include <cstdint>

#include "game/shared/ehandle.h"
#include "mathlib/mathlib.h"

struct GrappleData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    Vector3D m_grappleVel;               // 0x08
    Vector3D m_grapplePoints[4];         // 0x14
    std::int32_t m_grapplePointCount;    // 0x44
    bool m_grappleAttached;              // 0x48
    bool m_grapplePulling;               // 0x49
    bool m_grappleRetracting;            // 0x4A
    bool m_grappleForcedRetracting;      // 0x4B
    float m_grappleUsedPower;            // 0x4C
    float m_grapplePullTime;             // 0x50
    float m_grappleAttachTime;           // 0x54
    float m_grappleDetachTime;           // 0x58
    EHANDLE m_grappleMeleeTarget;        // 0x5C
    bool m_grappleHasGoodVelocity;       // 0x60
    std::byte _pad61[0x3];               // 0x61
    float m_grappleLastGoodVelocityTime; // 0x64
};

static_assert(sizeof(GrappleData) == 0x68);
