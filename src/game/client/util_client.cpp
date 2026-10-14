#include "engine/deferredtrace.h"

void (*UTIL_DeferredTraceAddRef)(DeferredTraceHandle_t);
void (*UTIL_DeferredTraceFree)(DeferredTraceHandle_t*);
DeferredTraceState_e (*UTIL_DeferredTraceGetResult)(DeferredTraceHandle_t, trace_t**);
DeferredTraceState_e (*UTIL_DeferredTraceGetResult_Sync)(DeferredTraceHandle_t, trace_t**);

void UTIL_DeferredTraceAddRef_Client(DeferredTraceHandle_t traceHandle)
{
    if (traceHandle != DEFERRED_TRACE_INVALID)
        UTIL_DeferredTraceAddRef(traceHandle);
}

void UTIL_DeferredTraceFree_Client(DeferredTraceHandle_t* traceHandle)
{
    if (*traceHandle != DEFERRED_TRACE_INVALID)
        UTIL_DeferredTraceFree(traceHandle);
}

DeferredTraceState_e UTIL_DeferredTraceGetResult_Client(DeferredTraceHandle_t traceHandle, trace_t** outTr)
{
    return UTIL_DeferredTraceGetResult(traceHandle, outTr);
}

DeferredTraceState_e UTIL_DeferredTraceGetResult_Sync_Client(DeferredTraceHandle_t traceHandle, trace_t** outTr)
{
    return UTIL_DeferredTraceGetResult_Sync(traceHandle, outTr);
}

ON_DLL_LOAD_CLIENT("client.dll", DeferredTrace, [](CModule module)
{
    UTIL_DeferredTraceAddRef = module.Offset(0x343DD0).RCast<decltype(UTIL_DeferredTraceAddRef)>();
    UTIL_DeferredTraceFree = module.Offset(0x343E50).RCast<decltype(UTIL_DeferredTraceFree)>();
    UTIL_DeferredTraceGetResult = module.Offset(0x343F10).RCast<decltype(UTIL_DeferredTraceGetResult)>();
    UTIL_DeferredTraceGetResult_Sync = module.Offset(0x343F40).RCast<decltype(UTIL_DeferredTraceGetResult_Sync)>();
})
