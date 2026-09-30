#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

class ThreadPool final
{
public:
    explicit ThreadPool(std::size_t threadCount)
        : m_stopping(false)
    {
        if (threadCount == 0)
        {
            threadCount = 1;
        }

        m_workers.reserve(threadCount);
        for (std::size_t i = 0; i < threadCount; ++i)
        {
            m_workers.emplace_back([this] { runWorkerLoop(); });
        }
    }

    ~ThreadPool()
    {
        shutdown();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    template <typename Fn, typename... Args>
    auto submit(Fn&& fn, Args&&... args) -> std::future<std::invoke_result_t<Fn, Args...>>
    {
        using Result = std::invoke_result_t<Fn, Args...>;

        auto boundTask = std::make_shared<std::packaged_task<Result()>>(
            [callable = std::forward<Fn>(fn),
             arguments = std::make_tuple(std::forward<Args>(args)...)]() mutable -> Result {
                return std::apply(callable, arguments);
            });

        std::future<Result> result = boundTask->get_future();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping)
            {
                throw std::runtime_error("ThreadPool::submit called after shutdown");
            }
            m_tasks.emplace([boundTask] { (*boundTask)(); });
        }

        m_pendingWork.notify_one();
        return result;
    }

    std::size_t threadCount() const
    {
        return m_workers.size();
    }

    void shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping)
            {
                return;
            }
            m_stopping = true;
        }

        m_pendingWork.notify_all();

        for (std::thread& worker : m_workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

private:
    void runWorkerLoop()
    {
        for (;;)
        {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_pendingWork.wait(lock, [this] { return m_stopping || !m_tasks.empty(); });

                if (m_tasks.empty())
                {
                    return;
                }

                task = std::move(m_tasks.front());
                m_tasks.pop();
            }

            task();
        }
    }

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_pendingWork;
    bool m_stopping;
};

#endif
