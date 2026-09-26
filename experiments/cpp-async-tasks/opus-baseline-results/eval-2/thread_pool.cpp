#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace parallel {

class ThreadPool final
{
public:
    explicit ThreadPool(std::size_t threadCount = 0)
    {
        if (threadCount == 0) {
            threadCount = std::thread::hardware_concurrency();
        }
        if (threadCount == 0) {
            threadCount = 2;
        }

        m_workers.reserve(threadCount);
        for (std::size_t index = 0; index < threadCount; ++index) {
            m_workers.emplace_back([this] { runWorker(); });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool()
    {
        {
            const std::scoped_lock lock(m_mutex);
            m_stopping = true;
        }
        m_condition.notify_all();

        for (auto& worker : m_workers) {
            worker.join();
        }
    }

    template <typename Task, typename... Args>
    auto submit(Task&& task, Args&&... args)
        -> std::future<std::invoke_result_t<Task, Args...>>
    {
        using Result = std::invoke_result_t<Task, Args...>;

        auto packaged = std::make_shared<std::packaged_task<Result()>>(
            std::bind(std::forward<Task>(task), std::forward<Args>(args)...));

        std::future<Result> future = packaged->get_future();

        {
            const std::scoped_lock lock(m_mutex);
            m_queue.emplace([packaged] { (*packaged)(); });
        }
        m_condition.notify_one();

        return future;
    }

    std::size_t threadCount() const { return m_workers.size(); }

private:
    void runWorker()
    {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock lock(m_mutex);
                m_condition.wait(lock, [this] { return m_stopping || !m_queue.empty(); });

                if (m_queue.empty()) {
                    return;
                }

                task = std::move(m_queue.front());
                m_queue.pop();
            }
            task();
        }
    }

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_stopping = false;
};

} // namespace parallel

namespace {

int processItem(int item)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    return item * item;
}

} // namespace

int main()
{
    std::vector<int> items;
    items.reserve(2000);
    for (int value = 0; value < 2000; ++value) {
        items.push_back(value);
    }

    parallel::ThreadPool pool;

    const auto start = std::chrono::steady_clock::now();

    std::vector<std::future<int>> futures;
    futures.reserve(items.size());

    for (const auto& item : items) {
        futures.push_back(pool.submit(processItem, item));
    }

    long long total = 0;
    for (auto& future : futures) {
        total += future.get();
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    std::cout << "pool of " << pool.threadCount() << " threads ran " << items.size()
              << " short tasks in " << elapsed.count() << " ms, checksum " << total << '\n';

    return 0;
}
