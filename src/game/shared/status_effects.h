#pragma once

#include <cstddef>
#include <cstdint>

struct StatusEffectTimedData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    std::int32_t seComboVars; // 0x08
    float seTimeEnd;          // 0x0C
    float seEaseOut;          // 0x10
    std::byte _pad14[0x4];    // 0x14
};

static_assert(sizeof(StatusEffectTimedData) == 0x18);

struct StatusEffectEndlessData
{
    virtual void NetworkStateChanged() { }
    virtual void NetworkStateChanged(void* pProp) { }
    std::int32_t seComboVars; // 0x08
    std::byte _padC[0x4];     // 0x0C
};

static_assert(sizeof(StatusEffectEndlessData) == 0x10);
