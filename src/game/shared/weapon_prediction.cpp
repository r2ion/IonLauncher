#include "game/shared/weapon_prediction.h"

#include "common/netmessages.h"
#include "dt_recv.h"
#include "engine/client/clientstate.h"
#include "engine/net_chan.h"
#include "engine/server/server.h"
#include "engine/server/sv_main.h"
#include "game/client/c_player.h"
#include "game/client/cdll_client_int.h"
#include "game/client/prediction.h"
#include "game/server/player.h"
#include "game/shared/igamemovement.h"
#include "game/shared/r1/c_weapon_x.h"
#include "game/shared/r1/weapon_x.h"
#include "game/shared/usercmd.h"
#include "modsystem/modmanager.h"
#include "tier0/hooks.h"
#include "tier1/generichash.h"
#include "tier1/keyvalues.h"
#include "vscript/languages/squirrel_re/squirrel.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

template <typename WeaponInfo>
using WeaponPredictionDefinitionMap = std::unordered_map<const WeaponInfo*, std::shared_ptr<PredictedWeaponDefinition_t>>;

WeaponPredictionDefinitionMap<FileWeaponInfo_Client> g_ClientWeaponPredictionDefinitions;
WeaponPredictionDefinitionMap<FileWeaponInfo_Server> g_ServerWeaponPredictionDefinitions;

thread_local WeaponPredictionCommand_t* g_pWeaponPredictionCommand = nullptr;

void CPlayerPredictionState::Clear()
{
    ++generation;
    player = nullptr;
    context = ScriptContext::INVALID;
    handle = 0xFFFFFFFFu;
    serverCount = -1;
    playerClass = -1;
    lastAuthoritativeCommand = -1;
    lastAcknowledged = -1;
    instances.fill(nullptr);
    movementHistory.reset();
    lastSent = {};
    sent = false;
    retired = false;
}

void CWeaponXPredictionState::Clear()
{
    weaponHandle = 0xFFFFFFFFu;
    owner = nullptr;
    ownerGeneration = 0;
    definition.reset();
    state = {};
    history.reset();
    presentationActive = false;
    presentationCommand = -1;
    ending = false;
}

bool PredictedWeaponState_t::Matches(const PredictedWeaponState_t& other, const int fieldCount) const
{
    return active == other.active && std::equal(values.begin(), values.begin() + fieldCount, other.values.begin());
}

bool PredictedWeaponWireState_t::Matches(const PredictedWeaponDefinition_t& definition) const
{
    return schema == definition.schema && fieldCount == definition.fieldCount &&
           std::equal(types.begin(), types.begin() + fieldCount, definition.types.begin());
}

const PredictedWeaponWireState_t* PredictedWeaponSnapshot_t::FindState(const std::uint32_t weaponHandle) const
{
    for (int i = 0; i < count; ++i)
        if (states[i].weaponHandle == weaponHandle)
            return &states[i];
    return nullptr;
}

void PredictedWeaponSnapshot_t::CopyFrom(const PredictedWeaponSnapshot_t& source)
{
    serverCount = source.serverCount;
    playerHandle = source.playerHandle;
    playerClass = source.playerClass;
    command = source.command;
    count = source.count;
    movement = source.movement;
    std::copy_n(source.states.begin(), source.count, states.begin());
}

void PredictedWeaponDefinition_t::Release()
{
    if (vm)
        for (HSCRIPT function : functions)
            vm->ReleaseFunction(function);
    functions.fill(nullptr);
    vm = nullptr;
    resolved = false;
    usable = false;
}

bool PredictedWeaponDefinition_t::Resolve(const ScriptContext context)
{
    CSquirrelVM* const currentVM = g_pSquirrel[context]->m_pSQVM;
    if (!currentVM)
        return false;
    if (vm != currentVM)
    {
        Release();
        vm = currentVM;
    }
    if (!resolved)
    {
        resolved = true;
        usable = true;
        for (int i = 0; i < PREDICTED_WEAPON_CALLBACK_COUNT; ++i)
        {
            if (names[i].empty())
                continue;
            if (context == ScriptContext::SERVER && i == PREDICTED_WEAPON_RECONCILE)
                continue;
            functions[i] = vm->FindFunction(names[i].c_str());
            if (!IsValid(functions[i]))
            {
                spdlog::error("Predicted weapon callback {} is unavailable; retaining legacy callbacks", names[i]);
                usable = false;
            }
        }
    }
    return usable;
}

const PredictedWeaponSnapshot_t* FindAuthoritativeWeaponPrediction(const int command, const int serverCount, const std::uint32_t playerHandle,
                                                                   const int playerClass)
{
    const C_PlayerExtended& prediction = C_Player::sm_PlayerExtended;
    for (unsigned int i = 0; i < MULTIPLAYER_BACKUP; ++i)
    {
        const auto& snapshot = prediction.m_Authoritative[(prediction.m_nNextAuthoritative + MULTIPLAYER_BACKUP - i - 1) % MULTIPLAYER_BACKUP];
        if (snapshot.serverCount == serverCount && snapshot.playerHandle == playerHandle && snapshot.playerClass == playerClass &&
            snapshot.command >= 0 && snapshot.command <= command)
            return &snapshot;
    }
    return nullptr;
}

CPlayerPredictionState* FindWeaponPredictionOwner(const ScriptContext context, const std::uint32_t handle)
{
    if (context == ScriptContext::CLIENT)
    {
        auto& owner = C_Player::sm_PlayerExtended.m_WeaponPrediction;
        return owner.player && owner.handle == handle ? &owner : nullptr;
    }
    for (auto& extension : CPlayer::sm_PlayersExtended)
    {
        auto& owner = extension.m_WeaponPrediction;
        if (owner.player && owner.handle == handle)
            return &owner;
    }
    return nullptr;
}

