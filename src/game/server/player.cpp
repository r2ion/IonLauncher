#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "dedicated/dedicated.h"
#include "dedicated/dedicatedlogtoclient.h"
#include "engine/client/clientstate.h"
#include "engine/server/server.h"
#include "engine/server/serverauthentication.h"
#include "engine/server/sv_client.h"
#include "engine/server/sv_main.h"
#include "game/server/player.h"
#include "game/shared/r1/weapon_x.h"
#include "game/shared/usercmd.h"
#include "tier0/hooks.h"
#include "tier0/vanilla.h"
#include "tier1/convar.h"
#include "tier1/cvar.h"
#include "util/utils.h"
#include "vscript/languages/squirrel_re/squirrel.h"

class CLagCompensationManager;

CPlayer*(__fastcall* s_UTIL_PlayerByIndex)(int playerIndex);

DECLARE_MODULE(PlayerHooks)

ConVar* Cvar_ns_enable_weapon_attack_callback;
ConVar* Cvar_ns_skip_historical_hitbox_check;
ConVar* Cvar_ns_allow_post_death_shots;

bool g_bReachedLagCompensationRayGeometry;
static ConVar* sv_maxunlag;

void (*CPlayer__SetGroundFrictionScale)(CPlayer*, float);
void (*CPlayer__SetWallrunFrictionScale)(CPlayer*, float);
void (*CPlayer__Script_TurnDodgeDisabledOn)(CPlayer*);
void (*CPlayer__Script_TurnDodgeDisabledOff)(CPlayer*);
void (*CPlayer__SetTitanDisembarkEnabled)(CPlayer*, bool);
void (*CPlayer__SetPowerRegenRateScale)(CPlayer*, float);
void (*CPlayer__ItemPostFrame)(CPlayer*);

CPlayer* UTIL_PlayerByIndex(const int playerIndex)
{
    return s_UTIL_PlayerByIndex(playerIndex);
}

CPlayerExtended CPlayer::sm_PlayersExtended[MAX_PLAYERS];

CPlayerExtended* CPlayer::GetPlayerExtended()
{
    return m_nPlayerIndex > 0 && m_nPlayerIndex <= MAX_PLAYERS ? &sm_PlayersExtended[m_nPlayerIndex - 1] : nullptr;
}

CPlayerExtended::CPlayerExtended()
{
    Clear();
}

void CPlayerExtended::Clear()
{
    m_WeaponPrediction.Clear();
    m_PostDeathWeapon = {};
    m_PostDeathReplay = {};
    m_bProtectPostDeathWeapon = false;
    m_bCyclingObserverTarget = false;
}

void CPlayerExtended::Reset()
{
    auto& prediction = m_WeaponPrediction;
    if (prediction.retired)
    {
        m_PostDeathWeapon = {};
        m_PostDeathReplay = {};
        m_bProtectPostDeathWeapon = false;
        m_bCyclingObserverTarget = false;
        return;
    }
    if (prediction.player)
        RetireWeaponPredictionOwner(prediction);
    Clear();
}

bool CPlayer::UsesWeaponPrediction() const
{
    if (g_pVanillaCompatibility->GetVanillaCompatibility() || !g_pServer || m_nPlayerIndex == 0 ||
        m_nPlayerIndex > static_cast<unsigned int>(g_pServer->GetMaxClients()))
        return false;
    return g_pClientArray[m_nPlayerIndex - 1].GetClientExtended()->IsExtendedClient();
}

void CPlayer::SetGroundFrictionScale(const float scale)
{
    CPlayer__SetGroundFrictionScale(this, scale);
}

void CPlayer::SetWallrunFrictionScale(const float scale)
{
    CPlayer__SetWallrunFrictionScale(this, scale);
}

void CPlayer::Script_TurnDodgeDisabledOn()
{
    CPlayer__Script_TurnDodgeDisabledOn(this);
}

void CPlayer::Script_TurnDodgeDisabledOff()
{
    CPlayer__Script_TurnDodgeDisabledOff(this);
}

