#include "game/shared/util_shared.h"
#include "game/shared/weapon_prediction.h"

#include "game/client/c_player.h"
#include "game/server/player.h"
#include "game/shared/igamemovement.h"
#include "game/shared/r1/c_weapon_x.h"
#include "game/shared/r1/weapon_x.h"
#include "game/shared/usercmd.h"
#include "mathlib/mathlib.h"
#include "vscript/languages/squirrel_re/squirrel.h"

#include <bit>
#include <cmath>
#include <type_traits>

template <ScriptContext context> static HSCRIPT SelectPredictedWeaponCallback(HSCRIPT function, ScriptVariant_t* arguments, const int count)
{
    if (count < 1 || arguments[0].GetType() != FIELD_HSCRIPT || !IsValid(arguments[0].m_hScript))
        return function;
    SquirrelManager* const manager = g_pSquirrel[context];
    void* entity = manager->__sq_getentityfrominstance(manager->m_pSQVM, reinterpret_cast<SQObject*>(arguments[0].m_hScript),
                                                       manager->__sq_GetEntityConstant_CBaseEntity());
    if (!entity)
        return function;
    WeaponPredictionCommand_t* const command = g_pWeaponPredictionCommand;
    if constexpr (context == ScriptContext::CLIENT)
    {
        C_WeaponX* weapon = static_cast<C_BaseEntity*>(entity)->MyWeaponXPointer();
        if (!weapon)
            return function;
        if (weapon->m_callbacks[WEAPONX_SCRIPTCB_OWNER_CHANGED] == function)
            weapon->GetWeaponExtended()->Forget();
        if (!command || !command->owner || command->ownerGeneration != command->owner->generation || command->readOnly ||
            command->context != context || weapon->GetWeaponOwner() != command->player)
            return function;
        for (int i = 0; i < WEAPONX_SCRIPTCB_COUNT; ++i)
            if (weapon->m_callbacks[i] == function)
                if (auto* instance = GetWeaponPredictionInstance(weapon, *command->owner, true))
                    return IsValid(instance->definition->functions[i]) ? instance->definition->functions[i] : function;
    }
    else
    {
        auto* base = static_cast<CBaseEntity*>(entity)->MyCombatWeaponPointer();
        CWeaponX* weapon = base ? static_cast<CWeaponX*>(base) : nullptr;
        if (!weapon)
            return function;
        if (weapon->m_weaponScriptCB_[WEAPONX_SCRIPTCB_OWNER_CHANGED] == function)
            weapon->GetWeaponExtended()->Forget();
        if (!command || !command->owner || command->ownerGeneration != command->owner->generation || command->readOnly ||
            command->context != context || ResolvePredictionEntity(context, weapon->m_weaponOwner.ToInt()) != command->player)
            return function;
        for (int i = 0; i < WEAPONX_SCRIPTCB_COUNT; ++i)
            if (weapon->m_weaponScriptCB_[i] == function)
                if (auto* instance = GetWeaponPredictionInstance(weapon, *command->owner, true))
                    return IsValid(instance->definition->functions[i]) ? instance->definition->functions[i] : function;
    }
    return function;
}

HSCRIPT SelectPredictedWeaponCallback(const ScriptContext context, HSCRIPT function, ScriptVariant_t* arguments, const int count)
{
    if (!IsScriptWeaponPredictionEnabled())
        return function;
    if (context == ScriptContext::CLIENT)
        return SelectPredictedWeaponCallback<ScriptContext::CLIENT>(function, arguments, count);
    if (context == ScriptContext::SERVER)
        return SelectPredictedWeaponCallback<ScriptContext::SERVER>(function, arguments, count);
    return function;
}

template <ScriptContext context> using PredictedScriptWeapon_t = std::conditional_t<context == ScriptContext::CLIENT, C_WeaponX, CWeaponX>;

