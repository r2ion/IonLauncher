#pragma once

#include "rtech/rui/rui_image_atlas_types.h"
#include "tier0/module.h"

#include <cstdint>

// Native engine/RTech bridge used by the dynamic image-atlas service. The
// reserve/commit pair requires the native descriptor-map lock. Removal is also
// unlocked and must run from a serialized replacement path or under that lock.
bool RuiImageAtlas_AreRuntimeBindingsReady();
uint64_t RuiImageAtlas_HashAssetPath(const char* path);
RuiImageAssetDescriptor* RuiImageAtlas_FindAssetDescriptor(uint32_t nameHash);
RuiImageAssetDescriptor* RuiImageAtlas_FindOrReserveDescriptorUnlocked(
	uint32_t nameHash,
	uint8_t* reservedNewEntry);
bool RuiImageAtlas_HasFreeDescriptor();
void RuiImageAtlas_CommitReservedDescriptor();
uint32_t* RuiImageAtlas_RemoveExistingDescriptor(uint32_t nameHash);

uint32_t RuiImageAtlas_CreateGpuBuffer(
	RuiImageAtlas* atlas,
	const RuiImageAtlasGpuRecord* records);
void RuiImageAtlas_DestroyGpuBuffer(RuiImageAtlas* atlas);

void RuiImageAtlas_OnEngineLoaded(CModule module);
void RuiImageAtlas_DispatchRtechHooks();
