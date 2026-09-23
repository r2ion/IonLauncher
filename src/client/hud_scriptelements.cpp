#include "client/hud_scriptelements.h"

#include "client/player.h"
#include "engine/cdll_int.h"
#include "engine/r2engine.h"
#include "engine/usermessages.h"
#include "tier0/hooks.h"
#include "tier0/vanilla.h"
#include "tier1/convar.h"
#include "tier1/cvar.h"
#include "vscript/languages/squirrel_re/squirrel.h"

#include <array>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

DECLARE_MODULE(HudScriptElementsHooks)

HudScriptElementsRemoteFunctionState CHudScriptElements::s_RemoteFunctions;

const std::set<std::string> CHudScriptElements::s_VanillaRemoteFunctions = {
    "ServerCallback_CreateSpectrePaletteLighting",
    "ServerCallback_StopWargamesPodAmbienceSound",
    "ServerCallback_SpawnIMCFactionLeaderForIntro",
    "ServerCallback_SpawnMilitiaFactionLeaderForIntro",
    "ServerCallback_ClearFactionLeaderIntro",
    "ServerCallback_PlayPodTransitionScreenFX",
    "ServerCallback_DpadCommSay",
    "ServerCallback_CaptialShips",
    "ServerCallback_RewardReadyMessage",
    "ServerCallback_TitanReadyMessage",
    "ServerCallback_FPS_Test",
    "ServerCallback_FPS_Avg",
    "DebugSetFrontline",
    "ServerCallback_StartCinematicNodeEditor",
    "ServerCallback_AISkitDebugMessage",
    "ServerCallback_UpdateClientChallengeProgress",
    "ServerCallback_EventNotification",
    "SCB_RefreshBurnCardSelector",
    "ServerCallback_EjectConfirmed",
    "SCB_AddGrenadeIndicatorForEntity",
    "SCB_SetUserPerformance",
    "SCB_UpdateSponsorables",
    "SCB_ClientDebug",
    "ScriptCallback_UnlockAchievement",
    "ServerCallback_UpdateHeroStats",
    "ServerCallback_GiveSentryTurret",
    "ServerCallback_TurretReport",
    "ServerCallback_TurretWorldIconShow",
    "ServerCallback_TurretWorldIconHide",
    "ServerCallback_LoadoutNotification",
    "ServerCallback_ItemNotification",
    "ServerCallback_AnnouncePathLevelUp",
    "ServerCallback_SonarPulseFromPosition",
    "ServerCallback_OpenShopMenu",
    "ServerCallback_CloseShopMenu",
    "ServerCallback_StartShieldPlayer",
    "ServerCallback_StopShieldPlayer",
    "ServerCallback_AddShieldedPlayer",
    "ServerCallback_RemoveShieldedPlayer",
    "SCB_SmartAmmoForceLockedOntoHudDraw",
    "ServerCallback_UpdateMarker",
    "DisablePrecacheErrors",
    "RestorePrecacheErrors",
    "SCB_PlayTitanCockpitSounds",
    "SCB_StopTitanCockpitSounds",
    "ServerCallback_RewardUsed",
    "ServerCallback_VanguardUpgradeMessage",
    "ServerCallback_HardpointChanged",
    "ServerCallback_DisableHudForEvac",
    "ServerCallback_AT_AnnouncePreParty",
    "ServerCallback_AT_AnnounceBoss",
    "ServerCallback_AT_AnnounceWaveOver",
    "ServerCallback_AT_YouKilledBoss",
    "ServerCallback_AT_YouCollectedBox",
    "ServerCallback_AT_WarnPlayerBounty",
    "ServerCallback_AT_YouSurvivedBounty",
    "ServerCallback_AT_TeammateSurvivedBounty",
    "ServerCallback_AT_PromptBossRodeo",
    "ServerCallback_AT_PromptBossExecute",
    "ServerCallback_AT_BossDoomed",
    "ServerCallback_AT_OnPlayerConnected",
    "ServerCallback_AT_UpdateMostWanted",
    "ServerCallback_AT_ScoreSplashStartMultTimer",
    "ServerCallback_AT_ShowRespawnBonusLoss",
    "ServerCallback_AT_BankOpen",
    "ServerCallback_AT_BankClose",
    "ServerCallback_AT_FinishDeposit",
    "ServerCallback_AT_ShowATScorePopup",
    "ServerCallback_AT_BossDamageScorePopup",
    "ServerCallback_AT_PlayerKillScorePopup",
    "ServerCallback_AT_ShowStolenBonus",
    "ServerCallback_AT_ClearCampAndBossPortraits",
    "ServerCallback_AT_PulseBankAntena",
    "ServerCallback_AITDM_OnPlayerConnected",
    "ServerCallback_CP_PlayMatchEndingMusic",
    // "ServerCallback_CP_PrintHardpointOccupants",
    "ServerCallback_CTF_PlayMatchNearEndMusic",
    "ServerCallback_CTF_StartReturnFlagProgressBar",
    "ServerCallback_CTF_StopReturnFlagProgressBar",
    "ServerCallback_FW_FriendlyBaseAttacked",
    "ServerCallback_FW_NotifyTitanRequired",
    "ServerCallback_FW_NotifyEnterFriendlyArea",
    "ServerCallback_FW_NotifyExitFriendlyArea",
    "ServerCallback_FW_NotifyEnterEnemyArea",
    "ServerCallback_FW_NotifyExitEnemyArea",
    "ServerCallback_FW_SetObjective",
    "ServerCallback_MFD_StartNewMarkCountdown",
    "ServerCallback_LTSThirtySecondWarning",
    "ServerCallback_ColiseumDisplayTickets",
    "ServerCallback_ColiseumIntro",
    "ServerCallback_SPEEDBALL_LastPlayer",
    "ServerCallback_SPEEDBALL_LastFlagOwner",
    "ServerCallback_FD_AnnouncePreParty",
    "ServerCallback_FD_ClearPreParty",
    "ServerCallback_FD_PingMinimap",
    "ServerCallback_FD_MoneyFly",
    "ServerCallback_FD_SayThanks",
    "ServerCallback_FD_DisplayHarvesterKiller",
    "ServerCallback_FD_NotifyStoreOpen",
    "ServerCallback_ShowCycleHint",
    "ServerCallback_OpenBoostStore",
    "ServerCallback_UpdateMoney",
    "ServerCallback_UpdateTeamReserve",
    "ServerCallback_EnableDropshipBoostStore",
    "ServerCallback_DisableDropshipBoostStore",
    "ServerCallback_UpdateTurretCount",
    "ServerCallback_UpdatePlayerHasBattery",
    "ServerCallback_UpdateAmpedWeaponState",
    "ServerCallback_BoostStoreTitanHint",
    "ServerCallback_UpdateGameStats",
    "ServerCallback_ShowGameStats",
    "ServerCallback_FD_UpdateWaveInfo",
    "ServerCallback_FD_NotifyMVP",
    "ServerCallback_NukeGrenadeWindowOpen",
    "ServerCallback_NukeGrenadeWindowClosed",
    "ServerCallback_RegisterTeamTitanMenuButtons",
    "ServerCallback_OpenTeamTitanMenu",
    "ServerCallback_CloseTeamTitanMenu",
    "ServerCallback_UpdateTeamTitanMenuTime",
    "ServerCallback_UpdateTeamTitanSelectionMenu",
    "ServerCallback_ResetEntSkyScale",
    "ServerCallback_SetEntSkyScale",
    "ServerCallback_ResetMapSettings",
    "ServerCallback_SetMapSettings",
    "ServerCallback_ToneMapping",
    "ServerCallback_LaptopFX",
    "ServerCallback_YouDied",
    "ServerCallback_YouRespawned",
    "ServerCallback_ShowDeathHint",
    "ServerCallback_ShowNextSpawnMessage",
    "ServerCallback_HideNextSpawnMessage",
    "ServerCallback_AnnounceWinner",
    "ServerCallback_AnnounceRoundWinner",
    "ServerCallback_GuidedMissileDestroyed",
    "ServerCallback_DoClientSideCinematicMPMoment",
    "ServerCallback_SetAssistInformation",
    "ServerCallback_TitanEMP",
    "ServerCallback_AirburstIconUpdate",
    "ServerCallback_TitanCockpitBoot",
    "ServerCallback_DataKnifeStartLeech",
    "ServerCallback_DataKnifeCancelLeech",
    "ServerCallback_ControlPanelRefresh",
    "ServerCallback_TurretRefresh",
    "ServerCallback_CreateEvacShipIcon",
    "ServerCallback_DestroyEvacShipIcon",
    "ServerCallback_AddCapturePoint",
    "ServerCallback_TitanDisembark",
    "ServerCallback_OnEntityKilled",
    "ServerCallback_OnTitanKilled",
    "ServerCallback_PlayerConnectedOrDisconnected",
    "SCBUI_PlayerConnectedOrDisconnected",
    "ServerCallback_PlayerChangedTeams",
    "ServerCallback_AnnounceTitanReservation",
    "ServerCallback_ReplacementTitanSpawnpoint",
    "ServerCallback_TitanTookDamage",
    "ServerCallback_PilotTookDamage",
    "ServerCallback_PlayerUsesBurnCard",
    "ServerCallback_ScreenShake",
    "ServerCallback_MinimapPulse",
    "ServerCallback_UpdateOverheadIconForNPC",
    "ServerCallback_SetFlagHomeOrigin",
    "ServerCallback_StartBatteryTimer",
    "ServerCallback_TitanBatteryDown",
    "ServerCallback_SpottingHighlight",
    "ServerCallback_SpottingDeny",
    "ServerCallback_PlayerLeveledUp",
    "ServerCallback_TitanLeveledUp",
    "ServerCallback_TitanXPAdded",
    "ServerCallback_WeaponLeveledUp",
    "ServerCallback_WeaponXPAdded",
    "ServerCallback_WeaponChallengeCompleted",
    "ServerCallback_TitanChallengeCompleted",
    "ServerCallback_PlayerChallengeCompleted",
    "ServerCallback_UpdateRodeoRiderHud",
    "ServerCallback_UpdateTeamTitanSelection",
    "ServerCallback_FFASuddenDeathAnnouncement",
    "ServerCallback_IncomingAirdrop",
    "ServerCallback_TitanLostHealthSegment",
    "ServerCallback_PlayScreenFXWarpJump",
    "ServerCallback_Phantom_Scan",
    "ServerCallback_RodeoScreenShake",
    "ServerCallback_RodeoerEjectWarning",
    "ServerCallback_TitanEmbark",
    "ServerCallback_DogFight",
    "ServerCallback_Announcement",
    "ServerCallback_GameModeAnnouncement",
    "ServerCallback_ScoreEvent",
    "ServerCallback_CallingCardEvent",
    "ServerCallback_PlayConversation",
    "ServerCallback_PlayTitanConversation",
    "ServerCallback_PlaySquadConversation",
    "ServerCallback_CreateDropShipIntLighting",
    "ServerCallback_EvacObit",
    "ServerCallback_ShowTurretHint",
    "ServerCallback_HideTurretHint",
    "ServerCallback_ShowTurretInUseHint",
    "ServerCallback_UpdateBurnCardTitle",
    "ServerCallback_UpdateTitanModeHUD",
    "ServerCallback_GiveMatchLossProtection",
    "ServerCallback_SquadLeaderBonus",
    "ServerCallback_SquadLeaderDoubleXP",
    "ServerCallback_TitanFallWarning",
    "SCB_TitanDialogue",
    "ServerCallback_PlayLobbyScene",
    "ServerCallback_PilotCreatedGunShield",
    "ServerCallback_BeginSmokeSight",
    "ServerCallback_EndSmokeSight",
    "UpdateCachedPilotLoadout",
    "UpdateCachedTitanLoadout",
    "UpdateAllCachedPilotLoadouts",
    "UpdateAllCachedTitanLoadouts",
    "ServerCallback_UpdatePilotModel",
    "ServerCallback_UpdateTitanModel",
    "ServerCallback_MVUpdateModelBounds",
    "ServerCallback_MVEnable",
    "ServerCallback_MVDisable",
    "ServerCallback_ModelViewerDisableConflicts",
    "ServerCallback_Test",
    "ServerCallback_SetClassicSkyScale",
    "ServerCallback_ResetClassicSkyScale",
    "ServerCallback_ClientInitComplete",
    "SCB_LockCapturePointForTeam",
    "SCB_UnlockCapturePointForTeam",
    "ServerCallback_SetEntityVar",
    "ServerCallback_SetServerVar",
    "ServerCallback_PlayTeamMusicEvent",
    "ServerCallback_PlayMusicToCompletion",
    "ServerCallback_PlayMusic",
    "ServerCallback_TitanCockpitEMP",
    "ServerCallback_PlayerEarnedBurnCard",
    "ServerCallback_PlayerStoppedBurnCard",
    "ServerCallback_SetUIVar",
    "ServerCallback_ShopPurchaseStatus",
    "ServerCallback_OpenPilotLoadoutMenu",
    "ServerCallback_GenericDialog",
    "Dev_PrintClientMessage",
    "Dev_BuildClientMessage",
    "ServerCallback_DeploymentDeath",
    "ServerCallback_AddArcConnectorToy",
    "ServerCallback_PlayDialogueOnEntity",
    "ServerCallback_PlayDialogueAtPosition",
    "ServerCallback_PlayerConversation",
    "SCB_SetDoubleXPStatus",
    "SCB_SetScoreMeritState",
    "SCB_SetCompleteMeritState",
    "SCB_SetWinMeritState",
    "SCB_SetEvacMeritState",
    "SCB_SetMeritCount",
    "SCB_SetWeaponMeritCount",
    "SCB_SetTitanMeritCount",
    "SCB_UpdateTitanLoadouts",
    "SCB_SetHighlightFlagDisableDeathFade",
    "SCB_UpdateRankedPlayMenu",
    "SCB_UpdateBC",
    "SCB_RefreshBlackMarket",
    "ServerCallback_ShopOpenBurnCardPack",
    "ServerCallback_ShopOpenGenericItem",
    "SCB_RefreshCards",
    "SCB_UpdateEmptySlots",
    "SCB_UpdateBCFooter",
    "SCB_MarkedChanged",
    "ServerCallback_PlayBattleChatter",
    "ServerCallback_PlayFactionDialogue",
    "ServerCallback_ForcePlayFactionDialogue",
    "ServerCallback_SpawnFactionCommanderInDropship",
    "ServerCallback_PlaySpectreChatterMP",
    "ServerCallback_PlayGruntChatterMP",
    "ServerCallback_EarnMeterAwarded",
    "ServerCallback_GetObjectiveReminderOnLoad",
    "ServerCallback_ClearObjectiveReminderOnLoad",
    "ServerCallback_PingMinimap",
};

