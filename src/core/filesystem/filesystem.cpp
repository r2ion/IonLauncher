#include "filesystem.h"
#include "core/tier1.h"
#include "modsystem/modmanager.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

DECLARE_MODULE(FilesystemHooks)

// the currently accepted sources for files
int iFileSourceType = FileSourceType_Any;

ConVar* Cvar_ns_fs_log_reads;

IFileSystem* g_pFilesystem;

using AddSearchPathFn = void(__fastcall*)(IFileSystem*, const char*, const char*, SearchPathAdd_t);
using MountVPKFn = CPackedStore* (*)(IFileSystem*, const char*);
using UnmountVPKFn = void (*)(IFileSystem*, const char*);

struct AddedModSearchPath_s
{
    std::string m_Path;
    std::string m_PathID;
    bool m_bHasPathID;
};

struct ModFilesystemState_s
{
    std::mutex m_SearchPathMutex;
    std::string m_CurrentModPath;
    std::vector<AddedModSearchPath_s> m_AddedSearchPaths;
    std::mutex m_MountedVPKsMutex;
    std::unordered_map<std::string, CPackedStore*> m_MountedVPKs;
    AddSearchPathFn m_AddSearchPath = nullptr;
    MountVPKFn m_MountVPK = nullptr;
    UnmountVPKFn m_UnmountVPK = nullptr;
};

static ModFilesystemState_s s_ModFilesystem;

std::string ReadVPKFile(const char* path)
{
    if (!g_pFilesystem || !path)
        return {};

    FileHandle_t fileHandle = g_pFilesystem->m_vtable2->Open(&g_pFilesystem->m_vtable2, path, "rb", "GAME", 0);
    if (!fileHandle)
        return {};

    std::stringstream fileStream;
    char data[4096];
    int bytesRead = 0;
    while ((bytesRead = g_pFilesystem->m_vtable2->Read(&g_pFilesystem->m_vtable2, data, static_cast<int>(std::size(data)), fileHandle)) > 0)
    {
        fileStream.write(data, bytesRead);
    }

    g_pFilesystem->m_vtable2->Close(g_pFilesystem, fileHandle);
    if (bytesRead < 0)
        return {};

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

static void AddTrackedModSearchPathLocked(IFileSystem* const fileSystem, const std::string& path, const char* const pathID,
                                          const SearchPathAdd_t addType)
{
    if (!fileSystem || path.empty())
        return;

    s_ModFilesystem.m_AddSearchPath(fileSystem, path.c_str(), pathID, addType);

    const std::string trackedPathID = pathID ? pathID : "";
    const auto existing =
        std::find_if(s_ModFilesystem.m_AddedSearchPaths.begin(), s_ModFilesystem.m_AddedSearchPaths.end(), [&](const AddedModSearchPath_s& added)
    { return added.m_Path == path && added.m_PathID == trackedPathID && added.m_bHasPathID == (pathID != nullptr); });
    if (existing == s_ModFilesystem.m_AddedSearchPaths.end())
        s_ModFilesystem.m_AddedSearchPaths.push_back({path, trackedPathID, pathID != nullptr});
}

DECLARE_HOOK(AddSearchPath, filesystem_stdio.dll + 0xB510,
             [](auto& hook, IFileSystem* fileSystem, const char* pPath, const char* pathID, SearchPathAdd_t addType)
{
    hook.Original(fileSystem, pPath, pathID, addType);

    std::scoped_lock lock(s_ModFilesystem.m_SearchPathMutex);

    // make sure current mod paths are at head
    if (s_ModFilesystem.m_CurrentModPath.compare(pPath) && addType == PATH_ADD_TO_HEAD)
    {
        AddTrackedModSearchPathLocked(fileSystem, s_ModFilesystem.m_CurrentModPath, pathID, PATH_ADD_TO_HEAD);
        s_ModFilesystem.m_AddSearchPath(fileSystem, GetCompiledAssetsPath().string().c_str(), pathID, PATH_ADD_TO_HEAD);
    }
})

void SetNewCompiledSearchPaths(const char* pPathID)
{
    std::scoped_lock lock(s_ModFilesystem.m_SearchPathMutex);

    // push compiled to head
    s_ModFilesystem.m_AddSearchPath(g_pFilesystem, fs::absolute(GetCompiledAssetsPath()).string().c_str(), pPathID, PATH_ADD_TO_HEAD);
    s_ModFilesystem.m_CurrentModPath.clear();
}

void SetNewModSearchPaths(Mod* mod, const char* pPathID)
{
    // put our new path to the head if we need to read from a different mod path
    // in the future we could also determine whether the file we're setting paths for needs a mod dir, or compiled assets
    if (mod == nullptr)
        return;
    const std::string modPath = (fs::absolute(mod->m_ModDirectory) / MOD_OVERRIDE_DIR).string();
    std::scoped_lock lock(s_ModFilesystem.m_SearchPathMutex);
    if (modPath.compare(s_ModFilesystem.m_CurrentModPath))
    {
        AddTrackedModSearchPathLocked(g_pFilesystem, modPath, pPathID, PATH_ADD_TO_HEAD);
        s_ModFilesystem.m_CurrentModPath = modPath;
    }
}

bool RemoveModSearchPaths()
{
    if (!g_pFilesystem || !g_pFilesystem->m_vtable || !g_pFilesystem->m_vtable->RemoveSearchPath)
        return false;

    std::scoped_lock lock(s_ModFilesystem.m_SearchPathMutex);

    for (const AddedModSearchPath_s& added : s_ModFilesystem.m_AddedSearchPaths)
        g_pFilesystem->m_vtable->RemoveSearchPath(g_pFilesystem, added.m_Path.c_str(), added.m_bHasPathID ? added.m_PathID.c_str() : nullptr);

    s_ModFilesystem.m_AddedSearchPaths.clear();
    s_ModFilesystem.m_CurrentModPath.clear();
    return true;
}

bool TryReplaceFile(const char* pPath, bool shouldCompile, const char* pPathID = "GAME")
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
            SetNewCompiledSearchPaths(pPathID);
            return true;
        }
    }

    if (iFileSourceType & FileSourceType_ModOverride)
    {
        auto file = g_pModManager->m_ModFiles.find(normalisedPath);

        if (file != g_pModManager->m_ModFiles.end())
        {
            SetNewModSearchPaths(file->second.m_pOwningMod, pPathID);
            return true;
        }
    }

    return false;
}

