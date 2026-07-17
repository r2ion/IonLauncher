#pragma once

#include "rtech/pakstate.h"

#include <regex>

struct ModPak_t
{
	std::string m_modName;

	std::string m_path;
	size_t m_pathHash = 0;

	// If the map being loaded matches this regex, this pak will be loaded.
	std::regex m_mapRegex;
	// If a pak with a hash matching this is loaded, this pak will be loaded.
	size_t m_dependentPakHash = 0;
	// If this is set, this pak will be loaded whenever any other pak is loaded.
	bool m_preload = false;

	// If this is set, the Pak will be unloaded on next map load
	bool m_markedForDelete = false;
	// The current rpak handle associated with this Pak
	PakHandle_t m_handle = PAK_INVALID_HANDLE;
};

class PakLoadManager
{
public:
	void UnloadAllModPaks();
	void TrackModPaks(Mod& mod);

	void CleanUpUnloadedPaks();
	void UnloadMarkedPaks();

	void LoadModPaksForMap(const char* mapName);
	void UnloadModPaks();

	// Whether the current context is a vanilla call to a function, or a modded one
	bool IsVanillaCall() const { return m_reentranceCounter == 0; }
	// Whether paks will be forced to reload on the next map load
	bool GetForceReloadOnMapLoad() const { return m_forceReloadOnMapLoad; }
	void SetForceReloadOnMapLoad(bool value) { m_forceReloadOnMapLoad = value; }

	void OnPakLoaded(std::string& originalPath, std::string& resultingPath, PakHandle_t resultingHandle);
	void OnPakUnloading(PakHandle_t handle);

	void FixupPakPath(std::string& path);

	void LoadPreloadPaks();
	void ReloadPostloadPaks();

	void* FindAssetByName(const char* name);
	std::vector<PakHandle_t> GetPakHandles() { std::vector<PakHandle_t> handles; for (auto& modPak : m_modPaks) { if (modPak.m_handle != PAK_INVALID_HANDLE) handles.push_back(modPak.m_handle); } return handles; }

private:
	void LoadDependentPaks(std::string& path, PakHandle_t handle);
	void UnloadDependentPaks(PakHandle_t handle);

	// All paks that vanilla has attempted to load. (they may have been aliased away)
	// Also known as a list of rpaks that the vanilla game would have loaded at this point in time.
	std::vector<std::pair<std::string, PakHandle_t>> m_vanillaPaks;

	// All mod Paks that are currently tracked
	std::vector<ModPak_t> m_modPaks;
	// Hashes of the currently loaded map mod paks
	std::vector<size_t> m_mapPaks;
	// Currently loaded Pak path hashes that depend on a handle to remain loaded (Postload)
	std::vector<std::pair<PakHandle_t, size_t>> m_dependentPaks;

	// Used to force rpaks to be unloaded and reloaded on the next map load.
	// Vanilla behaviour is to not do this when loading into mp_lobby, or loading into the same map you were last in.
	bool m_forceReloadOnMapLoad = false;
	// Used to track if the current hook call is a vanilla call or not.
	// When loading/unloading a mod Pak, increment this before doing so, and decrement afterwards.
	int m_reentranceCounter = 0;
};

extern PakLoadManager* g_pPakLoadManager;

struct PakLoadFuncs_s
{
	using Callback_t = void(*)();
	using AsyncReadCallback_t = void(__fastcall*)(void* context, uint8_t status, const char* errorText);

	void (__fastcall* InitRpakSystem)();
	__int64 (__fastcall* RegisterAssetBindingType)(PakAssetBinding_s*, uint32_t, uint32_t);
	uint64_t reserved10;
	PakHandle_t (__fastcall* AllocateEmptyPak)(const char*, PakAllocator_s*, int);
	PakHandle_t (__fastcall* AllocAndLoadPak)(const char*, PakAllocator_s*, int, Callback_t, Callback_t);
	void (__fastcall* BeginUnload)(PakHandle_t);
	void (__fastcall* UnloadAndWait)(PakHandle_t, Callback_t);
	uint64_t reserved38;
	void (__fastcall* PumpLoadJobs)(Callback_t);
	bool (__fastcall* WaitForLoadCompletion)(PakHandle_t, Callback_t, Callback_t);
	void (__fastcall* WaitForUnloadCompletion)(PakHandle_t, Callback_t);
	FARPROC (__fastcall* GetModuleProcAddress)(PakHandle_t, const char*);
	char* (__fastcall* GetAssetBinding)(PakGuid_t);
	char* (__fastcall* GetAssetBindingFromFlag)(uint8_t);
	uint64_t reserved70;
	PakGuid_t (__fastcall* GetLoadedAsset)(int, int);
	void (__fastcall* LinkAssetBinding)(int, int, __int64);
	void (__fastcall* UnlinkAssetBinding)(int, int, __int64);
	PakHandle_t (__fastcall* GetStreamingFileHandle)(__int64);
	uint64_t reserved98;
	uint64_t reservedA0;
	uint64_t reservedA8;
	uint64_t reservedB0;
	uint64_t reservedB8;
	PakHandle_t (__fastcall* OpenFile)(const char*, uint64_t*);
	void (__fastcall* ReleaseFileHandle)(PakHandle_t);
	void (__fastcall* AddRefFileHandle)(PakHandle_t);
	int (__fastcall* QueueAsyncRead)(PakHandle_t, uint64_t, uint64_t, void*, int);
	int (__fastcall* QueueAsyncReadEx)(PakHandle_t, uint64_t, uint64_t, void*, AsyncReadCallback_t, void*, int);
	uint8_t (__fastcall* PollAsyncRead)(uint8_t, uint64_t*, const char**);
	uint8_t (__fastcall* WaitAsyncRead)(uint8_t, uint64_t*, const char**);
	uint8_t (__fastcall* CancelAndWaitAsyncRead)(uint8_t);
	void (__fastcall* CancelAsyncRead)(uint8_t);
	HANDLE (__fastcall* GetWorkerThreadHandle)(HANDLE*);
};
static_assert(sizeof(PakLoadFuncs_s) == 0x110);
static_assert(offsetof(PakLoadFuncs_s, AllocateEmptyPak) == 0x18);
static_assert(offsetof(PakLoadFuncs_s, UnloadAndWait) == 0x30);
static_assert(offsetof(PakLoadFuncs_s, OpenFile) == 0xC0);
static_assert(offsetof(PakLoadFuncs_s, GetWorkerThreadHandle) == 0x108);

extern PakLoadFuncs_s* g_pakLoadApi;