void CPlayer::SetTitanDisembarkEnabled(const bool enabled)
{
    CPlayer__SetTitanDisembarkEnabled(this, enabled);
}

void CPlayer::SetPowerRegenRateScale(const float scale)
{
    CPlayer__SetPowerRegenRateScale(this, scale);
}

PredictedMovementState CPlayer::GetPredictedMovementState()
{
    PredictedMovementState state;
    for (int i = 0; i < PREDICTED_MOVEMENT_SCALE_COUNT; ++i)
        state.scales[i] = GetPredictedMovementScale(static_cast<PredictedMovementScale>(i));
    for (int i = 0; i < PREDICTED_MOVEMENT_FLAG_COUNT; ++i)
        state.flags[i] = GetPredictedMovementFlag(static_cast<PredictedMovementFlag>(i));
    return state;
}

float CPlayer::GetPredictedMovementScale(const PredictedMovementScale setting)
{
    switch (setting)
    {
    case PredictedMovementScale::Gravity:
        return m_flGravity;
    case PredictedMovementScale::AirSpeed:
        return m_airSpeed;
    case PredictedMovementScale::AirAcceleration:
        return m_airAcceleration;
    case PredictedMovementScale::GroundFriction:
        return m_groundFrictionScale;
    case PredictedMovementScale::WallrunFriction:
        return m_wallrunFrictionScale;
    case PredictedMovementScale::PowerRegenRate:
        return m_Local.m_powerRegenRateScale;
    default:
        assert(false);
        return 0.0f;
    }
}

void CPlayer::SetPredictedMovementScale(const PredictedMovementScale setting, const float value)
{
    switch (setting)
    {
    case PredictedMovementScale::Gravity:
        m_flGravity = value;
        break;
    case PredictedMovementScale::AirSpeed:
    case PredictedMovementScale::AirAcceleration:
    {
        float& field = setting == PredictedMovementScale::AirSpeed ? m_airSpeed : m_airAcceleration;
        if (field != value)
        {
            NetworkStateChanged();
            field = value;
        }
        break;
    }
    case PredictedMovementScale::GroundFriction:
        SetGroundFrictionScale(value);
        break;
    case PredictedMovementScale::WallrunFriction:
        SetWallrunFrictionScale(value);
        break;
    case PredictedMovementScale::PowerRegenRate:
        SetPowerRegenRateScale(value);
        break;
    default:
        assert(false);
        break;
    }
}

bool CPlayer::GetPredictedMovementFlag(const PredictedMovementFlag setting) const
{
    switch (setting)
    {
    case PredictedMovementFlag::DodgeDisabled:
        return (m_playerFlags & PF_DODGE_DISABLED) != 0;
    case PredictedMovementFlag::TitanDisembarkEnabled:
        return m_Local.m_titanDisembarkEnabled;
    default:
        assert(false);
        return false;
    }
}

void CPlayer::SetPredictedMovementFlag(const PredictedMovementFlag setting, const bool value)
{
    switch (setting)
    {
    case PredictedMovementFlag::DodgeDisabled:
        if (value)
            Script_TurnDodgeDisabledOn();
        else
            Script_TurnDodgeDisabledOff();
        break;
    case PredictedMovementFlag::TitanDisembarkEnabled:
        SetTitanDisembarkEnabled(value);
        break;
    default:
        assert(false);
        break;
    }
}

Vector3D CPlayer::GetPredictedMovementVelocity()
{
    return GetAbsVelocity();
}

void CPlayer::SetPredictedMovementVelocity(const Vector3D& velocity)
{
    SetAbsVelocity(velocity);
}

static CPlayerExtended::PostDeathReplayState* GetActivePostDeathReplay(CPlayer* player)
{
    CPlayerExtended* const extended = player ? player->GetPlayerExtended() : nullptr;
    if (!extended)
        return nullptr;

    auto& replay = extended->m_PostDeathReplay;
    return replay.player == player ? &replay : nullptr;
}

void CPlayer::ItemPostFrame()
{
    CPlayer__ItemPostFrame(this);
}

