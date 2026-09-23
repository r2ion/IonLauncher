#pragma once

#include "vscript/ivscript.h"
#include "vscript/languages/squirrel_re/squirrel/sqvm.h"

struct CSquirrelVM
{
  public:
    void SetTime(float time);
    HSCRIPT FindFunction(const char* functionName, const char* requiredType = nullptr, HSCRIPT scope = nullptr);
    void ReleaseFunction(HSCRIPT function);
    ScriptStatus_t ExecuteFunction(HSCRIPT function, ScriptVariant_t* arguments, int argumentCount, ScriptVariant_t* returnValue = nullptr,
                                   HSCRIPT scope = nullptr);

    BYTE gap_0[8];
    HSQUIRRELVM sqvm;
    BYTE gap_10[8];
    SQObject unknownObject_18;
    __int64 unknown_28;
    BYTE gap_30[12];
    __int32 vmContext;
    BYTE gap_40[648];
    char* (*formatString)(__int64 a1, const char* format, ...);
    BYTE gap_2D0[24];
};
static_assert(sizeof(CSquirrelVM) == 744);