CPlayerPredictionState* GetWeaponPredictionOwner(C_Player* player, const int serverCount)
{
    if (!player)
        return nullptr;
    const CBaseHandle handle = player->GetRefEHandle();
    if (handle.GetEntryIndex() < 0 || handle.GetEntryIndex() >= NUM_ENT_ENTRIES)
        return nullptr;
    auto& storage = C_Player::sm_PlayerExtended.m_WeaponPrediction;
    if (storage.retired)
        return nullptr;
    if (storage.player != player || storage.handle != handle.ToInt() || storage.serverCount != serverCount ||
        storage.playerClass != player->pl.currentClass)
    {
        if (storage.player)
            RetireWeaponPredictionOwner(storage);
        storage.Clear();
        storage.context = ScriptContext::CLIENT;
        storage.player = player;
        storage.handle = handle.ToInt();
        storage.serverCount = serverCount;
        storage.playerClass = player->pl.currentClass;
        storage.movementHistory = std::make_unique<std::array<PredictedMovementFrame_t, MULTIPLAYER_BACKUP>>();
    }
    return &storage;
}

CPlayerPredictionState* GetWeaponPredictionOwner(CPlayer* player, const int serverCount)
{
    if (!player)
        return nullptr;
    const CBaseHandle handle = player->GetRefEHandle();
    if (handle.GetEntryIndex() < 0 || handle.GetEntryIndex() >= NUM_ENT_ENTRIES)
        return nullptr;
    CPlayerExtended* const extension = player->GetPlayerExtended();
    if (!extension)
        return nullptr;
    auto& storage = extension->m_WeaponPrediction;
    if (storage.retired)
        return nullptr;
    if (storage.player != player || storage.handle != handle.ToInt() || storage.serverCount != serverCount ||
        storage.playerClass != player->pl.currentClass)
    {
        if (storage.player)
            RetireWeaponPredictionOwner(storage);
        storage.Clear();
        storage.context = ScriptContext::SERVER;
        storage.player = player;
        storage.handle = handle.ToInt();
        storage.serverCount = serverCount;
        storage.playerClass = player->pl.currentClass;
    }
    return &storage;
}

static std::shared_ptr<PredictedWeaponDefinition_t> GetWeaponPredictionDefinition(C_WeaponX* weapon)
{
    if (weapon->GetWeaponFileInfoHandle() == INVALID_WEAPON_INFO_HANDLE)
        return nullptr;
    const auto found = g_ClientWeaponPredictionDefinitions.find(&weapon->GetWpnData());
    return found != g_ClientWeaponPredictionDefinitions.end() && found->second->Resolve(ScriptContext::CLIENT) ? found->second : nullptr;
}

static std::shared_ptr<PredictedWeaponDefinition_t> GetWeaponPredictionDefinition(CWeaponX* weapon)
{
    if (weapon->GetWeaponFileInfoHandle() == INVALID_WEAPON_INFO_HANDLE)
        return nullptr;
    const auto found = g_ServerWeaponPredictionDefinitions.find(&weapon->GetWpnData());
    return found != g_ServerWeaponPredictionDefinitions.end() && found->second->Resolve(ScriptContext::SERVER) ? found->second : nullptr;
}

template <typename Weapon>
static CWeaponXPredictionState* GetWeaponPredictionInstanceImpl(Weapon* weapon, CPlayerPredictionState& owner, const bool create)
{
    if (owner.retired)
        return nullptr;
    const CBaseHandle handle = weapon->GetRefEHandle();
    auto* const extension = weapon->GetWeaponExtended();
    CWeaponXPredictionState* const prediction = extension ? &extension->m_WeaponPrediction : nullptr;
    if (!prediction)
        return nullptr;
    if (prediction->weaponHandle == handle.ToInt() && prediction->owner == &owner && prediction->ownerGeneration == owner.generation)
        return prediction;
    if (!create)
        return nullptr;
    if (prediction->owner && prediction->ownerGeneration == prediction->owner->generation)
    {
        for (auto& slot : prediction->owner->instances)
            if (slot == prediction)
            {
                RetireWeaponPredictionInstance(*prediction->owner, slot, 2);
                break;
            }
    }
    else
        prediction->Clear();

    const auto definition = GetWeaponPredictionDefinition(weapon);
    if (!definition)
        return nullptr;
    for (auto& slot : owner.instances)
    {
        if (slot)
            continue;
        prediction->Clear();
        prediction->weaponHandle = handle.ToInt();
        prediction->owner = &owner;
        prediction->ownerGeneration = owner.generation;
        prediction->definition = definition;
        if (owner.context == ScriptContext::CLIENT)
        {
            prediction->history = std::make_unique<std::array<PredictedWeaponFrame_t, MULTIPLAYER_BACKUP>>();
            if (const auto* snapshot = FindAuthoritativeWeaponPrediction(owner.lastAcknowledged, owner.serverCount, owner.handle, owner.playerClass))
                if (const auto* state = snapshot->FindState(handle.ToInt()); state && state->Matches(*definition))
                    prediction->state = state->state;
        }
        slot = prediction;
        return prediction;
    }
    spdlog::error("Player exceeded {} simultaneous predicted weapon behaviors", MAX_PREDICTED_WEAPON_BEHAVIORS);
    return nullptr;
}

CWeaponXPredictionState* GetWeaponPredictionInstance(C_WeaponX* weapon, CPlayerPredictionState& owner, const bool create)
{
    return GetWeaponPredictionInstanceImpl(weapon, owner, create);
}

CWeaponXPredictionState* GetWeaponPredictionInstance(CWeaponX* weapon, CPlayerPredictionState& owner, const bool create)
{
    return GetWeaponPredictionInstanceImpl(weapon, owner, create);
}

