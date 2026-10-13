#pragma once

#include "vstdlib/random.h"

#include <cstdint>

int SharedSeedFileLineHash(int seedValue, std::uint32_t sharedName, int additionalSeed = 0);
float SharedRandomFloat(IUniformRandomStream& randomStream, int seedValue, std::uint32_t sharedName, float minimum, float maximum,
                        int additionalSeed = 0);
