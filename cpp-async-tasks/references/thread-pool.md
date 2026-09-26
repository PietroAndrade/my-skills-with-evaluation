# C++17 thread pool

C++17 has no standard thread pool (`std::async` doesn't pool — see the SKILL for its trap). The course uses `boost::asio::thread_pool`; when Boost isn't available, generate this dependency-free pool. It's a fixed set of worker threads draining a task queue — the producer-consumer topology specialized to `std::function<void()>` tasks. If the project already has the `cpp-producer-consumer` `Queue`, build on that instead of duplicating it.

Follows `docs/cpp-conventions.md` (namespace, `m_`, guard, `final`, no comments). Replace `<lib>`/`<LIB>`.

`submit` returns a `std::future` so callers can retrieve results and exceptions, using `std::packaged_task` to bind the task to its future.

```cpp
#ifndef <LIB>_THREAD_POOL_H
#define <LIB>_THREAD_POOL_H

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace <lib>
{

class ThreadPool final
{
public:
    explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency())
    {
        const std::size_t count = threadCount == 0 ? 1 : threadCount;
        m_workers.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            m_workers.emplace_back([this] { workerLoop(); });
        }
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        m_condition.notify_all();
        for (std::thread& worker : m_workers)
        {
            if (worker.joinable()) { worker.join(); }
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename Callable>
    auto submit(Callable&& task) -> std::future<decltype(task())>
    {
        using ResultType = decltype(task());
        auto packaged = std::make_shared<std::packaged_task<ResultType()>>(std::forward<Callable>(task));
        std::future<ResultType> result = packaged->get_future();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping) { throw std::runtime_error("submit on stopped ThreadPool"); }
            m_tasks.emplace([packaged] { (*packaged)(); });
        }
        m_condition.notify_one();
        return result;
    }

private:
    void workerLoop()
    {
        while (true)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this] { return m_stopping || !m_tasks.empty(); });
                if (m_stopping && m_tasks.empty()) { return; }
                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
            task();
        }
    }

    std::mutex                        m_mutex;
    std::condition_variable           m_condition;
    std::queue<std::function<void()>> m_tasks;
    std::vector<std::thread>          m_workers;
    bool                              m_stopping{false};
};

} // namespace <lib>

#endif /* <LIB>_THREAD_POOL_H */
```

Usage:

```cpp
<lib>::ThreadPool pool(4);
std::vector<std::future<int>> results;
for (int i = 0; i < 100; ++i)
{
    results.push_back(pool.submit([i] { return process(i); }));
}
int total = 0;
for (std::future<int>& result : results) { total += result.get(); }
```

Correctness notes carried from `cpp-producer-consumer`: predicate on `wait`, `notify_one` per task / `notify_all` on stop, the pool is non-copyable (it owns a mutex), and the destructor drains + joins so no task is lost and no thread is left running. `get()` on a future re-throws any exception the task threw.
