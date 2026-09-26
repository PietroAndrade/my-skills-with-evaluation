#include "compute/parallel_sum.hpp"

#include <algorithm>
#include <future>
#include <numeric>
#include <system_error>
#include <thread>

namespace compute {
namespace {

unsigned int hardwareThreads()
{
    const unsigned int reported = std::thread::hardware_concurrency();
    return reported == 0u ? 1u : reported;
}

unsigned int depthForThreadBudget(unsigned int threadBudget)
{
    unsigned int depth = 0u;
    while ((1u << depth) < threadBudget) {
        ++depth;
    }
    return depth;
}

std::int64_t sumRange(const std::int64_t* first, std::size_t count)
{
    return std::accumulate(first, first + count, std::int64_t{0});
}

std::int64_t forkJoinSum(const std::int64_t* first,
                         std::size_t count,
                         unsigned int remainingDepth,
                         std::size_t sequentialCutoff)
{
    if (remainingDepth == 0u || count <= sequentialCutoff) {
        return sumRange(first, count);
    }

    const std::size_t half = count / 2u;

    std::future<std::int64_t> leftHalf;
    try {
        leftHalf = std::async(std::launch::async,
                              forkJoinSum,
                              first,
                              half,
                              remainingDepth - 1u,
                              sequentialCutoff);
    } catch (const std::system_error&) {
        return sumRange(first, count);
    }

    const std::int64_t rightTotal = forkJoinSum(first + half,
                                                count - half,
                                                remainingDepth - 1u,
                                                sequentialCutoff);

    return leftHalf.get() + rightTotal;
}

}

ParallelSumConfig makeDefaultParallelSumConfig()
{
    ParallelSumConfig config;
    config.maxSplitDepth = depthForThreadBudget(hardwareThreads());
    config.sequentialCutoff = std::size_t{1} << 16;
    return config;
}

std::int64_t sequentialSum(const std::vector<std::int64_t>& values)
{
    return sumRange(values.data(), values.size());
}

std::int64_t parallelSum(const std::vector<std::int64_t>& values)
{
    return parallelSum(values, makeDefaultParallelSumConfig());
}

std::int64_t parallelSum(const std::vector<std::int64_t>& values, const ParallelSumConfig& config)
{
    if (values.empty()) {
        return 0;
    }

    const std::size_t cutoff = std::max<std::size_t>(config.sequentialCutoff, 1u);
    return forkJoinSum(values.data(), values.size(), config.maxSplitDepth, cutoff);
}

}
