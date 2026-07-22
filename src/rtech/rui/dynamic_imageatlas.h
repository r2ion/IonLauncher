#pragma once

#include "rtech/pakasset.h"

#include <filesystem>
#include <optional>
#include <string>

// Registers any RUI atlas sidecar stored next to a pak. The atlas is created
// after the pak finishes loading so its referenced texture is already bound.
void RuiDynamicImageAtlas_OnPakLoaded(const std::filesystem::path& pakPath, PakHandle_t handle);
void RuiDynamicImageAtlas_OnPakLoadCompleted(PakHandle_t handle);
void RuiDynamicImageAtlas_OnPakUnloading(PakHandle_t handle);

std::optional<int32_t> RuiDynamicImageAtlas_Create(
	uint64_t textureGuid,
	const char* jsonData,
	std::string& errorMessage);
bool RuiDynamicImageAtlas_Destroy(int32_t atlasHandle);

// Called by the native image lookup hook before the engine resolves a name.
bool RuiDynamicImageAtlas_TryRegisterImage(const char* imagePath);