std::size_t ClientRemoteFunctionNameHash::operator()(const std::string_view name) const noexcept
{
    return std::hash<std::string_view>{}(name);
}

void CHudScriptElements::InitializeRemoteFunctions(const HudScriptElementsRemoteFunctionState::MessageCallback remoteFunctionCallCallback,
                                                   const HudScriptElementsRemoteFunctionState::MessageCallback remoteFunctionCallsChecksumCallback,
                                                   ScriptRemoteFunctionRegistrationState* const nativeRegistrationState,
                                                   C_CBaseScriptRemoteFunctionCallReceiveQueue* const nativeCallQueue,
                                                   ConVar* const killReplayPlayNonReplayRemoteCallsOnLocalClientPlayer)
{
    s_RemoteFunctions.m_RemoteFunctionCallCallback = remoteFunctionCallCallback;
    s_RemoteFunctions.m_RemoteFunctionCallsChecksumCallback = remoteFunctionCallsChecksumCallback;
    s_RemoteFunctions.m_NativeRegistrationState = nativeRegistrationState;
    s_RemoteFunctions.m_NativeCallQueue = nativeCallQueue;
    s_RemoteFunctions.m_KillReplayPlayNonReplayRemoteCallsOnLocalClientPlayer = killReplayPlayNonReplayRemoteCallsOnLocalClientPlayer;
}

