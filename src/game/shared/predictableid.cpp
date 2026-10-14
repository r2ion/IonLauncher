//========= Copyright Valve Corporation, All rights reserved. ============//
#include "game/shared/predictableid.h"

void (*C_PredictableId__Init)(C_PredictableId*, int, int, const char*, const char*, int);
void (*C_PredictableId__ResetInstanceCounters)();
const char* (*C_PredictableId__Describe)(const C_PredictableId*);

void C_PredictableId::Init(int player, int command, const char* classname, const char* module, int line)
{
    C_PredictableId__Init(this, player, command, classname, module, line);
}

void C_PredictableId::ResetInstanceCounters()
{
    C_PredictableId__ResetInstanceCounters();
}

const char* C_PredictableId::Describe() const
{
    return C_PredictableId__Describe(this);
}

ON_DLL_LOAD_CLIENT("client.dll", PredictableId, [](CModule module)
{
    C_PredictableId__Init = module.Offset(0x2D5760).RCast<decltype(C_PredictableId__Init)>();
    C_PredictableId__ResetInstanceCounters = module.Offset(0x2D5870).RCast<decltype(C_PredictableId__ResetInstanceCounters)>();
    C_PredictableId__Describe = module.Offset(0x2D5630).RCast<decltype(C_PredictableId__Describe)>();
})
