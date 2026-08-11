#include "rtech/rui/topology.h"

#include "rtech/rui/rui_core_types.h"
#include "tier0/module.h"
#include "vscript/squirrel/squirrel.h"

#include <array>
#include <bit>
#include <atomic>
#include <cstddef>
#include <cstdint>

using GetTopologyArgumentFn = RuiTopology* (*)(HSQUIRRELVM sqvm, int argumentIndex);

static GetTopologyArgumentFn s_GetTopologyArgument;
static std::atomic<uint64_t> s_HiddenTopologyMask = 0;
static std::array<std::atomic<RuiTopologyHandle>, RUI_TOPOLOGY_CAPACITY> s_HiddenTopologyHandles{};
static std::array<std::atomic<const RuiTopology*>, RUI_TOPOLOGY_CAPACITY> s_HiddenTopologies{};

static void RuiTopology_SetHidden(const RuiTopology* topology, bool hidden)
{
    const RuiTopologyHandle topologyHandle = topology->handle;
    const size_t topologyIndex = topologyHandle & RUI_TOPOLOGY_INDEX_MASK;
    const uint64_t topologyBit = uint64_t{1} << topologyIndex;
    if (hidden)
    {
        s_HiddenTopologies[topologyIndex].store(topology, std::memory_order_relaxed);
        s_HiddenTopologyHandles[topologyIndex].store(topologyHandle, std::memory_order_relaxed);
        s_HiddenTopologyMask.fetch_or(topologyBit, std::memory_order_release);
        return;
    }

    if (s_HiddenTopologyHandles[topologyIndex].load(std::memory_order_acquire) == topologyHandle)
        s_HiddenTopologyMask.fetch_and(~topologyBit, std::memory_order_release);
}

static RuiTopology* RuiTopology_GetFromScript(HSQUIRRELVM sqvm)
{
    if (!s_GetTopologyArgument)
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

    return s_GetTopologyArgument(sqvm, 1);
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


bool RuiTopology_IsHidden(const RuiInstance* rui) noexcept
{
    if (!rui || !rui->drawInfo)
        return false;

    uint64_t hiddenMask = s_HiddenTopologyMask.load(std::memory_order_acquire);
    while (hiddenMask != 0)
    {
        const size_t topologyIndex = std::countr_zero(hiddenMask);
        const RuiTopology* topology = s_HiddenTopologies[topologyIndex].load(std::memory_order_relaxed);
        if (topology && rui->drawInfo == &topology->drawInfo.base)
        {
            const RuiTopologyHandle hiddenHandle = s_HiddenTopologyHandles[topologyIndex].load(std::memory_order_relaxed);
            return hiddenHandle == topology->handle;
        }

        hiddenMask &= hiddenMask - 1;
    }

    return false;
}

ON_DLL_LOAD_CLIENT("client.dll", RuiTopology, [](CModule module)
{
    s_HiddenTopologyMask.store(0, std::memory_order_release);
    s_GetTopologyArgument = module.Offset(0x308A30).RCast<GetTopologyArgumentFn>();
})
