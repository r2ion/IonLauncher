#pragma once

#include <cstdint>
#include <functional>

using MaterialRenderCallback_t = uint64_t (*)(uint64_t, uint32_t, uint32_t, uint64_t);
extern void (*QueueMaterialSystemRenderThreadCallback)(MaterialRenderCallback_t, uint64_t, uint32_t, uint32_t, uint64_t);
extern void (*FlushMaterialSystemRenderCommands)();

bool ThreadInRenderThread();
void RunInRenderThread(std::function<void()> functor);