void CHudScriptElements::BeginRemoteFunctionRegistration()
{
    s_RemoteFunctions.m_NamedCallReadIndex = 0;
    s_RemoteFunctions.m_NamedCallWriteIndex = 0;
    s_RemoteFunctions.m_NamedCallSnapshotTicks.fill(0);
    s_RemoteFunctions.m_Functions.clear();
    s_RemoteFunctions.m_RegistrationState = ClientRemoteFunctionRegistrationState::Registering;
}

void CHudScriptElements::FinishRemoteFunctionRegistration()
{
    s_RemoteFunctions.m_RegistrationState = ClientRemoteFunctionRegistrationState::Done;
}

SQRESULT CHudScriptElements::RegisterRemoteFunction(HSQUIRRELVM sqvm, const bool extended, bool& callOriginal)
{
    SquirrelManager* manager = g_pSquirrel[ScriptContext::CLIENT];
    const char* functionName = manager->getstring(sqvm, 1);

    callOriginal = false;
    if (s_RemoteFunctions.m_RegistrationState != ClientRemoteFunctionRegistrationState::Registering)
    {
        const std::string error = fmt::format("Can't register '{}' - {}.", functionName,
                                              s_RemoteFunctions.m_RegistrationState == ClientRemoteFunctionRegistrationState::Done
                                                  ? "Already called EndRegisteringFunctions()"
                                                  : "Haven't called BeginRegisteringFunctions()");
        manager->raiseerror(sqvm, error.c_str());
        return SQRESULT_ERROR;
    }

    const auto [registration, inserted] = s_RemoteFunctions.m_Functions.try_emplace(std::string(functionName), extended);
    if (!inserted)
    {
        const std::string error = fmt::format("Remote function '{}' is already registered.", functionName);
        manager->raiseerror(sqvm, error.c_str());
        return SQRESULT_ERROR;
    }

    if (extended)
        return SQRESULT_NULL;

    if (g_pVanillaCompatibility->GetVanillaCompatibility() && !s_VanillaRemoteFunctions.contains(registration->first))
    {
        spdlog::warn("Remote_RegisterFunction called with unknown function name: {}", registration->first);
        return SQRESULT_NULL;
    }

    callOriginal = true;
    return SQRESULT_NULL;
}