template <ScriptContext context>
static CWeaponXPredictionState* GetPredictedScriptWeapon(HSQUIRRELVM sqvm, const bool write, bool* validOwner = nullptr)
{
    if (validOwner)
        *validOwner = false;
    auto* manager = g_pSquirrel[context];
    WeaponPredictionCommand_t* const command = g_pWeaponPredictionCommand;
    if (!command || !command->owner || command->ownerGeneration != command->owner->generation || command->context != context ||
        (write && command->readOnly) || (command->owner->retired && !command->cleanupInstance) ||
        ResolvePredictionEntity(context, command->owner->handle) != command->player)
    {
        manager->raiseerror(sqvm, "Prediction state requires a command/lifecycle scope; reconciliation is read-only");
        return nullptr;
    }

    PredictedScriptWeapon_t<context>* weapon = nullptr;
    if (!manager->getthisentity(sqvm, &weapon) || !weapon)
    {
        manager->raiseerror(sqvm, "Prediction method requires a valid WeaponX receiver");
        return nullptr;
    }
    if (command->cleanupInstance && command->cleanupWeapon == weapon->GetRefEHandle().ToInt())
    {
        if (validOwner)
            *validOwner = true;
        return command->cleanupInstance;
    }

    CWeaponXPredictionState* instance = nullptr;
    if constexpr (context == ScriptContext::CLIENT)
    {
        if (weapon->GetWeaponOwner() == command->player)
        {
            if (validOwner)
            {
                *validOwner = true;
                return GetWeaponPredictionInstance(weapon, *command->owner, false);
            }
            instance = GetWeaponPredictionInstance(weapon, *command->owner, !command->readOnly);
        }
    }
    else if (ResolvePredictionEntity(context, weapon->m_weaponOwner.ToInt()) == command->player)
    {
        if (validOwner)
        {
            *validOwner = true;
            return GetWeaponPredictionInstance(weapon, *command->owner, false);
        }
        instance = GetWeaponPredictionInstance(weapon, *command->owner, !command->readOnly);
    }
    if (!instance)
        manager->raiseerror(sqvm, "Weapon has no compatible prediction declaration for the command owner");
    return instance;
}

template <ScriptContext context> static PredictedWeaponValue_t* GetPredictedScriptValue(HSQUIRRELVM sqvm, const fieldtype_t type, const bool write)
{
    CWeaponXPredictionState* instance = GetPredictedScriptWeapon<context>(sqvm, write);
    if (!instance)
        return nullptr;
    const int slot = g_pSquirrel[context]->getinteger(sqvm, 1);
    if (slot < 0 || slot >= instance->definition->fieldCount || instance->definition->types[slot] != type)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Prediction slot is out of range or has a different declared type");
        return nullptr;
    }
    return &instance->state.values[slot];
}

template <ScriptContext context> static WeaponPredictionCommand_t* GetWeaponPredictionCommand(HSQUIRRELVM sqvm, const bool write = false)
{
    WeaponPredictionCommand_t* const command = g_pWeaponPredictionCommand;
    if (command && command->owner && command->ownerGeneration == command->owner->generation && command->context == context &&
        !(write && command->readOnly) && (!command->owner->retired || command->cleanupInstance) &&
        ResolvePredictionEntity(context, command->owner->handle) == command->player)
        return command;
    g_pSquirrel[context]->raiseerror(sqvm, "Prediction movement requires a command/lifecycle scope; reconciliation is read-only");
    return nullptr;
}

