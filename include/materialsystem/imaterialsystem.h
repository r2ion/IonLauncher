#pragma once

#include "appframework/IAppSystem.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

class IMaterial;
class IMaterialProxyFactory;
class IMaterialSystemHardwareConfig;
class IMatRenderContext;
class ITexture;
class KeyValues;

using MaterialHandle_t = std::uint16_t;

inline constexpr char MATERIAL_SYSTEM_INTERFACE_VERSION[] = "VMaterialSystem083";
inline constexpr char MATERIAL_SYSTEM_CONFIG_INTERFACE_VERSION[] = "VMaterialSystemConfig004";

struct MaterialVideoMode_t
{
	int width;
	int height;
	int format;
	int refreshRate;
};
static_assert(sizeof(MaterialVideoMode_t) == 0x10);

struct MaterialAdapterInfo_t
{
	char driverName[512];
	std::uint32_t vendorId;
	std::uint32_t deviceId;
	std::uint32_t subsystemId;
	std::uint32_t revision;
	int dxSupportLevel;
	int minDxSupportLevel;
	int maxDxSupportLevel;
	std::uint32_t driverVersionHigh;
	std::uint32_t driverVersionLow;
};
static_assert(sizeof(MaterialAdapterInfo_t) == 0x224);
static_assert(offsetof(MaterialAdapterInfo_t, vendorId) == 0x200);

struct MaterialSystem_Config_t
{
	MaterialVideoMode_t videoMode;
	int pad;
	std::uint32_t flags;
	std::uint8_t unknown18[0x10];
	std::uint32_t windowedSizeLimitWidth;
	std::uint32_t windowedSizeLimitHeight;
	std::uint8_t unknown30[4];
};
static_assert(sizeof(MaterialSystem_Config_t) == 0x34);
static_assert(offsetof(MaterialSystem_Config_t, windowedSizeLimitWidth) == 0x28);

