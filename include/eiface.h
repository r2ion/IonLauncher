#pragma once

#include "edict.h"
#include "interface.h"

#include <cstdarg>
#include <cstddef>
#include <cstdint>

class CBaseEntity;
class CCommand;
class CUtlBuffer;
class CPlayerState;
class KeyValues;
class IRecipientFilter;
class Vector3;
class bf_read;
class bf_write;
struct client_textmessage_t;
struct con_nprint_s;
struct player_info_t;
struct CGlobalVars;
struct CSaveRestoreData;
struct CStandardSendProxies;
struct ServerClass;
struct datamap_t;
struct typedescription_t;

inline constexpr char SERVER_GAME_DLL_INTERFACE_VERSION[] = "ServerGameDLL005";
inline constexpr char SERVER_GAME_ENTS_INTERFACE_VERSION[] = "ServerGameEnts002";
inline constexpr char SERVER_GAME_CLIENTS_INTERFACE_VERSION[] = "ServerGameClients004";
inline constexpr char SERVER_DLL_SHARED_APP_SYSTEMS_INTERFACE_VERSION[] = "VServerDllSharedAppSystems001";
inline constexpr char VENGINE_SERVER_INTERFACE_VERSION[] = "VEngineServer022";

class IServerDLLSharedAppSystems
{
public:
	virtual int Count() = 0;
	virtual const char* GetDllName(int index) = 0;
	virtual const char* GetInterfaceName(int index) = 0;
	virtual void* Destroy(std::uint32_t flags) = 0;
};

class IServerGameClients
{
public:
	virtual void GetPlayerLimits(int& minPlayers, int& maxPlayers, int& defaultMaxPlayers) const = 0;
	virtual bool ClientConnect(edict_t entity, const char* name, const char* address, char* reject,
		int maxRejectLength) = 0;
	virtual void ClientActive(edict_t entity, bool loadGame) = 0;
	virtual void ClientFullyConnect(edict_t entity, bool restore) = 0;
	virtual void ClientDisconnect(edict_t entity, void* reason) = 0;
	virtual void ClientPutInServer(edict_t entity, const char* playerName) = 0;
	virtual void ClientCommand(edict_t entity, const CCommand& command) = 0;
	virtual void ClientSettingsChanged(edict_t entity) = 0;
	virtual void ProcessUsercmds(edict_t player, bf_read* buffer, int commandCount,
		std::uint32_t totalCommandCount, int droppedPackets, bool ignore, bool paused) = 0;
	virtual CPlayerState* GetPlayerState(edict_t player) = 0;
	virtual void ClientEarPosition(edict_t player, Vector3* earOrigin) = 0;
	virtual void ClientCommandKeyValues(edict_t player, KeyValues* keyValues) = 0;
	virtual bool SendPlayerScreenshot(edict_t player, const void* jpegData, std::uint32_t jpegSize) = 0;
	virtual void SetPlayerName(edict_t player, const char* playerName) = 0;
	virtual void SetPlayerPlatformUserId(edict_t player, std::uint64_t platformUserId) = 0;
	virtual const char* GetPlayerName(edict_t player) = 0;
	virtual void SetCommandClient(int index) = 0;
	virtual void PostClientMessagesSent() = 0;
	virtual void NullSub18() = 0;
	virtual int GetMaxSplitscreenPlayers() = 0;
	virtual int GetMaxHumanPlayers() = 0;
};

