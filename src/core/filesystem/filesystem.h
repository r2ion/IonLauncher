#pragma once

#include <cstddef>
#include <cstdint>

// taken from ttf2sdk
typedef void* FileHandle_t;
using FileFindHandle_t = std::uint64_t;
inline constexpr FileFindHandle_t FILESYSTEM_INVALID_FIND_HANDLE = UINT64_MAX;

class CPackedStore;

enum SearchPathAdd_t
{
    PATH_ADD_TO_HEAD, // First path searched
    PATH_ADD_TO_TAIL, // Last path searched
};

enum FileSourceType_t : int
{
    FileSourceType_Original = 1 << 0,
    FileSourceType_ModOverride = 1 << 1,
    FileSourceType_Compiled = 1 << 2,

    FileSourceType_Modded = FileSourceType_ModOverride | FileSourceType_Compiled,
    FileSourceType_Any = FileSourceType_Original | FileSourceType_ModOverride | FileSourceType_Compiled,
};

class CSearchPath
{
  public:
    unsigned char unknown[0x18];
    const char* debugPath;
};

class IFileSystem
{
  public:
    struct VTable
    {
        void* unknown[10];
        void (*AddSearchPath)(IFileSystem* fileSystem, const char* pPath, const char* pathID, SearchPathAdd_t addType);
        bool (*RemoveSearchPath)(IFileSystem* fileSystem, const char* pPath, const char* pathID);
        void* unknown2[18];
        const char* (*FindFirst)(IFileSystem* fileSystem, const char* pWildCard, FileFindHandle_t* pHandle);
        const char* (*FindNext)(IFileSystem* fileSystem, FileFindHandle_t handle);
        bool (*FindIsDirectory)(IFileSystem* fileSystem, FileFindHandle_t handle);
        void (*FindClose)(IFileSystem* fileSystem, FileFindHandle_t handle);
        const char* (*FindFirstEx)(IFileSystem* fileSystem, const char* pWildCard, const char* pPathID, FileFindHandle_t* pHandle);
        void* unknown3[60];
        bool (*ReadFromCache)(IFileSystem* fileSystem, const char* path, void* result);
        void* unknown4[15];
        CPackedStore* (*MountVPK)(IFileSystem* fileSystem, const char* vpkPath);
        void (*UnmountVPK)(IFileSystem* fileSystem, const char* vpkPath);
    };

    struct VTable2
    {
        int (*Read)(IFileSystem::VTable2** fileSystem, void* pOutput, int size, FileHandle_t file);
        void* unknown[1];
        FileHandle_t (*Open)(IFileSystem::VTable2** fileSystem, const char* pFileName, const char* pOptions, const char* pathID, int64_t unknown);
        void (*Close)(IFileSystem* fileSystem, FileHandle_t file);
        long long (*Seek)(IFileSystem::VTable2** fileSystem, FileHandle_t file, long long offset, long long whence);
        void* unknown2[5];
        bool (*FileExists)(IFileSystem::VTable2** fileSystem, const char* pFileName, const char* pPathID);
    };

    VTable* m_vtable;
    VTable2* m_vtable2;
};

static_assert(offsetof(IFileSystem::VTable, AddSearchPath) == 10 * sizeof(void*));
static_assert(offsetof(IFileSystem::VTable, RemoveSearchPath) == 11 * sizeof(void*));
static_assert(offsetof(IFileSystem::VTable, FindFirst) == 30 * sizeof(void*));
static_assert(offsetof(IFileSystem::VTable, FindNext) == 31 * sizeof(void*));
static_assert(offsetof(IFileSystem::VTable, FindIsDirectory) == 32 * sizeof(void*));
static_assert(offsetof(IFileSystem::VTable, FindClose) == 33 * sizeof(void*));
static_assert(offsetof(IFileSystem::VTable, FindFirstEx) == 34 * sizeof(void*));
static_assert(offsetof(IFileSystem::VTable, ReadFromCache) == 95 * sizeof(void*));
static_assert(offsetof(IFileSystem::VTable, MountVPK) == 111 * sizeof(void*));
static_assert(offsetof(IFileSystem::VTable, UnmountVPK) == 112 * sizeof(void*));

extern IFileSystem* g_pFilesystem;

std::string ReadVPKFile(const char* path);
std::string ReadVPKFile(const char* path, int fileSourceFilter);

// Mounts an already-resolved VPK base path without invoking the mod VPK hook.
CPackedStore* MountVPKDirect(const char* vpkPath);
bool UnmountVPKDirect(const char* vpkPath);
bool IsVPKMounted(const char* vpkPath);

// Removes every loose mod directory added through SetNewModSearchPaths.
bool RemoveModSearchPaths();