void* ResolvePredictionEntity(const ScriptContext context, const std::uint32_t handle)
{
    if (context == ScriptContext::CLIENT)
        return CHandle<C_BaseEntity>::UnsafeFromIndex(static_cast<int>(handle)).Get();
    if (handle == 0xFFFFFFFFu)
        return nullptr;
    const unsigned int index = handle & 0xFFFFu;
    if (index >= NUM_ENT_ENTRIES)
        return nullptr;
    CBaseEntity* const entity = CBaseEntity::Instance(index);
    return entity && entity->GetRefEHandle().ToInt() == handle ? entity : nullptr;
}

void ExecutePredictedWeaponCallback(CPlayerPredictionState& owner, CWeaponXPredictionState& instance, const int callback, ScriptVariant_t* arguments,
                                    const int count)
{
    const auto definition = instance.definition;
    if (!definition->Resolve(owner.context) || !IsValid(definition->functions[callback]))
        return;
    if (definition->vm->ExecuteFunction(definition->functions[callback], arguments, count) != SCRIPT_DONE)
        spdlog::error("Predicted weapon callback {} failed", definition->names[callback]);
}

void EndWeaponPrediction(CPlayerPredictionState& owner, CWeaponXPredictionState& instance, const int reason)
{
    if (!instance.state.active || instance.ending)
        return;
    instance.state.active = false;
    if (ResolvePredictionEntity(owner.context, owner.handle) != owner.player)
        return;
    instance.ending = true;
    void* const weapon = ResolvePredictionEntity(owner.context, instance.weaponHandle);
    WeaponPredictionCommand_t command;
    InitializeWeaponPredictionLifecycle(command, owner);
    command.cleanupWeapon = instance.weaponHandle;
    command.cleanupInstance = &instance;
    RunWeaponPredictionCommand(command, [&]
    {
        ScriptVariant_t args[] = {
            g_pSquirrel[owner.context]->createscriptinstance(weapon),
            g_pSquirrel[owner.context]->createscriptinstance(owner.player),
            reason,
        };
        ExecutePredictedWeaponCallback(owner, instance, PREDICTED_WEAPON_END, args, 3);
    });
    instance.ending = false;
}

void RetireWeaponPredictionInstance(CPlayerPredictionState& owner, CWeaponXPredictionState*& slot, const int reason)
{
    CWeaponXPredictionState* const instance = slot;
    slot = nullptr;
    if (!instance)
        return;
    EndWeaponPrediction(owner, *instance, reason);
    instance->Clear();
}

void RetireWeaponPredictionOwner(CPlayerPredictionState& owner)
{
    if (owner.retired)
        return;
    owner.retired = true;
    for (auto& instance : owner.instances)
        RetireWeaponPredictionInstance(owner, instance, 3);
}

bool WeaponBelongsToPredictionOwner(const ScriptContext context, void* entity, void* player)
{
    if (!entity)
        return false;
    if (context == ScriptContext::CLIENT)
    {
        C_WeaponX* weapon = static_cast<C_BaseEntity*>(entity)->MyWeaponXPointer();
        return weapon && weapon->GetWeaponOwner() == player;
    }
    auto* base = static_cast<CBaseEntity*>(entity)->MyCombatWeaponPointer();
    auto* weapon = base ? static_cast<CWeaponX*>(base) : nullptr;
    return weapon && ResolvePredictionEntity(context, weapon->m_weaponOwner.ToInt()) == player;
}

void DispatchWeaponPredictionUpdates(WeaponPredictionCommand_t& command, const bool before)
{
    if (!command.owner || command.ownerGeneration != command.owner->generation || command.owner->retired)
        return;
    const float time = command.context == ScriptContext::CLIENT ? g_pClientGlobals->curtime : g_pGlobals->curtime;
    for (auto& slot : command.owner->instances)
    {
        if (command.owner->retired)
            break;
        const auto instance = slot;
        if (!instance)
            continue;
        void* weapon = ResolvePredictionEntity(command.context, instance->weaponHandle);
        if (!WeaponBelongsToPredictionOwner(command.context, weapon, command.player))
        {
            RetireWeaponPredictionInstance(*command.owner, slot, 2);
            continue;
        }
        if (!instance->state.active)
            continue;
        ScriptVariant_t args[] = {
            g_pSquirrel[command.context]->createscriptinstance(weapon),
            g_pSquirrel[command.context]->createscriptinstance(command.player),
            time,
            command.duration,
            before,
            command.firstExecution,
        };
        ExecutePredictedWeaponCallback(*command.owner, *instance, PREDICTED_WEAPON_UPDATE, args, 6);
    }
}

void InitializeWeaponPredictionCommand(WeaponPredictionCommand_t& prediction, C_Player* player, const CUserCmd* command)
{
    prediction = {};
    prediction.player = player;
    prediction.userCommand = command;
    prediction.context = ScriptContext::CLIENT;
    prediction.commandNumber = command ? command->command_number : -1;
    prediction.duration = command ? command->frameTime : 0.0f;
    if (command && player && player->UsesWeaponPrediction())
        prediction.owner = GetWeaponPredictionOwner(player, GetBaseLocalClient()->m_nServerCount);
    if (prediction.owner)
        prediction.ownerGeneration = prediction.owner->generation;
}

void InitializeWeaponPredictionCommand(WeaponPredictionCommand_t& prediction, CPlayer* player, const SV_CUserCmd* command)
{
    prediction = {};
    prediction.player = player;
    prediction.userCommand = command;
    prediction.context = ScriptContext::SERVER;
    prediction.commandNumber = command ? static_cast<int>(command->command_number) : -1;
    prediction.duration = command ? command->frameTime : 0.0f;
    if (command && player && player->UsesWeaponPrediction())
        prediction.owner = GetWeaponPredictionOwner(player, g_pServer->GetSpawnCount());
    if (prediction.owner)
        prediction.ownerGeneration = prediction.owner->generation;
}

