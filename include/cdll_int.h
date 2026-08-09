#pragma once

#include "client_class.h"
#include "engine/createinterface.h"

#include <cstdint>

class CMoveData;
class CUserCmd;
class Vector3;
class bf_read;
class bf_write;
struct ScreenFade_t;

enum ClientFrameStage_t : int
{
	FRAME_UNDEFINED = -1,
	FRAME_START,
	FRAME_NET_UPDATE_START,
	FRAME_NET_UPDATE_POSTDATAUPDATE_START,
	FRAME_NET_UPDATE_POSTDATAUPDATE_END,
	FRAME_NET_UPDATE_END,
	FRAME_RENDER_START,
	FRAME_RENDER_END,
};

inline constexpr char CLIENT_DLL_INTERFACE_VERSION[] = "VClient018";
inline constexpr char CLIENT_DLL_SHARED_APPSYSTEMS[] = "VClientDllSharedAppSystems001";

class IClientDLLSharedAppSystems
{
public:
	virtual int Count() = 0;
	virtual const char* GetDllName(int index) = 0;
	virtual const char* GetInterfaceName(int index) = 0;
};

static_assert(sizeof(IClientDLLSharedAppSystems) == sizeof(void*));

class IBaseClientDLL
{
public:
	virtual int Init(CreateInterfaceFn appSystemFactory, void* pGlobals) = 0; // 0
	virtual void Disconnect() = 0; // 1
	virtual bool ConnectEngineInterfaces(CreateInterfaceFn engineFactory) = 0; // 2
	virtual bool InitSubsystems() = 0; // 3
	virtual int PostInit(void* pUnknown) = 0; // 4
	virtual void ShutdownSubsystems() = 0; // 5
	virtual void Shutdown() = 0; // 6
	virtual std::intptr_t InitializeWorldVisibilityData(void* pData) = 0; // 7
	virtual bool LevelInitPreEntity(const char* pMapName) = 0; // 8
	virtual void LevelInitPostEntity() = 0; // 9
	virtual void LevelShutdown() = 0; // 10
	virtual ClientClass* GetAllClasses() = 0; // 11
	virtual int HudVidInit() = 0; // 12
	virtual void HudProcessInput(bool active) = 0; // 13
	virtual void HudUpdate(bool active) = 0; // 14
	virtual void HudReset() = 0; // 15
	virtual void HudText(const char* pMessage) = 0; // 16
	virtual void ActivateMouse() = 0; // 17
	virtual void DeactivateMouse() = 0; // 18
	virtual void AccumulateMouse() = 0; // 19
	virtual void ClearInputStates() = 0; // 20
	virtual bool IsKeyDown(const char* pName, bool* pIsDown) = 0; // 21
	virtual void OnMouseWheeled(int delta) = 0; // 22
	virtual int KeyEvent(int eventCode, int keyCode, const char* pCurrentBinding) = 0; // 23
	virtual const char* GetInputButtonBinding(std::uint32_t buttonIndex,
		std::uint32_t splitScreenSlot) = 0; // 24
	virtual void CreateMove(int sequenceNumber, float inputSampleFrameTime, bool active) = 0; // 25
	virtual CUserCmd* GetUserCmd(int sequenceNumber) = 0; // 26
	virtual void ExtraMouseSample(float frameTime) = 0; // 27
	virtual bool WriteUsercmdDeltaToBuffer(int splitScreenSlot, bf_write* pBuffer, int from,
		int to) = 0; // 28
	virtual void EncodeUserCmdToBuffer(int splitScreenSlot, bf_write& buffer,
		int sequenceNumber) = 0; // 29
	virtual void DecodeUserCmdFromBuffer(int splitScreenSlot, bf_read& buffer,
		int sequenceNumber) = 0; // 30
	virtual void ViewRender(void* pRect) = 0; // 31
	virtual void EndViewRenderJobs() = 0; // 32
	virtual int BeginViewRenderJobGroup() = 0; // 33
	virtual int FindLightProbeVolumeContainingPoint(const float* pPoint) = 0; // 34
	virtual int TransformPointToLightProbeVolume(const float* pPoint, float* pTransformedPoint) = 0; // 35
	virtual void* GetLightProbeVolumeTransformByEntityIndex(int entityIndex) = 0; // 36
	virtual void RenderView(void* pViewSetup, int clearFlags, int whatToDraw) = 0; // 37
	virtual void ViewFade(const ScreenFade_t* pScreenFade) = 0; // 38
	virtual void SetCrosshairAngle(const float* pAngle) = 0; // 39
	virtual void GrantClientSidePickup(int pickupType, int count, const Vector3* pOrigin,
		std::uint32_t flags, int value) = 0; // 40
	virtual void* AllocateStaticPropRecords(std::uint32_t firstIndex,
		std::uint32_t recordCount) = 0; // 41
	virtual void* AllocateStaticPropRecordsWithBaseIndex(std::uint32_t firstIndex,
		std::uint32_t baseIndex, std::uint32_t recordCount) = 0; // 42
	virtual bool InitSprite(void* pSprite, const char* pFilename) = 0; // 43
	virtual void ShutdownSprite(void* pSprite) = 0; // 44
	virtual int GetSpriteSize() = 0; // 45
	virtual void InstallStringTableCallback(const char* pTableName) = 0; // 46
	virtual void NullSub47() = 0; // 47
	virtual void BeginEntitySnapshotUpdate() = 0; // 48
	virtual void EndEntitySnapshotUpdate() = 0; // 49
	virtual void BeginEntityCreation() = 0; // 50
	virtual void EndEntityCreation() = 0; // 51
	virtual void BeginEntityDeletion() = 0; // 52
	virtual void EndEntityDeletion() = 0; // 53
	virtual void BeginEntityPacketProcessing() = 0; // 54
	virtual void EndEntityPacketProcessing() = 0; // 55
	virtual void FinishParticleSimulationJobs() = 0; // 56
	virtual void FrameStageNotify(ClientFrameStage_t stage) = 0; // 57
	virtual bool DispatchUserMessage(int messageType, bf_read* pMessageData) = 0; // 58
	virtual void SaveWriteFields(void* pSaveData, const char* pFieldName, void* pBaseData,
		void* pDataMap, void* pFields, int fieldCount) = 0; // 59
	virtual void SaveReadFields(void* pSaveData, const char* pFieldName, void* pBaseData,
		void* pDataMap, void* pFields, int fieldCount) = 0; // 60
	virtual void PreSave(void* pSaveData) = 0; // 61
	virtual void Save(void* pSaveData) = 0; // 62
	virtual void WriteSaveHeaders(void* pSaveData) = 0; // 63
	virtual void ReadRestoreHeaders(void* pSaveData) = 0; // 64
	virtual void Restore(void* pSaveData, bool createPlayers) = 0; // 65
	virtual void DispatchOnRestore() = 0; // 66
	virtual void* GetStandardRecvProxies() = 0; // 67
	virtual void WriteSaveGameScreenshot(const char* pFileName) = 0; // 68
	virtual void EmitSentenceCloseCaption(const char* pCaptionName) = 0; // 69
	virtual void EmitCloseCaption(const char* pCaptionName, float duration) = 0; // 70
	virtual bool CanRecordDemo(char* pErrorMessage, int errorMessageLength) = 0; // 71
	virtual void OnDemoRecordStart(const char* pDemoName) = 0; // 72
	virtual void OnDemoRecordStop() = 0; // 73
	virtual void OnDemoPlaybackStart(const char* pDemoName) = 0; // 74
	virtual void OnDemoPlaybackStop() = 0; // 75
	virtual bool ShouldDrawDropdownConsole() = 0; // 76
	virtual int GetScreenWidth() = 0; // 77
	virtual int GetScreenHeight() = 0; // 78
	virtual void WriteSaveGameScreenshotOfSize(const char* pFileName, int width, int height,
		bool createPowerOfTwoPadded, bool writeVtf) = 0; // 79
	virtual bool GetPlayerView(void* pPlayerView) = 0; // 80
	virtual void SetupGameProperties(void* pContexts, void* pProperties) = 0; // 81
	virtual std::uint32_t GetPresenceID(const char* pIdName) = 0; // 82
	virtual const char* GetPropertyIdString(std::uint32_t id) = 0; // 83
	virtual void GetPropertyDisplayString(std::uint32_t id, std::uint32_t value,
		char* pOutput, int outputSize) = 0; // 84
	virtual void StartStatsReporting(void* pHandle, bool arbitrated) = 0; // 85
	virtual void InvalidateMdlCache() = 0; // 86
	virtual void IN_SetSampleTime(float frameTime) = 0; // 87
	virtual void ReloadFilesInList(void* pFilesToReload) = 0; // 88
	virtual bool HandleUiToggle() = 0; // 89
	virtual bool ShouldAllowConsole() = 0; // 90
	virtual void* GetRenamedRecvTableInfos() = 0; // 91
	virtual void* GetClientUIMouthInfo() = 0; // 92
	virtual void FileReceived(const char* pFileName, std::uint32_t transferId) = 0; // 93
	virtual const char* TranslateEffectForVisionFilter(const char* pEffectType,
		const char* pEffectName) = 0; // 94
	virtual void ClientAdjustStartSoundParams(void* pStartSoundParams) = 0; // 95
	virtual bool DisconnectAttempt() = 0; // 96
	virtual bool IsConnectedUserInfoChangeAllowed(void* pConVar) = 0; // 97
	virtual bool BHaveChatSuspensionInCurrentMatch() = 0; // 98
	virtual void DisplayVoiceUnavailableMessage() = 0; // 99
	virtual void SetRuiBoolByIndex(void* pState, std::uint32_t index, bool value) = 0; // 100
	virtual void DispatchEntitlementsChanged() = 0; // 101
	virtual void DispatchStoreTransactionCompleted() = 0; // 102
	virtual void DispatchGamePurchased() = 0; // 103
	virtual bool ReturnFalse104() = 0; // 104
	virtual void StopAllBinkVideos() = 0; // 105
	virtual bool IsFullscreenGameUIPanelVisible() = 0; // 106
	virtual bool ReturnFalse107() = 0; // 107
	virtual void InitializeEntityScriptInstanceCache() = 0; // 108
	virtual void ShutdownEntityScriptInstanceCache() = 0; // 109
	virtual void NullSub110() = 0; // 110
	virtual void ResetClientSimulationSystems() = 0; // 111
	virtual void UpdateProjectedShadows(void* pLights, int* pShadowHandles,
		std::uint32_t lightCount) = 0; // 112
	virtual std::intptr_t ForwardModelShadowData(std::uint32_t modelIndex, void* pArg3,
		void* pArg4) = 0; // 113
	virtual void SetSplitScreenStateCachingEnabled(bool enabled) = 0; // 114
	virtual void GetCachedViewScales(float* pFirstScale, float* pSecondScale) = 0; // 115
	virtual void GetCachedViewVector(Vector3* pValue) = 0; // 116
	virtual int ForwardFindLightProbeVolumeContainingPoint(const float* pPoint) = 0; // 117
	virtual void FireEvents(float frameTime) = 0; // 118
	virtual bool GetGamesWonTotal(std::uint32_t index, int* pValue) = 0; // 119
	virtual int GetNetWorth(std::uint32_t index) = 0; // 120
	virtual int GetMaxPlayerLevel() = 0; // 121
	virtual bool CanInvitePlayers() = 0; // 122
	virtual void GetPlayerListEntries(int* pCount, void** ppEntries) = 0; // 123
	virtual void SetPendingUiNavigationAction(int action) = 0; // 124
	virtual void CollectLocalPlayerSettings(void* pData, std::uint32_t value) = 0; // 125
	virtual void NullSub126(void* pData, std::uint32_t value) = 0; // 126
	virtual void ResetClientWeaponTracking() = 0; // 127
	virtual float GetMasterVolumeRef() = 0; // 128
	virtual void StopAllMilesSounds() = 0; // 129
	virtual void QueueMilesPcmSamples(const void* pSamples, int sampleCount, int channel) = 0; // 130
	virtual void ResetGameUiMenus() = 0; // 131
	virtual std::intptr_t EmitSoundForLocalPlayerWithoutFalloff(const char* pSoundName) = 0; // 132
	virtual void BringGameUiPanelToFront133() = 0; // 133
	virtual void BringGameUiPanelToFront134() = 0; // 134
	virtual void DispatchAcceptInvite(void* pData) = 0; // 135
	virtual void DispatchSetChatroomMode(void* pData) = 0; // 136
	virtual void DispatchGetOnPartyServer() = 0; // 137
	virtual bool TryCloseDialog() = 0; // 138
	virtual void NullSub139() = 0; // 139
	virtual void NullSub140() = 0; // 140
	virtual bool IsLocalViewPlayerMoving() = 0; // 141
	virtual void ProcessClientEntityPostUpdateJobs() = 0; // 142
	virtual void ClearClientUiText(std::uint32_t contextId) = 0; // 143
	virtual bool WriteClientUiText(const char* pMessage, const char* pPrefix,
		int contextId) = 0; // 144
	virtual void DispatchCommunityUpdated() = 0; // 145
	virtual void DispatchFactionUpdated() = 0; // 146
	virtual void DispatchInboxUpdated() = 0; // 147
	virtual void DispatchOpenInviteUpdated() = 0; // 148
	virtual void DispatchPartyUpdated() = 0; // 149
	virtual void DispatchRemoteMatchInfoUpdated() = 0; // 150
	virtual void DispatchShowCommunityInfo(std::uint32_t value) = 0; // 151
	virtual void DispatchCommunitySaved(std::uint32_t value) = 0; // 152
	virtual void DispatchCommunitySaveFailed(std::uint32_t value) = 0; // 153
	virtual void DispatchShowCommunityJoinRequest(void* pData) = 0; // 154
	virtual void DispatchMainMenuPromosUpdated() = 0; // 155
	virtual bool TryToggleInGameMenu() = 0; // 156
	virtual void SetPlayerListGeneration(int generation) = 0; // 157
	virtual void NotifyClientConnected() = 0; // 158
	virtual bool LoadParticleSystemDefinitions(int arg1, int arg2, void* pData) = 0; // 159
	virtual void FinishParticleSystemDefinitionLoad() = 0; // 160
};

static_assert(sizeof(IBaseClientDLL) == sizeof(void*));
