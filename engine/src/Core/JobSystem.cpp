#include "LeirEngine/Core/JobSystem.h"

#include <algorithm>

namespace Leir {

#if defined(__EMSCRIPTEN__)
// Web: single-threaded by decision (no -pthread / SharedArrayBuffer). The pool
// degrades to inline execution — same API, zero threads.
JobSystem::JobSystem(unsigned threadCount)
    : m_ThreadCount(1)
{
}

JobSystem::~JobSystem() = default;

void JobSystem::ParallelFor(size_t count, const std::function<void(size_t)>& fn)
{
    for (size_t i = 0; i < count; ++i)
        fn(i);
}

void JobSystem::Dispatch(const std::function<void()>& fn)
{
    fn();
}

void JobSystem::WaitAll() {}

#else

JobSystem::JobSystem(unsigned threadCount)
{
    unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    m_ThreadCount = threadCount == 0 ? hw : std::min(threadCount, hw);
    m_ThreadCount = std::max(1u, m_ThreadCount);
    m_Workers.reserve(m_ThreadCount);
    for (unsigned i = 0; i < m_ThreadCount; ++i)
        m_Workers.emplace_back(&JobSystem::WorkerLoop, this);
}

JobSystem::~JobSystem()
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Stop = true;
    }
    m_Cv.notify_all();
    for (auto& w : m_Workers)
        if (w.joinable())
            w.join();
}

void JobSystem::ParallelFor(size_t count, const std::function<void(size_t)>& fn)
{
    if (count == 0)
        return;
    if (m_ThreadCount <= 1) {
        for (size_t i = 0; i < count; ++i)
            fn(i);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Next.store(0, std::memory_order_relaxed);
        m_Count = count;
        m_Fn = fn;
        m_RangeActive.store(true, std::memory_order_relaxed);
    }
    m_ActiveWorkers.store(0, std::memory_order_relaxed);
    m_Cv.notify_all(); // wake workers to help with the range

    WorkerLoopRange(); // the calling thread also participates

    // Wait for every worker to finish the range.
    std::unique_lock<std::mutex> doneLock(m_DoneMutex);
    m_DoneCv.wait(doneLock, [this] {
        return m_ActiveWorkers.load(std::memory_order_relaxed) == 0;
    });
    doneLock.unlock();

    // Deactivate the range so workers stop participating.
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_RangeActive.store(false, std::memory_order_relaxed);
    m_Count = 0;
}

void JobSystem::WorkerLoopRange()
{
    // Copy the fn once (no per-index locking).
    std::function<void(size_t)> fn;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        fn = m_Fn;
    }
    for (;;) {
        const size_t idx = m_Next.fetch_add(1, std::memory_order_relaxed);
        if (idx >= m_Count)
            break;
        fn(idx);
    }
}

void JobSystem::Dispatch(const std::function<void()>& fn)
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Tasks.push(fn);
        m_Pending.fetch_add(1, std::memory_order_relaxed);
    }
    m_Cv.notify_one();
}

void JobSystem::WaitAll()
{
    std::unique_lock<std::mutex> lock(m_Mutex);
    m_DoneCv.wait(lock, [this] {
        return m_Pending.load(std::memory_order_relaxed) == 0;
    });
}

void JobSystem::WorkerLoop()
{
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Cv.wait(lock, [this] {
                return m_Stop || !m_Tasks.empty() || m_RangeActive.load(std::memory_order_relaxed);
            });
            if (m_Stop && m_Tasks.empty() && !m_RangeActive.load(std::memory_order_relaxed))
                return;
            if (m_RangeActive.load(std::memory_order_relaxed) && m_Tasks.empty()) {
                // A ParallelFor range is pending: help.
                lock.unlock();
                m_ActiveWorkers.fetch_add(1, std::memory_order_relaxed);
                WorkerLoopRange();
                if (m_ActiveWorkers.fetch_sub(1, std::memory_order_relaxed) == 1)
                    m_DoneCv.notify_one();
                continue;
            }
            if (m_Tasks.empty())
                continue;
            task = std::move(m_Tasks.front());
            m_Tasks.pop();
        }
        task();
        if (m_Pending.fetch_sub(1, std::memory_order_relaxed) == 1)
            m_DoneCv.notify_one();
    }
}

#endif // __EMSCRIPTEN__

} // namespace Leir