ADD_SQFUNC("int", Prediction_GetCommandNumber, "", "Identifies the accepted player command for matching predicted presentation.",
           ScriptContext::CLIENT | ScriptContext::SERVER)
{
    const auto* command = g_pWeaponPredictionCommand;
    if (!command || !command->owner || command->ownerGeneration != command->owner->generation || command->context != context || !command->userCommand)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Prediction_GetCommandNumber requires an accepted player command");
        return SQRESULT_ERROR;
    }
    g_pSquirrel[context]->pushinteger(sqvm, command->commandNumber);
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("float", Prediction_RandomFloat, "int stream, float minimum, float maximum",
           "Returns command-seeded randomness without reading or changing the shared random stream.", ScriptContext::CLIENT | ScriptContext::SERVER)
{
    auto* manager = g_pSquirrel[context];
    const auto* command = g_pWeaponPredictionCommand;
    if (!command || !command->owner || command->ownerGeneration != command->owner->generation || command->context != context || !command->userCommand)
    {
        manager->raiseerror(sqvm, "Prediction_RandomFloat requires an accepted player command");
        return SQRESULT_ERROR;
    }
    const float minimum = manager->getfloat(sqvm, 2);
    const float maximum = manager->getfloat(sqvm, 3);
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum)
    {
        manager->raiseerror(sqvm, "Prediction_RandomFloat requires a finite, ordered range");
        return SQRESULT_ERROR;
    }
    std::uint32_t seed;
    if constexpr (context == ScriptContext::CLIENT)
        seed = static_cast<const CUserCmd*>(command->userCommand)->random_seed;
    else
        seed = static_cast<const SV_CUserCmd*>(command->userCommand)->random_seed;

    const std::uint32_t stream = static_cast<std::uint32_t>(manager->getinteger(sqvm, 1));
    static thread_local CUniformRandomStream randomStream;
    manager->pushfloat(sqvm, SharedRandomFloat(randomStream, std::bit_cast<std::int32_t>(seed), stream, minimum, maximum));
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("int", Prediction_GetEntityHandle, "entity ent", "Reads the exact encoded entity handle, including its serial.",
           ScriptContext::CLIENT | ScriptContext::SERVER)
{
    auto* manager = g_pSquirrel[context];
    using Entity = std::conditional_t<context == ScriptContext::CLIENT, C_BaseEntity, CBaseEntity>;
    Entity* const entity = manager->template getentity<Entity>(sqvm, 1);
    if (!entity)
    {
        manager->raiseerror(sqvm, "Prediction_GetEntityHandle requires a valid entity");
        return SQRESULT_ERROR;
    }
    const std::uint32_t handle = entity->GetRefEHandle().ToInt();
    if (ResolvePredictionEntity(context, handle) != entity)
    {
        manager->raiseerror(sqvm, "Prediction_GetEntityHandle requires a live entity handle");
        return SQRESULT_ERROR;
    }
    manager->pushinteger(sqvm, std::bit_cast<std::int32_t>(handle));
    return SQRESULT_NOTNULL;
}

