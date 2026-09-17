#include "engine/deferredtrace.h"
#include "core/tier1.h"

void (*s_DeferredTraceAddRef)(DeferredTraceHandle_t);
void (*s_DeferredTraceFree)(DeferredTraceHandle_t*);
DeferredTraceState_e (*s_DeferredTraceGetResult)(DeferredTraceHandle_t, trace_t**);
DeferredTraceState_e (*s_DeferredTraceGetResultSync)(DeferredTraceHandle_t, trace_t**);

void UTIL_DeferredTraceAddRef_Client(DeferredTraceHandle_t traceHandle)
{
    if (traceHandle != DEFERRED_TRACE_INVALID)
        s_DeferredTraceAddRef(traceHandle);
}

void UTIL_DeferredTraceFree_Client(DeferredTraceHandle_t* traceHandle)
{
    if (*traceHandle != DEFERRED_TRACE_INVALID)
        s_DeferredTraceFree(traceHandle);
}

DeferredTraceState_e UTIL_DeferredTraceGetResult_Client(DeferredTraceHandle_t traceHandle, trace_t** outTr)
{
    return s_DeferredTraceGetResult(traceHandle, outTr);
}

DeferredTraceState_e UTIL_DeferredTraceGetResult_Sync_Client(DeferredTraceHandle_t traceHandle, trace_t** outTr)
{
    return s_DeferredTraceGetResultSync(traceHandle, outTr);
}

ON_DLL_LOAD_CLIENT("client.dll", DeferredTrace, [](CModule module)
{
    s_DeferredTraceAddRef = module.Offset(0x343DD0).RCast<decltype(s_DeferredTraceAddRef)>();
    s_DeferredTraceFree = module.Offset(0x343E50).RCast<decltype(s_DeferredTraceFree)>();
    s_DeferredTraceGetResult = module.Offset(0x343F10).RCast<decltype(s_DeferredTraceGetResult)>();
    s_DeferredTraceGetResultSync = module.Offset(0x343F40).RCast<decltype(s_DeferredTraceGetResultSync)>();
})