void InitializeWeaponPredictionLifecycle(WeaponPredictionCommand_t& command, CPlayerPredictionState& owner, const bool readOnly)
{
    command = {};
    command.owner = &owner;
    command.ownerGeneration = owner.generation;
    command.player = owner.player;
    command.context = owner.context;
    command.readOnly = readOnly;
}

template <typename WeaponInfo>
static void ResetWeaponPredictionDefinition(WeaponPredictionDefinitionMap<WeaponInfo>& definitions, ScriptContext context,
                                            const WeaponInfo* weaponInfo);

template <typename WeaponInfo>
static void ParseWeaponPredictionDefinition(WeaponPredictionDefinitionMap<WeaponInfo>& definitions, const ScriptContext context,
                                            const WeaponInfo* weaponInfo, KeyValues* root, const char* weaponName)
{
    ResetWeaponPredictionDefinition(definitions, context, weaponInfo);
    const CClientState* client = nullptr;
    if (context == ScriptContext::CLIENT && GetBaseLocalClient)
        client = GetBaseLocalClient();
    const bool extended = context == ScriptContext::SERVER || (client && client->IsConnectedToExtendedServer());
    if (!extended || !root)
        return;
    KeyValues* block = root->FindKey("PredictedCallbacks");
    if (!block)
        return;
    if (!g_scriptCBInfo)
    {
        spdlog::error("Cannot parse predicted callbacks for {} before weapon callback metadata is available", weaponName);
        return;
    }
    const std::string script = g_pModManager->NormaliseModFilePath(std::filesystem::path("scripts/vscripts") / block->GetString("Script"));
    const auto source = g_pModManager->m_ModFiles.find(script);
    const char* ownerName = block->GetString("_SourceMod");
    if (!block->GetInt("_CompatibleSource", 0) || source == g_pModManager->m_ModFiles.end() || !source->second.m_pOwningMod ||
        source->second.m_pOwningMod->Name != ownerName)
    {
        spdlog::warn("Ignoring inherited predicted callbacks for {}: legacy/script provenance differs", weaponName);
        return;
    }
    auto definition = std::make_shared<PredictedWeaponDefinition_t>();
    CMurmurHash3 schema;
    schema.ProcessString(script.c_str());
    for (int i = 0; i < WEAPONX_SCRIPTCB_COUNT; ++i)
    {
        definition->names[i] = block->GetString(g_scriptCBInfo[i].name);
        schema.ProcessString(definition->names[i].c_str());
    }
    static constexpr const char* predictionCallbacks[] = {
        "OnPredictionUpdate",
        "OnPredictionEnd",
        "OnPredictionReconcile",
    };
    static_assert(std::size(predictionCallbacks) == PREDICTED_WEAPON_CALLBACK_COUNT - WEAPONX_SCRIPTCB_COUNT);
    for (int i = 0; i < std::size(predictionCallbacks); ++i)
    {
        const int callback = WEAPONX_SCRIPTCB_COUNT + i;
        definition->names[callback] = block->GetString(predictionCallbacks[i]);
        schema.ProcessString(definition->names[callback].c_str());
    }
    if (KeyValues* state = block->FindKey("State"))
    {
        for (KeyValues* field = state->GetFirstSubKey(); field; field = field->GetNextKey())
        {
            if (definition->fieldCount == MAX_PREDICTED_WEAPON_FIELDS)
            {
                spdlog::error("{} exceeds {} declared prediction fields", weaponName, MAX_PREDICTED_WEAPON_FIELDS);
                return;
            }
            const std::string_view type(field->GetString());
            fieldtype_t valueType;
            if (type == "bool")
                valueType = FIELD_BOOLEAN;
            else if (type == "int")
                valueType = FIELD_INTEGER;
            else if (type == "float")
                valueType = FIELD_FLOAT;
            else if (type == "vector")
                valueType = FIELD_VECTOR;
            else
            {
                spdlog::error("{} has unsupported prediction field type {}", weaponName, type);
                return;
            }
            definition->types[definition->fieldCount++] = valueType;
            schema.ProcessString(field->GetName());
            schema.ProcessString(field->GetString());
        }
    }
    definition->schema = schema.Finalize();
    definitions.insert_or_assign(weaponInfo, std::move(definition));
}

template <typename WeaponInfo>
static void ResetWeaponPredictionDefinition(WeaponPredictionDefinitionMap<WeaponInfo>& definitions, const ScriptContext context,
                                            const WeaponInfo* weaponInfo)
{
    const auto found = definitions.find(weaponInfo);
    if (found == definitions.end())
        return;
    const auto definition = found->second;
    definitions.erase(found);
    if (context == ScriptContext::CLIENT)
        C_PlayerExtended::ClearClientState();
    else
        for (auto& owner : CPlayer::sm_PlayersExtended)
            owner.Reset();
    definition->Release();
}

template <typename WeaponInfo>
static void ShutdownWeaponPredictionVM(const ScriptContext context, WeaponPredictionDefinitionMap<WeaponInfo>& definitions)
{
    if (context != ScriptContext::CLIENT && context != ScriptContext::SERVER)
        return;
    if (context == ScriptContext::CLIENT)
        C_PlayerExtended::ClearClientState();
    else
        for (auto& owner : CPlayer::sm_PlayersExtended)
            owner.Reset();
    for (auto& entry : definitions)
        entry.second->Release();
}

static void EndPlayerWeaponPrediction(const ScriptContext context, const std::uint32_t handle)
{
    CPlayerPredictionState* owner = FindWeaponPredictionOwner(context, handle);
    if (!owner)
        return;
    WeaponPredictionCommand_t command;
    InitializeWeaponPredictionLifecycle(command, *owner);
    RunWeaponPredictionCommand(command, [&]
    {
        for (CWeaponXPredictionState* instance : owner->instances)
            if (instance)
                EndWeaponPrediction(*owner, *instance, 3);
    });
}