class IServerGameDLL
{
public:
	virtual bool DLLInit(CreateInterfaceFn engineFactory, CreateInterfaceFn physicsFactory,
		CreateInterfaceFn fileSystemFactory, CGlobalVars* globals) = 0;
	virtual bool GameInit() = 0;
	virtual bool LevelInit(const char* mapName, const char* mapEntities, const char* oldLevel,
		const char* landmarkName, bool loadGame, bool background) = 0;
	virtual void ServerActivate() = 0;
	virtual void GameFrame(bool simulating) = 0;
	virtual void PreClientUpdate(bool simulating) = 0;
	virtual void LevelShutdown() = 0;
	virtual void GameShutdown() = 0;
	virtual void DLLShutdown() = 0;
	virtual float GetTickInterval() const = 0;
	virtual ServerClass* GetAllServerClasses() = 0;
	virtual bool IsServerClassListLocked() = 0;
	virtual void LockServerClassList() = 0;
	virtual const char* GetGameDescription() = 0;
	virtual void CreateNetworkStringTables() = 0;
	virtual void SaveWriteFields(CSaveRestoreData* saveData, const char* name, void* baseData,
		datamap_t* dataMap, typedescription_t* fields, int fieldCount) = 0;
	virtual void SaveReadFields(CSaveRestoreData* saveData, const char* name, void* baseData,
		datamap_t* dataMap, typedescription_t* fields, int fieldCount) = 0;
	virtual void SaveGlobalState(CSaveRestoreData* saveData) = 0;
	virtual void RestoreGlobalState(CSaveRestoreData* saveData) = 0;
	virtual void PreSave(CSaveRestoreData* saveData) = 0;
	virtual void Save(CSaveRestoreData* saveData) = 0;
	virtual void GetSaveComment(char* comment, int maxLength, float minutes, float seconds,
		bool noTime = false) = 0;
	virtual void WriteSaveHeaders(CSaveRestoreData* saveData) = 0;
	virtual void ReadRestoreHeaders(CSaveRestoreData* saveData) = 0;
	virtual void Restore(CSaveRestoreData* saveData, bool createPlayers) = 0;
	virtual bool IsRestoring() = 0;
	virtual bool SupportsSaveRestore(bool requested) = 0;
	virtual void OnSaveGameFinished(bool succeeded) = 0;
	virtual bool IsSaveGameSafeToCommit() = 0;
	virtual std::uint32_t GetServerClassSchemaChecksum() = 0;
	virtual bool GetUserMessageInfo(std::uint32_t messageType, char* name, int maxNameLength,
		int& size) = 0;
	virtual CStandardSendProxies* GetStandardSendProxies() = 0;
	virtual void PostInit() = 0;
	virtual void Think(bool finalTick) = 0;
	virtual void PreSaveGameLoaded(const char* saveName, bool currentlyInGame) = 0;
	virtual bool ShouldHideServer() = 0;
	virtual void InvalidateMdlCache() = 0;
	virtual void PostToolsInit() = 0;
	virtual void ApplyGameSettings(KeyValues* settings) = 0;
	virtual void GameServerSteamAPIActivated() = 0;
	virtual void ServerHibernationUpdate(bool hibernating) = 0;
	virtual bool ShouldPreferSteamAuth() = 0;
	virtual bool IsPlayerLevelComplete(edict_t player, std::uint32_t level) = 0;
	virtual int GetPlayerGeneration(edict_t player) = 0;
	virtual int GetPlayerLevel(edict_t player) = 0;
	virtual int GetPlayerXP(edict_t player) = 0;
	virtual int GetPlayerRank(edict_t player) = 0;
	virtual int GetPlayerScore(edict_t player) = 0;
	virtual int GetPlayerKills(edict_t player) = 0;
	virtual int GetPlayerDeaths(edict_t player) = 0;
	virtual void SetPlayerSkill(edict_t player, float value) = 0;
	virtual void GetPlayerVoiceStatus(edict_t player, bool* hasMic, bool* inPartyChat) = 0;
	virtual void OnClientSendingPersistenceToNewServer(edict_t player) = 0;
	virtual void ClaimClientSidePickup(edict_t player, int pickupType, std::uint32_t first,
		std::uint32_t second, std::uint32_t third) = 0;
	virtual void SetTeamFloatState(std::uint32_t teamIndex, float value) = 0;
	virtual void GetTeamFloatState(std::uint32_t teamIndex, float* value) = 0;
	virtual int GetTeamCount() = 0;
	virtual int GetTeamPlayerCount(int teamIndex) = 0;
	virtual int GetTeamScore(int teamIndex) = 0;
	virtual void SetTeamStatsFromIntegers(std::uint32_t teamIndex, int first, int second,
		int third) = 0;
	virtual void ClearRespawnExtensionRecords() = 0;
	virtual void ClearRespawnRecordActiveFlags() = 0;
	virtual void NullSub62() = 0;
	virtual void NullSub63() = 0;
	virtual void NullSub64() = 0;
	virtual void RefreshAnimatingEntityList(std::uint32_t value) = 0;
	virtual bool GetPlayerLinkedEntityIndices(int playerIndex, int* entityIndices,
		int* classIds, int* serialNumbers, int* count) = 0;
	virtual void OnReceivedSayTextMessage(std::uint32_t senderId, const char* text,
		bool teamChat) = 0;
	virtual std::uintptr_t SetPlayerLinkedEntityState(std::uintptr_t first, int second,
		std::uintptr_t third, std::uintptr_t fourth, bool fifth, int sixth,
		std::uintptr_t seventh) = 0;
	virtual bool GivePlayerItem(edict_t player, const char* itemName, std::uint32_t subtype) = 0;
	virtual bool LoadParticleSystemDefinitions(CUtlBuffer* buffer, bool preload,
		const char* sourceName) = 0;
	virtual void FinishParticleSystemDefinitionLoad() = 0;
};

