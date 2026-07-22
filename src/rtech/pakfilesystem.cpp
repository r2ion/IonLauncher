#include "pakfilesystem.h"
#include "modsystem/modmanager.h"
#include "dedicated/dedicated.h"
#include "core/tier0.h"
#include "util/utils.h"
#include "rtech/pakstate.h"
#include "rtech/paktools.h"
#include "rtech/imageatlas.h"
#include <mutex>
#include <algorithm>

DECLARE_MODULE(PakFilesystemHooks)

PakLoadFuncs_s* g_pakLoadApi;
PakLoadManager* g_pPakLoadManager;

static char* pszCurrentMapRpakPath = nullptr;
static PakHandle_t* piCurrentMapRpakHandle = nullptr;
static PakHandle_t* piCurrentMapPatchRpakHandle = nullptr;
static /*CModelLoader*/ void** ppModelLoader = nullptr;
static PakAllocator_s** g_pPakAllocator = nullptr;

static __int64 (*o_pLoadGametypeSpecificRpaks)(const char* levelName) = nullptr;
static void (**o_pCleanMaterialSystemStuff)() = nullptr;
static __int64 (**o_pCModelLoader_UnreferenceAllModels)(/*CModelLoader*/ void* a1) = nullptr;
static char* (*o_pLoadlevelLoadscreen)(const char* levelName) = nullptr;
static unsigned int (*o_pGetPakPatchNumber)(const char* pPakPath) = nullptr;

using PakFifoLockFn = void(__fastcall*)(JobFifoLock_s* lock);
using PakReadFileFn = bool(__fastcall*)(PakFile* pakFile);
static PakFifoLockFn s_AcquirePakFifoLockOrHelp = nullptr;
static PakFifoLockFn s_ReleasePakFifoLock = nullptr;
static PakReadFileFn s_PakReadFile = nullptr;

struct PakFailureState_s
{
	std::mutex m_Mutex;
	std::vector<PakHandle_t> m_UnsafeLoadedPaks;
	std::vector<PakHandle_t> m_SafeFailedPaks;
};

static PakFailureState_s s_PakFailureState;

static bool HasAllocatedSlab(const PakLoadedInfo_s& info)
{
	for (void* const slabBuffer : info.slabBuffers)
	{
		if (slabBuffer)
			return true;
	}

	return false;
}

static void MarkUnsafeLoadedPak(const PakHandle_t handle)
{
	std::scoped_lock lock(s_PakFailureState.m_Mutex);
	if (std::find(s_PakFailureState.m_UnsafeLoadedPaks.begin(), s_PakFailureState.m_UnsafeLoadedPaks.end(), handle) ==
		s_PakFailureState.m_UnsafeLoadedPaks.end())
		s_PakFailureState.m_UnsafeLoadedPaks.push_back(handle);
}

static void MarkSafeFailedPak(const PakHandle_t handle)
{
	std::scoped_lock lock(s_PakFailureState.m_Mutex);
	if (std::find(s_PakFailureState.m_SafeFailedPaks.begin(), s_PakFailureState.m_SafeFailedPaks.end(), handle) ==
		s_PakFailureState.m_SafeFailedPaks.end())
		s_PakFailureState.m_SafeFailedPaks.push_back(handle);
}

static bool IsSafeFailedPak(const PakHandle_t handle)
{
	std::scoped_lock lock(s_PakFailureState.m_Mutex);
	return std::find(s_PakFailureState.m_SafeFailedPaks.begin(), s_PakFailureState.m_SafeFailedPaks.end(), handle) !=
		s_PakFailureState.m_SafeFailedPaks.end();
}

static void ForgetSafeFailedPak(const PakHandle_t handle)
{
	std::scoped_lock lock(s_PakFailureState.m_Mutex);
	const auto failed = std::find(s_PakFailureState.m_SafeFailedPaks.begin(), s_PakFailureState.m_SafeFailedPaks.end(), handle);
	if (failed != s_PakFailureState.m_SafeFailedPaks.end())
		s_PakFailureState.m_SafeFailedPaks.erase(failed);
}

static bool HasActivePakTransactionsLocked(const PakGlobalState_s& pakGlobals)
{
	for (size_t i = 0; i < PAK_MAX_LOADED_PAKS; ++i)
	{
		const PakLoadedInfo_s& pakInfo = pakGlobals.loadedPaks[i];
		const PakStatus_e status = pakInfo.status;
		if (status == PAK_STATUS_FREED || status == PAK_STATUS_LOADED || status == PAK_STATUS_INVALID_PAKHANDLE)
			continue;

		// A failed pack with no PakFile left and no slab allocation is quiescent:
		// it owns no loaded assets and can safely wait for a later unload request.
		if (status == PAK_STATUS_ERROR && !pakInfo.pakFile && IsSafeFailedPak(pakInfo.handle))
			continue;

		return true;
	}

	return false;
}