ADD_SQFUNC("void", NSEarlyWritePlayerPersistenceForLeave, "entity player", "", ScriptContext::SERVER)
{
    const CPlayer* pPlayer = g_pSquirrel[context]->template getentity<CPlayer>(sqvm, 1);
    if (!pPlayer)
    {
        spdlog::warn("NSEarlyWritePlayerPersistenceForLeave got null player");

        g_pSquirrel[context]->pushbool(sqvm, false);
        return SQRESULT_NOTNULL;
    }

    CClient* pClient = &g_pClientArray[pPlayer->m_nPlayerIndex - 1];
    if (g_pServerAuthentication->m_PlayerAuthenticationData.find(pClient) == g_pServerAuthentication->m_PlayerAuthenticationData.end())
    {
        g_pSquirrel[context]->pushbool(sqvm, false);
        return SQRESULT_NOTNULL;
    }

    g_pServerAuthentication->m_PlayerAuthenticationData[pClient].needPersistenceWriteOnLeave = false;
    g_pServerAuthentication->WritePersistentData(pClient);
    return SQRESULT_NULL;
}

ADD_SQFUNC("bool", NSIsPlayerLocalPlayer, "entity player", "", ScriptContext::SERVER)
{
    if (IsDedicatedServer())
    {
        g_pSquirrel[context]->pushbool(sqvm, false);
        return SQRESULT_NOTNULL;
    }
    const CPlayer* pPlayer = g_pSquirrel[ScriptContext::SERVER]->template getentity<CPlayer>(sqvm, 1);
    if (!pPlayer)
    {
        spdlog::warn("NSIsPlayerLocalPlayer got null player");
        g_pSquirrel[context]->pushbool(sqvm, false);
        return SQRESULT_NOTNULL;
    }
    if (!g_pLocalPlayerUserID)
    {
        spdlog::warn("NSIsPlayerLocalPlayer got null uid");
        g_pSquirrel[context]->pushbool(sqvm, false);
        return SQRESULT_NOTNULL;
    }
    CClient* pClient = &g_pClientArray[pPlayer->m_nPlayerIndex - 1];
    g_pSquirrel[context]->pushbool(sqvm, !strcmp(g_pLocalPlayerUserID, pClient->m_szPlatformID));
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSIsPlayerExtendedClient, "entity player", "", ScriptContext::SERVER)
{
    const CPlayer* pPlayer = g_pSquirrel[context]->template getentity<CPlayer>(sqvm, 1);
    const std::uint32_t playerIndex = pPlayer ? pPlayer->m_nPlayerIndex : 0;
    if (playerIndex == 0 || playerIndex > MAX_PLAYERS)
    {
        g_pSquirrel[context]->pushbool(sqvm, false);
        return SQRESULT_NOTNULL;
    }

    const CClientExtended* pExtendedClient = g_pClientArray[playerIndex - 1].GetClientExtended();
    g_pSquirrel[context]->pushbool(sqvm, pExtendedClient && pExtendedClient->IsExtendedClient());
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", NSDisconnectPlayer, "entity player, string reason", "Disconnects the player from the server with the given reason",
           ScriptContext::SERVER)
{
    const CPlayer* pPlayer = g_pSquirrel[context]->template getentity<CPlayer>(sqvm, 1);
    const char* reason = g_pSquirrel[context]->getstring(sqvm, 2);

    if (!pPlayer)
    {
        spdlog::warn("Attempted to call NSDisconnectPlayer() with null player.");

        g_pSquirrel[context]->pushbool(sqvm, false);
        return SQRESULT_NOTNULL;
    }

    // Shouldn't happen but I like sanity checks.
    CClient* pClient = &g_pClientArray[pPlayer->m_nPlayerIndex - 1];
    if (!pClient)
    {
        spdlog::warn("NSDisconnectPlayer(): player entity has null CClient!");

        g_pSquirrel[context]->pushbool(sqvm, false);
        return SQRESULT_NOTNULL;
    }

    if (reason)
        pClient->Disconnect(REP_REMOVE_ONLY, reason);
    else
        pClient->Disconnect(REP_REMOVE_ONLY, "Disconnected by the server.");

    g_pSquirrel[context]->pushbool(sqvm, true);
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", NSSendClientPrint, "entity player, string msg", "Sends a message to the player's console", ScriptContext::SERVER)
{
    const CPlayer* pPlayer = g_pSquirrel[context]->template getentity<CPlayer>(sqvm, 1);
    const char* msg = g_pSquirrel[context]->getstring(sqvm, 2);

    if (!pPlayer)
    {
        spdlog::warn("NSSendClientPrint(): got null player");
        return SQRESULT_NULL;
    }

    CClient* pClient = &g_pClientArray[pPlayer->m_nPlayerIndex - 1];
    CGameClient__ClientPrintf(pClient, "%s", msg);

    return SQRESULT_NULL;
}

// clang-format off
ADD_SQFUNC("string", GetUserInfoKVString_Internal, "entity player, string key, string defaultValue = \"\"",
	"Gets the string value of a given player's userinfo convar by name", ScriptContext::SERVER)
// clang-format on
{
    const CPlayer* pPlayer = g_pSquirrel[ScriptContext::SERVER]->template getentity<CPlayer>(sqvm, 1);
    if (!pPlayer)
    {
        g_pSquirrel[ScriptContext::SERVER]->raiseerror(sqvm, "player is null");
        return SQRESULT_ERROR;
    }

    const char* pKey = g_pSquirrel[ScriptContext::SERVER]->getstring(sqvm, 2);
    const char* pDefaultValue = g_pSquirrel[ScriptContext::SERVER]->getstring(sqvm, 3);

    const char* pResult = g_pClientArray[pPlayer->m_nPlayerIndex - 1].m_ConVars->GetString(pKey, pDefaultValue);
    g_pSquirrel[ScriptContext::SERVER]->pushstring(sqvm, pResult);
    return SQRESULT_NOTNULL;
}

// clang-format off
ADD_SQFUNC("asset", GetUserInfoKVAsset_Internal, "entity player, string key, asset defaultValue = $\"\"",
	"Gets the asset value of a given player's userinfo convar by name", ScriptContext::SERVER)
// clang-format on
{
    const CPlayer* pPlayer = g_pSquirrel[ScriptContext::SERVER]->template getentity<CPlayer>(sqvm, 1);
    if (!pPlayer)
    {
        g_pSquirrel[ScriptContext::SERVER]->raiseerror(sqvm, "player is null");
        return SQRESULT_ERROR;
    }

    const char* pKey = g_pSquirrel[ScriptContext::SERVER]->getstring(sqvm, 2);
    const char* pDefaultValue;
    g_pSquirrel[ScriptContext::SERVER]->getasset(sqvm, 3, &pDefaultValue);

    const char* pResult = g_pClientArray[pPlayer->m_nPlayerIndex - 1].m_ConVars->GetString(pKey, pDefaultValue);
    g_pSquirrel[ScriptContext::SERVER]->pushasset(sqvm, pResult);
    return SQRESULT_NOTNULL;
}

// clang-format off
ADD_SQFUNC("int", GetUserInfoKVInt_Internal, "entity player, string key, int defaultValue = 0",
	"Gets the int value of a given player's userinfo convar by name", ScriptContext::SERVER)
// clang-format on
{
    const CPlayer* pPlayer = g_pSquirrel[ScriptContext::SERVER]->template getentity<CPlayer>(sqvm, 1);
    if (!pPlayer)
    {
        g_pSquirrel[ScriptContext::SERVER]->raiseerror(sqvm, "player is null");
        return SQRESULT_ERROR;
    }

    const char* pKey = g_pSquirrel[ScriptContext::SERVER]->getstring(sqvm, 2);
    const int iDefaultValue = g_pSquirrel[ScriptContext::SERVER]->getinteger(sqvm, 3);

    const int iResult = g_pClientArray[pPlayer->m_nPlayerIndex - 1].m_ConVars->GetInt(pKey, iDefaultValue);
    g_pSquirrel[ScriptContext::SERVER]->pushinteger(sqvm, iResult);
    return SQRESULT_NOTNULL;
}

// clang-format off
ADD_SQFUNC("float", GetUserInfoKVFloat_Internal, "entity player, string key, float defaultValue = 0",
	"Gets the float value of a given player's userinfo convar by name", ScriptContext::SERVER)
// clang-format on
{
    const CPlayer* pPlayer = g_pSquirrel[ScriptContext::SERVER]->getentity<CPlayer>(sqvm, 1);
    if (!pPlayer)
    {
        g_pSquirrel[ScriptContext::SERVER]->raiseerror(sqvm, "player is null");
        return SQRESULT_ERROR;
    }

    const char* pKey = g_pSquirrel[ScriptContext::SERVER]->getstring(sqvm, 2);
    const float flDefaultValue = g_pSquirrel[ScriptContext::SERVER]->getfloat(sqvm, 3);

    const float flResult = g_pClientArray[pPlayer->m_nPlayerIndex - 1].m_ConVars->GetFloat(pKey, flDefaultValue);
    g_pSquirrel[ScriptContext::SERVER]->pushfloat(sqvm, flResult);
    return SQRESULT_NOTNULL;
}

// clang-format off
ADD_SQFUNC("bool", GetUserInfoKVBool_Internal, "entity player, string key, bool defaultValue = false",
	"Gets the bool value of a given player's userinfo convar by name", ScriptContext::SERVER)
// clang-format on
{
    const CPlayer* pPlayer = g_pSquirrel[ScriptContext::SERVER]->getentity<CPlayer>(sqvm, 1);
    if (!pPlayer)
    {
        g_pSquirrel[ScriptContext::SERVER]->raiseerror(sqvm, "player is null");
        return SQRESULT_ERROR;
    }

    const char* pKey = g_pSquirrel[ScriptContext::SERVER]->getstring(sqvm, 2);
    const bool bDefaultValue = g_pSquirrel[ScriptContext::SERVER]->getbool(sqvm, 3);

    const bool bResult = g_pClientArray[pPlayer->m_nPlayerIndex - 1].m_ConVars->GetInt(pKey, bDefaultValue);
    g_pSquirrel[ScriptContext::SERVER]->pushbool(sqvm, bResult);
    return SQRESULT_NOTNULL;
}

DECLARE_HOOK(CPlayer::CycleObserverTarget, server.dll + 0x57BFD0, [](auto& hook, CPlayer* player, bool reverse) -> bool
{
    if (!player)
        return hook.Original(player, reverse);

    CPlayerExtended* const extended = player->GetPlayerExtended();
    if (!extended)
        return hook.Original(player, reverse);
    if (extended->m_bCyclingObserverTarget)
        return true;

    extended->m_bCyclingObserverTarget = true;
    const bool result = hook.Original(player, reverse);
    extended->m_bCyclingObserverTarget = false;
    return result;
})

DECLARE_HOOK(CWeaponX::PrimaryAttack, server.dll + 0x6A0220, [](auto& hook, CWeaponX* weapon, int attackIndex) -> bool
{
    if (!Cvar_ns_enable_weapon_attack_callback->GetBool() || !weapon)
        return hook.Original(weapon, attackIndex);

    CBaseCombatCharacter* player = weapon->GetWeaponOwner();
    const char* weaponName = player ? weapon->GetWeaponName() : nullptr;
    if (!weaponName || !*weaponName)
        return hook.Original(weapon, attackIndex);

    bool ret = hook.Original(weapon, attackIndex);
    if (!ret)
        return ret;

    auto* sq = g_pSquirrel[ScriptContext::SERVER];
    if (!sq->m_pSQVM || !sq->m_pSQVM->sqvm)
        return ret;

    weaponName = weapon->GetWeaponName();
    auto playerInstance = sq->createscriptinstance(player);
    auto weaponInstance = sq->createscriptinstance(weapon);

    if (playerInstance && weaponInstance && weaponName)
        sq->Call("CodeCallback_OnWeaponAttack", playerInstance, weaponInstance, weaponName, 1);

    return ret;
})

DECLARE_HOOK(CalcDistanceSqrToLineSegment, server.dll + 0x6EF440,
             [](auto& hook, const Vector3D* point, const Vector3D* segmentStart, const Vector3D* segmentEnd, float* outFraction) -> float
{
    g_bReachedLagCompensationRayGeometry = true;

    return hook.Original(point, segmentStart, segmentEnd, outFraction);
})

DECLARE_HOOK(CLagCompensationManager::ShotShapeAllowsEntity, server.dll + 0x5C4A30,
             [](auto& hook, CLagCompensationManager* manager, CBaseEntity* candidate, const CCheckTransmitInfo* transmitInfo,
                unsigned int attackerMask) -> bool
{
    const bool previousReachedRayGeometry = g_bReachedLagCompensationRayGeometry;
    g_bReachedLagCompensationRayGeometry = false;
    const ScopeGuard restoreRayGeometry([previousReachedRayGeometry] { g_bReachedLagCompensationRayGeometry = previousReachedRayGeometry; });

    const bool nativeResult = hook.Original(manager, candidate, transmitInfo, attackerMask);
    const bool reachedRayGeometry = g_bReachedLagCompensationRayGeometry;
    return nativeResult || (Cvar_ns_skip_historical_hitbox_check->GetBool() && reachedRayGeometry && candidate && candidate->IsPlayer());
})

DECLARE_HOOK(CPlayer::SetObserverMode, server.dll + 0x5B26D0, [](auto& hook, CPlayer* player, int mode)
{
    if (auto* const replay = GetActivePostDeathReplay(player); replay && !replay->observerChanged)
    {
        player->m_iObserverMode = replay->observerMode;
        replay->observerChanged = true;
    }
    hook.Original(player, mode);
})

DECLARE_HOOK(CPlayer::StopObserverMode, server.dll + 0x5B5DA0, [](auto& hook, CPlayer* player)
{
    if (auto* const replay = GetActivePostDeathReplay(player); replay && !replay->observerChanged)
    {
        player->m_iObserverMode = replay->observerMode;
        replay->observerChanged = true;
    }
    hook.Original(player);
})

DECLARE_HOOK(CPlayer::Script_StopObserverMode, server.dll + 0x581640, [](auto& hook, HSQUIRRELVM sqvm) -> SQRESULT
{
    CPlayer* player = nullptr;
    if (g_pSquirrel[ScriptContext::SERVER]->getthisentity(sqvm, &player))
    {
        if (auto* const replay = GetActivePostDeathReplay(player); replay && !replay->observerChanged)
        {
            player->m_iObserverMode = replay->observerMode;
            replay->observerChanged = true;
        }
    }
    return hook.Original(sqvm);
})

DECLARE_HOOK(CBaseCombatCharacter::HolsterWeapon, server.dll + 0xE9AD0, [](auto& hook, CBaseCombatCharacter* character)
{
    if (character && character->IsPlayer())
    {
        CPlayer* const player = static_cast<CPlayer*>(character);
        CPlayerExtended* const extended = player->GetPlayerExtended();
        if (extended && extended->m_bProtectPostDeathWeapon)
            return;
    }
    hook.Original(character);
})

DECLARE_HOOK(CPlayer::EyePosition, server.dll + 0x5CF7D0, [](auto& hook, CPlayer* player, Vector3D* result) -> Vector3D*
{
    const CPlayerExtended::PostDeathReplayState* const replay = GetActivePostDeathReplay(player);
    if (!replay || replay->observerChanged || player->m_lifeState == 0 || player->m_iSpawnParity != replay->spawnParity ||
        player->m_flDeathTime != replay->deathTime)
        return hook.Original(player, result);

    Vector3D& viewOffset = player->GetViewOffset();
    const Vector3D previousViewOffset = viewOffset;
    viewOffset = replay->viewOffset;
    Vector3D* const eyePosition = hook.Original(player, result);
    viewOffset = previousViewOffset;
    return eyePosition;
})

DECLARE_HOOK(CPlayer::Event_Killed, server.dll + 0x58AFC0, [](auto& hook, CPlayer* player, const CTakeDamageInfo* damageInfo)
{
    CPlayerExtended* const extended = player->GetPlayerExtended();
    CPlayerExtended::PostDeathWeaponState* const state = extended ? &extended->m_PostDeathWeapon : nullptr;

    if (state)
        state->active = false;

    const float maxUnlag = sv_maxunlag ? sv_maxunlag->GetFloat() : 0.0f;
    CWeaponX* weapon = nullptr;
    if (state && Cvar_ns_allow_post_death_shots->GetBool() && player->m_lifeState == 0 && g_pGlobals && std::isfinite(maxUnlag) && maxUnlag > 0 &&
        !player->IsWeaponHolstered())
        weapon = player->GetActiveWeapon();

    const std::uint32_t weaponHandle = weapon ? weapon->GetRefEHandle().ToInt() : 0;
    const std::uint32_t deathTick = g_pGlobals ? g_pGlobals->tickcount : 0;
    const Vector3D viewOffset = weapon ? player->GetViewOffset() : Vector3D{};
    const bool previousProtection = extended && extended->m_bProtectPostDeathWeapon;
    if (extended)
        extended->m_bProtectPostDeathWeapon = weapon != nullptr;
    hook.Original(player, damageInfo);
    if (extended)
        extended->m_bProtectPostDeathWeapon = previousProtection;

    if (!weapon)
        return;

    CWeaponX* currentWeapon = player->GetActiveWeapon();
    if (player->m_lifeState == 0 || !currentWeapon || currentWeapon->GetRefEHandle().ToInt() != weaponHandle || player->IsWeaponHolstered() ||
        !std::isfinite(player->m_flDeathTime))
    {
        if (player->m_lifeState != 0 && currentWeapon && currentWeapon->GetRefEHandle().ToInt() == weaponHandle)
            player->HolsterWeapon();
        return;
    }

    state->playerHandle = player->GetRefEHandle().ToInt();
    state->weaponHandle = weaponHandle;
    state->deathTick = deathTick;
    state->deathTime = player->m_flDeathTime;
    state->viewOffset = viewOffset;
    state->spawnParity = player->m_iSpawnParity;
    state->active = true;
})

DECLARE_HOOK(CPlayer::PostThink, server.dll + 0x5D7880, [](auto& hook, CPlayer* player)
{
    const bool wasAlive = player->m_lifeState == 0;
    hook.Original(player);
    if (wasAlive)
        return;

    CPlayerExtended* const extended = player->GetPlayerExtended();
    CPlayerExtended::PostDeathWeaponState* const state = extended ? &extended->m_PostDeathWeapon : nullptr;
    if (!state || !state->active)
        return;

    if (player->m_lifeState == 0 || player->GetRefEHandle().ToInt() != state->playerHandle || player->m_iSpawnParity != state->spawnParity ||
        player->m_flDeathTime != state->deathTime)
    {
        state->active = false;
        return;
    }

    const float maxUnlag = sv_maxunlag ? sv_maxunlag->GetFloat() : 0.0f;
    if (!Cvar_ns_allow_post_death_shots->GetBool() || !g_pGlobals || !std::isfinite(maxUnlag) || maxUnlag <= 0 ||
        g_pGlobals->tickcount < state->deathTick || (g_pGlobals->tickcount - state->deathTick) * g_pGlobals->interval_per_tick > maxUnlag)
    {
        state->active = false;
        CWeaponX* weapon = player->GetActiveWeapon();
        if (weapon && weapon->GetRefEHandle().ToInt() == state->weaponHandle)
            player->HolsterWeapon();
        return;
    }

    CWeaponX* weapon = player->GetActiveWeapon();
    if (!weapon || weapon->GetRefEHandle().ToInt() != state->weaponHandle || player->IsWeaponHolstered())
    {
        state->active = false;
        return;
    }

    const SV_CUserCmd* command = player->m_pCurrentCommand;
    if (!command || !std::isfinite(command->command_time) || command->command_time > state->deathTime ||
        state->deathTime - command->command_time > maxUnlag)
        return;

    bool& allowDeadOwner = weapon->m_modVars.canAttackWhenDead;
    const bool previousAllowDeadOwner = allowDeadOwner;
    const std::uint32_t weaponHandle = state->weaponHandle;
    {
        auto& replay = extended->m_PostDeathReplay;
        const CPlayerExtended::PostDeathReplayState previousReplayState = replay;
        const ScopeGuard restoreReplay([extended, previousReplayState] { extended->m_PostDeathReplay = previousReplayState; });
        replay = (CPlayerExtended::PostDeathReplayState{player, player->m_iObserverMode, state->viewOffset, state->spawnParity, state->deathTime});
        allowDeadOwner = true;
        player->m_iObserverMode = 0;
        player->ItemPostFrame();
        if (!replay.observerChanged && player->m_lifeState != 0 && player->m_iSpawnParity == replay.spawnParity &&
            player->m_flDeathTime == replay.deathTime && player->m_iObserverMode == 0)
            player->m_iObserverMode = replay.observerMode;
    }

    if (!previousAllowDeadOwner)
    {
        CBaseEntity* retainedEntity = CBaseEntity::Instance(weaponHandle & 0xFFFF);
        if (retainedEntity && retainedEntity->GetRefEHandle().ToInt() == weaponHandle)
        {
            CWeaponX* retainedWeapon = retainedEntity->MyWeaponXPointer();
            if (retainedWeapon)
                retainedWeapon->m_modVars.canAttackWhenDead = false;
        }
    }
})

ON_DLL_LOAD_RELIESON("server.dll", CPlayer, (ConVar, CmdBuffer, EngineServerGlobals, EngineSystem), [](CModule module)
{
    Cvar_ns_enable_weapon_attack_callback =
        new ConVar("ns_enable_weapon_attack_callback", "0", FCVAR_GAMEDLL, "Enables script weapon attack callback.");
    Cvar_ns_skip_historical_hitbox_check =
        new ConVar("ns_skip_historical_hitbox_check", "1", FCVAR_GAMEDLL,
                   "Improves hitreg against faster targets, minor performance impact when enabled.", true, 0.0f, true, 1.0f, nullptr);
    Cvar_ns_allow_post_death_shots =
        new ConVar("ns_allow_post_death_shots", "0", FCVAR_GAMEDLL,
                   "Allow pre-death user commands to finish weapon firing after death. Good if you have people on high ping");

    sv_maxunlag = module.Offset(0x14E6790).RCast<ConVar*>();

    CPlayer__ItemPostFrame = module.Offset(0x5D52F0).RCast<decltype(CPlayer__ItemPostFrame)>();
    s_UTIL_PlayerByIndex = module.Offset(0x26AA10).RCast<decltype(s_UTIL_PlayerByIndex)>();
    CPlayer__SetGroundFrictionScale = module.Offset(0x5B2180).RCast<decltype(CPlayer__SetGroundFrictionScale)>();
    CPlayer__SetWallrunFrictionScale = module.Offset(0x5B43E0).RCast<decltype(CPlayer__SetWallrunFrictionScale)>();
    CPlayer__Script_TurnDodgeDisabledOn = module.Offset(0x5AD3A0).RCast<decltype(CPlayer__Script_TurnDodgeDisabledOn)>();
    CPlayer__Script_TurnDodgeDisabledOff = module.Offset(0x5AD320).RCast<decltype(CPlayer__Script_TurnDodgeDisabledOff)>();
    CPlayer__SetTitanDisembarkEnabled = module.Offset(0x5E0C70).RCast<decltype(CPlayer__SetTitanDisembarkEnabled)>();
    CPlayer__SetPowerRegenRateScale = module.Offset(0x5AD020).RCast<decltype(CPlayer__SetPowerRegenRateScale)>();
    RegisterPlayerPredictionScriptMethods(ScriptContext::SERVER, module.Offset(0x14DAA20).RCast<ScriptClassDesc_t*>());

    DISPATCH_MODULE(PlayerHooks);
})
