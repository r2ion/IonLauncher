#pragma once

#include <functional>

bool ThreadInRenderThread();
void RunInRenderThread(std::function<void()> functor);
