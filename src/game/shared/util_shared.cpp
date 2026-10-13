#include "game/shared/util_shared.h"

#include <bit>

constexpr std::uint32_t Util_Shared_QuickHashInt(std::uint32_t hash, std::uint32_t value)
{
    for (int byte = 0; byte < 4; ++byte)
    {
        hash += value & 0xFFu;
        hash += hash << 10;
        hash ^= hash >> 6;
        value >>= 8;
    }
    return hash;
}

constexpr std::uint32_t Util_Shared_QuickFinalize(std::uint32_t hash)
{
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

int SharedSeedFileLineHash(const int seedValue, const std::uint32_t sharedName, int)
{
    std::uint32_t hash = 0;
    hash = Util_Shared_QuickHashInt(hash, std::bit_cast<std::uint32_t>(seedValue));
    hash = Util_Shared_QuickHashInt(hash, 0);
    hash = Util_Shared_QuickHashInt(hash, sharedName);
    return std::bit_cast<std::int32_t>(Util_Shared_QuickFinalize(hash));
}

float SharedRandomFloat(IUniformRandomStream& randomStream, const int seedValue, const std::uint32_t sharedName, const float minimum,
                        const float maximum, const int additionalSeed)
{
    randomStream.SetSeed(SharedSeedFileLineHash(seedValue, sharedName, additionalSeed));
    return randomStream.RandomFloat(minimum, maximum);
}
