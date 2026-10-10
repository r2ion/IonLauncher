#include "game/shared/predictioncopy.h"
#include "logging/logging.h"
#include "tier0/hooks.h"

#include <cstdio>
#include <string_view>

DECLARE_MODULE(ClientPredictionCopy)

thread_local int g_nPredictionErrorCommand = -1;
DECLARE_HOOK_CC(CPredictionCopy_ReportFieldsDiffer, client.dll + 0x2DE3D0, __cdecl,
                [](auto& hook, CPredictionCopy* self, const datamap_t* map, const typedescription_t* field, const char* format, ...)
{
    ++self->m_nErrorCount;
    if (self->m_FieldCompareFunc)
        return;

    char message[4096];
    va_list args;
    va_copy(args, *hook.VarArgs());
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);
    std::string_view text(message);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        text.remove_suffix(1);

    NS::log::NATIVE_CL->info("[prediction] cmd={} ent={} error={} {}::{} tolerance={} {}", g_nPredictionErrorCommand, self->m_nEntIndex,
                             self->m_nErrorCount, map && map->dataClassName ? map->dataClassName : "unknown",
                             field && field->fieldName ? field->fieldName : "unknown", field ? field->fieldTolerance : 0.0f, text);
})

ON_DLL_LOAD_CLIENT("client.dll", PredictionCopy, [](CModule)
{
    DISPATCH_MODULE(ClientPredictionCopy)
})
