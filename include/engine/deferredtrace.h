#pragma once

#include <type_traits>

#include "gametrace.h"

enum DeferredTraceHandle_t : int
{
    DEFERRED_TRACE_INVALID = -1
};

enum DeferredTraceState_e : int
{
    DEFERREDTRACE_STATE_INCOMPLETE = 0,
    DEFERREDTRACE_STATE_COMPLETE = 1,
    DEFERREDTRACE_STATE_CANCELLED = 2
};

void UTIL_DeferredTraceAddRef_Client(DeferredTraceHandle_t traceHandle);
void UTIL_DeferredTraceFree_Client(DeferredTraceHandle_t* traceHandle);
DeferredTraceState_e UTIL_DeferredTraceGetResult_Client(DeferredTraceHandle_t traceHandle, trace_t** outTr);
DeferredTraceState_e UTIL_DeferredTraceGetResult_Sync_Client(DeferredTraceHandle_t traceHandle, trace_t** outTr);

struct AutoDeferredTraceHandle_Client_s
{
    AutoDeferredTraceHandle_Client_s() : m_handle(DEFERRED_TRACE_INVALID) {}
    explicit AutoDeferredTraceHandle_Client_s(DeferredTraceHandle_t handle) : m_handle(handle) {}

    AutoDeferredTraceHandle_Client_s(const AutoDeferredTraceHandle_Client_s& other) : m_handle(other.m_handle)
    {
        UTIL_DeferredTraceAddRef_Client(m_handle);
    }

    ~AutoDeferredTraceHandle_Client_s() { UTIL_DeferredTraceFree_Client(&m_handle); }

    AutoDeferredTraceHandle_Client_s& operator=(AutoDeferredTraceHandle_Client_s other)
    {
        Swap(other);
        return *this;
    }

    AutoDeferredTraceHandle_Client_s& operator=(DeferredTraceHandle_t handle)
    {
        UTIL_DeferredTraceFree_Client(&m_handle);
        m_handle = handle;
        return *this;
    }

    operator DeferredTraceHandle_t() const { return m_handle; }

    void Swap(AutoDeferredTraceHandle_Client_s& other)
    {
        const DeferredTraceHandle_t handle = m_handle;
        m_handle = other.m_handle;
        other.m_handle = handle;
    }

    DeferredTraceHandle_t m_handle;
};