void CHudScriptElements::Init()
{
    usermessages->HookMessage("RemoteFunctionCall", s_RemoteFunctions.m_RemoteFunctionCallCallback);
    usermessages->HookMessage("RemoteFunctionCallsChecksum", s_RemoteFunctions.m_RemoteFunctionCallsChecksumCallback);
}

void CHudScriptElements::MsgFunc_RemoteFunctionCall(bf_read* message)
{
    ScriptRemoteFunctionRegistrationState* const nativeRegistrationState = s_RemoteFunctions.m_NativeRegistrationState;
    if (!nativeRegistrationState->m_RegistrationFinished || !g_pEngineClient->IsInGame())
        return;

    if (!m_RemoteFunctionCallsChecksumReceived)
    {
        MsgFunc_NamedRemoteFunctionCall(message);
        return;
    }

    const std::uint8_t functionIndex = static_cast<std::uint8_t>(message->ReadUBitLong(8));
    const bool callFromUI = message->ReadOneBit() != 0;
    const std::int32_t snapshotTick = static_cast<std::int32_t>(message->ReadUBitLong(32));
    const std::uint32_t parameterCount = message->ReadUBitLong(4);
    if (parameterCount > C_BaseScriptRemoteFunctionCall::MAX_PARAMETERS)
        return;

    if (m_RemoteFunctionCallsChecksumReceived && nativeRegistrationState->m_RegistrationFinished &&
        m_RemoteFunctionCallsChecksum != nativeRegistrationState->m_Checksum)
    {
        g_pEngineClient->Disconnect("#DISCONNECT_OUT_OF_SYNC");
    }

    C_CBaseScriptRemoteFunctionCallReceiveQueue* const nativeCallQueue = s_RemoteFunctions.m_NativeCallQueue;
    const std::uint32_t writeIndex = nativeCallQueue->m_WriteIndex;
    if (nativeCallQueue->m_SnapshotTicks[writeIndex] != 0)
    {
        g_pEngineClient->Disconnect("#DISCONNECT_OUT_OF_SYNC");
        return;
    }

    nativeCallQueue->m_SnapshotTicks[writeIndex] = snapshotTick;
    C_BaseScriptRemoteFunctionCall& call = nativeCallQueue->m_Calls[writeIndex];
    nativeCallQueue->m_WriteIndex = (writeIndex + 1) % C_CBaseScriptRemoteFunctionCallReceiveQueue::CAPACITY;

    call.m_FunctionIndex = functionIndex;
    call.m_ParameterCount = parameterCount;
    call.m_CallFromUI = callFromUI;

    for (std::uint32_t i = 0; i < parameterCount; ++i)
    {
        ScriptVariant_t& parameter = call.m_Parameters[i];
        parameter.FreeVariantMemory();
        parameter.m_dataPointer = nullptr;
        parameter.m_flags = 0;

        switch (static_cast<ScriptRemoteFunctionWireType>(message->ReadUBitLong(2)))
        {
        case ScriptRemoteFunctionWireType::Null:
            parameter.m_type = FIELD_VOID;
            break;
        case ScriptRemoteFunctionWireType::Boolean:
            parameter.m_bool = message->ReadOneBit() != 0;
            parameter.m_type = FIELD_BOOLEAN;
            break;
        case ScriptRemoteFunctionWireType::Integer:
            parameter.m_int = static_cast<std::int32_t>(message->ReadUBitLong(32));
            parameter.m_type = FIELD_INTEGER;
            break;
        case ScriptRemoteFunctionWireType::Float:
            parameter.m_float = message->ReadBitFloat();
            parameter.m_type = FIELD_FLOAT;
            break;
        }
    }

    call.m_Replay = message->ReadOneBit() != 0;
}