bool Pak_IsUnsafeLoadedPak(const PakHandle_t handle)
{
	std::scoped_lock lock(s_PakFailureState.m_Mutex);
	return std::find(s_PakFailureState.m_UnsafeLoadedPaks.begin(), s_PakFailureState.m_UnsafeLoadedPaks.end(), handle) !=
		s_PakFailureState.m_UnsafeLoadedPaks.end();
}

bool Pak_HasUnsafeLoadedPaks()
{
	std::scoped_lock lock(s_PakFailureState.m_Mutex);
	return !s_PakFailureState.m_UnsafeLoadedPaks.empty();
}

bool PakLoadManager::TryAcquireIdlePakLock() const
{
	// m_forceReloadOnMapLoad only describes future work. Treating it as an active
	// transaction would make loose/VPK model reloads wait indefinitely for a map
	// change even though no native Rpak job can race them yet.
	if (m_reentranceCounter != 0 || !s_AcquirePakFifoLockOrHelp || !s_ReleasePakFifoLock)
		return false;

	PakGlobalState_s* const pakGlobals = Pak_GetGlobals();
	if (!pakGlobals)
		return false;

	s_AcquirePakFifoLockOrHelp(&pakGlobals->fifoLock);
	if (m_reentranceCounter != 0 || HasActivePakTransactionsLocked(*pakGlobals))
	{
		s_ReleasePakFifoLock(&pakGlobals->fifoLock);
		return false;
	}

	return true;
}

void PakLoadManager::ReleasePakLock() const
{
	if (PakGlobalState_s* const pakGlobals = Pak_GetGlobals(); pakGlobals && s_ReleasePakFifoLock)
		s_ReleasePakFifoLock(&pakGlobals->fifoLock);
}


// Marks all mod Paks to be unloaded on next map load.
// Also cleans up any mod Paks that are already unloaded.
void PakLoadManager::UnloadAllModPaks()
{
	NS::log::rpak->info("Reloading RPaks on next map load...");
	for (auto& modPak : m_modPaks)
	{
		modPak.m_markedForDelete = true;
	}
	// clean up any paks that are both marked for unload and already unloaded
	CleanUpUnloadedPaks();
	SetForceReloadOnMapLoad(true);
}

// Tracks all Paks related to a mod.
void PakLoadManager::TrackModPaks(Mod& mod)
{
	const fs::path modPakPath("./" / mod.m_ModDirectory / "paks");

	for (auto& modRpakEntry : mod.Rpaks)
	{
		ModPak_t pak;
		pak.m_modName = mod.Name;
		pak.m_path = (modPakPath / modRpakEntry.m_pakName).string();
		pak.m_pathHash = STR_HASH(pak.m_path);

		pak.m_preload = modRpakEntry.m_preload;
		pak.m_dependentPakHash = modRpakEntry.m_dependentPakHash;
		pak.m_mapRegex = modRpakEntry.m_loadRegex;

		// An unsafe pack cannot be unloaded in-process. Reuse its tracking entry
		// when the same mod remains enabled instead of loading a second copy beside
		// the quarantined handle.
		auto existing = std::find_if(m_modPaks.begin(), m_modPaks.end(), [&](const ModPak_t& trackedPak)
		{
			return trackedPak.m_path == pak.m_path && trackedPak.m_handle != PAK_INVALID_HANDLE &&
				Pak_IsUnsafeLoadedPak(trackedPak.m_handle);
		});
		if (existing != m_modPaks.end())
		{
			existing->m_modName = std::move(pak.m_modName);
			existing->m_preload = pak.m_preload;
			existing->m_dependentPakHash = pak.m_dependentPakHash;
			existing->m_mapRegex = std::move(pak.m_mapRegex);
			existing->m_markedForDelete = false;
			continue;
		}

		m_modPaks.push_back(std::move(pak));
	}
}

// Untracks all paks that aren't currently loaded and are marked for unload.
void PakLoadManager::CleanUpUnloadedPaks()
{
	auto fnRemovePredicate = [](ModPak_t& pak) -> bool {
			return pak.m_markedForDelete && pak.m_handle == PAK_INVALID_HANDLE;
		};

	m_modPaks.erase(std::remove_if(m_modPaks.begin(), m_modPaks.end(), fnRemovePredicate), m_modPaks.end());
}

