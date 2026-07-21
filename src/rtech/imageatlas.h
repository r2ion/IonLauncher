#pragma once

#include "rtech/pakstate.h"

#include <filesystem>

// Registers any RUI atlas sidecar stored next to a pak. The atlas is created
// after the pak finishes loading so its referenced texture is already bound.
void RuiImageAtlas_OnPakLoaded(const std::filesystem::path& pakPath, PakHandle_t handle);
void RuiImageAtlas_OnPakLoadCompleted(PakHandle_t handle);
void RuiImageAtlas_OnPakUnloading(PakHandle_t handle);