void CHudScriptElements::MsgFunc_RemoteFunctionCallsChecksum(bf_read* message)
{
    m_RemoteFunctionCallsChecksum = message->ReadUBitLong(32);
    m_RemoteFunctionCallsChecksumReceived = true;

    const ScriptRemoteFunctionRegistrationState* const nativeRegistrationState = s_RemoteFunctions.m_NativeRegistrationState;
    if (nativeRegistrationState->m_RegistrationFinished && m_RemoteFunctionCallsChecksum != nativeRegistrationState->m_Checksum)
    {
        g_pEngineClient->Disconnect("#DISCONNECT_OUT_OF_SYNC");
    }
}

void CHudScriptElements::MsgFunc_NamedRemoteFunctionCall(bf_read* message)
{
    std::array<char, MAX_NAMED_REMOTE_FUNCTION_NAME> functionName{};
    if (!message->ReadString(functionName.data(), static_cast<int>(functionName.size())))
    {
        g_pEngineClient->Disconnect("#DISCONNECT_OUT_OF_SYNC");
        return;
    }

    const auto registration = s_RemoteFunctions.m_Functions.find(std::string_view(functionName.data()));
    if (registration == s_RemoteFunctions.m_Functions.end())
    {
        spdlog::warn("Received unregistered named remote function '{}'.", functionName.data());
        return;
    }

    NamedScriptRemoteFunctionCall call;
    call.m_FunctionName = &registration->first;
    call.m_CallFromUI = message->ReadOneBit() != 0;
    const std::int32_t snapshotTick = static_cast<std::int32_t>(message->ReadUBitLong(32));
    call.m_ParameterCount = message->ReadUBitLong(4);
    if (snapshotTick == 0 || call.m_ParameterCount > call.m_Parameters.size())
    {
        g_pEngineClient->Disconnect("#DISCONNECT_OUT_OF_SYNC");
        return;
    }

    for (std::uint32_t i = 0; i < call.m_ParameterCount; ++i)
    {
        ScriptVariant_t& parameter = call.m_Parameters[i];
        switch (static_cast<ScriptRemoteFunctionWireType>(message->ReadUBitLong(2)))
        {
        case ScriptRemoteFunctionWireType::Null:
            parameter.m_type = FIELD_VOID;
            break;
        case ScriptRemoteFunctionWireType::Boolean:
            parameter.m_bool = message->ReadOneBit() != 0;
            parameter.m_type = FIELD_BOOLEAN;
            break;
        case ScriptRemoteFunctionWireType::Integer:
            parameter.m_int = static_cast<std::int32_t>(message->ReadUBitLong(32));
            parameter.m_type = FIELD_INTEGER;
            break;
        case ScriptRemoteFunctionWireType::Float:
            parameter.m_float = message->ReadBitFloat();
            parameter.m_type = FIELD_FLOAT;
            break;
        }
    }

    call.m_Replay = message->ReadOneBit() != 0;
    if (message->IsOverflowed())
    {
        g_pEngineClient->Disconnect("#DISCONNECT_OUT_OF_SYNC");
        return;
    }

    const std::uint32_t writeIndex = s_RemoteFunctions.m_NamedCallWriteIndex;
    if (s_RemoteFunctions.m_NamedCallSnapshotTicks[writeIndex] != 0)
    {
        g_pEngineClient->Disconnect("#DISCONNECT_OUT_OF_SYNC");
        return;
    }

    s_RemoteFunctions.m_NamedCalls[writeIndex] = std::move(call);
    s_RemoteFunctions.m_NamedCallSnapshotTicks[writeIndex] = snapshotTick;
    s_RemoteFunctions.m_NamedCallWriteIndex = (writeIndex + 1) % HudScriptElementsRemoteFunctionState::CALL_QUEUE_CAPACITY;
}

