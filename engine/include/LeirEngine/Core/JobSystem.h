#pragma once

#include "LeirEngine/Core/Export.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Leir {

// Lightweight thread pool (Fase 2, TODO_HYBRID_ECS.md §6). ParallelFor splits a
// range across the workers; Dispatch/WaitAll run arbitrary tasks and block until
// drained. Single-threaded on web (__EMSCRIPTEN__) — the engine's web decision
// keeps wasm single-threaded, so the pool degrades to inline execution.
class LEIR_API JobSystem {
public:
    explicit JobSystem(unsigned threadCount = 0);
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    unsigned ThreadCount() const { return m_ThreadCount; }

    // fn(index) for every index in [0, count). Blocks until the loop is done.
    void ParallelFor(size_t count, const std::function<void(size_t)>& fn);

    // Queue a task; WaitAll() blocks until all dispatched tasks finish.
    void Dispatch(const std::function<void()>& fn);
    void WaitAll();

private:
    void WorkerLoop();
    void WorkerLoopRange();

    unsigned m_ThreadCount;
    std::vector<std::thread> m_Workers;
    std::mutex m_Mutex;
    std::condition_variable m_Cv;
    std::queue<std::function<void()>> m_Tasks;
    bool m_Stop = false;
    std::atomic<size_t> m_Pending{0};

    // ParallelFor shared state.
    std::atomic<size_t> m_Next{0};
    size_t m_Count = 0;
    std::function<void(size_t)> m_Fn;
    std::atomic<bool> m_RangeActive{false};
    std::atomic<unsigned> m_ActiveWorkers{0};
    std::condition_variable m_DoneCv;
    std::mutex m_DoneMutex;
};

} // namespace Leir