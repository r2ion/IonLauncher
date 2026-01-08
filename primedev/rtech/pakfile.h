#pragma once

#define PAK_MAX_LOADED_PAKS 512
#define PAK_MAX_LOADED_PAKS_MASK (PAK_MAX_LOADED_PAKS-1)

struct PakGlobalState_s
{

};

extern PakGlobalState_s* g_pakGlobalState;