void CHudScriptElements::ExecuteNamedRemoteFunctionCalls(const std::int32_t tick)
{
    const std::uint32_t firstIndex = s_RemoteFunctions.m_NamedCallReadIndex;
    const std::int32_t firstTick = s_RemoteFunctions.m_NamedCallSnapshotTicks[firstIndex];
    if (firstTick == 0 || firstTick > tick)
        return;

    C_Player* localViewPlayer = C_Player::GetLocalViewPlayer();
    C_Player* localPlayer = C_Player::GetLocalPlayer();
    SquirrelManager* clientSquirrel = g_pSquirrel[ScriptContext::CLIENT];
    if (!localViewPlayer || !localPlayer || !g_pGlobals || !clientSquirrel || !clientSquirrel->m_pSQVM)
        return;

    const HSCRIPT localViewPlayerScope = localViewPlayer->GetScriptInstance();
    const HSCRIPT localPlayerScope = localPlayer->GetScriptInstance();
    clientSquirrel->m_pSQVM->SetTime(g_pGlobals->m_flCurTime);

    while (true)
    {
        const std::uint32_t readIndex = s_RemoteFunctions.m_NamedCallReadIndex;
        const std::int32_t snapshotTick = s_RemoteFunctions.m_NamedCallSnapshotTicks[readIndex];
        if (snapshotTick == 0 || snapshotTick > tick)
            break;

        NamedScriptRemoteFunctionCall& call = s_RemoteFunctions.m_NamedCalls[readIndex];
        SquirrelManager* squirrel = g_pSquirrel[ScriptContext::UI];
        HSCRIPT scope = nullptr;
        if (!call.m_CallFromUI)
        {
            squirrel = clientSquirrel;
            const ConVar* const killReplayPlayNonReplayRemoteCallsOnLocalClientPlayer =
                s_RemoteFunctions.m_KillReplayPlayNonReplayRemoteCallsOnLocalClientPlayer;
            const bool useLocalViewPlayer = !call.m_Replay && killReplayPlayNonReplayRemoteCallsOnLocalClientPlayer &&
                                            killReplayPlayNonReplayRemoteCallsOnLocalClientPlayer->GetBool();
            scope = useLocalViewPlayer ? localViewPlayerScope : localPlayerScope;
        }

        if (!squirrel || !squirrel->m_pSQVM)
            break;

        CSquirrelVM* vm = squirrel->m_pSQVM;
        HSCRIPT function = vm->FindFunction(call.m_FunctionName->c_str(), nullptr, scope);
        if (IsValid(function))
        {
            const HSCRIPT executeScope = IsValid(scope) ? scope : nullptr;
            vm->ExecuteFunction(function, call.m_Parameters.data(), static_cast<int>(call.m_ParameterCount), nullptr, executeScope);
            vm->ReleaseFunction(function);
        }

        call.m_FunctionName = nullptr;
        s_RemoteFunctions.m_NamedCallSnapshotTicks[readIndex] = 0;
        s_RemoteFunctions.m_NamedCallReadIndex = (readIndex + 1) % HudScriptElementsRemoteFunctionState::CALL_QUEUE_CAPACITY;
    }
}

