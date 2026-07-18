#pragma once

#include <functional>
#include <list>
#include <mutex>
#include <utility>

struct QueuedTasks_s
{
    unsigned int m_nDelayedFrames;
    std::function<void()> m_rFunctor;

    QueuedTasks_s(unsigned int frames, std::function<void()> functor) : m_nDelayedFrames(frames), m_rFunctor(std::move(functor))
    {
    }
};

class IFrameTask
{
  public:
    virtual ~IFrameTask() = default;

    virtual void RunFrame() = 0;
    virtual bool IsFinished() const = 0;
};

class CFrameTask : public IFrameTask
{
  public:
    void RunFrame() override;
    bool IsFinished() const override
    {
        return false;
    }

    void Dispatch(std::function<void()> functor, unsigned int delayedFrames = 0);

  private:
    std::mutex m_Mutex;
    std::list<QueuedTasks_s> m_QueuedTasks;
};

extern CFrameTask g_TaskQueue;

void RunFrameTasks();
void RunInMainThread(std::function<void()> functor);
