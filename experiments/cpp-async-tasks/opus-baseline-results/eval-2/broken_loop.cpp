#include <chrono>
#include <cstddef>
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

    for (const auto& item : items) {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif
        std::async(std::launch::async, processItem, item);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    std::cout << "broken: " << items.size() << " items of 200 ms took " << elapsed.count()
              << " ms (each temporary future blocked in its destructor)\n";

    return 0;
}
