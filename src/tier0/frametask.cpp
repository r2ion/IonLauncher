#include "tier0/frametask.h"
#include "core/tier0.h"

CFrameTask g_TaskQueue;

void CFrameTask::RunFrame()
{
    std::list<std::function<void()>> readyTasks;
    {
        std::scoped_lock lock(m_Mutex);
        for (auto task = m_QueuedTasks.begin(); task != m_QueuedTasks.end();)
        {
            if (task->m_nDelayedFrames != 0)
            {
                --task->m_nDelayedFrames;
                ++task;
                continue;
            }

            readyTasks.push_back(std::move(task->m_rFunctor));
            task = m_QueuedTasks.erase(task);
        }
    }

    for (const std::function<void()>& task : readyTasks)
        task();
}

void CFrameTask::Dispatch(std::function<void()> functor, const unsigned int delayedFrames)
{
    std::scoped_lock lock(m_Mutex);
    m_QueuedTasks.emplace_back(delayedFrames, std::move(functor));
}

void RunFrameTasks()
{
    g_TaskQueue.RunFrame();
}

void RunInMainThread(std::function<void()> functor)
{
    if (ThreadInMainThread())
        functor();
    else
        g_TaskQueue.Dispatch(std::move(functor));
}