template <ScriptContext context> int Script_Prediction_GetBool(HSQUIRRELVM sqvm)
{
    auto* value = GetPredictedScriptValue<context>(sqvm, FIELD_BOOLEAN, false);
    if (!value)
        return SQRESULT_ERROR;
    g_pSquirrel[context]->pushbool(sqvm, value->bits[0] != 0);
    return SQRESULT_NOTNULL;
}
template <ScriptContext context> int Script_Prediction_SetBool(HSQUIRRELVM sqvm)
{
    auto* value = GetPredictedScriptValue<context>(sqvm, FIELD_BOOLEAN, true);
    if (!value)
        return SQRESULT_ERROR;
    value->bits[0] = g_pSquirrel[context]->getbool(sqvm, 2) != 0;
    return SQRESULT_NULL;
}
template <ScriptContext context> int Script_Prediction_GetInt(HSQUIRRELVM sqvm)
{
    auto* value = GetPredictedScriptValue<context>(sqvm, FIELD_INTEGER, false);
    if (!value)
        return SQRESULT_ERROR;
    g_pSquirrel[context]->pushinteger(sqvm, std::bit_cast<std::int32_t>(value->bits[0]));
    return SQRESULT_NOTNULL;
}
template <ScriptContext context> int Script_Prediction_SetInt(HSQUIRRELVM sqvm)
{
    auto* value = GetPredictedScriptValue<context>(sqvm, FIELD_INTEGER, true);
    if (!value)
        return SQRESULT_ERROR;
    value->bits[0] = static_cast<std::uint32_t>(g_pSquirrel[context]->getinteger(sqvm, 2));
    return SQRESULT_NULL;
}
template <ScriptContext context> int Script_Prediction_GetFloat(HSQUIRRELVM sqvm)
{
    auto* value = GetPredictedScriptValue<context>(sqvm, FIELD_FLOAT, false);
    if (!value)
        return SQRESULT_ERROR;
    g_pSquirrel[context]->pushfloat(sqvm, std::bit_cast<float>(value->bits[0]));
    return SQRESULT_NOTNULL;
}
template <ScriptContext context> int Script_Prediction_SetFloat(HSQUIRRELVM sqvm)
{
    auto* value = GetPredictedScriptValue<context>(sqvm, FIELD_FLOAT, true);
    if (!value)
        return SQRESULT_ERROR;
    const float number = g_pSquirrel[context]->getfloat(sqvm, 2);
    if (!std::isfinite(number))
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Prediction float must be finite");
        return SQRESULT_ERROR;
    }
    value->bits[0] = std::bit_cast<std::uint32_t>(number);
    return SQRESULT_NULL;
}
template <ScriptContext context> int Script_Prediction_GetVector(HSQUIRRELVM sqvm)
{
    auto* value = GetPredictedScriptValue<context>(sqvm, FIELD_VECTOR, false);
    if (!value)
        return SQRESULT_ERROR;
    g_pSquirrel[context]->pushvector(
        sqvm, Vector3D(std::bit_cast<float>(value->bits[0]), std::bit_cast<float>(value->bits[1]), std::bit_cast<float>(value->bits[2])));
    return SQRESULT_NOTNULL;
}
template <ScriptContext context> int Script_Prediction_SetVector(HSQUIRRELVM sqvm)
{
    auto* value = GetPredictedScriptValue<context>(sqvm, FIELD_VECTOR, true);
    if (!value)
        return SQRESULT_ERROR;
    const Vector3D vector = g_pSquirrel[context]->getvector(sqvm, 2);
    for (int i = 0; i < 3; ++i)
    {
        if (!std::isfinite(vector[i]))
        {
            g_pSquirrel[context]->raiseerror(sqvm, "Prediction vector must be finite");
            return SQRESULT_ERROR;
        }
    }
    for (int i = 0; i < 3; ++i)
        value->bits[i] = std::bit_cast<std::uint32_t>(vector[i]);
    return SQRESULT_NULL;
}
template <ScriptContext context> int Script_Prediction_Start(HSQUIRRELVM sqvm)
{
    auto* instance = GetPredictedScriptWeapon<context>(sqvm, true);
    if (!instance)
        return SQRESULT_ERROR;
    WeaponPredictionCommand_t* const command = g_pWeaponPredictionCommand;
    if (instance->ending || command->owner->retired)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Cannot restart a retiring predicted behavior");
        return SQRESULT_ERROR;
    }
    instance->state.active = true;
    return SQRESULT_NULL;
}
template <ScriptContext context> int Script_Prediction_Stop(HSQUIRRELVM sqvm)
{
    auto* instance = GetPredictedScriptWeapon<context>(sqvm, true);
    if (!instance)
        return SQRESULT_ERROR;
    EndWeaponPrediction(*g_pWeaponPredictionCommand->owner, *instance, g_pSquirrel[context]->getinteger(sqvm, 1));
    return SQRESULT_NULL;
}
template <ScriptContext context> int Script_Prediction_IsActive(HSQUIRRELVM sqvm)
{
    bool validOwner;
    auto* instance = GetPredictedScriptWeapon<context>(sqvm, false, &validOwner);
    if (!validOwner)
        return SQRESULT_ERROR;
    g_pSquirrel[context]->pushbool(sqvm, instance && instance->state.active);
    return SQRESULT_NOTNULL;
}

using ScriptMethod_t = int (*)(HSQUIRRELVM);

static void AddScriptMethod(ScriptClassDesc_t* scriptClass, const char* scriptName, const char* nativeName, const char* description,
                            const char* returnType, const char* parameters, ScriptMethod_t method)
{
    auto& binding = scriptClass->m_NativeFunctionBindings[scriptClass->m_NativeFunctionBindings.AddToTail()];
    binding.Init(scriptName, nativeName, description, returnType, parameters, false, method);
}

