#pragma once

#include "materialsystem/imaterialsystem.h"

inline constexpr unsigned int AMD_VENDOR_ID = 0x1002;
inline constexpr unsigned int NVIDIA_VENDOR_ID = 0x10DE;

IMaterialSystem* MaterialSystem();
