#include "filesystem.h"
#include "core/tier1.h"
#include "modsystem/modmanager.h"

#include <iostream>
#include <sstream>

DECLARE_MODULE(FilesystemHooks)

// the currently accepted sources for files
int iFileSourceType = FileSourceType_Any;

std::string sCurrentModPath;

ConVar* Cvar_ns_fs_log_reads;

IFileSystem* g_pFilesystem;

std::string ReadVPKFile(const char* path)
{
	// read scripts.rson file, todo: check if this can be overwritten
	FileHandle_t fileHandle = g_pFilesystem->m_vtable2->Open(&g_pFilesystem->m_vtable2, path, "rb", "GAME", 0);

	std::stringstream fileStream;
	int bytesRead = 0;
	char data[4096];
	do
	{
		bytesRead = g_pFilesystem->m_vtable2->Read(&g_pFilesystem->m_vtable2, data, (int)std::size(data), fileHandle);
		fileStream.write(data, bytesRead);
	} while (bytesRead == std::size(data));

	g_pFilesystem->m_vtable2->Close(g_pFilesystem, fileHandle);

	return fileStream.str();
}

std::string ReadVPKFile(const char* path, int fileSourceType)
{
	int oldType = iFileSourceType;
	iFileSourceType = fileSourceType;

	std::string ret = ReadVPKFile(path);

	iFileSourceType = oldType;

	return ret;
}

static void(__fastcall* o_pAddSearchPath)(IFileSystem* fileSystem, const char* pPath, const char* pathID, SearchPathAdd_t addType) =
	nullptr;
static void __fastcall h_AddSearchPath(IFileSystem* fileSystem, const char* pPath, const char* pathID, SearchPathAdd_t addType)
{
	o_pAddSearchPath(fileSystem, pPath, pathID, addType);

	// make sure current mod paths are at head
	if (!strcmp(pathID, "GAME") && sCurrentModPath.compare(pPath) && addType == PATH_ADD_TO_HEAD)
	{
		o_pAddSearchPath(fileSystem, sCurrentModPath.c_str(), "GAME", PATH_ADD_TO_HEAD);
		o_pAddSearchPath(fileSystem, GetCompiledAssetsPath().string().c_str(), "GAME", PATH_ADD_TO_HEAD);
	}
}

void SetNewCompiledSearchPaths()
{
	// push compiled to head
	o_pAddSearchPath(g_pFilesystem, fs::absolute(GetCompiledAssetsPath()).string().c_str(), "GAME", PATH_ADD_TO_HEAD);
	sCurrentModPath = "";
}

void SetNewModSearchPaths(Mod* mod)
{
	// put our new path to the head if we need to read from a different mod path
	// in the future we could also determine whether the file we're setting paths for needs a mod dir, or compiled assets
	if (mod == nullptr)
		return;
	if ((fs::absolute(mod->m_ModDirectory) / MOD_OVERRIDE_DIR).string().compare(sCurrentModPath))
	{
		o_pAddSearchPath(g_pFilesystem, (fs::absolute(mod->m_ModDirectory) / MOD_OVERRIDE_DIR).string().c_str(), "GAME", PATH_ADD_TO_HEAD);
		sCurrentModPath = (fs::absolute(mod->m_ModDirectory) / MOD_OVERRIDE_DIR).string();
	}
}

bool TryReplaceFile(const char* pPath, bool shouldCompile)
{
	// idk how efficient the lexically normal check is
	// can't just set all /s in path to \, since some paths aren't in writeable memory
	std::string normalisedPath = g_pModManager->NormaliseModFilePath(fs::path(pPath));

	if (iFileSourceType & FileSourceType_Compiled)
	{
		// only compile assets if we would accept a compiled asset in the first place
		if (shouldCompile)
			g_pModManager->CompileAssetsForFile(pPath);

		if (g_pModManager->m_CompiledFiles.contains(normalisedPath))
		{
			SetNewCompiledSearchPaths();
			return true;
		}
	}

	if (iFileSourceType & FileSourceType_ModOverride)
	{
		auto file = g_pModManager->m_ModFiles.find(normalisedPath);
		if (file != g_pModManager->m_ModFiles.end())
		{
			SetNewModSearchPaths(file->second.m_pOwningMod);
			return true;
		}
	}

	return false;
}

// force modded files to be read from mods, not cache
static bool(__fastcall* o_pReadFromCache)(IFileSystem* filesystem, char* pPath, void* result) = nullptr;
static bool __fastcall h_ReadFromCache(IFileSystem* filesystem, char* pPath, void* result)
{
	if (TryReplaceFile(pPath, true))
		return false;

	return o_pReadFromCache(filesystem, pPath, result);
}

