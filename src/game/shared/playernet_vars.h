#pragma once

#include <cstddef>
#include <cstdint>

#include "game/shared/ehandle.h"
#include "mathlib/color.h"
#include "mathlib/mathlib.h"

struct audioparams_t
{
  public:
    void* __vftable;              // 0x00
    Vector3D localSound[8];       // 0x08
    std::int32_t soundscapeIndex; // 0x68
    std::int32_t localBits;       // 0x6C
    std::int32_t entIndex;        // 0x70
    std::byte _pad74[0x4];        // 0x74
};

static_assert(sizeof(audioparams_t) == 0x78);

struct fogparams_t
{
  public:
    void* __vftable;       // 0x00
    float botAlt;          // 0x08
    float topAlt;          // 0x0C
    float halfDistBot;     // 0x10
    float halfDistTop;     // 0x14
    float distColorStr;    // 0x18
    float dirColorStr;     // 0x1C
    float distOffset;      // 0x20
    float densityScale;    // 0x24
    float halfAngleDeg;    // 0x28
    float HDRColorScale;   // 0x2C
    color32 distColor;     // 0x30
    color32 dirColor;      // 0x34
    Vector3D direction;    // 0x38
    float minFadeTime;     // 0x44
    bool forceOntoSky;     // 0x48
    bool enable;           // 0x49
    std::byte _pad4A[0x2]; // 0x4A
    std::int32_t id;       // 0x4C
};

static_assert(sizeof(fogparams_t) == 0x50);

struct fogplayerparamsstate_t
{
  public:
    void* __vftable;       // 0x00
    bool enable;           // 0x08
    std::byte _pad9[0x3];  // 0x09
    float botAlt;          // 0x0C
    float topAlt;          // 0x10
    float halfDistBot;     // 0x14
    float halfDistTop;     // 0x18
    float distOffset;      // 0x1C
    float densityScale;    // 0x20
    float halfAngleDeg;    // 0x24
    float distColorStr;    // 0x28
    float dirColorStr;     // 0x2C
    float HDRColorScale;   // 0x30
    float minFadeTime;     // 0x34
    bool forceOntoSky;     // 0x38
    color32 distColor;     // 0x39
    color32 dirColor;      // 0x3D
    std::byte _pad41[0x3]; // 0x41
    Vector3D direction;    // 0x44
    std::int32_t id;       // 0x50
    std::byte _pad54[0x4]; // 0x54
};

static_assert(sizeof(fogplayerparamsstate_t) == 0x58);

struct fogplayerparams_t
{
  public:
    void* __vftable;                 // 0x00
    EHANDLE m_hCtrl;                 // 0x08
    float m_flTransitionStartTime;   // 0x0C
    fogplayerparamsstate_t m_Old;    // 0x10
    fogplayerparamsstate_t m_New;    // 0x68
    fogplayerparamsstate_t m_OldSky; // 0xC0
    fogplayerparamsstate_t m_NewSky; // 0x118
};

static_assert(sizeof(fogplayerparams_t) == 0x170);

struct sky3dparams_t
{
  public:
    void* __vftable;       // 0x00
    std::int32_t scale;    // 0x08
    std::int32_t cellNum;  // 0x0C
    bool useWorldFog;      // 0x10
    std::byte _pad11[0x7]; // 0x11
    fogparams_t fog;       // 0x18
};

static_assert(sizeof(sky3dparams_t) == 0x68);

