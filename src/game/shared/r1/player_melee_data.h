#pragma once

#include <cstddef>
#include <cstdint>

#include "game/shared/ehandle.h"

struct PlayerMelee_PlayerData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    bool attackActive;                 // 0x08
    bool attackRecoveryShouldBeQuick;  // 0x09
    std::byte _padA[0x2];              // 0x0A
    float attackStartTime;             // 0x0C
    EHANDLE attackHitEntity;           // 0x10
    float attackHitEntityTime;         // 0x14
    float attackLastHitNonWorldEntity; // 0x18
    std::int32_t scriptedState;        // 0x1C
    bool pendingMeleePress;            // 0x20
    std::byte _pad21[0x7];             // 0x21
};

static_assert(sizeof(PlayerMelee_PlayerData) == 0x28);