template <ScriptContext context> static void RegisterPredictedWeaponScriptMethods(ScriptClassDesc_t* scriptClass)
{
    AddScriptMethod(scriptClass, "Prediction_GetBool", "Script_Prediction_GetBool", "Reads a declared replayable bool.", "bool", "int slot",
                    &Script_Prediction_GetBool<context>);
    AddScriptMethod(scriptClass, "Prediction_SetBool", "Script_Prediction_SetBool", "Writes a declared replayable bool.", "void",
                    "int slot, bool value", &Script_Prediction_SetBool<context>);
    AddScriptMethod(scriptClass, "Prediction_GetInt", "Script_Prediction_GetInt", "Reads a declared replayable integer.", "int", "int slot",
                    &Script_Prediction_GetInt<context>);
    AddScriptMethod(scriptClass, "Prediction_SetInt", "Script_Prediction_SetInt", "Writes a declared replayable integer.", "void",
                    "int slot, int value", &Script_Prediction_SetInt<context>);
    AddScriptMethod(scriptClass, "Prediction_GetFloat", "Script_Prediction_GetFloat", "Reads a declared replayable float.", "float", "int slot",
                    &Script_Prediction_GetFloat<context>);
    AddScriptMethod(scriptClass, "Prediction_SetFloat", "Script_Prediction_SetFloat", "Writes a declared replayable float.", "void",
                    "int slot, float value", &Script_Prediction_SetFloat<context>);
    AddScriptMethod(scriptClass, "Prediction_GetVector", "Script_Prediction_GetVector", "Reads a declared replayable vector.", "vector", "int slot",
                    &Script_Prediction_GetVector<context>);
    AddScriptMethod(scriptClass, "Prediction_SetVector", "Script_Prediction_SetVector", "Writes a declared replayable vector.", "void",
                    "int slot, vector value", &Script_Prediction_SetVector<context>);
    AddScriptMethod(scriptClass, "Prediction_Start", "Script_Prediction_Start", "Starts synchronous per-command prediction updates.", "void", "",
                    &Script_Prediction_Start<context>);
    AddScriptMethod(scriptClass, "Prediction_Stop", "Script_Prediction_Stop", "Ends a predicted behavior and invokes its cleanup callback.", "void",
                    "int reason", &Script_Prediction_Stop<context>);
    AddScriptMethod(scriptClass, "Prediction_IsActive", "Script_Prediction_IsActive",
                    "Queries this weapon's rewindable active state without creating it.", "bool", "", &Script_Prediction_IsActive<context>);
}

template <ScriptContext context> using PredictedScriptPlayer_t = std::conditional_t<context == ScriptContext::CLIENT, C_Player, CPlayer>;

template <ScriptContext context>
static PredictedScriptPlayer_t<context>* GetPredictedScriptPlayer(HSQUIRRELVM sqvm, bool write, WeaponPredictionCommand_t** commandOut = nullptr)
{
    if (commandOut)
        *commandOut = nullptr;
    auto* command = GetWeaponPredictionCommand<context>(sqvm, write);
    if (!command)
        return nullptr;

    auto* manager = g_pSquirrel[context];
    PredictedScriptPlayer_t<context>* player = nullptr;
    if (!manager->getthisentity(sqvm, &player) || !player)
    {
        manager->raiseerror(sqvm, "Prediction movement requires a valid player receiver");
        return nullptr;
    }
    if (player != command->player)
    {
        manager->raiseerror(sqvm, "Prediction movement receiver must own the current command");
        return nullptr;
    }
    if (commandOut)
        *commandOut = command;
    return player;
}

template <ScriptContext context> int Script_Prediction_GetInputDirection(HSQUIRRELVM sqvm)
{
    WeaponPredictionCommand_t* command = nullptr;
    if (!GetPredictedScriptPlayer<context>(sqvm, false, &command))
        return SQRESULT_ERROR;
    if (!command->userCommand)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Prediction input requires an accepted command, not a lifecycle scope");
        return SQRESULT_ERROR;
    }

    QAngle angles;
    float forwardMove, sideMove;
    if constexpr (context == ScriptContext::CLIENT)
    {
        const auto* input = static_cast<const CUserCmd*>(command->userCommand);
        angles = QAngle(0.0f, input->viewangles.y, 0.0f);
        forwardMove = input->forwardmove;
        sideMove = input->sidemove;
    }
    else
    {
        const auto* input = static_cast<const SV_CUserCmd*>(command->userCommand);
        angles = QAngle(0.0f, input->worldViewAngles.y, 0.0f);
        forwardMove = input->move.x;
        sideMove = input->move.y;
    }
    Vector3D forward, right;
    AngleVectors(angles, &forward, &right, nullptr);
    Vector3D direction = forward * forwardMove + right * sideMove;
    const float length = direction.Length();
    direction = length > 0.0f ? direction / length : Vector3D(1.0f, 0.0f, 0.0f);
    g_pSquirrel[context]->pushvector(sqvm, direction);
    return SQRESULT_NOTNULL;
}