void C_WeaponXExtended::ParseWeaponInfo(FileWeaponInfo_Client* weaponInfo, KeyValues* root, const char* weaponName)
{
    ParseWeaponPredictionDefinition(g_ClientWeaponPredictionDefinitions, ScriptContext::CLIENT, weaponInfo, root, weaponName);
}

void CWeaponXExtended::ParseWeaponInfo(FileWeaponInfo_Server* weaponInfo, KeyValues* root, const char* weaponName)
{
    ParseWeaponPredictionDefinition(g_ServerWeaponPredictionDefinitions, ScriptContext::SERVER, weaponInfo, root, weaponName);
}

void C_WeaponXExtended::ResetWeaponInfo(FileWeaponInfo_Client* weaponInfo)
{
    ResetWeaponPredictionDefinition(g_ClientWeaponPredictionDefinitions, ScriptContext::CLIENT, weaponInfo);
}

void CWeaponXExtended::ResetWeaponInfo(FileWeaponInfo_Server* weaponInfo)
{
    ResetWeaponPredictionDefinition(g_ServerWeaponPredictionDefinitions, ScriptContext::SERVER, weaponInfo);
}

void C_WeaponXExtended::OnVMShutdown()
{
    ShutdownWeaponPredictionVM(ScriptContext::CLIENT, g_ClientWeaponPredictionDefinitions);
}

void CWeaponXExtended::OnVMShutdown()
{
    ShutdownWeaponPredictionVM(ScriptContext::SERVER, g_ServerWeaponPredictionDefinitions);
}

void ForgetPredictedWeapon(CWeaponXPredictionState& prediction)
{
    if (prediction.weaponHandle == 0xFFFFFFFFu || prediction.ending)
        return;
    CPlayerPredictionState* const owner = prediction.owner;
    if (!owner || prediction.ownerGeneration != owner->generation)
    {
        prediction.Clear();
        return;
    }
    for (auto& slot : owner->instances)
    {
        if (slot != &prediction)
            continue;
        WeaponPredictionCommand_t command;
        InitializeWeaponPredictionLifecycle(command, *owner);
        RunWeaponPredictionCommand(command, [&] { RetireWeaponPredictionInstance(*owner, slot, 2); });
        return;
    }
    prediction.Clear();
}

void C_WeaponXExtended::Forget()
{
    ForgetPredictedWeapon(m_WeaponPrediction);
}

void CWeaponXExtended::Forget()
{
    ForgetPredictedWeapon(m_WeaponPrediction);
}

bool SelectWeaponPredictionBaseline(C_Player* player, const bool receivedNewWorldUpdate, const int acknowledgedCommand)
{
    bool mismatch = false;
    if (!player || !player->UsesWeaponPrediction())
        return false;
    CPlayerPredictionState* owner = GetWeaponPredictionOwner(player, GetBaseLocalClient()->m_nServerCount);
    if (!owner)
        return false;
    owner->lastAcknowledged = acknowledgedCommand;
    const auto* snapshot = FindAuthoritativeWeaponPrediction(acknowledgedCommand, owner->serverCount, owner->handle, owner->playerClass);
    if (!snapshot || (!receivedNewWorldUpdate && owner->lastAuthoritativeCommand == snapshot->command))
        return false;
    owner->lastAuthoritativeCommand = snapshot->command;
    for (const CWeaponXPredictionState* instance : owner->instances)
    {
        if (!instance || !instance->history || acknowledgedCommand < 0)
            continue;
        const auto& predicted = (*instance->history)[acknowledgedCommand % MULTIPLAYER_BACKUP];
        const auto* state = snapshot->FindState(instance->weaponHandle);
        if (state && !state->Matches(*instance->definition))
        {
            spdlog::error("Authoritative weapon prediction schema differs from the client declaration");
            mismatch = true;
            continue;
        }
        const PredictedWeaponState_t canonical = state ? state->state : PredictedWeaponState_t{};
        if (predicted.command != acknowledgedCommand || !predicted.state.Matches(canonical, instance->definition->fieldCount))
            mismatch = true;
    }
    if (owner->movementHistory && acknowledgedCommand >= 0)
    {
        const auto& predicted = (*owner->movementHistory)[acknowledgedCommand % MULTIPLAYER_BACKUP];
        if (predicted.command != acknowledgedCommand || predicted.movement != snapshot->movement)
            mismatch = true;
    }
    return mismatch;
}

void RestoreWeaponPredictionBaseline(CPlayerPredictionState& prediction, const int commandNumber)
{
    auto* owner = &prediction;
    if (!owner->player || commandNumber < 0 || ResolvePredictionEntity(ScriptContext::CLIENT, owner->handle) != owner->player)
        return;
    const auto* snapshot =
        FindAuthoritativeWeaponPrediction(std::min(commandNumber, owner->lastAcknowledged), owner->serverCount, owner->handle, owner->playerClass);
    for (CWeaponXPredictionState* instance : owner->instances)
    {
        if (!instance || !instance->history)
            continue;
        const auto& frame = (*instance->history)[commandNumber % MULTIPLAYER_BACKUP];
        if (commandNumber > owner->lastAcknowledged && frame.command == commandNumber)
            instance->state = frame.state;
        else
        {
            const auto* state = snapshot ? snapshot->FindState(instance->weaponHandle) : nullptr;
            instance->state = state && state->Matches(*instance->definition) ? state->state : PredictedWeaponState_t{};
        }
    }
    if (!owner->movementHistory)
        return;
    C_Player* const predictedPlayer = static_cast<C_Player*>(owner->player);
    const auto& frame = (*owner->movementHistory)[commandNumber % MULTIPLAYER_BACKUP];
    if (commandNumber > owner->lastAcknowledged && frame.command == commandNumber)
        predictedPlayer->RestorePredictedMovementState(frame.movement);
    else if (snapshot)
        predictedPlayer->RestorePredictedMovementState(snapshot->movement);
}