class IServerGameEnts
{
public:
	virtual ~IServerGameEnts() = default;
	virtual void MarkEntitiesAsTouching(edict_t first, edict_t second) = 0;
	virtual void FreeContainingEntity(edict_t entity) = 0;
	virtual std::uintptr_t AddEntityEFlags(std::uintptr_t entity, std::uint32_t flags) = 0;
	virtual std::uintptr_t MarkEntitiesAsTouchingIfValid(edict_t first, edict_t second) = 0;
	virtual edict_t BaseEntityToEdict(CBaseEntity* entity) = 0;
	virtual CBaseEntity* EdictToBaseEntity(edict_t entity) = 0;
	virtual std::uintptr_t PrepareEntityTransmitData(std::int32_t* state) = 0;
	virtual std::uintptr_t UpdateEntityTransmitState(std::int32_t* state) = 0;
	virtual std::uintptr_t PreparePlayersForTransmit() = 0;
	virtual std::uintptr_t BuildEntityChangeList(void* context, std::int32_t* state,
		void* output) = 0;
	virtual void NullSub11() = 0;
	virtual std::uintptr_t BuildClientSnapshotRange(void* context, void* state, void* output,
		int firstIndex, int lastIndex, bool fullUpdate) = 0;
	virtual std::uintptr_t FinalizeClientSnapshot(void* previousSnapshot,
		void* currentSnapshot) = 0;
	virtual bool WriteSnapshotDelta(std::uintptr_t client, void* previousSnapshot,
		void* currentSnapshot, void* output) = 0;
	virtual std::uintptr_t CopySnapshotGroupFields(std::uint8_t group, void* snapshot,
		void* output) = 0;
	virtual std::uintptr_t CopyUnflaggedSnapshotGroupFields(std::uint8_t group,
		void* snapshot, void* output) = 0;
	virtual std::uintptr_t WriteFullSnapshot(int group, void* snapshot, void* output) = 0;
	virtual void NullSub18() = 0;
	virtual void NullSub19() = 0;
	virtual bool BuildBitPackedSnapshotDelta(std::uint32_t clientIndex, void* state,
		void* output, std::uint32_t* result) = 0;
	virtual std::uintptr_t WriteTempEntitySnapshotDelta(void* first, void* second,
		void* third) = 0;
	virtual edict_t CreateEntityByName(const char* className) = 0;
	virtual void DispatchSpawn(edict_t entity) = 0;
	virtual std::uintptr_t CreateClassBaselineSnapshot(const edict_t* entities) = 0;
	virtual std::uintptr_t WriteClassBaseline(int classIndex, void* bitWriter) = 0;
	virtual std::uintptr_t ProcessQueuedEntityUpdates() = 0;
};

