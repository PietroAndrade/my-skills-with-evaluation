#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

namespace {

int processItem(int item)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return item * item;
}

} // namespace

int main()
{
    const std::vector<int> items{1, 2, 3, 4, 5, 6, 7, 8};

    const auto start = std::chrono::steady_clock::now();

    std::vector<std::future<int>> futures;
    futures.reserve(items.size());

    for (const auto& item : items) {
        futures.push_back(std::async(std::launch::async, processItem, item));
    }

    std::vector<int> results;
    results.reserve(futures.size());

    for (auto& future : futures) {
        results.push_back(future.get());
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    std::cout << "fixed: " << items.size() << " items of 200 ms took " << elapsed.count()
              << " ms\n";

    for (const auto& result : results) {
        std::cout << result << ' ';
    }
    std::cout << '\n';

    return 0;
}
