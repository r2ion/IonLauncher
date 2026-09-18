#pragma once

#include <cstddef>

#define MAX_PLAYLIST_VAR_OVERRIDES 255

class bf_read;
class bf_write;

struct PlaylistVarOverride
{
    char name[128];
    char value[64];
};

struct PlaylistVarOverrides
{
    int count = 0;
    PlaylistVarOverride entries[MAX_PLAYLIST_VAR_OVERRIDES];

    PlaylistVarOverride* Find(const char* name);
    bool Set(const char* name, const char* value);
    bool Read(bf_read& buffer, int lengthBits);
    void Write(bf_write& buffer) const;
};

// use the R2 namespace for game funcs
namespace R2
{
inline const char* (*GetCurrentPlaylistName)();
inline void (*SetCurrentPlaylist)(const char* pPlaylistName);
inline bool (*SetPlaylistVarOverride)(const char* pVarName, const char* pValue);
inline const char* (*GetCurrentPlaylistVar)(const char* pVarName, bool bUseOverrides);
} // namespace R2
