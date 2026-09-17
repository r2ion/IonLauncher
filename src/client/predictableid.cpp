//========= Copyright Valve Corporation, All rights reserved. ============//
#include "predictableid.h"
#include "core/tier1.h"

void (*s_PredictableIdInit)(C_PredictableId*, int, int, const char*, const char*, int);
void (*s_PredictableIdResetInstanceCounters)();
const char* (*s_PredictableIdDescribe)(const C_PredictableId*);

void C_PredictableId::Init(int player, int command, const char* classname, const char* module, int line)
{
    s_PredictableIdInit(this, player, command, classname, module, line);
}

void C_PredictableId::ResetInstanceCounters()
{
    s_PredictableIdResetInstanceCounters();
}

const char* C_PredictableId::Describe() const
{
    return s_PredictableIdDescribe(this);
}

ON_DLL_LOAD_CLIENT("client.dll", PredictableId, [](CModule module)
{
    s_PredictableIdInit = module.Offset(0x2D5760).RCast<decltype(s_PredictableIdInit)>();
    s_PredictableIdResetInstanceCounters = module.Offset(0x2D5870).RCast<decltype(s_PredictableIdResetInstanceCounters)>();
    s_PredictableIdDescribe = module.Offset(0x2D5630).RCast<decltype(s_PredictableIdDescribe)>();
})
