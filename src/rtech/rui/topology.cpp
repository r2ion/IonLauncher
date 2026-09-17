#include "rtech/rui/topology.h"

#include "tier0/module.h"
#include "vscript/languages/squirrel_re/squirrel.h"
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>

DECLARE_MODULE(RuiTopologyHooks)

using GetTopologyArgumentFn = RuiTopology* (*)(HSQUIRRELVM sqvm, int argumentIndex);

static GetTopologyArgumentFn RuiTopology_GetArgument;
static std::atomic<uint64_t> g_HiddenTopologyMask = 0;
static std::array<std::atomic<RuiTopologyHandle>, RUI_TOPOLOGY_CAPACITY> g_HiddenTopologyHandles{};
static std::array<std::atomic<const RuiTopology*>, RUI_TOPOLOGY_CAPACITY> g_HiddenTopologies{};

static void RuiTopology_SetHidden(const RuiTopology* topology, bool hidden)
{
    const RuiTopologyHandle topologyHandle = topology->handle;
    const size_t topologyIndex = topologyHandle & RUI_TOPOLOGY_INDEX_MASK;
    const uint64_t topologyBit = uint64_t{1} << topologyIndex;
    if (hidden)
    {
        g_HiddenTopologies[topologyIndex].store(topology, std::memory_order_relaxed);
        g_HiddenTopologyHandles[topologyIndex].store(topologyHandle, std::memory_order_relaxed);
        g_HiddenTopologyMask.fetch_or(topologyBit, std::memory_order_release);
        return;
    }

    if (g_HiddenTopologyHandles[topologyIndex].load(std::memory_order_acquire) == topologyHandle)
        g_HiddenTopologyMask.fetch_and(~topologyBit, std::memory_order_release);
}

static RuiTopology* RuiTopology_GetFromScript(HSQUIRRELVM sqvm)
{
    if (!RuiTopology_GetArgument)
    {
        g_pSquirrel[ScriptContext::CLIENT]->raiseerror(sqvm, "RUI topology API is unavailable");
        return nullptr;
    }

    SQObject topologyObject{};
    g_pSquirrel[ScriptContext::CLIENT]->__sq_getobject(sqvm, 1, &topologyObject);
    if (topologyObject._Type != _RT_USERPOINTER)
    {
        g_pSquirrel[ScriptContext::CLIENT]->raiseerror(sqvm, "Argument 1 is not a RUI topology");
        return nullptr;
    }

    return RuiTopology_GetArgument(sqvm, 1);
}

ADD_SQFUNC("void", RuiTopology_Hide, "var topology", "Prevents RUI instances using this topology from submitting render jobs.", ScriptContext::CLIENT)
{
    RuiTopology* topology = RuiTopology_GetFromScript(sqvm);
    if (!topology)
        return SQRESULT_ERROR;

    RuiTopology_SetHidden(topology, true);
    return SQRESULT_NULL;
}

ADD_SQFUNC("void", RuiTopology_Show, "var topology", "Allows RUI instances using this topology to submit render jobs.", ScriptContext::CLIENT)
{
    RuiTopology* topology = RuiTopology_GetFromScript(sqvm);
    if (!topology)
        return SQRESULT_ERROR;

    RuiTopology_SetHidden(topology, false);
    return SQRESULT_NULL;
}

static bool RuiTopology_IsHidden(const RuiInstance* rui) noexcept
{
    if (!rui || !rui->drawInfo)
        return false;

    uint64_t hiddenMask = g_HiddenTopologyMask.load(std::memory_order_acquire);
    while (hiddenMask != 0)
    {
        const size_t topologyIndex = std::countr_zero(hiddenMask);
        const RuiTopology* topology = g_HiddenTopologies[topologyIndex].load(std::memory_order_relaxed);
        if (topology && rui->drawInfo == &topology->drawInfo.base)
        {
            const RuiTopologyHandle hiddenHandle = g_HiddenTopologyHandles[topologyIndex].load(std::memory_order_relaxed);
            return hiddenHandle == topology->handle;
        }

        hiddenMask &= hiddenMask - 1;
    }

    return false;
}

DECLARE_HOOK(RuiRenderJobs, engine.dll + 0xF9530, [](auto& hook, RuiRenderContext* context, RuiInstance* rui, RuiDrawBatch* batch) -> bool
{
    if (RuiTopology_IsHidden(rui))
        return true;

    return hook.Original(context, rui, batch);
})

ON_DLL_LOAD_CLIENT("engine.dll", RuiTopologyRender, [](CModule module)
{
    (void)module;
    DISPATCH_MODULE(RuiTopologyHooks);
})

ON_DLL_LOAD_CLIENT("client.dll", RuiTopology, [](CModule module)
{
    g_HiddenTopologyMask.store(0, std::memory_order_release);
    RuiTopology_GetArgument = module.Offset(0x308A30).RCast<GetTopologyArgumentFn>();
})
