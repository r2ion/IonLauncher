#pragma once

#include "filesystem/ifilesystem.h"

#include <cstddef>
#include <string>

class CSearchPath
{
  public:
    unsigned char unknown[0x18];
    const char* debugPath;
};

static_assert(sizeof(CSearchPath) == 0x20);
static_assert(offsetof(CSearchPath, debugPath) == 0x18);

using AddSearchPathFn = void(__fastcall*)(IFileSystem*, const char*, const char*, SearchPathAdd_t);
using MountVPKFn = CPackedStore* (*)(IFileSystem*, const char*);
using UnmountVPKFn = void (*)(IFileSystem*, const char*);

// The registered VFileSystem017 pointer is the primary IFileSystem view. Its
// IBaseFileSystem base is the retail-proven secondary subobject at +0x8.
extern IFileSystem* g_pFilesystem;

std::string ReadVPKFile(const char* path);
std::string ReadVPKFile(const char* path, int fileSourceFilter);

// Mounts an already-resolved VPK base path without invoking the mod VPK hook.
CPackedStore* MountVPKDirect(const char* vpkPath);
bool UnmountVPKDirect(const char* vpkPath);
bool IsVPKMounted(const char* vpkPath);

// Removes every loose mod directory added through SetNewModSearchPaths.
bool RemoveModSearchPaths();
