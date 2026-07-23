#pragma once

#include "rtech/pakstate.h"

#include <mutex>
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
	// On success, the native Rpak FIFO lock remains held until ReleasePakLock.
	bool TryAcquireIdlePakLock() const;
	void ReleasePakLock() const;
	// A failed pak with populated slabs may have corrupted allocator state. Keep
	// it resident and block targeted model/material teardown until restart.
	bool HasUnsafeLoadedPaks() const;

	void OnPakLoaded(std::string& originalPath, std::string& resultingPath, PakHandle_t resultingHandle);
	void OnPakLoadFailed(const PakLoadedInfo_s& info);
	bool PreparePakUnload(PakHandle_t handle);
	void CommitPakUnload(PakHandle_t handle);
	void OnPakUnloadQueued(PakHandle_t handle);
	void OnPakFreed(PakHandle_t handle);

	void FixupPakPath(std::string& path);

	void LoadPreloadPaks();
	void ReloadPostloadPaks();

	void* FindAssetByName(const char* name);
	std::vector<PakHandle_t> GetPakHandles() { std::vector<PakHandle_t> handles; for (auto& modPak : m_modPaks) { if (modPak.m_handle != PAK_INVALID_HANDLE) handles.push_back(modPak.m_handle); } return handles; }

private:
	static bool HasAllocatedSlab(const PakLoadedInfo_s& info);
	bool IsUnsafeLoadedPak(PakHandle_t handle) const;
	void TrackFailedPak(const PakLoadedInfo_s& info);
	bool IsSafeFailedPak(PakHandle_t handle) const;
	void ForgetSafeFailedPak(PakHandle_t handle);
	bool HasActivePakTransactionsLocked(const PakGlobalState_s& pakGlobals) const;
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

	mutable std::mutex m_failureMutex;
	std::vector<PakHandle_t> m_unsafeLoadedPaks;
	std::vector<PakHandle_t> m_safeFailedPaks;
};

extern PakLoadManager* g_pPakLoadManager;

struct PakLoadFuncs_s
{
	using Callback_t = void(*)();
	using AsyncReadCallback_t = void(*)(void* context, uint8_t status, const char* errorText);

	void (*InitRpakSystem)();
	JobTypeID_t (*RegisterAssetBindingType)(PakAssetBinding_s*, JobPriority_e, uint32_t);
	uint64_t reserved10;
	PakHandle_t (*AllocateEmptyPak)(const char*, PakAllocator_s*, int);
	PakHandle_t (*AllocAndLoadPak)(const char*, PakAllocator_s*, int, Callback_t, Callback_t);
	void (*BeginUnload)(PakHandle_t);
	void (*UnloadAndWait)(PakHandle_t, Callback_t);
	uint64_t reserved38;
	void (*PumpLoadJobs)(Callback_t);
	bool (*WaitForLoadCompletion)(PakHandle_t, Callback_t, Callback_t);
	void (*WaitForUnloadCompletion)(PakHandle_t, Callback_t);
	FARPROC (*GetModuleProcAddress)(PakHandle_t, const char*);
	char* (*GetAssetBinding)(PakGuid_t);
	char* (*GetAssetBindingFromFlag)(uint8_t);
	uint64_t reserved70;
	PakGuid_t (*GetLoadedAsset)(int, int);
	void (*LinkAssetBinding)(
		uint32_t assetAddress,
		uint32_t extension,
		PakAssetBindingLink_s* link);
	void (*UnlinkAssetBinding)(
		uint32_t assetAddress,
		uint32_t extension,
		PakAssetBindingLink_s* link);
	PakHandle_t (*GetStreamingFileHandle)(__int64);
	uint64_t reserved98;
	uint64_t reservedA0;
	uint64_t reservedA8;
	uint64_t reservedB0;
	uint64_t reservedB8;
	PakHandle_t (*OpenFile)(const char*, uint64_t*);
	void (*ReleaseFileHandle)(PakHandle_t);
	void (*AddRefFileHandle)(PakHandle_t);
	int (*QueueAsyncRead)(PakHandle_t, uint64_t, uint64_t, void*, int);
	int (*QueueAsyncReadEx)(PakHandle_t, uint64_t, uint64_t, void*, AsyncReadCallback_t, void*, int);
	uint8_t (*PollAsyncRead)(uint8_t, uint64_t*, const char**);
	uint8_t (*WaitAsyncRead)(uint8_t, uint64_t*, const char**);
	uint8_t (*CancelAndWaitAsyncRead)(uint8_t);
	void (*CancelAsyncRead)(uint8_t);
	HANDLE (*GetWorkerThreadHandle)(HANDLE*);
};
static_assert(sizeof(PakLoadFuncs_s) == 0x110);
static_assert(offsetof(PakLoadFuncs_s, AllocateEmptyPak) == 0x18);
static_assert(offsetof(PakLoadFuncs_s, UnloadAndWait) == 0x30);
static_assert(offsetof(PakLoadFuncs_s, LinkAssetBinding) == 0x80);
static_assert(offsetof(PakLoadFuncs_s, UnlinkAssetBinding) == 0x88);
static_assert(offsetof(PakLoadFuncs_s, OpenFile) == 0xC0);
static_assert(offsetof(PakLoadFuncs_s, GetWorkerThreadHandle) == 0x108);

extern PakLoadFuncs_s* g_pakLoadApi;
