#pragma once

#include "rtech/rui/rui_render_types.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

struct RuiInstance;

using RuiTopologyHandle = uint32_t;

constexpr size_t RUI_TOPOLOGY_CAPACITY = 64;
constexpr RuiTopologyHandle RUI_TOPOLOGY_INDEX_MASK = static_cast<RuiTopologyHandle>(RUI_TOPOLOGY_CAPACITY - 1);

struct RuiDrawInfoPlanar
{
    RuiDrawInfoMode mode;
    uint8_t reserved04[0xC];
    RuiProjectionBasis basis;
};
static_assert(sizeof(RuiDrawInfoPlanar) == 0x70);
static_assert(offsetof(RuiDrawInfoPlanar, basis) == 0x10);

struct RuiDrawInfoAngular
{
    RuiDrawInfoMode mode;
    float longitudeScale;
    float latitudeScale;
    float tessellationScale;
    float transform[3][4];
};
static_assert(sizeof(RuiDrawInfoAngular) == 0x40);
static_assert(offsetof(RuiDrawInfoAngular, transform) == 0x10);

union RuiTopologyDrawInfo
{
    RuiDrawInfo base;
    RuiDrawInfoPlanar planar;
    RuiDrawInfoAngular angular;
    std::byte storage[0x80];
};
static_assert(sizeof(RuiTopologyDrawInfo) == 0x80);

struct RuiTopology
{
    RuiTopologyHandle handle;
    uint16_t referenceCount;
    uint16_t attachmentIndex;
    uint32_t parentEntityHandle;
    float origin[3];
    float right[3];
    float down[3];
    uint8_t lifecycleState;
    uint8_t reserved31[0xB];
    float sphereRadius;
    RuiTopologyDrawInfo drawInfo;
};
static_assert(std::is_standard_layout_v<RuiTopology>);
static_assert(sizeof(RuiTopology) == 0xC0);
static_assert(offsetof(RuiTopology, parentEntityHandle) == 0x8);
static_assert(offsetof(RuiTopology, origin) == 0xC);
static_assert(offsetof(RuiTopology, lifecycleState) == 0x30);
static_assert(offsetof(RuiTopology, sphereRadius) == 0x3C);
static_assert(offsetof(RuiTopology, drawInfo) == 0x40);

bool RuiTopology_IsHidden(const RuiInstance* rui) noexcept;
