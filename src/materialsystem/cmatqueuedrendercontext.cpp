#include "materialsystem/cmatqueuedrendercontext.h"

#include "dedicated/dedicated.h"
#include "tier0/frametask.h"
#include "tier0/module.h"

#include <cstdint>
#include <memory>
#include <utility>

void (*QueueMaterialSystemRenderThreadCallback)(MaterialRenderCallback_t, uint64_t, uint32_t, uint32_t, uint64_t);
void (*FlushMaterialSystemRenderCommands)();
int (*GetMaterialThreadIndex)();

bool ThreadInRenderThread()
{
    return GetMaterialThreadIndex && GetMaterialThreadIndex() == 1;
}

uint64_t RunMaterialTask(uint64_t argument, uint32_t, uint32_t, uint64_t)
{
    const std::unique_ptr<std::function<void()>> functor(reinterpret_cast<std::function<void()>*>(argument));
    (*functor)();
    return 0;
}

void RunInRenderThread(std::function<void()> functor)
{
    if (!functor || IsDedicatedServer())
        return;
    if (ThreadInRenderThread())
    {
        functor();
        return;
    }

    g_TaskQueue.Dispatch([functor = std::move(functor)]() mutable
    {
        auto* task = new std::function<void()>(std::move(functor));
        QueueMaterialSystemRenderThreadCallback(RunMaterialTask, reinterpret_cast<uint64_t>(task), 0, 0, 0);
    });
}

ON_DLL_LOAD_CLIENT("materialsystem_dx11.dll", MaterialRenderQueue, [](CModule module)
{
    QueueMaterialSystemRenderThreadCallback = module.Offset(0x88D50).RCast<decltype(QueueMaterialSystemRenderThreadCallback)>();
    FlushMaterialSystemRenderCommands = module.Offset(0x87D00).RCast<decltype(FlushMaterialSystemRenderCommands)>();
    GetMaterialThreadIndex = module.Offset(0x874E0).RCast<decltype(GetMaterialThreadIndex)>();
})
