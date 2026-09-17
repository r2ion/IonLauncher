#pragma once

#include "mathlib/ssemath.h"
#include "rtech/rui/rui.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

using RuiTessellate_t = bool (*)(RuiDrawInfo*, const RuiBaseUv*, RuiDrawQuad*, RuiDrawBatch*);
using RuiEvaluateProjectionBasis_t =
    fltx4* (*)(const RuiInstance*, const RuiProjectedQuad*, FourVectors*, FourVectors*, FourVectors*);

struct RuiProjectionBasis
{
    fltx4 positionOrigin;
    fltx4 positionBasisX;
    fltx4 positionBasisY;
    fltx4 secondaryOrigin;
    fltx4 secondaryBasisY;
    fltx4 secondaryBasisX;
};

using RuiTopologyHandle = uint32_t;

#define RUI_TOPOLOGY_CAPACITY 64
#define RUI_TOPOLOGY_INDEX_MASK (RUI_TOPOLOGY_CAPACITY - 1)

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
