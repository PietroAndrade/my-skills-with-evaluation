#include <chrono>
#include <cstddef>
#include <exception>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace parallel {

template <typename Item, typename Task>
auto transform(const std::vector<Item>& items, Task task)
    -> std::vector<std::invoke_result_t<Task, const Item&>>
{
    using Result = std::invoke_result_t<Task, const Item&>;

    std::vector<std::future<Result>> futures;
    futures.reserve(items.size());

    for (const auto& item : items) {
        futures.push_back(std::async(std::launch::async, task, std::cref(item)));
    }

    std::vector<Result> results;
    results.reserve(futures.size());

    for (auto& future : futures) {
        results.push_back(future.get());
    }

    return results;
}

template <typename Item, typename Task>
struct Outcome final
{
    Item item;
    std::invoke_result_t<Task, const Item&> value{};
    std::string error;

    bool succeeded() const { return error.empty(); }
};

template <typename Item, typename Task>
std::vector<Outcome<Item, Task>> transformCollectingErrors(const std::vector<Item>& items,
                                                           Task task)
{
    using Result = std::invoke_result_t<Task, const Item&>;

    std::vector<std::future<Result>> futures;
    futures.reserve(items.size());

    for (const auto& item : items) {
        futures.push_back(std::async(std::launch::async, task, std::cref(item)));
    }

    std::vector<Outcome<Item, Task>> outcomes;
    outcomes.reserve(futures.size());

    for (std::size_t index = 0; index < futures.size(); ++index) {
        Outcome<Item, Task> outcome{items[index], {}, {}};
        try {
            outcome.value = futures[index].get();
        }
        catch (const std::exception& error) {
            outcome.error = error.what();
        }
        catch (...) {
            outcome.error = "unknown exception";
        }
        outcomes.push_back(std::move(outcome));
    }

    return outcomes;
}

} // namespace parallel

namespace {

int processItem(const int& item)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (item == 5) {
        throw std::runtime_error("item 5 is poisoned");
    }
    return item * item;
}

} // namespace

int main()
{
    const std::vector<int> items{1, 2, 3, 4, 5, 6, 7, 8};

    const auto start = std::chrono::steady_clock::now();
    const auto outcomes = parallel::transformCollectingErrors(items, processItem);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    std::cout << "fan-out/gather took " << elapsed.count() << " ms\n";

    for (const auto& outcome : outcomes) {
        if (outcome.succeeded()) {
            std::cout << outcome.item << " -> " << outcome.value << '\n';
        }
        else {
            std::cout << outcome.item << " -> failed: " << outcome.error << '\n';
        }
    }

    return 0;
}
