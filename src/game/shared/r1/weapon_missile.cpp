#include "game/shared/r1/weapon_missile.h"

#include "game/shared/usercmd.h"
#include "game/shared/util_shared.h"
#include "game/shared/weapon_prediction.h"

#include <bit>
#include <cstdint>

template <ScriptContext context> void ApplyPredictedMissileRandomDrift(MissilePathSettings& settings)
{
    WeaponPredictionCommand_t* const command = g_pWeaponPredictionCommand;
    if (!command || !command->owner || command->ownerGeneration != command->owner->generation || command->context != context ||
        !command->userCommand || command->readOnly)
        return;

    std::uint32_t seed;
    if constexpr (context == ScriptContext::CLIENT)
        seed = static_cast<const CUserCmd*>(command->userCommand)->random_seed;
    else
        seed = static_cast<const SV_CUserCmd*>(command->userCommand)->random_seed;

    static thread_local CUniformRandomStream randomStream;
    constexpr std::uint32_t missileDriftStream = 0x4D445246;
    settings.randomFloat =
        SharedRandomFloat(randomStream, std::bit_cast<std::int32_t>(seed), missileDriftStream, 0.0f, 1.0f, command->missileRandomDriftIndex++);
}

template void ApplyPredictedMissileRandomDrift<ScriptContext::CLIENT>(MissilePathSettings& settings);
template void ApplyPredictedMissileRandomDrift<ScriptContext::SERVER>(MissilePathSettings& settings);