void StoreWeaponPredictionResults(CPlayerPredictionState& prediction, const int commandNumber)
{
    auto* owner = &prediction;
    if (!owner->player || !owner->movementHistory || commandNumber < 0 ||
        ResolvePredictionEntity(ScriptContext::CLIENT, owner->handle) != owner->player ||
        !static_cast<C_Player*>(owner->player)->UsesWeaponPrediction())
        return;
    auto& movement = (*owner->movementHistory)[commandNumber % MULTIPLAYER_BACKUP];
    movement.command = commandNumber;
    movement.movement = static_cast<C_Player*>(owner->player)->GetPredictedMovementState();
    for (CWeaponXPredictionState* instance : owner->instances)
    {
        if (owner->retired)
            break;
        if (!instance || !instance->history)
            continue;
        auto& frame = (*instance->history)[commandNumber % MULTIPLAYER_BACKUP];
        frame.command = commandNumber;
        frame.state.active = instance->state.active;
        std::copy_n(instance->state.values.begin(), instance->definition->fieldCount, frame.state.values.begin());
        if (commandNumber == GetBaseLocalClient()->m_nOutgoingCommandNumber &&
            (instance->presentationCommand != commandNumber || instance->presentationActive != instance->state.active))
        {
            if (void* weapon = ResolvePredictionEntity(ScriptContext::CLIENT, instance->weaponHandle))
            {
                WeaponPredictionCommand_t command;
                InitializeWeaponPredictionLifecycle(command, *owner, true);
                RunWeaponPredictionCommand(command, [&]
                {
                    ScriptVariant_t args[] = {
                        g_pSquirrel[ScriptContext::CLIENT]->createscriptinstance(weapon),
                        g_pSquirrel[ScriptContext::CLIENT]->createscriptinstance(owner->player),
                        instance->state.active,
                    };
                    ExecutePredictedWeaponCallback(*owner, *instance, PREDICTED_WEAPON_RECONCILE, args, 3);
                });
            }
            instance->presentationCommand = commandNumber;
            instance->presentationActive = instance->state.active;
        }
    }
}

DECLARE_MODULE(WeaponPredictionHooks)

DECLARE_HOOK(CPrediction_RunScriptCommand, client.dll + 0x2D7460,
             [](auto& hook, CPrediction* self, C_Player* player, CUserCmd* command, void* moveHelper) -> void*
{
    if (!player || !player->GetPlayerExtended())
        return hook.Original(self, player, command, moveHelper);
    WeaponPredictionCommand_t prediction;
    InitializeWeaponPredictionCommand(prediction, player, command);
    return RunWeaponPredictionCommand(prediction, [&] { return hook.Original(self, player, command, moveHelper); });
})

DECLARE_HOOK(ClientPredictionMovement, client.dll + 0x205140, [](auto& hook, void* self, C_Player* player, CMoveData* movement) -> void
{
    auto* command = g_pWeaponPredictionCommand;
    if (command && command->owner && command->ownerGeneration == command->owner->generation && command->context == ScriptContext::CLIENT &&
        command->player == player)
    {
        command->movement = movement;
        command->firstExecution = (static_cast<unsigned int>(movement->m_Flags) & 1) != 0;
        DispatchWeaponPredictionUpdates(*command, true);
    }
    hook.Original(self, player, movement);
    if (command && command->owner && command->ownerGeneration == command->owner->generation && command->context == ScriptContext::CLIENT &&
        command->player == player)
    {
        DispatchWeaponPredictionUpdates(*command, false);
        command->movement = nullptr;
    }
})

DECLARE_HOOK(ServerPredictionMovement, server.dll + 0x17ADE0, [](auto& hook, void* self, CPlayer* player, CMoveData* movement) -> void
{
    auto* command = g_pWeaponPredictionCommand;
    if (command && command->owner && command->ownerGeneration == command->owner->generation && command->context == ScriptContext::SERVER &&
        command->player == player)
    {
        command->movement = movement;
        command->firstExecution = (static_cast<unsigned int>(movement->m_Flags) & 1) != 0;
        DispatchWeaponPredictionUpdates(*command, true);
    }
    hook.Original(self, player, movement);
    if (command && command->owner && command->ownerGeneration == command->owner->generation && command->context == ScriptContext::SERVER &&
        command->player == player)
    {
        DispatchWeaponPredictionUpdates(*command, false);
        command->movement = nullptr;
    }
})

DECLARE_HOOK(ClientPredictedScriptCallback, client.dll + 0x10500,
             [](auto& hook, CSquirrelVM* vm, HSCRIPT function, ScriptVariant_t* args, int count, ScriptVariant_t* result, HSCRIPT scope,
                bool wait) -> ScriptStatus_t
{
    if (vm->vmContext == static_cast<int>(ScriptContext::CLIENT))
        function = SelectPredictedWeaponCallback(ScriptContext::CLIENT, function, args, count);
    return hook.Original(vm, function, args, count, result, scope, wait);
})

DECLARE_HOOK(ServerPredictedScriptCallback, server.dll + 0x1D930,
             [](auto& hook, CSquirrelVM* vm, HSCRIPT function, ScriptVariant_t* args, int count, ScriptVariant_t* result, HSCRIPT scope,
                bool wait) -> ScriptStatus_t
{
    function = SelectPredictedWeaponCallback(ScriptContext::SERVER, function, args, count);
    return hook.Original(vm, function, args, count, result, scope, wait);
})

