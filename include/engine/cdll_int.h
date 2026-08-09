#pragma once

#include "client_textmessage.h"
#include "con_nprint.h"
#include "mathlib/vector.h"
#include "player_info.h"

#include <cstddef>
#include <cstdint>

class KeyValues;
struct model_t;

inline constexpr char VENGINE_CLIENT_INTERFACE_VERSION[] = "VEngineClient013";

struct MatchmakingDatacenter_t
{
	char m_DisplayName[64];
	char m_ServiceName[64];
	std::uint8_t m_NetworkAddress[24];
	int m_PingMilliseconds;
	std::uint32_t m_State;
};

class IVEngineClient
{
public:
	virtual Vector3 GetLightForPoint(const Vector3& position, bool clamp) = 0; // 0
	virtual bool IsGameFullyInstalled() = 0; // 1
	virtual bool IsGamePartiallyInstalled() = 0; // 2
	virtual float GetGameInstallProgress() = 0; // 3
	virtual int LoadVPK(const char* pVpkPath) = 0; // 4
	virtual int ClientPrecacheLevel(const char* pLevelName, bool unknownFlag) = 0; // 5
	virtual int PrecacheLevelDuringVideo(const char* pLevelName) = 0; // 6
	virtual void SetLevelNameForLoading(const char* pLevelName) = 0; // 7
	virtual bool ClientIsPreCaching() = 0; // 8
	virtual void EnablePaintmapRender() = 0; // 9
	virtual void UnloadVPK() = 0; // 10
	virtual const char* GetLoadedVPKName() = 0; // 11
	virtual void SetAllLevelAssetsLoaded(bool loaded) = 0; // 12
	virtual void SetLightingProvider(void* pProvider) = 0; // 13
	virtual void SetLightingTransform(const float* pTransform) = 0; // 14
	virtual void GetLightingAtPoint(const Vector3* pPosition, Vector3* pColor) = 0; // 15
	virtual void SetWorldRenderListIndex(int renderListIndex) = 0; // 16
	virtual int GetWorldRenderListIndex() = 0; // 17
	virtual void GetIndexedRenderBounds(int index, Vector3* pBounds) = 0; // 18
	virtual std::uint32_t GetIndexedRenderFlags(int index) = 0; // 19
	virtual bool HasIndexedWorldModelData(int index) = 0; // 20
	virtual const char* ParseFile(const char* pData, char* pToken, int maxLength) = 0; // 21
	virtual bool CopyLocalFile(const char* pSource, const char* pDestination) = 0; // 22
	virtual void GetScreenSize(int* pWidth, int* pHeight) = 0; // 23
	virtual int GetWindowRefreshRate() = 0; // 24
	virtual void Disconnect(const char* pReason) = 0; // 25
	virtual void ServerCmd(const char* pCommand) = 0; // 26
	virtual void ClientCmd(const char* pCommand) = 0; // 27
	virtual bool GetPlayerInfo(int playerIndex, player_info_t* pInfo) = 0; // 28
	virtual int GetPlayerForUserID(int userId, int splitScreenSlot) = 0; // 29
	virtual client_textmessage_t* TextMessageGet(const char* pMessageName) = 0; // 30
	virtual player_info_t* GetPlayerInfoByIndex(int playerIndex) = 0; // 31
	virtual bool ConIsVisible() = 0; // 32
	virtual int GetLastEntitlementIndex() = 0; // 33
	virtual bool HasPlayerEntitlement(int entitlementIndex) = 0; // 34
	virtual bool IsDLCStoreUnavailable() = 0; // 35
	virtual bool IsCommunicationRestricted() = 0; // 36
	virtual void QueryOriginOffers() = 0; // 37
	virtual void OnOpenDLCStore() = 0; // 38
	virtual void OnCloseDLCStore() = 0; // 39
	virtual void IsOOB() = 0; // 40
	virtual bool ShowCheckout(int offerIndex) = 0; // 41
	virtual int GetActiveSplitscreenPlayer() = 0; // 42
	virtual int GetLocalPlayer() = 0; // 43
	virtual int GetLocalPlayerForSplitScreenSlot(int splitScreenSlot) = 0; // 44
	virtual void SetLocalClientStateValue(int splitScreenSlot, int value) = 0; // 45
	virtual void ClearLocalClientStateValue(int splitScreenSlot) = 0; // 46
	virtual std::uint32_t GetLatestSnapshotField14(int splitScreenSlot) = 0; // 47
	virtual std::uint32_t GetLatestSnapshotField18(int splitScreenSlot) = 0; // 48
	virtual void InitModelLoader() = 0; // 49
	virtual bool IsWorldModelReady() = 0; // 50
	virtual const model_t* GetWorldModel() = 0; // 51
	virtual void RefreshBrushModelRenderData() = 0; // 52
	virtual void SetBrushModelRenderable(int modelIndex, void* pRenderable) = 0; // 53
	virtual void AllocateClientEntityRenderData(void* pRenderable, std::uint8_t flags) = 0; // 54
	virtual void FreeClientEntityRenderData(void* pRenderable) = 0; // 55
	virtual const model_t* LoadModel(const char* pModelName, bool prop) = 0; // 56
	virtual float Time() = 0; // 57
	virtual float GetLastTimeStamp() = 0; // 58
	virtual float GetLastServerTick() = 0; // 59
	virtual bool GetViewOriginOverride(Vector3* pOrigin, int* pValue, int stateId) = 0; // 60
	virtual void SetViewOriginOverride(const Vector3* pOrigin, int value, int stateId,
		std::uint32_t sequence, std::uint32_t activationTick) = 0; // 61
	virtual void ResetViewOriginOverride() = 0; // 62
	virtual void GetViewAngles(QAngle* pAngles) = 0; // 63
	virtual void SetViewAngles(const QAngle* pAngles) = 0; // 64
	virtual bool IsLocalClientViewStateEnabled() = 0; // 65
	virtual void SetLocalClientViewStateEnabled(bool enabled) = 0; // 66
	virtual int GetMaxClients() = 0; // 67
	virtual std::uint32_t GetEntitySnapshotSerialNumber(const void* pSnapshotEntity) = 0; // 68
	virtual float GetClientDeltaBufferUsagePercent() = 0; // 69
	virtual int GetPlaylistVersion() = 0; // 70
	virtual bool ReturnTrue71() = 0; // 71
	virtual int GetPlaylistCount() = 0; // 72
	virtual const char* GetPlaylistName(int playlistIndex) = 0; // 73
	virtual KeyValues* FindPlaylistVarWithOverrides(const char* pPlaylistName,
		const char* pVariableName) = 0; // 74
	virtual KeyValues* FindPlaylistVar(const char* pPlaylistName, const char* pVariableName) = 0; // 75
	virtual int GetPlaylistMaxPlayers(const char* pPlaylistName) = 0; // 76
	virtual int GetPlaylistMaxPlayersForGameMode(const char* pPlaylistName) = 0; // 77
	virtual int GetPlaylistMaxTeams(const char* pPlaylistName) = 0; // 78
	virtual int GetPlaylistMaxTeamsForGameMode(const char* pPlaylistName) = 0; // 79
	virtual bool IsPlaylistVisible(const char* pPlaylistName) = 0; // 80
	virtual bool IsClientPlaylistSelectionDisabled() = 0; // 81
	virtual bool HasPendingPlaylistVarOverrides() = 0; // 82
	virtual int GetPlaylistVarOverrideCount() = 0; // 83
	virtual void* GetPlaylistVarOverride(int overrideIndex) = 0; // 84
	virtual bool SetPlaylistVarOverride(const char* pName, const char* pValue) = 0; // 85
	virtual bool SetGameMode(const char* pGameModeName) = 0; // 86
	virtual const char* GetCurrentGameMode() = 0; // 87
	virtual const char* GetGameModeVar(const char* pGameModeName, const char* pVariableName) = 0; // 88
	virtual bool SetPlaylist(const char* pPlaylistName) = 0; // 89
	virtual const char* GetCurrentPlaylistName() = 0; // 90
	virtual const char* GetCurrentPlaylistNameOrEmpty() = 0; // 91
	virtual const char* GetCurrentPlaylistVar(const char* pVariableName) = 0; // 92
	virtual const char* GetCurrentPlaylistVarForGameMode(const char* pVariableName,
		const char* pGameModeName) = 0; // 93
	virtual const char* GetCurrentPlaylistVarWithoutOverrides(const char* pVariableName) = 0; // 94
	virtual bool UseSingleplayerAimAssistRules() = 0; // 95
	virtual int GetCurrentPlaylistGameModeCount() = 0; // 96
	virtual int GetPlaylistGameModeCount(const char* pPlaylistName) = 0; // 97
	virtual const char* GetCurrentPlaylistGameModeName(std::uint8_t gameModeIndex) = 0; // 98
	virtual const char* GetPlaylistGameModeName(const char* pPlaylistName,
		std::uint8_t gameModeIndex) = 0; // 99
	virtual const char* GetPlaylistGameModeVar(const char* pPlaylistName, std::uint8_t gameModeIndex,
		const char* pVariableName) = 0; // 100
	virtual const char* GetCurrentPlaylistGameModeVar(std::uint8_t gameModeIndex,
		const char* pVariableName, bool includeOverrides) = 0; // 101
	virtual int GetCurrentPlaylistMapCount(std::uint8_t gameModeIndex) = 0; // 102
	virtual int GetPlaylistMapCount(const char* pPlaylistName, std::uint8_t gameModeIndex) = 0; // 103
	virtual const char* GetCurrentPlaylistMapName(std::uint8_t gameModeIndex,
		std::uint8_t mapIndex) = 0; // 104
	virtual const char* GetPlaylistMapName(const char* pPlaylistName, std::uint8_t gameModeIndex,
		std::uint8_t mapIndex) = 0; // 105
	virtual int GetCurrentPlaylistMapCountAllGameModes() = 0; // 106
	virtual int GetPlaylistMapCountAllGameModes(const char* pPlaylistName) = 0; // 107
	virtual bool IsPrivateMatch() = 0; // 108
	virtual bool ReturnFalse109() = 0; // 109
	virtual bool IsMultiplayer() = 0; // 110
	virtual int GetDatacenterCount() = 0; // 111
	virtual void SetDatacenterIndex(int datacenterIndex) = 0; // 112
	virtual const char* GetDatacenterName(int datacenterIndex) = 0; // 113
	virtual const char* GetDatacenterDisplayName(int datacenterIndex) = 0; // 114
	virtual int GetDatacenterPing(int datacenterIndex) = 0; // 115
	virtual int GetCurrentDatacenterIndex() = 0; // 116
	virtual const char* GetDatacenterPreference() = 0; // 117
	virtual const char* GetCurrentDatacenterDisplayName() = 0; // 118
	virtual int GetCurrentDatacenterPing() = 0; // 119
	virtual std::uint64_t GetDatacenterListRequestCount() = 0; // 120
	virtual void ResetPartyServerInfo(void*) = 0; // 121
	virtual void ResetMatchmakingStatus() = 0; // 122
	virtual void LeavePartyAndResetMatchmaking() = 0; // 123
	virtual bool IsMatchmakingAvailable() = 0; // 124
	virtual int GetMatchmakingStatus(int* pQueueIndex, int* pQueueCount,
		unsigned int* pStatusValue) = 0; // 125
	virtual const char* GetMatchmakingStatusString() = 0; // 126
	virtual const char* GetMatchmakingStatusField(std::uint8_t fieldIndex) = 0; // 127
	virtual bool ReturnTrue128() = 0; // 128
	virtual bool GetOriginGameInfoFlag129() = 0; // 129
	virtual bool IsOriginOnline() = 0; // 130
	virtual bool IsOriginAuthenticated() = 0; // 131
	virtual bool IsOriginFullyConnected() = 0; // 132
	virtual bool IsOriginInviteServiceEnabled() = 0; // 133
	virtual std::uint32_t GetOriginInitializationError() = 0; // 134
	virtual void ReinitializeOrigin() = 0; // 135
	virtual void OriginRequestTicket() = 0; // 136
	virtual bool StartOriginInviteThread() = 0; // 137
	virtual void NullSub138(void*, void*) = 0; // 138
	virtual bool ReturnFalse139() = 0; // 139
	virtual const char* ReturnEmptyString140() = 0; // 140
	virtual void NullSub141() = 0; // 141
	virtual void NullSub142() = 0; // 142
	virtual bool ReturnFalse143() = 0; // 143
	virtual bool ReturnFalse144() = 0; // 144
	virtual bool ReturnFalse145() = 0; // 145
	virtual bool ReturnFalse146() = 0; // 146
	virtual bool ReturnFalse147() = 0; // 147
	virtual bool ReturnFalse148() = 0; // 148
	virtual bool ReturnFalse149() = 0; // 149
	virtual bool ReturnFalse150() = 0; // 150
	virtual std::int64_t ReturnZero151() = 0; // 151
	virtual const char* ReturnEmptyString152() = 0; // 152
	virtual void NullSub153() = 0; // 153
	virtual bool ReturnTrue154() = 0; // 154
	virtual bool ReturnFalse155() = 0; // 155
	virtual std::int64_t ReturnZero156() = 0; // 156
	virtual void NullSub157() = 0; // 157
	virtual bool ReturnFalse158() = 0; // 158
	virtual bool ReturnFalse159() = 0; // 159
	virtual void NullSub160() = 0; // 160
	virtual void NullSub161() = 0; // 161
	virtual void NullSub162() = 0; // 162
	virtual void NullSub163() = 0; // 163
	virtual void NullSub164() = 0; // 164
	virtual void NullSub165() = 0; // 165
	virtual void NullSub166() = 0; // 166
	virtual void NullSub167() = 0; // 167
	virtual bool ReturnFalse168() = 0; // 168
	virtual bool ReturnTrue169() = 0; // 169
	virtual void NullSub170() = 0; // 170
	virtual bool IsTextCommunicationRestricted() = 0; // 171
	virtual void NullSub172() = 0; // 172
	virtual bool IsUserGeneratedContentRestricted() = 0; // 173
	virtual void NullSub174() = 0; // 174
	virtual void NullSub175() = 0; // 175
	virtual bool ReturnFalse176() = 0; // 176
	virtual void GrantAchievement(std::uint32_t achievementId) = 0; // 177
	virtual bool HasEAAccess() = 0; // 178
	virtual bool ReturnFalse179() = 0; // 179
	virtual bool ReturnFalse180() = 0; // 180
	virtual bool ReturnTrue181() = 0; // 181
	virtual int ReturnZero182() = 0; // 182
	virtual bool ReturnFalse183() = 0; // 183
	virtual bool ReturnFalse184() = 0; // 184
	virtual bool ReturnTrue185() = 0; // 185
	virtual bool ReturnTrue186() = 0; // 186
	virtual bool ReturnFalse187() = 0; // 187
	virtual bool ReturnTrue188() = 0; // 188
	virtual bool ReturnFalse189() = 0; // 189
	virtual bool ReturnFalse190() = 0; // 190
	virtual bool ReturnFalse191() = 0; // 191
	virtual bool ReturnFalse192() = 0; // 192
	virtual int ReturnZero193() = 0; // 193
	virtual void NullSub194() = 0; // 194
	virtual int ReturnZero195() = 0; // 195
	virtual bool ReturnFalse196() = 0; // 196
	virtual bool ReturnFalse197() = 0; // 197
	virtual void* GetNetProfileFrame(int historyIndex) = 0; // 198
	virtual void GetDefaultScreenSize(int* pWidth, int* pHeight) = 0; // 199
	virtual int GetClientFrameCount() = 0; // 200
	virtual int GetClientFrameHistoryCapacity() = 0; // 201
	virtual void ShowFriendProfile(std::uint64_t userId) = 0; // 202
	virtual bool IsMatchmakingDevModeEnabled() = 0; // 203
	virtual void GetPreferredDatacenterServerValues(const char* pServerName, int* pGlobalValue,
		int* pDatacenterValue) = 0; // 204
	virtual void* FindButtonForBinding(const char* pBinding) = 0; // 205
	virtual const char* GetBindingForButtonCode(std::uint32_t buttonCode) = 0; // 206
	virtual void StartKeyTrapMode() = 0; // 207
	virtual bool CheckDoneKeyTrapping(std::uint32_t* pButtonCode) = 0; // 208
	virtual bool IsInGame() = 0; // 209
	virtual bool IsConnected() = 0; // 210
	virtual bool IsDrawingLoadingImage() = 0; // 211
	virtual void EndLoadingPlaque() = 0; // 212
	virtual int GetLocalPlayerTeam() = 0; // 213
	virtual void ConNPrintf(int position, const char* pFormat, ...) = 0; // 214
	virtual void ConNXPrintf(const con_nprint_s* pInfo, const char* pFormat, ...) = 0; // 215
	virtual const char* GetGameDirectory() = 0; // 216
	virtual int GameLumpVersion(int lumpId) = 0; // 217
	virtual void LinearToGamma(const float* pLinear, float* pGamma) = 0; // 218
	virtual float LightStyleValue(int style) = 0; // 219
	virtual int GetDXSupportLevel() = 0; // 220
	virtual bool ReturnFalse221() = 0; // 221
	virtual const char* GetLevelName() = 0; // 222
	virtual const char* GetLevelNameShort() = 0; // 223
	virtual bool IsInMultiplayerLobby() = 0; // 224
	virtual void* GetVoiceTweakAPI() = 0; // 225
	virtual void EngineStatsBeginFrame() = 0; // 226
	virtual void NullSub227() = 0; // 227
	virtual void FireEvents(float eventTime) = 0; // 228
	virtual bool ComputeLighting(const float* pPoint, const float* pNormal, bool clamp, float scale,
		const float* pModulation, const float* pLightingOrigin, float* pColor) = 0; // 229
	virtual void* GetNetChannelInfo() = 0; // 230
	virtual float GetNetChannelAvgLatency(int flow) = 0; // 231
	virtual float GetNetChannelAvgLoss(int flow) = 0; // 232
	virtual float GetNetChannelAvgChoke(int flow) = 0; // 233
	virtual float GetNetChannelAvgData(int flow) = 0; // 234
	virtual float GetNetChannelAvgPackets(int flow) = 0; // 235
	virtual int GetNetChannelTotalPackets(int flow) = 0; // 236
	virtual int GetNetChannelTotalData(int flow) = 0; // 237
	virtual int GetNetChannelSequenceNumber(int flow) = 0; // 238
	virtual void GetNetChannelRemoteFramerate(float* pFrameTime, float* pFrameTimeStdDeviation,
		std::uint8_t* pServerCpu) = 0; // 239
	virtual bool IsValidNetPacket(int flow, int frameNumber) = 0; // 240
	virtual float GetNetPacketTime(int flow, int frameNumber) = 0; // 241
	virtual int GetNetPacketBytes(int flow, int frameNumber, int group) = 0; // 242
	virtual void GetNetPacketResponseLatency(int flow, int frameNumber,
		int* pLatencyMilliseconds, int* pChoke) = 0; // 243
	virtual int GetNetPacketHistoryCount() = 0; // 244
	virtual void DebugDrawPhysCollide(const void* pCollide, void* pMaterial,
		const float* pTransform, const std::uint8_t* pColor) = 0; // 245
	virtual void DebugDrawCollisionMesh(const void* pMesh, int meshCount,
		const float* pTransform) = 0; // 246
	virtual bool IsClientPaused() = 0; // 247
	virtual const char* GetModelName(const model_t* pModel) = 0; // 248
	virtual void CheckPoint(const char* pName) = 0; // 249
	virtual bool IsPlayingDemo() = 0; // 250
	virtual bool IsRecordingDemo() = 0; // 251
	virtual bool IsPlayingTimeDemo() = 0; // 252
	virtual int GetDemoRecordingTick() = 0; // 253
	virtual int GetDemoPlaybackTick() = 0; // 254
	virtual int GetDemoPlaybackStartTick() = 0; // 255
	virtual float GetDemoPlaybackTimeScale() = 0; // 256
	virtual int GetDemoPlaybackTotalTicks() = 0; // 257
	virtual bool IsPaused() = 0; // 258
	virtual bool HasDebugOverlayBounds() = 0; // 259
	virtual float GetTimescale() = 0; // 260
	virtual bool IsTakingScreenshot() = 0; // 261
	virtual bool IsReplayActive() = 0; // 262
	virtual bool GetLocalClientFlag263() = 0; // 263
	virtual bool GetLocalClientFlag264() = 0; // 264
	virtual bool GetLocalClientFlag265() = 0; // 265
	virtual bool CanProcessLocalClientInput() = 0; // 266
	virtual bool CanProcessSignedOnLocalClientInput() = 0; // 267
	virtual bool IsLevelMainMenuBackground() = 0; // 268
	virtual void GetUILanguage(char* pDestination, int destinationLength) = 0; // 269
	virtual void GetRuiFunctionTable(void** pFunctions) = 0; // 270
	virtual const char* GetMapEntitiesString() = 0; // 271
	virtual bool ReturnFalse272() = 0; // 272
	virtual float GetScreenAspectRatio(int width, int height) = 0; // 273
	virtual void GrabPreColorCorrectedFrame(int x, int y, int width, int height) = 0; // 274
	virtual int GetHostVersion() = 0; // 275
	virtual const char* GetVersionString() = 0; // 276
	virtual void ClientSayText(const char* pText, int destination, bool teamOnly) = 0; // 277
	virtual void ExecuteClientCmd(const char* pCommand) = 0; // 278
	virtual void ClientCmdUnrestricted(const char* pCommand) = 0; // 279
	virtual int GetAppID() = 0; // 280
	virtual void ClientCmdUnrestrictedForCurrentPlayer(const char* pCommand) = 0; // 281
	virtual void SetRestrictServerCommands(bool restrict) = 0; // 282
	virtual void SetRestrictClientCommands(bool restrict) = 0; // 283
	virtual bool ReturnFalse284() = 0; // 284
	virtual void ReadConfiguration(bool readDefault) = 0; // 285
	virtual bool MapLoadFailed() = 0; // 286
	virtual bool SaveGameExists(const char* pFileName) = 0; // 287
	virtual bool GetSaveGameMetadata(const char* pFileName, void* pMapName,
		void* pComment, void* pElapsedTime) = 0; // 288
	virtual bool ValidateSaveGame(const char* pFileName) = 0; // 289
	virtual bool IsLowViolence() = 0; // 290
	virtual void NullSub291() = 0; // 291
	virtual void NullSub292() = 0; // 292
	virtual const char* GetMostRecentSaveGame() = 0; // 293
	virtual void ResetDemoInterpolation() = 0; // 294
	virtual int ReturnZero295() = 0; // 295
	virtual int ReturnZero296() = 0; // 296
	virtual void SetLocalPlayerQueryEnabled(void* pContext, void* pPlayer,
		bool enabled) = 0; // 297
	virtual bool CanGetLocalPlayer() = 0; // 298
	virtual int GetLocalPlayerEntityIndex(int localPlayerSlot) = 0; // 299
	virtual bool HasActiveSplitScreenPlayer() = 0; // 300
	virtual bool IsLocalPlayerActive(int localPlayerSlot) = 0; // 301
	virtual int ReturnZero302() = 0; // 302
	virtual int GetNextActiveLocalPlayerSlot(int localPlayerSlot) = 0; // 303
	virtual void RegisterDemoCustomDataCallback(void* pCallback,
		std::uint64_t identifier) = 0; // 304
	virtual void RecordDemoCustomData(std::uint64_t identifier,
		const void* pData, std::size_t dataSize) = 0; // 305
	virtual bool InitializeToolFramework() = 0; // 306
	virtual void ShutdownToolFramework() = 0; // 307
	virtual bool IsGeneratingReslist() = 0; // 308
	virtual bool IsGeneratingReslistForCurrentMap() = 0; // 309
	virtual void SetTimescale(float timescale) = 0; // 310
	virtual void SetGamestatsData(void* pGamestatsData) = 0; // 311
	virtual void* GetGamestatsData() = 0; // 312
	virtual void* FindButtonForBinding(const char* pBinding, std::uint32_t userId,
		std::uint32_t flags, int fallbackCode) = 0; // 313
	virtual int KeyCodeForBinding(const char* pBinding, std::uint32_t userId,
		std::uint32_t flags, int fallbackCode) = 0; // 314
	virtual void InvalidateInputBindingCache() = 0; // 315
	virtual void UpdateDynamicLights() = 0; // 316
	virtual void UploadDynamicLightShaderData() = 0; // 317
	virtual void RestoreDynamicLights(const void* pLights, int lightCount) = 0; // 318
	virtual float GetHostFrameTime319() = 0; // 319
	virtual void SolidMoved(void* pSolidEntity, void* pSolidCollide,
		const Vector3* pPreviousOrigin) = 0; // 320
	virtual void TriggerMoved(void* pTriggerEntity) = 0; // 321
	virtual void* GetClientUIMouthInfo() = 0; // 322
	virtual bool IsTransitioningToLoad() = 0; // 323
	virtual void FileReceived(const char* pFileName, std::uint32_t transferId) = 0; // 324
	virtual void SendCmdKeyValues(KeyValues* pCommand) = 0; // 325
	virtual bool IsGameActiveWindow() = 0; // 326
	virtual bool IsServerActive() = 0; // 327
	virtual void ResetLevelLoadingProgress() = 0; // 328
	virtual std::uintptr_t GetInputContext(int contextType) = 0; // 329
	virtual int FormatMainMenuBackgroundName(char* pOutput, int outputSize) = 0; // 330
	virtual int GetHunkMemoryStats(void** ppStats) = 0; // 331
	virtual bool IsServerSpawnIdle() = 0; // 332
	virtual bool IsSaveRestoreInProgress() = 0; // 333
	virtual int GetClockDriftServerTick() = 0; // 334
	virtual float GetClockDriftFrameTime() = 0; // 335
	virtual const char* GetGameDirectoryName() = 0; // 336
	virtual bool IsGameLogicProcessing() = 0; // 337
	virtual void BeginGameLogicProcessing(bool forceTimeoutCheck) = 0; // 338
	virtual void EndGameLogicProcessing() = 0; // 339
	virtual std::uintptr_t ReturnZero340() = 0; // 340
	virtual bool ReturnFalse341() = 0; // 341
	virtual bool FormatLocalizedText(const char* pToken, const char* const* pSubstitutions,
		std::uint32_t substitutionCount, char* pOutput, std::uint32_t outputSize,
		std::uint32_t* pOutputLength) = 0; // 342
	virtual void RemoveAllDecals() = 0; // 343
	virtual void BeginSuppressDecalUpdates() = 0; // 344
	virtual void EndSuppressDecalUpdates() = 0; // 345
	virtual std::uintptr_t FinishModelRendering() = 0; // 346
	virtual const char* GetGameName() = 0; // 347
	virtual const char* GetGameVersion() = 0; // 348
	virtual void SubmitQueuedLightData() = 0; // 349
	virtual void SetDemoPlaybackInputEnabled(bool enabled) = 0; // 350
	virtual void NullSub351() = 0; // 351
	virtual int PrecacheModel(const char* pModelName) = 0; // 352
	virtual int GetPreviousFrameSnapshotDeltaTick() = 0; // 353
	virtual int GetPreviousFrameSnapshotServerTick() = 0; // 354
	virtual int GetPendingOrPreviousSnapshotTick() = 0; // 355
	virtual bool IsUsingCachedPersistenceDefFile() = 0; // 356
	virtual bool GetPersistentArrayCount(const char* pArrayName,
		std::uintptr_t* pCount) = 0; // 357
	virtual bool FindPersistentArrayElement(const char* pArrayName,
		const char* pElementName, std::uintptr_t* pElement) = 0; // 358
	virtual bool GetPersistentArrayElementName(const char* pArrayName, int elementIndex,
		const char** ppElementName) = 0; // 359
	virtual bool ResolvePersistentVarTypeValue(const char* pVariableName,
		std::uintptr_t* pValue) = 0; // 360
	virtual bool IsPersistentArrayElementValid(const char* pArrayName,
		const char* pElementName, bool* pIsValid) = 0; // 361
	virtual void* GetLocalClient(int clientSlot) = 0; // 362
	virtual bool ParsePersistenceData(int clientSlot) = 0; // 363
	virtual bool HasPersistenceBaseline(int clientSlot) = 0; // 364
	virtual bool ResolvePersistentVar(int clientSlot, const char* pVariableName,
		std::uintptr_t* pHandle) = 0; // 365
	virtual std::uint32_t GetPersistentVarType(int clientSlot,
		std::uintptr_t variableHandle) = 0; // 366
	virtual bool GetPersistentBool(int clientSlot, std::uintptr_t variableHandle) = 0; // 367
	virtual std::int32_t GetPersistentInt(int clientSlot,
		std::uintptr_t variableHandle) = 0; // 368
	virtual float GetPersistentFloat(int clientSlot, std::uintptr_t variableHandle) = 0; // 369
	virtual const char* GetPersistentString(int clientSlot,
		std::uintptr_t variableHandle) = 0; // 370
	virtual const char* GetPersistentEnumValue(int clientSlot,
		std::uintptr_t variableHandle) = 0; // 371
	virtual bool GetPersistentVarAsInt(int clientSlot, std::uintptr_t variableHandle,
		int* pValue) = 0; // 372
	virtual void* CreatePersistentVarIterator() = 0; // 373
	virtual void DestroyPersistentVarIterator(void* pIterator) = 0; // 374
	virtual bool IsPersistentVarIteratorFinished(const void* pIterator) = 0; // 375
	virtual bool IsPersistentVarIteratorActive(const void* pIterator) = 0; // 376
	virtual int GetPersistentVarDefinitionCount(const void* pIterator) = 0; // 377
	virtual int GetPersistentVarIteratorCount(const void* pIterator) = 0; // 378
	virtual void AdvancePersistentVarIterator(void* pIterator) = 0; // 379
	virtual int GetPersistentVarIteratorIndex(const void* pIterator) = 0; // 380
	virtual const char* GetPersistentVarIteratorName(void* pIterator) = 0; // 381
	virtual const char* GetPersistentVarIteratorValue(void* pIterator) = 0; // 382
	virtual bool IsMoveOneCmdPerClientFrameEnabled() = 0; // 383
	virtual void HostError(const char* pFormat, ...) = 0; // 384
	virtual bool ReturnFalse385() = 0; // 385
	virtual bool ReturnTrue386() = 0; // 386
	virtual bool ReturnTrue387() = 0; // 387
	virtual void* GetEntitlementNames() = 0; // 388
	virtual std::uintptr_t OpenOriginStorePage() = 0; // 389
	virtual std::uintptr_t OpenExternalWebBrowser(const char* pUrl, bool muteSound) = 0; // 390
	virtual bool ClaimClientSidePickup(int pickupType, std::uint32_t first,
		std::uint32_t second, std::uint32_t third) = 0; // 391
	virtual void ConfigureNetworkSocketPorts(bool useServerPorts) = 0; // 392
	virtual int GetTimeSinceLastUserCmd() = 0; // 393
	virtual const char* GetInviteRegionFilter() = 0; // 394
	virtual int GetChatServerUserCount() = 0; // 395
	virtual bool GetChatServerUserData(int userIndex, void* pUserData) = 0; // 396
	virtual bool GetChatServerUserName(int userIndex, char* pOutput, int outputSize) = 0; // 397
	virtual const char* GetChatServerUserPlatform(int userIndex) = 0; // 398
	virtual bool IsChatServerUserActive(int userIndex) = 0; // 399
	virtual bool GetChatServerUserServerFlag400(int userIndex) = 0; // 400
	virtual bool IsChatServerUserMuted(int userIndex) = 0; // 401
	virtual bool IsChatServerUserLocal(int userIndex) = 0; // 402
	virtual bool GetChatServerUserDataByStorageIndex(int storageIndex, void* pUserData) = 0; // 403
	virtual bool IsChatServerVoiceMuted() = 0; // 404
	virtual void ToggleChatServerVoiceMute() = 0; // 405
	virtual bool ReturnFalse406(void* pFirst, void* pSecond, void* pThird, void* pFourth, bool flag) = 0; // 406
	virtual bool MuteChatServerUser(std::uint64_t userId) = 0; // 407
	virtual bool UnmuteChatServerUser(std::uint64_t userId) = 0; // 408
	virtual bool ToggleChatServerUserMute(std::uint64_t userId) = 0; // 409
	virtual bool IsChatServerUserBlockedOrMuted(std::uint64_t userId) = 0; // 410
	virtual int GetCommunityHappyHourTimeLeft() = 0; // 411
	virtual int GetCommunityCount(const char* pListName) = 0; // 412
	virtual bool GetCommunityData(const char* pListName, int index, void* pCommunityData) = 0; // 413
	virtual void NullSub414() = 0; // 414
	virtual int GetPartyJoinFailureReason(const char** ppReason) = 0; // 415
	virtual void ClearPartyJoinFailureReason(int reason) = 0; // 416
	virtual const char* GetCommunityClanTag() = 0; // 417
	virtual void SetCommunityClanTag(const char* pClanTag) = 0; // 418
	virtual int GetCurrentCommunityId() = 0; // 419
	virtual int GetCurrentCommunityPopulation() = 0; // 420
	virtual const char* GetCurrentCommunityName() = 0; // 421
	virtual bool AreCurrentCommunityInvitesAllowed() = 0; // 422
	virtual bool IsCurrentCommunityChatAllowed() = 0; // 423
	virtual bool IsCurrentCommunityHappyHourActive() = 0; // 424
	virtual float GetCurrentCommunityNextHappyHourTime() = 0; // 425
	virtual float GetCurrentCommunityNextHappyHourEndTime() = 0; // 426
	virtual const char* GetCurrentCommunityRole() = 0; // 427
	virtual bool IsCurrentCommunityAdmin() = 0; // 428
	virtual bool IsCurrentCommunityOpen() = 0; // 429
	virtual std::int64_t JoinPartyFromRoom(void* pRoom) = 0; // 430
	virtual void ClearCommunityHttpCookies() = 0; // 431
	virtual void* GetLocalClientPartyState() = 0; // 432
	virtual bool GetOnlineFriendNamesAndIds(void* pNames, void* pIds) = 0; // 433
	virtual bool GetOnlineFriendNamesIdsAndPresence(void* pNames, void* pIds, void* pPresence) = 0; // 434
	virtual void JoinCommunity(std::uint32_t communityId) = 0; // 435
	virtual void LeaveCommunity(std::uint32_t communityId) = 0; // 436
	virtual bool IsCommunityMember(int communityId) = 0; // 437
	virtual bool SetCurrentCommunity(std::uint32_t communityId) = 0; // 438
	virtual void InviteUserToCommunity(std::uint64_t userId, int communityId) = 0; // 439
	virtual void ReportCommunity(std::uint32_t communityId, std::uint32_t severity) = 0; // 440
	virtual void ReportCommunityMessage(int messageId, int severity) = 0; // 441
	virtual void* GetLobby() = 0; // 442
	virtual void* GetParty() = 0; // 443
	virtual bool GetCommunityDataById(std::uint32_t communityId, void* pCommunityData) = 0; // 444
	virtual void SaveCommunitySettings(void* pCommunityData) = 0; // 445
	virtual void BrowseCommunities(void* pFilters) = 0; // 446
	virtual bool GetCommunityBrowsePageInfo(int* pStart, int* pEnd, int* pTotal, bool* pComplete) = 0; // 447
	virtual void ResetLocalCommunityMemberCache(void* pContext) = 0; // 448
	virtual bool GetCommunityMemberData(const char* pHardware, std::uint64_t userId, void* pMemberData) = 0; // 449
	virtual bool HasPendingInboxUpdates() = 0; // 450
	virtual const char* GetHardwareName(std::uint8_t hardware) = 0; // 451
	virtual bool GetInboxMessage(int type, int messageId, void* pMessageData) = 0; // 452
	virtual int GetMessageInboxCount() = 0; // 453
	virtual bool ConsumeMessageInboxUpdated() = 0; // 454
	virtual bool HasUnreadMessages() = 0; // 455
	virtual int GetMessageIdAtOrAfterIndex(int startIndex) = 0; // 456
	virtual int GetNextMessageId(int messageId) = 0; // 457
	virtual int GetPreviousMessageId(int messageId) = 0; // 458
	virtual void MarkMessageRead(int messageId) = 0; // 459
	virtual void DeleteMessage(int messageId) = 0; // 460
	virtual bool ExecuteMessage(int messageId) = 0; // 461
	virtual int GetNoteInboxCount() = 0; // 462
	virtual bool IsNoteInboxUpdated() = 0; // 463
	virtual bool HasUnreadNotes() = 0; // 464
	virtual int GetNoteIdAtOrAfterIndex(int startIndex) = 0; // 465
	virtual int GetNextNoteId(int noteId) = 0; // 466
	virtual int GetPreviousNoteId(int noteId) = 0; // 467
	virtual void AcknowledgeNote(int noteId) = 0; // 468
	virtual int GetEventInboxCount() = 0; // 469
	virtual bool IsEventInboxUpdated() = 0; // 470
	virtual bool HasUnreadEvents() = 0; // 471
	virtual int GetEventIdAtOrAfterIndex(int startIndex) = 0; // 472
	virtual int GetNextEventId(int eventId) = 0; // 473
	virtual int GetPreviousEventId(int eventId) = 0; // 474
	virtual void AcknowledgeEvent(int eventId) = 0; // 475
	virtual void SendCommunityAdminMessage(int expires, const char* pText) = 0; // 476
	virtual int GetAccountRegistrationState() = 0; // 477
	virtual void RequestAccountRegistrationDefaults() = 0; // 478
	virtual void SubmitAccountRegistration(const char* pEmail, bool optIn) = 0; // 479
	virtual const char* GetAccountRegistrationError() = 0; // 480
	virtual const char* GetAccountRegistrationEmail() = 0; // 481
	virtual bool GetAccountRegistrationOptIn() = 0; // 482
	virtual const char* GetAccountRegistrationCountry() = 0; // 483
	virtual int GetAccountRegistrationAge() = 0; // 484
	virtual bool SendPINTelemetryData(void* pTelemetryData) = 0; // 485
	virtual bool SendGraphicsSettingsTelemetry(void* pSettingsProvider) = 0; // 486
	virtual bool RequestMainMenuPromos() = 0; // 487
	virtual void* GetMainMenuPromos() = 0; // 488
	virtual ~IVEngineClient() = default; // 489
};

static_assert(sizeof(MatchmakingDatacenter_t) == 0xA0);
static_assert(offsetof(MatchmakingDatacenter_t, m_ServiceName) == 0x40);
static_assert(offsetof(MatchmakingDatacenter_t, m_NetworkAddress) == 0x80);
static_assert(offsetof(MatchmakingDatacenter_t, m_PingMilliseconds) == 0x98);
static_assert(sizeof(IVEngineClient) == sizeof(void*));