template <ScriptContext context> int Script_Prediction_GetVelocity(HSQUIRRELVM sqvm)
{
    WeaponPredictionCommand_t* command = nullptr;
    auto* player = GetPredictedScriptPlayer<context>(sqvm, false, &command);
    if (!player)
        return SQRESULT_ERROR;
    const Vector3D velocity = command->movement ? command->movement->m_Velocity : player->GetPredictedMovementVelocity();
    g_pSquirrel[context]->pushvector(sqvm, velocity);
    return SQRESULT_NOTNULL;
}

template <ScriptContext context> int Script_Prediction_SetVelocity(HSQUIRRELVM sqvm)
{
    WeaponPredictionCommand_t* command = nullptr;
    auto* player = GetPredictedScriptPlayer<context>(sqvm, true, &command);
    if (!player)
        return SQRESULT_ERROR;
    const Vector3D velocity = g_pSquirrel[context]->getvector(sqvm, 1);
    if (!std::isfinite(velocity.x) || !std::isfinite(velocity.y) || !std::isfinite(velocity.z))
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Prediction velocity must be finite");
        return SQRESULT_ERROR;
    }
    if (command->movement)
        command->movement->m_Velocity = velocity;
    player->SetPredictedMovementVelocity(velocity);
    return SQRESULT_NULL;
}

template <ScriptContext context> int Script_Prediction_SetDodgePower(HSQUIRRELVM sqvm)
{
    static_assert(context == ScriptContext::CLIENT);
    WeaponPredictionCommand_t* command = nullptr;
    auto* player = GetPredictedScriptPlayer<context>(sqvm, true, &command);
    if (!player)
        return SQRESULT_ERROR;
    const float power = g_pSquirrel[context]->getfloat(sqvm, 1);
    if (!command->userCommand || !std::isfinite(power))
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Prediction dodge power requires an accepted command and finite value");
        return SQRESULT_ERROR;
    }
    player->SetPredictedDodgePower(power);
    return SQRESULT_NULL;
}

template <ScriptContext context> int Script_Prediction_GetMovementScale(HSQUIRRELVM sqvm)
{
    auto* player = GetPredictedScriptPlayer<context>(sqvm, false);
    if (!player)
        return SQRESULT_ERROR;
    const int settingValue = g_pSquirrel[context]->getinteger(sqvm, 1);
    if (settingValue < 0 || settingValue >= PREDICTED_MOVEMENT_SCALE_COUNT)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Invalid movement scale");
        return SQRESULT_ERROR;
    }
    const auto setting = static_cast<PredictedMovementScale>(settingValue);
    g_pSquirrel[context]->pushfloat(sqvm, player->GetPredictedMovementScale(setting));
    return SQRESULT_NOTNULL;
}

template <ScriptContext context> int Script_Prediction_SetMovementScale(HSQUIRRELVM sqvm)
{
    auto* player = GetPredictedScriptPlayer<context>(sqvm, true);
    if (!player)
        return SQRESULT_ERROR;
    const int settingValue = g_pSquirrel[context]->getinteger(sqvm, 1);
    const float value = g_pSquirrel[context]->getfloat(sqvm, 2);
    if (settingValue < 0 || settingValue >= PREDICTED_MOVEMENT_SCALE_COUNT || !std::isfinite(value))
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Invalid movement scale/value");
        return SQRESULT_ERROR;
    }
    const auto setting = static_cast<PredictedMovementScale>(settingValue);
    player->SetPredictedMovementScale(setting, value);
    return SQRESULT_NULL;
}

template <ScriptContext context> int Script_Prediction_GetMovementFlag(HSQUIRRELVM sqvm)
{
    auto* player = GetPredictedScriptPlayer<context>(sqvm, false);
    if (!player)
        return SQRESULT_ERROR;
    const int settingValue = g_pSquirrel[context]->getinteger(sqvm, 1);
    if (settingValue < 0 || settingValue >= PREDICTED_MOVEMENT_FLAG_COUNT)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Invalid movement flag");
        return SQRESULT_ERROR;
    }
    const auto setting = static_cast<PredictedMovementFlag>(settingValue);
    g_pSquirrel[context]->pushbool(sqvm, player->GetPredictedMovementFlag(setting));
    return SQRESULT_NOTNULL;
}