RecvVarProxyFn s_OriginalPredictionClassProxy;

void PredictionClassProxy(const CRecvProxyData* data, void* base, void* output)
{
    auto& extension = C_Player::sm_PlayerExtended;
    auto& owner = extension.m_WeaponPrediction;
    if (owner.player)
    {
        auto* const player = static_cast<C_Player*>(owner.player);
        if (output == &player->pl.currentClass && player->pl.currentClass != data->m_Value.m_Value.m_Int &&
            ResolvePredictionEntity(ScriptContext::CLIENT, owner.handle) == player)
            EndPlayerWeaponPrediction(ScriptContext::CLIENT, player->GetRefEHandle().ToInt());
    }
    if (s_OriginalPredictionClassProxy)
        s_OriginalPredictionClassProxy(data, base, output);
    else
        *static_cast<int*>(output) = data->m_Value.m_Value.m_Int;
}

DECLARE_HOOK(ServerPredictionClassChange, server.dll + 0x5A4390, [](auto& hook, CPlayerState* state, void* field) -> void
{
    if (field == &state->currentClass)
    {
        auto* const player = reinterpret_cast<CPlayer*>(reinterpret_cast<std::byte*>(state) - offsetof(CPlayer, pl));
        EndPlayerWeaponPrediction(ScriptContext::SERVER, player->GetRefEHandle().ToInt());
    }
    hook.Original(state, field);
})

ON_DLL_LOAD_CLIENT("client.dll", ClientWeaponPrediction, [](CModule module)
{
    DISPATCH_HOOK(WeaponPredictionHooks, CPrediction_RunScriptCommand)
    DISPATCH_HOOK(WeaponPredictionHooks, ClientPredictionMovement)
    DISPATCH_HOOK(WeaponPredictionHooks, ClientPredictedScriptCallback)
    CRecvProp* classProperty = module.Offset(0xC21EE0).RCast<CRecvProp*>();
    s_OriginalPredictionClassProxy = classProperty->m_ProxyFn;
    classProperty->m_ProxyFn = PredictionClassProxy;
})

ON_DLL_LOAD("server.dll", ServerWeaponPrediction, [](CModule)
{
    DISPATCH_HOOK(WeaponPredictionHooks, ServerPredictionMovement)
    DISPATCH_HOOK(WeaponPredictionHooks, ServerPredictedScriptCallback)
    DISPATCH_HOOK(WeaponPredictionHooks, ServerPredictionClassChange)
})

PredictedWeaponSnapshot_t& SVC_WeaponPredictionState::GetSnapshot()
{
    return m_Snapshot;
}

SVC_WeaponPredictionState::SVC_WeaponPredictionState()
{
    m_nGroup = 0;
    m_bReliable = true;
    m_NetChannel = nullptr;
    m_pMessageHandler = nullptr;
}

int SVC_WeaponPredictionState::GetType() const
{
    return static_cast<int>(NetMessageType::svc_WeaponPredictionState);
}

const char* SVC_WeaponPredictionState::GetName() const
{
    return "svc_WeaponPredictionState";
}

const char* SVC_WeaponPredictionState::ToString() const
{
    return GetName();
}

std::size_t SVC_WeaponPredictionState::GetSize() const
{
    return sizeof(*this);
}

bool SVC_WeaponPredictionState::ReadWeaponState(bf_read* buffer, PredictedWeaponWireState_t& state)
{
    state.weaponHandle = buffer->ReadLong();
    state.schema = buffer->ReadLong();
    state.fieldCount = buffer->ReadUBitLong(PREDICTED_WEAPON_FIELD_COUNT_BITS);
    state.state.active = buffer->ReadOneBit() != 0;
    if (state.fieldCount > MAX_PREDICTED_WEAPON_FIELDS)
        return false;

    for (int field = 0; field < state.fieldCount; ++field)
    {
        const auto type = static_cast<fieldtype_t>(buffer->ReadUBitLong(PREDICTED_WEAPON_FIELD_TYPE_BITS));
        if (type != FIELD_BOOLEAN && type != FIELD_INTEGER && type != FIELD_FLOAT && type != FIELD_VECTOR)
            return false;
        state.types[field] = type;
        const int wordCount = type == FIELD_VECTOR ? 3 : 1;
        for (int word = 0; word < wordCount; ++word)
        {
            const std::uint32_t bits = buffer->ReadLong();
            state.state.values[field].bits[word] = bits;
            if ((type == FIELD_FLOAT || type == FIELD_VECTOR) && !std::isfinite(std::bit_cast<float>(bits)))
                return false;
        }
    }
    return true;
}

void SVC_WeaponPredictionState::WriteWeaponState(bf_write* buffer, const PredictedWeaponWireState_t& state)
{
    buffer->WriteLong(state.weaponHandle);
    buffer->WriteLong(state.schema);
    buffer->WriteUBitLong(state.fieldCount, PREDICTED_WEAPON_FIELD_COUNT_BITS);
    buffer->WriteOneBit(state.state.active);
    for (int field = 0; field < state.fieldCount; ++field)
    {
        buffer->WriteUBitLong(static_cast<unsigned int>(state.types[field]), PREDICTED_WEAPON_FIELD_TYPE_BITS);
        const int wordCount = state.types[field] == FIELD_VECTOR ? 3 : 1;
        for (int word = 0; word < wordCount; ++word)
            buffer->WriteLong(state.state.values[field].bits[word]);
    }
}