// Unloads all paks that are marked for unload.
void PakLoadManager::UnloadMarkedPaks()
{
	if (Pak_HasUnsafeLoadedPaks())
		return;

	++m_reentranceCounter;
	const ScopeGuard guard([&]() { --m_reentranceCounter; });

	(*o_pCModelLoader_UnreferenceAllModels)(*ppModelLoader);
	(*o_pCleanMaterialSystemStuff)();

	for (auto& modPak : m_modPaks)
	{
		if (modPak.m_handle == PAK_INVALID_HANDLE || !modPak.m_markedForDelete)
			continue;

		if (Pak_IsUnsafeLoadedPak(modPak.m_handle))
			continue;

		const PakHandle_t handle = modPak.m_handle;
		g_pakLoadApi->UnloadAndWait(handle, *o_pCleanMaterialSystemStuff);
		if (Pak_HasUnsafeLoadedPaks())
		{
			modPak.m_handle = handle;
			continue;
		}
		modPak.m_handle = PAK_INVALID_HANDLE;
	}
}

// Loads all modded paks for the given map.
void PakLoadManager::LoadModPaksForMap(const char* mapName)
{
	++m_reentranceCounter;
	const ScopeGuard guard([&]() { --m_reentranceCounter; });

	for (auto& modPak : m_modPaks)
	{
		// don't load paks that are already loaded
		if (modPak.m_handle != PAK_INVALID_HANDLE)
			continue;
		std::cmatch matches;
		if (!std::regex_match(mapName, matches, modPak.m_mapRegex))
			continue;

		modPak.m_handle = g_pakLoadApi->AllocateEmptyPak(modPak.m_path.c_str(), *g_pPakAllocator, 7);
		m_mapPaks.push_back(modPak.m_pathHash);
	}
}

// Unloads all modded map paks.
void PakLoadManager::UnloadModPaks()
{
	if (Pak_HasUnsafeLoadedPaks())
		return;

	++m_reentranceCounter;
	const ScopeGuard guard([&]() { --m_reentranceCounter; });

	(*o_pCModelLoader_UnreferenceAllModels)(*ppModelLoader);
	(*o_pCleanMaterialSystemStuff)();

	for (auto& modPak : m_modPaks)
	{
		for (auto it = m_mapPaks.begin(); it != m_mapPaks.end(); ++it)
		{
			if (*it != modPak.m_pathHash)
				continue;

			if (Pak_IsUnsafeLoadedPak(modPak.m_handle))
			{
				break;
			}

			const PakHandle_t handle = modPak.m_handle;
			g_pakLoadApi->UnloadAndWait(handle, *o_pCleanMaterialSystemStuff);
			if (Pak_HasUnsafeLoadedPaks())
			{
				modPak.m_handle = handle;
				break;
			}

			modPak.m_handle = PAK_INVALID_HANDLE;
			m_mapPaks.erase(it, it + 1);
			break;
		}
	}

	// If this has happened, we may have leaked a pak?
	// It basically means that none of the entries in m_modPaks matched the hash in m_mapPaks so we didn't end up unloading it
	if (!Pak_HasUnsafeLoadedPaks())
		assert_msg(m_mapPaks.size() == 0, "Not all map paks were unloaded?");
}

// Called after a Pak was loaded.
void PakLoadManager::OnPakLoaded(std::string& originalPath, std::string& resultingPath, PakHandle_t resultingHandle)
{
	RuiImageAtlas_OnPakLoaded(resultingPath, resultingHandle);

	if (IsVanillaCall())
	{
		// add entry to loaded vanilla rpaks
		m_vanillaPaks.emplace_back(originalPath, resultingHandle);
	}

	LoadDependentPaks(resultingPath, resultingHandle);
}

// Called before a Pak was unloaded.
void PakLoadManager::OnPakUnloading(PakHandle_t handle)
{
	RuiImageAtlas_OnPakUnloading(handle);

	UnloadDependentPaks(handle);

	if (IsVanillaCall())
	{
		// remove entry from loaded vanilla rpaks
		auto fnRemovePredicate = [handle](std::pair<std::string, PakHandle_t>& pair) -> bool { return pair.second == handle; };

		m_vanillaPaks.erase(std::remove_if(m_vanillaPaks.begin(), m_vanillaPaks.end(), fnRemovePredicate), m_vanillaPaks.end());

		// no need to handle aliasing here, if vanilla wants it gone, it's gone
	}

	// set handle of the mod pak (if any) that has this handle for proper tracking
	for (auto& modPak : m_modPaks)
	{
		if (modPak.m_handle == handle)
			modPak.m_handle = PAK_INVALID_HANDLE;
	}
}

