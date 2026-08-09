#pragma once

#include "engine/modelloader.h"
#include "mathlib/vector.h"
#include "model_types.h"
#include "tier1/keyvalues.h"

#include <cstddef>
#include <cstdint>

using ModelFileNameHandle_t = std::int32_t;
using ModelStudioHandle_t = std::uint16_t;

struct worldbrushdata_t;
class CEngineSprite;

struct brushdata_t
{
	worldbrushdata_t* m_pShared;
	std::int32_t m_nFirstModelSurface;
	std::int32_t m_nModelSurfaces;
	std::int32_t m_nLightStyleLastComputedFrame;
	std::uint16_t m_nLightStyleIndex;
	std::uint16_t m_nLightStyleCount;
	std::uint16_t m_nRenderHandle;
	std::uint16_t m_nFirstNode;
};

struct spritedata_t
{
	std::int32_t m_nFrames;
	std::int32_t m_nWidth;
	std::int32_t m_nHeight;
	std::uint32_t m_Padding0C;
	CEngineSprite* m_pSprite;
};

union model_data_t
{
	brushdata_t m_Brush;
	ModelStudioHandle_t m_Studio;
	spritedata_t m_Sprite;
};

struct model_t
{
	ModelFileNameHandle_t m_FileNameHandle;
	char m_PathName[260];
	IModelLoader::REFERENCETYPE m_LoadFlags;
	std::int32_t m_ServerCount;
	modtype_t m_Type;
	std::int32_t m_Flags;
	Vector3 m_Mins;
	Vector3 m_Maxs;
	float m_Radius;
	std::uint32_t m_Padding134;
	KeyValues* m_pKeyValues;
	model_data_t m_Data;
};

static_assert(sizeof(brushdata_t) == 0x20);
static_assert(sizeof(spritedata_t) == 0x18);
static_assert(sizeof(model_data_t) == 0x20);
static_assert(sizeof(model_t) == 0x160);
static_assert(offsetof(model_t, m_PathName) == 0x4);
static_assert(offsetof(model_t, m_LoadFlags) == 0x108);
static_assert(offsetof(model_t, m_Type) == 0x110);
static_assert(offsetof(model_t, m_Mins) == 0x118);
static_assert(offsetof(model_t, m_Radius) == 0x130);
static_assert(offsetof(model_t, m_pKeyValues) == 0x138);
static_assert(offsetof(model_t, m_Data) == 0x140);
