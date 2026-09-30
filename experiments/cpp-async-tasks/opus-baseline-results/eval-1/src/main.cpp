#include "compute/parallel_sum.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

namespace {

std::vector<std::int64_t> makeValues(std::size_t count)
{
    std::vector<std::int64_t> values(count);
    for (std::size_t index = 0; index < count; ++index) {
        values[index] = static_cast<std::int64_t>(index % 1000) - 500;
    }
    return values;
}

template <typename Callable>
std::pair<std::int64_t, double> timed(Callable&& callable)
{
    const auto start = std::chrono::steady_clock::now();
    const std::int64_t total = callable();
    const auto stop = std::chrono::steady_clock::now();
    return {total, std::chrono::duration<double, std::milli>(stop - start).count()};
}

}

int main(int argc, char** argv)
{
    const std::size_t count = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 200000000u;
    const std::vector<std::int64_t> values = makeValues(count);
    const compute::ParallelSumConfig config = compute::makeDefaultParallelSumConfig();

    std::cout << "elements: " << count << '\n'
              << "hardware_concurrency: " << std::thread::hardware_concurrency() << '\n'
              << "max split depth: " << config.maxSplitDepth << '\n'
              << "max concurrent workers: " << (1u << config.maxSplitDepth) << '\n';

    const auto sequential = timed([&] { return compute::sequentialSum(values); });
    const auto parallel = timed([&] { return compute::parallelSum(values, config); });

    std::cout << "sequential: " << sequential.first << " in " << sequential.second << " ms\n"
              << "parallel:   " << parallel.first << " in " << parallel.second << " ms\n"
              << "match: " << (sequential.first == parallel.first ? "yes" : "no") << '\n';

    return sequential.first == parallel.first ? 0 : 1;
}
