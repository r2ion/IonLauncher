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

enum class GrappleTitanRule_t
{
    Default, // defer to the game's own eligibility check
    Deny,
    Allow,
};

//-----------------------------------------------------------------------------
// Purpose: Whether ion_custom_titan_grapple is enabled, this is replicated so the client predicts what the server does
//-----------------------------------------------------------------------------
bool IsCustomTitanGrappleEnabled();

//-----------------------------------------------------------------------------
// Purpose: Titan overrides for the "may this entity be moved by a grapple" check.
//          Titan-owned hooks reel the Titan in towards its target, and a Titan grappled by a Titan is left untouched
//          (no pull, no speed loss). Other grappled entities keep the game's behaviour.
// Input  : *mover - entity that would have its velocity modified
//          *other - hook owner (isSelfPull) or the grappled entity, may be null
//          isSelfPull - true when mover is the grappler, false when mover is the one being grappled
// Output : Deny/Allow when a Titan is involved, Default otherwise or when ion_custom_titan_grapple is off
//-----------------------------------------------------------------------------
template <typename TEntity> GrappleTitanRule_t GetTitanGrappleRule(const TEntity* mover, const TEntity* other, bool isSelfPull)
{
    if (!IsCustomTitanGrappleEnabled())
        return GrappleTitanRule_t::Default;

    if (!isSelfPull && mover->IsTitan() && other && other->IsTitan())
        return GrappleTitanRule_t::Deny;

    if (isSelfPull && mover->IsTitan())
        return GrappleTitanRule_t::Allow;

    return GrappleTitanRule_t::Default;
}