static uint32 Pak_GetPatchIndexForPak(const char* const pakName)
{
    int totalPatchCount = Pak_GetGlobals()->numPatchedPaks;

    if (!totalPatchCount)
        return 0;

    int iterator = 0;

    while (iterator < totalPatchCount)
    {
        const int index = (totalPatchCount + iterator) >> 1;
        const int compareResult = stricmp(pakName, Pak_GetGlobals()->patchedPakNames[index]);

        if (compareResult < 0)
            totalPatchCount = index;
        else if (compareResult > 0)
            iterator = index + 1;
        else
            return Pak_GetGlobals()->patchNumbers[index];
    }

    return 0; // Found nothing.
}

// Whether the vanilla game has this rpak
static bool VanillaHasPak(const char* pakName)
{
	fs::path originalPath = fs::path("./r2/paks/Win64") / pakName;
	unsigned int highestPatch = o_pGetPakPatchNumber(pakName);
	if (highestPatch)
	{
		// add the patch path to the extension
		char buf[16];
		snprintf(buf, sizeof(buf), "(%02u).rpak", highestPatch);
		// remove the .rpak and add the new suffix
		originalPath = originalPath.replace_extension().string() + buf;
	}
	else
	{
		originalPath /= pakName;
	}

	return fs::exists(originalPath);
}

// If vanilla doesn't have an rpak for this path, tries to map it to a modded rpak of the same name.
void PakLoadManager::FixupPakPath(std::string& pakPath)
{
	if (VanillaHasPak(pakPath.c_str()))
		return;

	for (ModPak_t& modPak : m_modPaks)
	{
		if (modPak.m_markedForDelete)
			continue;

		fs::path modPakFilename = fs::path(modPak.m_path).filename();
		if (pakPath == modPakFilename.string())
		{
			pakPath = modPak.m_path;
			return;
		}
	}
}

// Loads all "Preload" Paks. todo: deprecate Preload.
void PakLoadManager::LoadPreloadPaks()
{
	++m_reentranceCounter;
	const ScopeGuard guard([&]() { --m_reentranceCounter; });

	for (auto& modPak : m_modPaks)
	{
		if (modPak.m_markedForDelete || modPak.m_handle != PAK_INVALID_HANDLE || !modPak.m_preload)
			continue;

		modPak.m_handle = g_pakLoadApi->AllocateEmptyPak(modPak.m_path.c_str(), *g_pPakAllocator, 7);
	}
}

// Causes all "Postload" paks to be loaded again.
void PakLoadManager::ReloadPostloadPaks()
{
	++m_reentranceCounter;
	const ScopeGuard guard([&]() { --m_reentranceCounter; });

	// pretend that we just loaded all of these vanilla paks
	for (auto& [path, handle] : m_vanillaPaks)
	{
		LoadDependentPaks(path, handle);
	}
}

void* PakLoadManager::FindAssetByName(const char* name)
{
	return g_pakLoadApi->GetAssetBinding(Pak_StringToGuid(name));
}

// Loads Paks that depend on this Pak.
void PakLoadManager::LoadDependentPaks(std::string& path, PakHandle_t handle)
{
	++m_reentranceCounter;
	const ScopeGuard guard([&]() { --m_reentranceCounter; });

	const size_t hash = STR_HASH(path);
	for (auto& modPak : m_modPaks)
	{
		if (modPak.m_handle != PAK_INVALID_HANDLE)
			continue;
		if (modPak.m_dependentPakHash != hash)
			continue;

		// load pak
		modPak.m_handle = g_pakLoadApi->AllocateEmptyPak(modPak.m_path.c_str(), *g_pPakAllocator, 7);
		// Track the dependent mod pak by its own path hash so we can unload it when the dependency handle is unloaded.
		m_dependentPaks.emplace_back(handle, modPak.m_pathHash);
	}
}

// Unloads Paks that depend on this Pak.
void PakLoadManager::UnloadDependentPaks(PakHandle_t handle)
{
	if (Pak_HasUnsafeLoadedPaks())
		return;

	++m_reentranceCounter;
	const ScopeGuard guard([&]() { --m_reentranceCounter; });

	auto fnRemovePredicate = [&](std::pair<PakHandle_t, size_t>& pair) -> bool
	{
		if (pair.first != handle)
			return false;
		bool unloadedAll = true;

		for (auto& modPak : m_modPaks)
		{
			if (modPak.m_pathHash != pair.second || modPak.m_handle == PAK_INVALID_HANDLE)
				continue;

			if (Pak_IsUnsafeLoadedPak(modPak.m_handle))
			{
				unloadedAll = false;
				continue;
			}

			// unload pak
			const PakHandle_t dependentHandle = modPak.m_handle;
			g_pakLoadApi->UnloadAndWait(dependentHandle, *o_pCleanMaterialSystemStuff);
			if (Pak_HasUnsafeLoadedPaks())
			{
				modPak.m_handle = dependentHandle;
				unloadedAll = false;
				continue;
			}
			modPak.m_handle = PAK_INVALID_HANDLE;
		}

		return unloadedAll;
	};
	m_dependentPaks.erase(std::remove_if(m_dependentPaks.begin(), m_dependentPaks.end(), fnRemovePredicate), m_dependentPaks.end());
}