DECLARE_HOOK(ReadFromCache, filesystem_stdio.dll + 0xFE50, [](auto& hook, IFileSystem* filesystem, const char* pPath, void* result) -> bool
{
    // A VPK remount does not invalidate filesystem_stdio's cached source choice.
    const bool isReloadModel = g_pModManager->IsModModelFile(pPath);
    if (TryReplaceFile(pPath, true, "GAME") || isReloadModel)
        return false;

    return hook.Original(filesystem, pPath, result);
})

static std::string NormaliseVPKPath(const char* path)
{
    std::string normalised = fs::path(path).lexically_normal().generic_string();
    std::transform(normalised.begin(), normalised.end(), normalised.begin(), [](const unsigned char c) { return std::tolower(c); });
    return normalised;
}

static void ForgetMountedVPK(const char* path)
{
    if (!path)
        return;

    std::scoped_lock lock(s_ModFilesystem.m_MountedVPKsMutex);
    s_ModFilesystem.m_MountedVPKs.erase(NormaliseVPKPath(path));
}

static CPackedStore* FindMountedModVPK(const char* path)
{
    if (!path)
        return nullptr;

    const std::string normalisedPath = NormaliseVPKPath(path);
    std::scoped_lock lock(s_ModFilesystem.m_MountedVPKsMutex);
    const auto mounted = s_ModFilesystem.m_MountedVPKs.find(normalisedPath);
    return mounted == s_ModFilesystem.m_MountedVPKs.end() ? nullptr : mounted->second;
}

DECLARE_HOOK(ReadFileFromVPK, filesystem_stdio.dll + 0x5CBA0, [](auto& hook, CPackedStore* vpkInfo, uint64_t* b, char* filename) -> FileHandle_t
{
    // don't compile here because this is only ever called from OpenEx, which already compiles
    if (TryReplaceFile(filename, false))
    {
        *b = -1;
        return b;
    }

    std::string sourcePath;
    const bool isModVPKModel = g_pModManager->GetModVPKModelSource(filename, sourcePath);
    CPackedStore* const preferredVPK = isModVPKModel ? FindMountedModVPK(sourcePath.c_str()) : nullptr;

    // Remounted mod VPKs are appended after vanilla, so make the registered mod
    // source authoritative for its model paths.
    if (preferredVPK && vpkInfo != preferredVPK)
    {
        if (b)
            *reinterpret_cast<int32_t*>(b) = -1;
        return b;
    }

    return hook.Original(vpkInfo, b, filename);
})

