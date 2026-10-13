#pragma once

#include "datamap.h"
#include "game/shared/playerdata.h"
#include "game/shared/predictioncopy.h"
#include "game/shared/weapon_parse.h"
#include "mathlib/vector.h"
#include "vscript/ivscript.h"

#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

class C_Player;
class CPlayer;
class C_WeaponX;
class CWeaponX;
class SV_CUserCmd;

#define MAX_PREDICTED_WEAPON_FIELDS 16
#define MAX_PREDICTED_WEAPON_BEHAVIORS 16
#define PREDICTED_WEAPON_BEHAVIOR_COUNT_BITS std::bit_width(static_cast<unsigned int>(MAX_PREDICTED_WEAPON_BEHAVIORS))
#define PREDICTED_WEAPON_FIELD_COUNT_BITS std::bit_width(static_cast<unsigned int>(MAX_PREDICTED_WEAPON_FIELDS))
#define PREDICTED_WEAPON_FIELD_TYPE_BITS 3

enum PredictedWeaponCallback_t : int
{
    PREDICTED_WEAPON_UPDATE = WEAPONX_SCRIPTCB_COUNT,
    PREDICTED_WEAPON_END,
    PREDICTED_WEAPON_RECONCILE,
    PREDICTED_WEAPON_CALLBACK_COUNT,
};

struct PredictedWeaponValue_t
{
    std::array<std::uint32_t, 3> bits{};
    bool operator==(const PredictedWeaponValue_t&) const = default;
};

struct PredictedWeaponState_t
{
    bool Matches(const PredictedWeaponState_t& other, int fieldCount) const;

    std::array<PredictedWeaponValue_t, MAX_PREDICTED_WEAPON_FIELDS> values{};
    bool active = false;
};

struct PredictedWeaponFrame_t
{
    int command = -1;
    PredictedWeaponState_t state;
};

struct PredictedWeaponDefinition_t
{
    void Release();
    bool Resolve(ScriptContext context);

    std::array<std::string, PREDICTED_WEAPON_CALLBACK_COUNT> names;
    std::array<HSCRIPT, PREDICTED_WEAPON_CALLBACK_COUNT> functions{};
    std::array<fieldtype_t, MAX_PREDICTED_WEAPON_FIELDS> types{};
    int fieldCount = 0;
    std::uint32_t schema = 0;
    CSquirrelVM* vm = nullptr;
    bool resolved = false;
    bool usable = false;
};

struct PredictedWeaponWireState_t
{
    bool Matches(const PredictedWeaponDefinition_t& definition) const;

    std::uint32_t weaponHandle = 0;
    std::uint32_t schema = 0;
    int fieldCount = 0;
    std::array<fieldtype_t, MAX_PREDICTED_WEAPON_FIELDS> types{};
    PredictedWeaponState_t state;
};

struct PredictedWeaponSnapshot_t
{
    const PredictedWeaponWireState_t* FindState(std::uint32_t weaponHandle) const;
    void CopyFrom(const PredictedWeaponSnapshot_t& source);

    int serverCount = -1;
    std::uint32_t playerHandle = 0;
    int playerClass = -1;
    int command = -1;
    int count = 0;
    PredictedMovementState movement;
    std::array<PredictedWeaponWireState_t, MAX_PREDICTED_WEAPON_BEHAVIORS> states{};
};

struct CWeaponXPredictionState;

struct CPlayerPredictionState
{
    void Clear();

    void* player = nullptr;
    ScriptContext context = ScriptContext::INVALID;
    std::uint32_t handle = 0xFFFFFFFFu;
    std::uint32_t generation = 0;
    int serverCount = -1;
    int playerClass = -1;
    int lastAuthoritativeCommand = -1;
    int lastAcknowledged = -1;
    std::array<CWeaponXPredictionState*, MAX_PREDICTED_WEAPON_BEHAVIORS> instances{};
    std::unique_ptr<std::array<PredictedMovementFrame_t, MULTIPLAYER_BACKUP>> movementHistory;
    PredictedWeaponSnapshot_t lastSent;
    bool sent = false;
    bool retired = false;
};

struct CWeaponXPredictionState
{
    void Clear();

    std::uint32_t weaponHandle = 0xFFFFFFFFu;
    CPlayerPredictionState* owner = nullptr;
    std::uint32_t ownerGeneration = 0;
    std::shared_ptr<PredictedWeaponDefinition_t> definition;
    PredictedWeaponState_t state;
    std::unique_ptr<std::array<PredictedWeaponFrame_t, MULTIPLAYER_BACKUP>> history;
    bool presentationActive = false;
    int presentationCommand = -1;
    bool ending = false;
};

struct WeaponPredictionCommand_t
{
    CPlayerPredictionState* owner = nullptr;
    std::uint32_t ownerGeneration = 0;
    void* player = nullptr;
    class CMoveData* movement = nullptr;
    const void* userCommand = nullptr;
    ScriptContext context = ScriptContext::INVALID;
    int commandNumber = -1;
    float duration = 0.0f;
    bool firstExecution = false;
    bool readOnly = false;
    std::uint32_t cleanupWeapon = 0xFFFFFFFFu;
    CWeaponXPredictionState* cleanupInstance = nullptr;
};

extern thread_local WeaponPredictionCommand_t* g_pWeaponPredictionCommand;

CWeaponXPredictionState* GetWeaponPredictionInstance(C_WeaponX* weapon, CPlayerPredictionState& owner, bool create);
CWeaponXPredictionState* GetWeaponPredictionInstance(CWeaponX* weapon, CPlayerPredictionState& owner, bool create);
void* ResolvePredictionEntity(ScriptContext context, std::uint32_t handle);
void EndWeaponPrediction(CPlayerPredictionState& owner, CWeaponXPredictionState& instance, int reason);
bool SelectWeaponPredictionBaseline(C_Player* player, bool receivedNewWorldUpdate, int acknowledgedCommand);
void RestoreWeaponPredictionBaseline(CPlayerPredictionState& prediction, int commandNumber);
void StoreWeaponPredictionResults(CPlayerPredictionState& prediction, int commandNumber);
void InitializeWeaponPredictionCommand(WeaponPredictionCommand_t& command, CPlayer* player, const SV_CUserCmd* input);
void SendWeaponPredictionSnapshot(WeaponPredictionCommand_t& command);
HSCRIPT SelectPredictedWeaponCallback(ScriptContext context, HSCRIPT function, ScriptVariant_t* arguments, int count);
void RegisterWeaponPredictionScriptMethods(ScriptContext context, ScriptClassDesc_t* scriptClass);
void RegisterPlayerPredictionScriptMethods(ScriptContext context, ScriptClassDesc_t* scriptClass);
void RetireWeaponPredictionOwner(CPlayerPredictionState& owner);
void InitializeWeaponPredictionLifecycle(WeaponPredictionCommand_t& command, CPlayerPredictionState& owner, bool readOnly = false);

template <typename Function> decltype(auto) RunWeaponPredictionCommand(WeaponPredictionCommand_t& command, Function&& function)
{
    WeaponPredictionCommand_t* const previous = g_pWeaponPredictionCommand;
    g_pWeaponPredictionCommand = &command;
    struct RestoreCommand
    {
        WeaponPredictionCommand_t& command;
        WeaponPredictionCommand_t* previous;

        ~RestoreCommand()
        {
            assert(g_pWeaponPredictionCommand == &command);
            g_pWeaponPredictionCommand = previous;
        }
    } restore{command, previous};
    return std::forward<Function>(function)();
}