// Handles aliases for rpaks defined in rpak.json, effectively redirecting an rpak load to a different path.
static void HandlePakAliases(std::string& originalPath)
{
	// convert the pak being loaded to its aliased one, e.g. aliasing mp_hub_timeshift => sp_hub_timeshift
	for (int64_t i = g_pModManager->m_LoadedMods.size() - 1; i > -1; i--)
	{
		Mod* mod = &g_pModManager->m_LoadedMods[i];
		if (!mod->m_bEnabled)
			continue;

		if (mod->RpakAliases.find(originalPath) != mod->RpakAliases.end())
		{
			originalPath = mod->RpakAliases[originalPath];
			return;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_HOOK(LoadMapRpaks, engine.dll + 0x15A8C0, [](auto& hook, char* mapPath) -> bool
{
	NOTE_UNUSED(hook);
	if (Pak_HasUnsafeLoadedPaks())
		return false;
	const bool forceModelReload = g_pPakLoadManager->GetForceReloadOnMapLoad();

	// unload all mod rpaks that are marked for unload
	g_pPakLoadManager->UnloadMarkedPaks();
	g_pPakLoadManager->CleanUpUnloadedPaks();

	// strip file extension
	const std::string mapName = fs::path(mapPath).replace_extension().string();

	// load mp_common, sp_common etc.
	o_pLoadGametypeSpecificRpaks(mapName.c_str());

	// unload old modded map paks
	g_pPakLoadManager->UnloadModPaks();
	// load modded map paks
	g_pPakLoadManager->LoadModPaksForMap(mapName.c_str());

	// don't load/unload anything when going to the lobby, presumably to save load times when going back to the same map
	if (!g_pPakLoadManager->GetForceReloadOnMapLoad() && !strcmp("mp_lobby", mapName.c_str()))
		return false;

	if (g_pPakLoadManager->GetForceReloadOnMapLoad())
	{
		g_pPakLoadManager->LoadPreloadPaks();
		g_pPakLoadManager->ReloadPostloadPaks();
	}

	char mapRpakStr[272];
	snprintf(mapRpakStr, 272, "%s.rpak", mapName.c_str());

	// if level being loaded is the same as current level, do nothing
	if (!g_pPakLoadManager->GetForceReloadOnMapLoad() && !strcmp(mapRpakStr, pszCurrentMapRpakPath))
		return true;

	strcpy(pszCurrentMapRpakPath, mapRpakStr);

	(*o_pCleanMaterialSystemStuff)();
	o_pLoadlevelLoadscreen(mapName.c_str());

	// unload old map rpaks
	PakHandle_t curHandle = *piCurrentMapRpakHandle;
	PakHandle_t curPatchHandle = *piCurrentMapPatchRpakHandle;
	if (curHandle != PAK_INVALID_HANDLE)
	{
		(*o_pCModelLoader_UnreferenceAllModels)(*ppModelLoader);
		(*o_pCleanMaterialSystemStuff)();
		g_pakLoadApi->UnloadAndWait(curHandle, *o_pCleanMaterialSystemStuff);
		*piCurrentMapRpakHandle = PAK_INVALID_HANDLE;
	}
	if (curPatchHandle != PAK_INVALID_HANDLE)
	{
		(*o_pCModelLoader_UnreferenceAllModels)(*ppModelLoader);
		(*o_pCleanMaterialSystemStuff)();
		g_pakLoadApi->UnloadAndWait(curPatchHandle, *o_pCleanMaterialSystemStuff);
		*piCurrentMapPatchRpakHandle = PAK_INVALID_HANDLE;
	}

	*piCurrentMapRpakHandle = g_pakLoadApi->AllocateEmptyPak(mapRpakStr, *g_pPakAllocator, 7);

	// load special _patch rpak (seemingly used for dev things?)
	char levelPatchRpakStr[272];
	snprintf(levelPatchRpakStr, 272, "%s_patch.rpak", mapName.c_str());
	*piCurrentMapPatchRpakHandle = g_pakLoadApi->AllocateEmptyPak(levelPatchRpakStr, *g_pPakAllocator, 7);

	// we just reloaded the paks, so we don't need to force it again
	g_pPakLoadManager->SetForceReloadOnMapLoad(false);
	if (forceModelReload)
		g_pModManager->RequestModelReload();
	return true;
})

DECLARE_HOOK(Pak_AllocateEmptyPak, rtech_game.DLL + 0xB0F0, [](auto& hook, const char* pPath, PakAllocator_s* allocator, int flags) -> PakHandle_t
{
	// make a copy of the path for comparing to determine whether we should load this pak on dedi, before it could get overwritten
	std::string svOriginalPath(pPath);

	std::string resultingPath(pPath);
	HandlePakAliases(resultingPath);

	if (g_pPakLoadManager->IsVanillaCall())
	{
		g_pPakLoadManager->LoadPreloadPaks();
		g_pPakLoadManager->FixupPakPath(resultingPath);

		// do this after custom paks load and in bShouldLoadPaks so we only ever call this on the root pakload call
		// todo: could probably add some way to flag custom paks to not be loaded on dedicated servers in rpak.json

		// dedicated only needs common, common_mp, common_sp, and sp_<map> rpaks
		// sp_<map> rpaks contain tutorial ghost data
		// sucks to have to load the entire rpak for that but sp was never meant to be done on dedi
		if (IsDedicatedServer() &&
			(CommandLine()->CheckParm("-nopakdedi") || strncmp(&svOriginalPath[0], "common", 6) && strncmp(&svOriginalPath[0], "sp_", 3) && (strncmp(&svOriginalPath[0], "mp_", 3) || strstr(&svOriginalPath[0], "loadscreen"))))
		{
			NS::log::rpak->info("Not loading pak {} for dedicated server", svOriginalPath);
			return PAK_INVALID_HANDLE;
		}
	}

	PakHandle_t iPakHandle = hook.Original(resultingPath.c_str(), allocator, flags);
	NS::log::rpak->info("AllocateEmptyPak {} {}", resultingPath, static_cast<int>(iPakHandle));

	g_pPakLoadManager->OnPakLoaded(svOriginalPath, resultingPath, iPakHandle);
	return iPakHandle;
})

DECLARE_HOOK(Pak_UnloadAndWait, rtech_game.DLL + 0xB280, [](auto& hook, PakHandle_t nPakHandle, PakLoadFuncs_s::Callback_t callback)
{
	if (Pak_HasUnsafeLoadedPaks())
		return;

	g_pPakLoadManager->OnPakUnloading(nPakHandle);
	PakGlobalState_s* pakGlobals = Pak_GetGlobals();
	if (!pakGlobals)
	{
		hook.Original(nPakHandle, callback);
		return;
	}
	auto pakInfo = &pakGlobals->loadedPaks[nPakHandle & PAK_MAX_LOADED_PAKS_MASK];
	NS::log::rpak->info("UnloadAndWait {},Handle {},Status: {}", pakInfo->filename, static_cast<int>(pakInfo->handle), static_cast<int>(pakInfo->status));
	hook.Original(nPakHandle, callback);
})

// we hook this exclusively for resolving stbsp paths, but seemingly it's also used for other stuff like vpk, rpak, mprj and starpak loads
// tbh this actually might be for memory mapped files or something, would make sense i think
DECLARE_HOOK(Pak_OpenFile, rtech_game.DLL + 0x1E20, [](auto& hook, const char* pPath, uint64_t* fileSize) -> PakHandle_t
{
	// NOTE [Fifty]: For some reason some users are getting pPath as null when
	//               loading a server, Pak_OpenFile uses CreateFileA and checks
	//               its return value so this is completely safe
	// Additionally, guard against non-userland/sentinel pointers (e.g. 0xFFFFFFFFFFFFFFFF)
	// to avoid AVs when rtech passes an invalid pointer.
	const uintptr_t pathPtr = reinterpret_cast<uintptr_t>(pPath);
	if (pPath == NULL || (pathPtr & 0xFFFF000000000000ull) == 0xFFFF000000000000ull)
	{
		//NS::log::rpak->warn("OpenFile called with invalid pPath pointer: 0x{:X}", pathPtr);
		return PAK_INVALID_HANDLE;
	}

	fs::path path(pPath);
	std::string newPath = "";
	fs::path filename = path.filename();

	if (path.extension() == ".stbsp")
	{
		if (IsDedicatedServer())
			return PAK_INVALID_HANDLE;

		NS::log::rpak->info("LoadStreamBsp: {}", filename.string());

		// resolve modded stbsp path so we can load mod stbsps
		auto modFile = g_pModManager->m_ModFiles.find(g_pModManager->NormaliseModFilePath(fs::path("maps" / filename)));
		if (modFile != g_pModManager->m_ModFiles.end())
		{
			newPath = (modFile->second.m_pOwningMod->m_ModDirectory / "mod" / modFile->second.m_Path).string();
			pPath = newPath.c_str();
		}
	}
	else if (path.extension() == ".starpak")
	{
		if (IsDedicatedServer())
			return PAK_INVALID_HANDLE;

		// code for this is mostly stolen from above

		// unfortunately I can't find a way to get the rpak that is causing this function call, so I have to
		// store them on mod init and then compare the current path with the stored paths

		// game adds r2\ to every path, so assume that a starpak path that begins with r2\paks\ is a vanilla one
		// modded starpaks will be in the mod's paks folder (but can be in folders within the paks folder)

		// this might look a bit confusing, but its just an iterator over the various directories in a path.
		// path.begin() being the first directory, r2 in this case, which is guaranteed anyway,
		// so increment the iterator with ++ to get the first actual directory, * just gets the actual value
		// then we compare to "paks" to determine if it's a vanilla rpak or not
		if (*++path.begin() != "paks")
		{
			// remove the r2\ from the start used for path lookups
			std::string starpakPath = path.string().substr(3);
			// hash the starpakPath to compare with stored entries
			size_t hashed = STR_HASH(starpakPath);

			// loop through all loaded mods
			for (Mod& mod : g_pModManager->m_LoadedMods)
			{
				// ignore non-loaded mods
				if (!mod.m_bEnabled)
					continue;

				// loop through the stored starpak paths
				for (size_t hash : mod.StarpakPaths)
				{
					if (hash == hashed)
					{
						// construct new path
						newPath = (mod.m_ModDirectory / "paks" / starpakPath).string();
						// set path to the new path
						pPath = newPath.c_str();
						goto LOG_STARPAK;
					}
				}
			}
		}

	LOG_STARPAK:
		NS::log::rpak->info("LoadStreamPak: {}", filename.string());
	}

	return hook.Original(pPath, fileSize);
})

DECLARE_HOOK(Pak_RunRePak, rtech_game.DLL + 0xA9F0, [](auto& hook, PakLoadedInfo_s* info) -> bool
{
	if (!info || !info->pakFile || !s_PakReadFile)
		return hook.Original(info);

	PakFile* const pakFile = info->pakFile;

	// Native Pak_RunRePak reads the remaining metadata immediately before it
	// calculates and allocates the four slab buffers. Read it here so malformed
	// slab/page metadata can be rejected before any slab allocation is exposed
	// to asset loading. A false result has the same retry semantics as native.
	if (pakFile->copyBytesRemaining != 0 && !s_PakReadFile(pakFile))
		return false;

	PakSlabRepairReport_s repairReport = {};
	if (!pakFile->ValidateAndRepairSlabMetadata(repairReport))
	{
		NS::log::rpak->error("Rejecting invalid Rpak before slab allocation: {}", info->filename ? info->filename : "<unknown>");
		MarkSafeFailedPak(info->handle);
		info->status = PAK_STATUS_ERROR;
		return false;
	}

	if (repairReport.repairCount != 0)
		NS::log::rpak->warn(
			"Repaired {} Rpak slab entries in {} (+{} bytes)", repairReport.repairCount,
			info->filename ? info->filename : "<unknown>", repairReport.addedBytes);

	return hook.Original(info);
})

DECLARE_HOOK(Pak_UnloadInternal, rtech_game.DLL + 0x8B40, [](auto& hook, PakHandle_t handle)
{
	// This function runs under the native FIFO lock. Once corruption has been
	// quarantined, do not invoke asset/material unload callbacks for any pack.
	if (Pak_HasUnsafeLoadedPaks())
	{
		if (PakGlobalState_s* const pakGlobals = Pak_GetGlobals())
		{
			PakLoadedInfo_s& info = pakGlobals->loadedPaks[handle & PAK_MAX_LOADED_PAKS_MASK];
			if (info.handle == handle)
				info.status = PAK_STATUS_ERROR;
		}
		return;
	}

	hook.Original(handle);
})

DECLARE_HOOK(Pak_Free, rtech_game.DLL + 0x8900, [](auto& hook, PakHandle_t handle)
{
	// Pak_Free also runs under the FIFO lock and is the last authoritative gate
	// before allocator-owned slab buffers are released.
	if (Pak_HasUnsafeLoadedPaks())
		return;

	hook.Original(handle);
})

DECLARE_HOOK(Pak_BeginUnload, rtech_game.DLL + 0xB1B0, [](auto& hook, PakHandle_t handle)
{
	PakLoadedInfo_s* info = nullptr;
	bool isSafeFailure = false;
	if (PakGlobalState_s* const pakGlobals = Pak_GetGlobals())
	{
		info = &pakGlobals->loadedPaks[handle & PAK_MAX_LOADED_PAKS_MASK];
		if (info->handle == handle && info->status == PAK_STATUS_ERROR && HasAllocatedSlab(*info))
			MarkUnsafeLoadedPak(handle);

		isSafeFailure = info->handle == handle && IsSafeFailedPak(handle);
	}

	// Pak_WaitForLoadCompletion calls Pak_BeginUnload directly, bypassing the
	// public UnloadAndWait hook. Stop that path too once slab corruption may have
	// occurred, otherwise the pending FIFO entry can still reach Pak_Free.
	if (Pak_HasUnsafeLoadedPaks())
		return;

	// Native ERROR cleanup only reaches Pak_Free while the original load-FIFO
	// entry is still pending. If Finalise already consumed that entry, queue a
	// normal empty unload instead. No asset was populated before slab setup, so
	// assetCount must be zero before Pak_UnloadInternal sees the synthetic state.
	if (isSafeFailure && info->status == PAK_STATUS_ERROR && !info->pakFile)
	{
		info->assetCount = 0;
		info->status = PAK_STATUS_LOADED;
	}

	hook.Original(handle);

	if (isSafeFailure && info->status != PAK_STATUS_ERROR)
		ForgetSafeFailedPak(handle);
})

DECLARE_HOOK(Pak_Finalise, rtech_game.DLL + 0x8410, [](auto& hook, PakLoadedInfo_s* info)
{
	const PakHandle_t handle = info ? info->handle : PAK_INVALID_HANDLE;
	bool invalidPakFile = false;
	if (info && info->pakFile)
	{
		invalidPakFile = !info->pakFile->IsValid();
		if (invalidPakFile)
		{
			NS::log::rpak->error("Bad Rpak {}", info->filename);
		}

		Pak_ReleaseZStdDecoder(&info->pakFile->codec);
	}

	if (info && (invalidPakFile || info->status == PAK_STATUS_ERROR))
	{
		// No slab means no page data or assets could have been populated. Once a
		// slab exists, malformed offsets may already have overwritten allocator
		// metadata, so freeing anything in-process is unsafe.
		if (HasAllocatedSlab(*info))
			MarkUnsafeLoadedPak(info->handle);
		else
			MarkSafeFailedPak(info->handle);
	}

	hook.Original(info);

	if (handle != PAK_INVALID_HANDLE && info && info->status == PAK_STATUS_LOADED)
		RuiImageAtlas_OnPakLoadCompleted(handle);
})

ON_DLL_LOAD("engine.dll", RpakFilesystem, [](CModule module)
{
	g_pPakLoadManager = new PakLoadManager;

	g_pakLoadApi = module.Offset(0x5BED78).Deref().RCast<PakLoadFuncs_s*>();

	pszCurrentMapRpakPath = module.Offset(0x1315C3E0).RCast<decltype(pszCurrentMapRpakPath)>();
	piCurrentMapRpakHandle = module.Offset(0x7CB5A0).RCast<decltype(piCurrentMapRpakHandle)>();
	piCurrentMapPatchRpakHandle = module.Offset(0x7CB5A4).RCast<decltype(piCurrentMapPatchRpakHandle)>();
	ppModelLoader = module.Offset(0x7C4AC0).RCast<decltype(ppModelLoader)>();
	g_pPakAllocator = module.Offset(0x7C5E20).RCast<decltype(g_pPakAllocator)>();

	o_pLoadGametypeSpecificRpaks = module.Offset(0x15AD20).RCast<decltype(o_pLoadGametypeSpecificRpaks)>();
	o_pCleanMaterialSystemStuff = module.Offset(0x12A11F00).RCast<decltype(o_pCleanMaterialSystemStuff)>();
	o_pCModelLoader_UnreferenceAllModels = module.Offset(0x5ED580).RCast<decltype(o_pCModelLoader_UnreferenceAllModels)>();
	o_pLoadlevelLoadscreen = module.Offset(0x15A810).RCast<decltype(o_pLoadlevelLoadscreen)>();

	CModule rtechModule(GetModuleHandleA("rtech_game.DLL"));
	o_pGetPakPatchNumber = rtechModule.Offset(0x9A00).RCast<decltype(o_pGetPakPatchNumber)>();
	// IDASQL: rtech_game IAT entries used by Pak_RunLoadLoop at 0xB8B4/0xBB3D.
	s_ReleasePakFifoLock = rtechModule.Offset(0x2D3A0).Deref().RCast<PakFifoLockFn>();
	s_AcquirePakFifoLockOrHelp = rtechModule.Offset(0x2D3A8).Deref().RCast<PakFifoLockFn>();
	s_PakReadFile = rtechModule.Offset(0x8D10).RCast<PakReadFileFn>();

	DISPATCH_MODULE(PakFilesystemHooks)
})
