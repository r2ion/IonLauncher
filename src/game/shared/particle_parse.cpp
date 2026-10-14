//========= Copyright Valve Corporation, All rights reserved. ============//
#include "game/shared/particle_parse.h"
static int (*v_GetAttachTypeFromString)(const char*);

int GetAttachTypeFromString(const char* name)
{
    return v_GetAttachTypeFromString(name);
}

ON_DLL_LOAD_CLIENT("client.dll", ParticleParseMethods, [](CModule module)
{
    v_GetAttachTypeFromString = module.Offset(0x279E40).RCast<decltype(v_GetAttachTypeFromString)>();
})
