//========= Copyright Valve Corporation, All rights reserved. ============//
#include "particle_parse.h"

static int (*s_GetAttachTypeFromString)(const char*);

int GetAttachTypeFromString(const char* name)
{
    return s_GetAttachTypeFromString(name);
}

ON_DLL_LOAD_CLIENT("client.dll", ParticleParseMethods, [](CModule module)
{
    s_GetAttachTypeFromString = module.Offset(0x279E40).RCast<decltype(s_GetAttachTypeFromString)>();
})