class IVEngineServer
{
public:
	virtual void ChangeLevel(const char* pMapName, const char* pLandmarkName) = 0; // 0
	virtual int GetServerSpawnCount() = 0; // 1
	virtual bool IsMapValid(const char* pMapName) = 0; // 2
	virtual bool GetMapCRC(const char* pMapName, std::uint32_t* pCrc) = 0; // 3
	virtual bool IsDedicatedServer() = 0; // 4
	virtual bool IsInEditMode() = 0; // 5
	virtual KeyValues* GetLaunchOptions() = 0; // 6
	virtual int PrecacheModel(const char* pModelName) = 0; // 7
	virtual bool IsModelPrecached(const char* pModelName) = 0; // 8
	virtual bool IsSimulating() = 0; // 9
	virtual int GetEntityCount() = 0; // 10
	virtual float GetClientAvgLatency(int clientIndex, int flow) = 0; // 11
	virtual float GetClientAvgLoss(int clientIndex, int flow) = 0; // 12
	virtual bool IsClientValid(int clientIndex) = 0; // 13
	virtual int GetClientSnapshotHistorySize(int clientIndex) = 0; // 14
	virtual bool IsEntityInClientSnapshot(int clientIndex, int entityIndex) = 0; // 15
	virtual void PlayerChangesTeams(int clientIndex) = 0; // 16
	virtual bool RequestClientScreenshot(int clientIndex, bool unknownFlag,
		std::uint16_t requestId, bool anotherFlag) = 0; // 17
	virtual int CreateEdict(int forcedIndex) = 0; // 18
	virtual int RemoveEdict(std::uint16_t edictIndex) = 0; // 19
	virtual void FadeClientVolume(std::uint16_t clientIndex, float fadePercent,
		float fadeOutSeconds, float holdTime, float fadeInSeconds) = 0; // 20
	virtual void ServerCommand(const char* pCommand) = 0; // 21
	virtual void CbufExecute() = 0; // 22
	virtual void ClientCommand(std::uint16_t clientIndex, const char* pFormat, ...) = 0; // 23
	virtual void LightStyle(int styleIndex, const char* pStyleValue) = 0; // 24
	virtual bf_write* UserMessageBegin(IRecipientFilter* pRecipients, std::uint32_t messageType,
		const char* pMessageName, int bitCount) = 0; // 25
	virtual void MessageEnd() = 0; // 26
	virtual void ClientPrintf(std::uint16_t clientIndex, const char* pMessage) = 0; // 27
	virtual void ConNPrintf(int position, const char* pFormat, ...) = 0; // 28
	virtual void ConNXPrintf(const con_nprint_s* pInfo, const char* pFormat, ...) = 0; // 29
	virtual void SetView(std::uint16_t clientIndex, std::uint16_t viewEntityIndex) = 0; // 30
	virtual std::uint16_t GetViewEntity(std::uint16_t viewEntityIndex) = 0; // 31
	virtual void SetClientViewEntity(std::uint16_t clientIndex,
		std::int16_t viewEntityIndex) = 0; // 32
	virtual void SetClientSendTableCRC(std::uint16_t clientIndex, std::uint32_t crc) = 0; // 33
	virtual void CrosshairAngle(std::uint16_t clientIndex, float pitch, float yaw) = 0; // 34
	virtual bool GrantClientSidePickup(std::uint16_t clientIndex, int pickupType,
		int pickupId, const int* pPickupData, int count, int flags) = 0; // 35
	virtual void GetGameDir(char* pGameDir, int maxLength) = 0; // 36
	virtual int CompareFileTime(const char* pFileName1, const char* pFileName2,
		int* pCompare) = 0; // 37
	virtual bool LockNetworkStringTables(bool lock) = 0; // 38
	virtual int GetNumConnectedPlayers() = 0; // 39
	virtual int GetMaxClients() = 0; // 40
	virtual int GetNumAvailablePlayerSlots() = 0; // 41
	virtual int CreateFakeClient(const char* pName, const char* pRegion,
		const char* pPlaylistName, int team, float updateRate) = 0; // 42
	virtual const char* GetClientConVarValue(int clientIndex, const char* pName) = 0; // 43
	virtual const char* GetClientServerName(int clientIndex) = 0; // 44
	virtual const char* GetClientNetworkAddress(int clientIndex) = 0; // 45
	virtual float GetClientUpdateRate(int clientIndex) = 0; // 46
	virtual bool AreReplaysEnabled() = 0; // 47
	virtual void SetClientReplayParameters(int clientIndex, float delay,
		bool enabled, bool replayOnly) = 0; // 48
	virtual float GetClientReplayDelay(int clientIndex) = 0; // 49
	virtual bool IsClientReplayEnabled(int clientIndex) = 0; // 50
	virtual void SetClientReplayRequested(int clientIndex, bool requested) = 0; // 51
	virtual void SetClientReplaySnapshot(int clientIndex, float snapshotTime,
		int commandTick) = 0; // 52
	virtual void SetClientReplayState(int clientIndex, bool state) = 0; // 53
	virtual bool GetClientReplayState(int clientIndex) = 0; // 54
	virtual void NullSub55(int clientIndex, std::uint32_t value) = 0; // 55
	virtual void NullSub56(int clientIndex, std::uint32_t value) = 0; // 56
	virtual void NullSub57(int clientIndex, std::uint32_t value) = 0; // 57
	virtual const char* ParseFile(const char* pData, char* pToken, int maxLength) = 0; // 58
	virtual bool CopyLocalFile(const char* pSource, const char* pDestination) = 0; // 59
	virtual int GetClusterForOrigin(const Vector3& origin) = 0; // 60
	virtual bool LoadGameState(const char* pMapName, bool createPlayers) = 0; // 61
	virtual bool DoesSaveGameExist(const char* pSaveName) = 0; // 62
	virtual bool IsSaveGameValid(const char* pSaveName) = 0; // 63
	virtual bool ReadSaveGameMetadata(const char* pSaveName, char* pMapName,
		char* pComment, char* pAdditionalData) = 0; // 64
	virtual bool IsSaveRestoreInProgress() = 0; // 65
	virtual bool IsTrialVersion() = 0; // 66
	virtual const char* GetMapEntitiesString() = 0; // 67
	virtual int GetPlaylistCount() = 0; // 68
	virtual const char* GetPlaylistName(int playlistIndex) = 0; // 69
	virtual const char* FindPlaylistVarWithOverrides(const char* pPlaylistName,
		const char* pVariableName) = 0; // 70
	virtual const char* FindPlaylistVar(const char* pPlaylistName,
		const char* pVariableName) = 0; // 71
	virtual bool SetPlaylistVarOverride(const char* pName, const char* pValue) = 0; // 72
	virtual int GetPlaylistMaxPlayers(const char* pPlaylistName) = 0; // 73
	virtual int GetPlaylistMaxPlayersForGameMode(const char* pPlaylistName) = 0; // 74
	virtual int GetPlaylistMaxTeams(const char* pPlaylistName) = 0; // 75
	virtual int GetPlaylistMaxTeamsForGameMode(const char* pPlaylistName) = 0; // 76
	virtual void ClearPlaylistVarOverrides() = 0; // 77
	virtual bool SetGameMode(const char* pGameModeName) = 0; // 78
	virtual const char* GetCurrentGameMode() = 0; // 79
	virtual const char* GetGameModeVar(const char* pGameModeName,
		const char* pVariableName) = 0; // 80
	virtual bool SetPlaylist(const char* pPlaylistName) = 0; // 81
	virtual const char* GetCurrentPlaylistName() = 0; // 82
	virtual const char* GetCurrentPlaylistVar(const char* pVariableName) = 0; // 83
	virtual const char* GetCurrentPlaylistVarWithoutOverrides(const char* pVariableName) = 0; // 84
	virtual int GetCurrentPlaylistGameModeCount() = 0; // 85
	virtual int GetPlaylistGameModeCount(const char* pPlaylistName) = 0; // 86
	virtual const char* GetCurrentPlaylistGameModeName(std::uint8_t gameModeIndex) = 0; // 87
	virtual const char* GetPlaylistGameModeName(const char* pPlaylistName,
		std::uint8_t gameModeIndex) = 0; // 88
	virtual const char* GetPlaylistGameModeVar(const char* pPlaylistName,
		std::uint8_t gameModeIndex, const char* pVariableName) = 0; // 89
	virtual const char* GetCurrentPlaylistGameModeVar(std::uint8_t gameModeIndex,
		const char* pVariableName, bool includeOverrides) = 0; // 90
	virtual int GetCurrentPlaylistMapCount(std::uint8_t gameModeIndex) = 0; // 91
	virtual int GetPlaylistMapCount(const char* pPlaylistName,
		std::uint8_t gameModeIndex) = 0; // 92
	virtual const char* GetCurrentPlaylistMapName(std::uint8_t gameModeIndex,
		std::uint8_t mapIndex) = 0; // 93
	virtual const char* GetPlaylistMapName(const char* pPlaylistName,
		std::uint8_t gameModeIndex, std::uint8_t mapIndex) = 0; // 94
	virtual int GetCurrentPlaylistMapCountAllGameModes() = 0; // 95
	virtual int GetPlaylistMapCountAllGameModes(const char* pPlaylistName) = 0; // 96
	virtual bool IsMatchmakingDedicatedServer() = 0; // 97
	virtual void FlushMatchmakingTelemetry() = 0; // 98
	virtual bool IsClientSearching(int clientIndex) = 0; // 99
	virtual bool IsAnyClientSearching() = 0; // 100
	virtual bool IsClientInParty(int clientIndex) = 0; // 101
	virtual bool IsPrivateMatch() = 0; // 102
	virtual bool ReturnFalse103() = 0; // 103
	virtual bool IsMultiplayer() = 0; // 104
	virtual client_textmessage_t* TextMessageGet(const char* pMessageName) = 0; // 105
	virtual void* GetLoggingChannelRecord(std::uint32_t channelIndex) = 0; // 106
	virtual void SolidMoved(std::uint16_t entityIndex, void* pCollideable,
		const Vector3* pPreviousOrigin) = 0; // 107
	virtual void TriggerMoved(std::uint16_t entityIndex) = 0; // 108
	virtual void* CreateSpatialPartition(const Vector3* pWorldMins,
		const Vector3* pWorldMaxs) = 0; // 109
	virtual void DestroySpatialPartition(void* pPartition) = 0; // 110
	virtual const void* GetEntityTransmitBitsForClient(int clientIndex) = 0; // 111
	virtual int GetEntityTransmitState(std::uint16_t entityIndex) = 0; // 112
	virtual bool IsPaused() = 0; // 113
	virtual float GetTimescale() = 0; // 114
	virtual bool IsLevelMainMenuBackground() = 0; // 115
	virtual bool IsInternalBuild() = 0; // 116
	virtual int GetAppID() = 0; // 117
	virtual bool IsLowViolence() = 0; // 118
	virtual bool IsAnyClientLowViolence() = 0; // 119
	virtual void InsertServerCommand(const char* pCommand) = 0; // 120
	virtual bool GetPlayerInfo(int clientIndex, player_info_t* pInfo) = 0; // 121
	virtual const char* GetClientPlatformDescription(std::uint16_t clientIndex) = 0; // 122
	virtual void SetDedicatedServerBenchmarkMode(bool enabled) = 0; // 123
	virtual bool IsMapReslistGenerationEnabled() = 0; // 124
	virtual bool IsGeneratingMapReslist() = 0; // 125
	virtual bool IsServerStateFlagSet126() = 0; // 126
	virtual void SetTimescale(float timescale) = 0; // 127
	virtual void SetGamestatsData(void* pGamestatsData) = 0; // 128
	virtual void* GetGamestatsData() = 0; // 129
	virtual void ConfigureNetworkSocketPorts(bool useServerPorts) = 0; // 130
	virtual void ClientCommandKeyValues(int clientIndex, KeyValues* pCommand) = 0; // 131
	virtual void MarkTeamsAsBalancedOn() = 0; // 132
	virtual void MarkTeamsAsBalancedOff() = 0; // 133
	virtual void SetClientTeam(std::uint16_t clientIndex, int team) = 0; // 134
	virtual int GetClientTeam(std::uint16_t clientIndex) = 0; // 135
	virtual int GetClientStateValue136(std::uint16_t clientIndex) = 0; // 136
	virtual void DisconnectClient(std::uint16_t clientIndex, const char* pReason) = 0; // 137
	virtual void UpdateMatchState(int phase, int mode, int winningTeam, int winningRound,
		int stateValue5, int stateValue6, int stateValue7, int stateValue8,
		int stateValue9) = 0; // 138
	virtual void InitializeMatchState(int stateId, const char* pName) = 0; // 139
	virtual void NullSub140() = 0; // 140
	virtual void NullSub141() = 0; // 141
	virtual void NullSub142() = 0; // 142
	virtual bool RemoveClientRemoteSessions(void* pContext,
		std::uint16_t clientIndex) = 0; // 143
	virtual void IncrementClientCounter144(int clientIndex) = 0; // 144
	virtual void IncrementClientCounter145(int clientIndex) = 0; // 145
	virtual void AddClientCounter146(int clientIndex, int value) = 0; // 146
	virtual int RecordClientCounterValue147(int clientIndex, int value) = 0; // 147
	virtual void AddClientCounter148(int clientIndex, int value) = 0; // 148
	virtual bool IsGameFullyInstalled() = 0; // 149
	virtual bool ReturnTrue150() = 0; // 150
	virtual float GetGameInstallProgress() = 0; // 151
	virtual void WriteBandwidthStatsToFile(const char* pFileName) = 0; // 152
	virtual void BeginServerTelemetryBatch(int taxonomyMajor, int taxonomyMinor,
		const void* pHeaderFields) = 0; // 153
	virtual void AppendServerTelemetryEvent(const char* pEventName,
		const void* pCoreFields, const void* pEventFields) = 0; // 154
	virtual const char* GetTelemetryProjectId() = 0; // 155
	virtual std::uintptr_t ReturnZero156() = 0; // 156
	virtual void NullSub157() = 0; // 157
	virtual const char* ReturnEmptyString158() = 0; // 158
	virtual bool IsGameLogicProcessing() = 0; // 159
	virtual void BeginGameLogicProcessing() = 0; // 160
	virtual void EndGameLogicProcessing() = 0; // 161
	virtual std::uintptr_t ReturnZero162() = 0; // 162
	virtual bool ReturnFalse163() = 0; // 163
	virtual int GetHostStateValue164() = 0; // 164
	virtual int GetMaxTeams() = 0; // 165
	virtual int GetNumClientsWithoutPlayerEntities() = 0; // 166
	virtual int GetPlatformIndex() = 0; // 167
	virtual const char* ReturnEmptyString168() = 0; // 168
	virtual bool IsPartyServer() = 0; // 169
	virtual void SubmitTrainingStats(std::uint64_t userId, int numRuns,
		int numChallenges, float bestTime) = 0; // 170
	virtual void NullSub171() = 0; // 171
	virtual const char* GetServerSessionId() = 0; // 172
	virtual const char* GetBuildVersionString() = 0; // 173
	virtual void NullSub174() = 0; // 174
	virtual std::uintptr_t EngineSpewFunc(int channel, const char* pFormat,
		std::va_list arguments) = 0; // 175
	virtual void NullSub176(std::uint32_t value) = 0; // 176
	virtual bool SetPersistentVarFromCommand(int clientIndex, const void* pCommand,
		int argumentCount) = 0; // 177
	virtual bool IsUsingCachedPersistenceDefFile() = 0; // 178
	virtual bool GetPersistentArrayCount(const char* pArrayName,
		std::uintptr_t* pCount) = 0; // 179
	virtual bool FindPersistentArrayElement(const char* pArrayName,
		const char* pElementName, std::uintptr_t* pIndex) = 0; // 180
	virtual bool GetPersistentArrayElementName(const char* pArrayName,
		int elementIndex, const char** ppElementName) = 0; // 181
	virtual bool ResolvePersistentVarTypeValue(const char* pVarName,
		std::uintptr_t* pTypeValue) = 0; // 182
	virtual bool IsPersistentArrayElementValid(const char* pArrayName,
		const char* pElementName, bool* pIsValid) = 0; // 183
	virtual const char* GetClientPlatformId(int clientIndex) = 0; // 184
	virtual bool IsClientPersistenceReady(int clientIndex) = 0; // 185
	virtual bool FindPersistentVar(int clientIndex, const char* pVarName,
		std::uintptr_t* pVarIndex) = 0; // 186
	virtual int GetPersistentVarType(int clientIndex,
		std::uintptr_t varIndex) = 0; // 187
	virtual bool GetPersistentVarBool(int clientIndex,
		std::uintptr_t varIndex) = 0; // 188
	virtual int GetPersistentVarInt(int clientIndex,
		std::uintptr_t varIndex) = 0; // 189
	virtual float GetPersistentVarFloat(int clientIndex,
		std::uintptr_t varIndex) = 0; // 190
	virtual const char* GetPersistentVarString(int clientIndex,
		std::uintptr_t varIndex) = 0; // 191
	virtual const char* GetPersistentVarEnumName(int clientIndex,
		std::uintptr_t varIndex) = 0; // 192
	virtual std::uintptr_t GetPersistentVarMetadata(int clientIndex,
		std::uintptr_t varIndex) = 0; // 193
	virtual bool GetPersistentVarAsInt(int clientIndex,
		std::uintptr_t varIndex, int* pValue) = 0; // 194
	virtual bool SetPersistentVarBool(int clientIndex,
		std::uintptr_t varIndex, bool value) = 0; // 195
	virtual bool SetPersistentVarInt(int clientIndex,
		std::uintptr_t varIndex, int value) = 0; // 196
	virtual bool SetPersistentVarFloat(int clientIndex,
		std::uintptr_t varIndex, float value) = 0; // 197
	virtual bool SetPersistentVarString(int clientIndex,
		std::uintptr_t varIndex, const char* pValue) = 0; // 198
	virtual bool SetPersistentVarEnumName(int clientIndex,
		std::uintptr_t varIndex, const char* pValue) = 0; // 199
	virtual const char* const* GetEntitlementNames() = 0; // 200
	virtual bool SendRemoteServerLog(const char* pFormat, ...) = 0; // 201
	virtual void NullSub202() = 0; // 202
	virtual bool ShouldMoveOneCommandPerClientFrame() = 0; // 203
	virtual void MarkFakeClientCommunityDataReady(int clientIndex) = 0; // 204
	virtual void MarkServerStateDirty() = 0; // 205
	virtual void NullSub206() = 0; // 206
	virtual bool ShouldProcessServerTiming207() = 0; // 207
	virtual bool IsServerLoading() = 0; // 208
	virtual void* GetServerClassByIndex(int classIndex) = 0; // 209
	virtual void ResetLevelLoadingProgress() = 0; // 210
};
static_assert(sizeof(IVEngineServer) == sizeof(void*));

// Retail Titanfall 2 x64_retail/engine.dll SHA-256
// 58eb1a1b44b30275bdd21368de264d856bc310d37d93a0f76f111f0026913487.

static_assert(sizeof(IServerDLLSharedAppSystems) == sizeof(void*));
static_assert(sizeof(IServerGameClients) == sizeof(void*));
static_assert(sizeof(IServerGameDLL) == sizeof(void*));
static_assert(sizeof(IServerGameEnts) == sizeof(void*));

// Retail Titanfall 2 server.dll, SHA-256
// a5ca3a25c8ae56952a26141b0f6cdcb6c19086c8c39013f7cb345d5d723661af.