DECLARE_HOOK(C_BaseScriptRemoteFunctions_ExecuteCallQueue, client.dll + 0x52E400, [](auto& hook, std::int32_t tick) -> std::int64_t
{
    const std::int64_t result = hook.Original(tick);
    CHudScriptElements::ExecuteNamedRemoteFunctionCalls(tick);
    return result;
})

DECLARE_HOOK(CHudScriptElements_Init, client.dll + 0x5366C0, [](auto&, CHudScriptElements* hudScriptElements) { hudScriptElements->Init(); })

DECLARE_HOOK(CHudScriptElements_MsgFunc_RemoteFunctionCall, client.dll + 0x53CA40,
             [](auto&, CHudScriptElements* hudScriptElements, bf_read* message) { hudScriptElements->MsgFunc_RemoteFunctionCall(message); })

DECLARE_HOOK(CHudScriptElements_MsgFunc_RemoteFunctionCallsChecksum, client.dll + 0x53D2C0,
             [](auto&, CHudScriptElements* hudScriptElements, bf_read* message) { hudScriptElements->MsgFunc_RemoteFunctionCallsChecksum(message); })

ON_DLL_LOAD_CLIENT_RELIESON("client.dll", HudScriptElements, ConVar, [](CModule module)
{
    CHudScriptElements::InitializeRemoteFunctions(module.Offset(0x54D3B0).RCast<pfnUserMsgHook>(), module.Offset(0x54D3F0).RCast<pfnUserMsgHook>(),
                                                  module.Offset(0x2951A60).RCast<ScriptRemoteFunctionRegistrationState*>(),
                                                  module.Offset(0x290C0A0).RCast<C_CBaseScriptRemoteFunctionCallReceiveQueue*>(),
                                                  g_pCVar->FindVar("killReplay_playNonReplayRemoteCallsOnLocalClientPlayer"));

    DISPATCH_HOOK(HudScriptElementsHooks, CHudScriptElements_MsgFunc_RemoteFunctionCall)
    DISPATCH_HOOK(HudScriptElementsHooks, CHudScriptElements_MsgFunc_RemoteFunctionCallsChecksum)
    DISPATCH_HOOK(HudScriptElementsHooks, C_BaseScriptRemoteFunctions_ExecuteCallQueue)
})