DECLARE_HOOK(CBaseFileSystem_OpenEx, filesystem_stdio.dll + 0x15F50,
             [](auto& hook, IFileSystem* filesystem, const char* pPath, const char* pOptions, uint32_t flags, const char* pPathID,
                char** ppszResolvedFilename) -> FileHandle_t
{
    TryReplaceFile(pPath, true, pPathID);
    return hook.Original(filesystem, pPath, pOptions, flags, pPathID, ppszResolvedFilename);
})

static CPackedStore* MountModVPK(IFileSystem* fileSystem, const char* path)
{
    const std::string normalisedPath = NormaliseVPKPath(path);

    CPackedStore* const loaded = s_ModFilesystem.m_MountVPK(fileSystem, path);
    {
        std::scoped_lock mountedLock(s_ModFilesystem.m_MountedVPKsMutex);
        if (loaded)
            s_ModFilesystem.m_MountedVPKs.insert_or_assign(normalisedPath, loaded);
        else
            s_ModFilesystem.m_MountedVPKs.erase(normalisedPath);
    }
    return loaded;
}

CPackedStore* MountVPKDirect(const char* pVpkPath)
{
    if (!g_pFilesystem || !s_ModFilesystem.m_MountVPK || !pVpkPath)
        return nullptr;

    return MountModVPK(g_pFilesystem, pVpkPath);
}

bool UnmountVPKDirect(const char* pVpkPath)
{
    if (!g_pFilesystem || !s_ModFilesystem.m_UnmountVPK || !pVpkPath)
        return false;

    s_ModFilesystem.m_UnmountVPK(g_pFilesystem, pVpkPath);
    ForgetMountedVPK(pVpkPath);
    return true;
}

bool IsVPKMounted(const char* pVpkPath)
{
    if (!pVpkPath)
        return false;

    std::scoped_lock lock(s_ModFilesystem.m_MountedVPKsMutex);
    const std::string normalisedPath = NormaliseVPKPath(pVpkPath);
    return s_ModFilesystem.m_MountedVPKs.contains(normalisedPath);
}

DECLARE_HOOK(UnmountVPK, filesystem_stdio.dll + 0x1A120, [](auto& hook, IFileSystem* fileSystem, const char* pVpkPath)
{
    hook.Original(fileSystem, pVpkPath);
    ForgetMountedVPK(pVpkPath);
})

DECLARE_HOOK(MountVPK, filesystem_stdio.dll + 0xBEA0, [](auto& hook, IFileSystem* fileSystem, const char* pVpkPath) -> CPackedStore*
{
    CPackedStore* ret = hook.Original(fileSystem, pVpkPath);

    for (const Mod& mod : g_pModManager->m_LoadedMods)
    {
        if (!mod.m_bEnabled)
            continue;

        for (const ModVPKEntry& vpkEntry : mod.Vpks)
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

            CPackedStore* loaded = MountModVPK(fileSystem, vpkEntry.m_sVpkPath.c_str());
            if (loaded)
                g_pModManager->RegisterMountedVPKModels(vpkEntry);
            if (!ret) // this is primarily for map vpks and stuff, so the map's vpk is what gets returned from here
                ret = loaded;
        }
    }

    return ret;
})

ON_DLL_LOAD("filesystem_stdio.dll", Filesystem, [](CModule)
{
    g_pFilesystem = Sys_GetFactoryPtr("filesystem_stdio.dll", "VFileSystem017").RCast<IFileSystem*>();

    DISPATCH_MODULE(FilesystemHooks)

    s_ModFilesystem.m_AddSearchPath = HookSys::GetOriginalFunction<AddSearchPathFn>(HookSys::FindHook("AddSearchPath"));
    s_ModFilesystem.m_MountVPK = HookSys::GetOriginalFunction<MountVPKFn>(HookSys::FindHook("MountVPK"));
    s_ModFilesystem.m_UnmountVPK = HookSys::GetOriginalFunction<UnmountVPKFn>(HookSys::FindHook("UnmountVPK"));
})