template <ScriptContext context> int Script_Prediction_SetMovementFlag(HSQUIRRELVM sqvm)
{
    auto* player = GetPredictedScriptPlayer<context>(sqvm, true);
    if (!player)
        return SQRESULT_ERROR;
    const int settingValue = g_pSquirrel[context]->getinteger(sqvm, 1);
    const bool value = g_pSquirrel[context]->getbool(sqvm, 2) != 0;
    if (settingValue < 0 || settingValue >= PREDICTED_MOVEMENT_FLAG_COUNT)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Invalid movement flag");
        return SQRESULT_ERROR;
    }
    const auto setting = static_cast<PredictedMovementFlag>(settingValue);
    player->SetPredictedMovementFlag(setting, value);
    return SQRESULT_NULL;
}

template <ScriptContext context> static void RegisterPredictedPlayerScriptMethods(ScriptClassDesc_t* scriptClass)
{
    AddScriptMethod(scriptClass, "Prediction_GetInputDirection", "Script_Prediction_GetInputDirection",
                    "Reads planar movement direction from this player's accepted command, not live input.", "vector", "",
                    &Script_Prediction_GetInputDirection<context>);
    AddScriptMethod(scriptClass, "Prediction_GetVelocity", "Script_Prediction_GetVelocity",
                    "Reads this player's movement velocity for the accepted command.", "vector", "", &Script_Prediction_GetVelocity<context>);
    AddScriptMethod(scriptClass, "Prediction_SetVelocity", "Script_Prediction_SetVelocity",
                    "Writes this player's entity and in-flight movement velocity without changing time.", "void", "vector velocity",
                    &Script_Prediction_SetVelocity<context>);
    if constexpr (context == ScriptContext::CLIENT)
        AddScriptMethod(scriptClass, "Prediction_SetDodgePower", "Script_Prediction_SetDodgePower",
                        "Sets this player's native-predicted suit power for the accepted command.", "void", "float power",
                        &Script_Prediction_SetDodgePower<context>);
    AddScriptMethod(scriptClass, "Prediction_GetMovementScale", "Script_Prediction_GetMovementScale",
                    "Reads this player's gravity, air speed/acceleration, friction or exact power regeneration.", "float", "int setting",
                    &Script_Prediction_GetMovementScale<context>);
    AddScriptMethod(scriptClass, "Prediction_SetMovementScale", "Script_Prediction_SetMovementScale",
                    "Writes a command-rewindable movement scale for this player.", "void", "int setting, float value",
                    &Script_Prediction_SetMovementScale<context>);
    AddScriptMethod(scriptClass, "Prediction_GetMovementFlag", "Script_Prediction_GetMovementFlag",
                    "Reads this player's dodge-disabled or disembark-enabled flag.", "bool", "int setting",
                    &Script_Prediction_GetMovementFlag<context>);
    AddScriptMethod(scriptClass, "Prediction_SetMovementFlag", "Script_Prediction_SetMovementFlag",
                    "Writes a command-rewindable movement flag for this player.", "void", "int setting, bool value",
                    &Script_Prediction_SetMovementFlag<context>);
}

void RegisterWeaponPredictionScriptMethods(const ScriptContext context, ScriptClassDesc_t* scriptClass)
{
    if (context == ScriptContext::CLIENT)
        RegisterPredictedWeaponScriptMethods<ScriptContext::CLIENT>(scriptClass);
    else if (context == ScriptContext::SERVER)
        RegisterPredictedWeaponScriptMethods<ScriptContext::SERVER>(scriptClass);
}

void RegisterPlayerPredictionScriptMethods(const ScriptContext context, ScriptClassDesc_t* scriptClass)
{
    if (context == ScriptContext::CLIENT)
        RegisterPredictedPlayerScriptMethods<ScriptContext::CLIENT>(scriptClass);
    else if (context == ScriptContext::SERVER)
        RegisterPredictedPlayerScriptMethods<ScriptContext::SERVER>(scriptClass);
}