static FileHandle_t(__fastcall* o_pReadFileFromVPK)(VPKData* vpkInfo, uint64_t* b, char* filename) = nullptr;

DECLARE_HOOK(ReadFileFromVPK, filesystem_stdio.dll + 0x5CBA0, [](auto& hook, VPKData* vpkInfo, uint64_t* b, char* filename) -> FileHandle_t
{
	// don't compile here because this is only ever called from OpenEx, which already compiles
	if (TryReplaceFile(filename, false))
	{
		*b = -1;
		return b;
	}

	return hook.Original(vpkInfo, b, filename);
})

static FileHandle_t(__fastcall* o_pCBaseFileSystem__OpenEx)(
	IFileSystem* filesystem, const char* pPath, const char* pOptions, uint32_t flags, const char* pPathID, char** ppszResolvedFilename) =
	nullptr;

DECLARE_HOOK(CBaseFileSystem_OpenEx, filesystem_stdio.dll + 0x15F50, [](auto& hook,
	IFileSystem* filesystem,
	const char* pPath,
	const char* pOptions,
	uint32_t flags,
	const char* pPathID,
	char** ppszResolvedFilename) -> FileHandle_t
{
	TryReplaceFile(pPath, true);
	return hook.Original(filesystem, pPath, pOptions, flags, pPathID, ppszResolvedFilename);
})

static VPKData* (*o_pMountVPK)(IFileSystem* fileSystem, const char* pVpkPath) = nullptr;
static VPKData* h_MountVPK(IFileSystem* fileSystem, const char* pVpkPath)
{
	NS::log::fs->info("MountVPK {}", pVpkPath);
	VPKData* ret = o_pMountVPK(fileSystem, pVpkPath);

	for (Mod mod : g_pModManager->m_LoadedMods)
	{
		if (!mod.m_bEnabled)
			continue;

		for (ModVPKEntry& vpkEntry : mod.Vpks)
		{
			// if we're autoloading, just load no matter what
			if (!vpkEntry.m_bAutoLoad)
			{
				// resolve vpk name and try to load one with the same name
				// todo: we should be unloading these on map unload manually
				std::string mapName(fs::path(pVpkPath).filename().string());
				std::string modMapName(fs::path(vpkEntry.m_sVpkPath.c_str()).filename().string());
				if (mapName.compare(modMapName))
					continue;
			}

			VPKData* loaded = o_pMountVPK(fileSystem, vpkEntry.m_sVpkPath.c_str());
			if (!ret) // this is primarily for map vpks and stuff, so the map's vpk is what gets returned from here
				ret = loaded;
		}
	}

	return ret;
}

static ManualHook AddSearchPathHook("AddSearchPath", (LPVOID*)&o_pAddSearchPath, (LPVOID)h_AddSearchPath);
static ManualHook ReadFromCacheHook("ReadFromCache", (LPVOID*)&o_pReadFromCache, (LPVOID)h_ReadFromCache);
static ManualHook MountVPKHook("MountVPK", (LPVOID*)&o_pMountVPK, (LPVOID)h_MountVPK);

ON_DLL_LOAD("filesystem_stdio.dll", Filesystem, [](CModule module)
{
	DISPATCH_MODULE(FilesystemHooks)
	o_pReadFileFromVPK = HookSys::GetOriginalFunction<decltype(o_pReadFileFromVPK)>(HookSys::FindHook("ReadFileFromVPK"));
	o_pCBaseFileSystem__OpenEx =
		HookSys::GetOriginalFunction<decltype(o_pCBaseFileSystem__OpenEx)>(HookSys::FindHook("CBaseFileSystem_OpenEx"));

	g_pFilesystem = Sys_GetFactoryPtr("filesystem_stdio.dll", "VFileSystem017").RCast<IFileSystem*>();

	o_pAddSearchPath = reinterpret_cast<decltype(o_pAddSearchPath)>(g_pFilesystem->m_vtable->AddSearchPath);
	AddSearchPathHook.Dispatch(reinterpret_cast<LPVOID>(g_pFilesystem->m_vtable->AddSearchPath));
	o_pReadFromCache = reinterpret_cast<decltype(o_pReadFromCache)>(g_pFilesystem->m_vtable->ReadFromCache);
	ReadFromCacheHook.Dispatch(reinterpret_cast<LPVOID>(g_pFilesystem->m_vtable->ReadFromCache));
	o_pMountVPK = reinterpret_cast<decltype(o_pMountVPK)>(g_pFilesystem->m_vtable->MountVPK);
	MountVPKHook.Dispatch(reinterpret_cast<LPVOID>(g_pFilesystem->m_vtable->MountVPK));
})