class IMaterialSystem : public IAppSystem
{
public:
	virtual CreateInterfaceFn Init(int renderBackend, IMaterialProxyFactory *proxyFactory, CreateInterfaceFn fileSystemFactory, CreateInterfaceFn cvarFactory) = 0; // 8
	virtual void SetRenderThreadCallback(void *callback) = 0; // 9
	virtual void InitializeRenderThreadCallbacks() = 0; // 10
	virtual void SetRuntimeFlag11(bool enabled) = 0; // 11
	virtual bool GetRuntimeFlag12() = 0; // 12
	virtual void SetRenderBackend(int renderBackend) = 0; // 13
	virtual void SetAdapter(int adapter, int flags) = 0; // 14
	virtual bool InitializeGraphicsDevice(void *windowHandle, const MaterialSystem_Config_t *config) = 0; // 15
	virtual void ModInit() = 0; // 16
	virtual void ModShutdown() = 0; // 17
	virtual void PopulateShaderAPICallbacks(void **callbacks) = 0; // 18
	virtual bool IsRenderBackendModeOne() = 0; // 19
	virtual bool IsRenderBackendModeAtLeastTwo() = 0; // 20
	virtual void PopulateRenderCommandCallbacks(void **callbacks) = 0; // 21
	virtual bool IsExceptionInProgress() = 0; // 22
	virtual void NullSub23() = 0; // 23
	virtual void SetThreadLocalRenderFlag(bool enabled) = 0; // 24
	virtual void ExecuteQueued() = 0; // 25
	virtual IMaterialSystemHardwareConfig * GetHardwareConfig(const char *version, int *returnCode) = 0; // 26
	virtual bool UpdateConfig(bool forceUpdate) = 0; // 27
	virtual bool OverrideConfig(const MaterialSystem_Config_t *config, bool forceUpdate) = 0; // 28
	virtual const MaterialSystem_Config_t * GetCurrentConfigForVideoCard() = 0; // 29
	virtual bool GetRecommendedConfigurationInfo(int dxLevel, KeyValues *outConfig) = 0; // 30
	virtual unsigned int GetCurrentAdapterVendorID() = 0; // 31
	virtual int GetProcessedDisplayAdapterCount() = 0; // 32
	virtual int GetDisplayAdapterCount() = 0; // 33
	virtual int GetCurrentAdapter() = 0; // 34
	virtual void GetDisplayAdapterInfo(int adapter, MaterialAdapterInfo_t *info) = 0; // 35
	virtual std::uint64_t GetCurrentAdapterMemorySize() = 0; // 36
	virtual int GetModeCount(int adapter, int format) = 0; // 37
	virtual void GetModeInfo(int adapter, int format, int mode, MaterialVideoMode_t *info) = 0; // 38
	virtual void AddModeChangeCallback(void *callback) = 0; // 39
	virtual bool SupportsMSAAMode(int samples) = 0; // 40
	virtual bool SupportsCSAAMode(int samples, int quality) = 0; // 41
	virtual void GetDisplayMode(MaterialVideoMode_t *mode) = 0; // 42
	virtual const MaterialVideoMode_t * GetCurrentVideoMode() = 0; // 43
	virtual bool SetMode(const MaterialSystem_Config_t *config) = 0; // 44
	virtual void QueueModeChange(const MaterialSystem_Config_t *config) = 0; // 45
	virtual bool IsSettingMode() = 0; // 46
	virtual void GetBackBufferDimensions(int *width, int *height) = 0; // 47
	virtual int GetBackBufferFormat(bool linear) = 0; // 48
	virtual bool GetNextSupportedAntiAliasingMode(int *iterator, std::uint8_t *mode, char *modeName, unsigned int modeNameLength) = 0; // 49
	virtual bool SupportsHDRMode(int hdrMode) = 0; // 50
	virtual int GetHDRBackBufferFormat() = 0; // 51
	virtual void GetDisplaySize(int *width, int *height) = 0; // 52
	virtual int ReturnZero53() = 0; // 53
	virtual const void * GetAspectRatioInfo() = 0; // 54
	virtual bool HardwareConfigSupportsHDRMode(int hdrMode) = 0; // 55
	virtual bool SetCurrentConfigOption56(bool enabled) = 0; // 56
	virtual void GetWindowedSizeLimits(int *width, int *height, const MaterialSystem_Config_t *config) = 0; // 57
	virtual void SetViewHandles(void *primaryView, void *secondaryView) = 0; // 58
	virtual void GetViewHandles(void **primaryView, void **secondaryView) = 0; // 59
	virtual void BeginFrame(float frameTime) = 0; // 60
	virtual void EndFrame() = 0; // 61
	virtual void DispatchFormattedMessage(const char *format, void *arguments, void *output) = 0; // 62
	virtual unsigned int GetCurrentFrameCount() = 0; // 63
	virtual void SwapBuffers() = 0; // 64
	virtual void * CreateRenderResource65(const char *name, unsigned int width, unsigned int height, unsigned int flags, std::uint64_t resource, bool option) = 0; // 65
	virtual void AddReleaseFunc(void *callback) = 0; // 66
	virtual void RemoveReleaseFunc(void *callback) = 0; // 67
	virtual void AddRestoreFunc(void *callback) = 0; // 68
	virtual void RemoveRestoreFunc(void *callback) = 0; // 69
	virtual void NullSub70() = 0; // 70
	virtual void RunEndFrameCleanupFunctions() = 0; // 71
	virtual bool AddEndFrameCleanupFunc(void *callback, void *userData) = 0; // 72
	virtual bool RemoveEndFrameCleanupFunc(void *callback, void *userData) = 0; // 73
	virtual void NullSub74() = 0; // 74
	virtual void ReloadShader(const char *shaderName, int flags) = 0; // 75
	virtual void CreateDebugMaterials() = 0; // 76
	virtual void CleanUpDebugMaterials() = 0; // 77
	virtual bool CanUseEditorMaterials() = 0; // 78
	virtual void GetShaderFallback(const char *shaderName, char *fallback, int fallbackLength) = 0; // 79
	virtual IMaterialProxyFactory * GetMaterialProxyFactory() = 0; // 80
	virtual void SetMaterialProxyFactory(IMaterialProxyFactory *factory) = 0; // 81
	virtual void DebugPrintUsedMaterials(const char *searchSubstring, bool verbose) = 0; // 82
	virtual void DebugPrintUsedTextures() = 0; // 83
	virtual void ToggleSuppressMaterial(const char *materialName) = 0; // 84
	virtual void ToggleDebugMaterial(const char *materialName) = 0; // 85
	virtual void UncacheAllMaterials() = 0; // 86
	virtual void UncacheUnusedMaterials() = 0; // 87
	virtual void RecomputeAllMaterialState() = 0; // 88
	virtual void CacheUsedMaterials(void *callback) = 0; // 89
	virtual void ReloadTextures(bool reloadMode, void (*callback)()) = 0; // 90
	virtual void ReloadMaterials(const char *nameSubstring) = 0; // 91
	virtual IMaterial * CreateMaterial(const char *materialName, int materialType,
		KeyValues *materialKeyValues) = 0; // 92
	virtual IMaterial * FindOrCreateMaterialFromVmtPatch(const char *materialName,
		void *materialSource, KeyValues *patchValues) = 0; // 93
	virtual IMaterial* FindMaterial(const char* materialName, std::uint32_t textureGroup,
		int context, bool complain) = 0; // 94
	virtual bool IsMaterialLoaded(const char *materialName) = 0; // 95
	virtual MaterialHandle_t FirstMaterial() = 0; // 96
	virtual MaterialHandle_t NextMaterial(MaterialHandle_t handle) = 0; // 97
	virtual MaterialHandle_t InvalidMaterial() = 0; // 98
	virtual IMaterial * GetMaterial(MaterialHandle_t handle) = 0; // 99
	virtual int GetNumMaterials() = 0; // 100
	virtual ITexture* FindTexture(const char* textureName, std::uint32_t textureGroup,
		std::uint32_t flags, int additionalCreationFlags, std::uint32_t unknown) = 0; // 101
	virtual ITexture* FindLoadedTexture(const char* textureName, bool complain) = 0; // 102
	virtual bool HasTextureBackingResource(ITexture *texture) = 0; // 103
	virtual ITexture * CreateProceduralTexture(const char *textureName, std::uint32_t textureGroup,
		int width, int height, int format, std::uint32_t flags) = 0; // 104
	virtual ITexture * CreateProceduralTextureEx(const char *textureName, std::uint32_t textureGroup,
		int width, int height, int format, std::uint32_t flags, int additionalCreationFlags) = 0; // 105
	virtual ITexture * CreateNamedRenderTargetTexture(const char *textureName, int width, int height,
		int sizeMode, int format, int depthMode, std::uint32_t textureFlags,
		std::uint32_t renderTargetFlags) = 0; // 106
	virtual ITexture * CreateNamedRenderTargetTextureEx(const char *textureName, int width, int height,
		int sizeMode, int format, int depthMode, std::uint32_t textureFlags,
		std::uint32_t renderTargetFlags, int additionalCreationFlags) = 0; // 107
	virtual void CreditModelTextures(IMaterial *material, void *modelData, void *renderData,
		std::uint32_t flags, const float *viewOrigin, float tanHalfFov,
		float viewWidthPixels, int frameId) = 0; // 108
	virtual void UpdateStreamCamera(const float *cameraPosition, const float *cameraAngles,
		float halfFovX, float viewWidth) = 0; // 109
	virtual void BeginRenderTargetAllocation() = 0; // 110
	virtual void EndRenderTargetAllocation() = 0; // 111
	virtual void BeginLightmapAllocation() = 0; // 112
	virtual void EndLightmapAllocation(int width, int height, int pageCount,
		int sortCount, bool updateState) = 0; // 113
	virtual void CleanupLightmaps() = 0; // 114
	virtual int AllocateLightmap(int width, int height, int offsetIntoLightmapPage[2],
		IMaterial *material) = 0; // 115
	virtual void GetLightmapPageSize(int lightmapPage, int *width, int *height) = 0; // 116
	virtual void UpdateLightmap(int lightmapPage, int lightmapSize[2], int offsetIntoLightmapPage[2],
		float *floatImage, float *floatImageBump1, float *floatImageBump2,
		float *floatImageBump3) = 0; // 117
	virtual void SpinPresent(std::uint32_t frameCount) = 0; // 118
	virtual IMatRenderContext * GetRenderContext() = 0; // 119
	virtual bool HasRenderContext() = 0; // 120
	virtual void DestroyRenderContext(IMatRenderContext *renderContext) = 0; // 121
	virtual IMaterial * FindProceduralMaterial(const char *materialName, std::uint32_t textureGroup,
		int context, KeyValues *materialKeyValues) = 0; // 122
	virtual IMaterial * GetDebugMaterial(int debugMaterial) = 0; // 123
	virtual void AddTextureAlias(const char *alias, const char *realName) = 0; // 124
	virtual void RemoveTextureAlias(const char *alias) = 0; // 125
	virtual bool IsInFrame() = 0; // 126
	virtual void CompactMemory() = 0; // 127
	virtual void GetGPUMemoryStats(void *stats) = 0; // 128
	virtual int ComputeTextureMemorySize(int width, int height, int depth, int format,
		int mipCount, bool cubeMap, bool volumeTexture) = 0; // 129
	virtual ITexture * FindTextureWithDefaultFlags(const char *textureName) = 0; // 130
	virtual void FinishRenderTargetAllocation() = 0; // 131
	virtual void ReEnableRenderTargetAllocation() = 0; // 132
	virtual void RefreshMaterialDebugState133() = 0; // 133
	virtual bool AllowThreading(bool allow) = 0; // 134
	virtual void DoStartupShaderPreloading() = 0; // 135
	virtual bool ReturnTrue136() = 0; // 136
	virtual bool IsTextureDownloadEnabled() = 0; // 137
	virtual int GetNumLightmapPages() = 0; // 138
	virtual bool HasPaintmapDataManager() = 0; // 139
	virtual std::uint8_t FindPaintmapIndex(const char *name) = 0; // 140
	virtual void AdjustPaintmapIndexOffset(std::uint64_t handle, int *index) = 0; // 141
	virtual void UpdateGameTime(float gameTime, float frameTime) = 0; // 142
	virtual void SetRenderTargetFrameBufferSizeOverrides(int width, int height,
		std::uint32_t flags) = 0; // 143
	virtual void GetRenderTargetFrameBufferOverrideFlags(bool *first, bool *second, bool *third,
		bool *fourth, bool *fifth, bool *sixth) = 0; // 144
	virtual void NullSub145() = 0; // 145
	virtual void GetRenderTargetFrameBufferDimensions(int *width, int *height) = 0; // 146
	virtual double GetAverageFrameTime() = 0; // 147
	virtual double GetFrameTimestampDelta() = 0; // 148
	virtual void QueueFourFloatState149(const float *values) = 0; // 149
	virtual void QueueTwelveFloatState150(const float *values) = 0; // 150
	virtual void QueueFrameTimingState151(const float *values, float first, float second) = 0; // 151
	virtual void QueueFrameTimingState152(float first, float second, float third) = 0; // 152
	virtual void QueueFrameTimingState153(float first, float second, float third) = 0; // 153
	virtual void QueueFrameTimingState154(float value) = 0; // 154
	virtual bool InitializeMaterialCallbacks(void (**callbacks)()) = 0; // 155
	virtual void InitializeRenderContextCallbacks(void (**callbacks)()) = 0; // 156
	virtual void RecordFrameTimestamp(float timestamp) = 0; // 157
	virtual void SuspendTextureStreaming() = 0; // 158
	virtual void ResumeTextureStreaming() = 0; // 159
	virtual void SetTextureStreamingMode(int mode) = 0; // 160
	virtual IMaterial * GetCurrentMaterial() = 0; // 161
	virtual int GetCurrentLightmapPage() = 0; // 162
	virtual int GetLightmapPageWidth(int lightmapPage) = 0; // 163
	virtual int GetLightmapPageHeight(int lightmapPage) = 0; // 164
	virtual void * GetMaterialSystemState165() = 0; // 165
	virtual MaterialHandle_t RegisterMaterial(IMaterial *material) = 0; // 166
	virtual void UncacheMaterialResource167(IMaterial *material) = 0; // 167
	virtual void UncacheMaterialResource168(IMaterial *material) = 0; // 168
	virtual const char * GetCurrentMaterialPathID() = 0; // 169
	virtual void ResetMaterialContextIfCurrent(void *context) = 0; // 170
};
static_assert(std::is_base_of_v<IAppSystem, IMaterialSystem>);
static_assert(sizeof(IMaterialSystem) == sizeof(void*));