bool SVC_WeaponPredictionState::ReadFromBuffer(bf_read* buffer)
{
    m_Snapshot = {};
    m_Snapshot.serverCount = buffer->ReadLong();
    m_Snapshot.playerHandle = buffer->ReadLong();
    m_Snapshot.playerClass = buffer->ReadLong();
    m_Snapshot.command = buffer->ReadLong();
    m_Snapshot.count = buffer->ReadUBitLong(PREDICTED_WEAPON_BEHAVIOR_COUNT_BITS);
    if (m_Snapshot.count > MAX_PREDICTED_WEAPON_BEHAVIORS || m_Snapshot.command < 0)
        return false;

    for (float& scale : m_Snapshot.movement.scales)
    {
        scale = std::bit_cast<float>(buffer->ReadLong());
        if (!std::isfinite(scale))
            return false;
    }
    for (bool& flag : m_Snapshot.movement.flags)
        flag = buffer->ReadOneBit() != 0;
    for (int i = 0; i < m_Snapshot.count; ++i)
        if (!ReadWeaponState(buffer, m_Snapshot.states[i]))
            return false;
    return !buffer->IsOverflowed();
}

bool SVC_WeaponPredictionState::WriteToBuffer(bf_write* buffer)
{
    buffer->WriteLong(m_Snapshot.serverCount);
    buffer->WriteLong(m_Snapshot.playerHandle);
    buffer->WriteLong(m_Snapshot.playerClass);
    buffer->WriteLong(m_Snapshot.command);
    buffer->WriteUBitLong(m_Snapshot.count, PREDICTED_WEAPON_BEHAVIOR_COUNT_BITS);
    for (float scale : m_Snapshot.movement.scales)
        buffer->WriteLong(std::bit_cast<std::int32_t>(scale));
    for (bool flag : m_Snapshot.movement.flags)
        buffer->WriteOneBit(flag);
    for (int i = 0; i < m_Snapshot.count; ++i)
        WriteWeaponState(buffer, m_Snapshot.states[i]);
    return !buffer->IsOverflowed();
}

bool SVC_WeaponPredictionState::Process()
{
    CClientState* const client = GetBaseLocalClient ? GetBaseLocalClient() : nullptr;
    if (!client || m_NetChannel != client->m_NetChannel || !client->IsConnectedToExtendedServer())
        return false;
    if (m_Snapshot.serverCount != client->m_nServerCount)
        return true;

    auto& prediction = C_Player::sm_PlayerExtended;
    prediction.m_Authoritative[prediction.m_nNextAuthoritative].CopyFrom(m_Snapshot);
    prediction.m_nNextAuthoritative = (prediction.m_nNextAuthoritative + 1) % MULTIPLAYER_BACKUP;
    C_Player* const player = C_Player::GetLocalPlayer();
    if (player && player->GetRefEHandle().ToInt() == m_Snapshot.playerHandle && player->pl.currentClass == m_Snapshot.playerClass)
    {
        CPlayerPredictionState* owner = GetWeaponPredictionOwner(player, m_Snapshot.serverCount);
        for (int i = 0; owner && i < m_Snapshot.count; ++i)
        {
            auto* entity = static_cast<C_BaseEntity*>(ResolvePredictionEntity(ScriptContext::CLIENT, m_Snapshot.states[i].weaponHandle));
            C_WeaponX* weapon = entity ? entity->MyWeaponXPointer() : nullptr;
            if (weapon && weapon->GetWeaponOwner() == player)
                GetWeaponPredictionInstance(weapon, *owner, true);
        }
    }
    return true;
}

ON_DLL_LOAD("engine.dll", WeaponPredictionNetMessage, [](CModule) { g_NetMessageHandler->AddMessageSVC<SVC_WeaponPredictionState>(); })

bool WeaponPredictionSnapshotChanged(const CPlayerPredictionState& owner, const PredictedMovementState& movement)
{
    if (!owner.sent || owner.lastSent.movement != movement)
        return true;
    int count = 0;
    for (const auto& instance : owner.instances)
    {
        if (!instance)
            continue;
        if (count >= owner.lastSent.count)
            return true;
        const PredictedWeaponWireState_t& state = owner.lastSent.states[count++];
        if (state.weaponHandle != instance->weaponHandle || state.schema != instance->definition->schema ||
            !state.state.Matches(instance->state, instance->definition->fieldCount))
            return true;
    }
    return count != owner.lastSent.count;
}

void SendWeaponPredictionSnapshot(WeaponPredictionCommand_t& command)
{
    if (!command.owner || command.ownerGeneration != command.owner->generation || command.owner->retired ||
        command.context != ScriptContext::SERVER || command.commandNumber < 0 ||
        ResolvePredictionEntity(command.context, command.owner->handle) != command.player)
        return;
    CPlayerPredictionState& owner = *command.owner;
    CPlayer* player = static_cast<CPlayer*>(command.player);
    CClient* const client = &g_pClientArray[player->m_nPlayerIndex - 1];
    if (!client->m_NetChannel)
        return;
    const PredictedMovementState movement = player->GetPredictedMovementState();
    if (!WeaponPredictionSnapshotChanged(owner, movement))
        return;
    SVC_WeaponPredictionState message;
    PredictedWeaponSnapshot_t& snapshot = message.GetSnapshot();
    snapshot.serverCount = owner.serverCount;
    snapshot.playerHandle = owner.handle;
    snapshot.playerClass = owner.playerClass;
    snapshot.command = command.commandNumber;
    snapshot.movement = movement;
    for (const auto& instance : owner.instances)
    {
        if (!instance)
            continue;
        PredictedWeaponWireState_t& state = snapshot.states[snapshot.count++];
        state.weaponHandle = instance->weaponHandle;
        state.schema = instance->definition->schema;
        state.fieldCount = instance->definition->fieldCount;
        state.types = instance->definition->types;
        state.state = instance->state;
    }

	if (client->m_NetChannel->SendNetMsg(message, true, false))
    {
        owner.lastSent.CopyFrom(snapshot);
        owner.sent = true;
    }
}
