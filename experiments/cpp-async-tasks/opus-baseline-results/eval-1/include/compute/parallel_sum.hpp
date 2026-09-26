#ifndef COMPUTE_PARALLEL_SUM_HPP
#define COMPUTE_PARALLEL_SUM_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace compute {

struct ParallelSumConfig {
    unsigned int maxSplitDepth = 0;
    std::size_t sequentialCutoff = 1u << 16;
};

ParallelSumConfig makeDefaultParallelSumConfig();

std::int64_t parallelSum(const std::vector<std::int64_t>& values);

std::int64_t parallelSum(const std::vector<std::int64_t>& values, const ParallelSumConfig& config);

std::int64_t sequentialSum(const std::vector<std::int64_t>& values);

}

#endif
