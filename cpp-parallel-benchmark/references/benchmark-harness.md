# Reusable benchmark harness (C++17)

A generic timing harness that measures a callable, averages over N runs after a warm-up, and reports mean and standard deviation. Follows `docs/cpp-conventions.md` (namespace, `m_`, guard, no comments). Replace `<lib>`/`<LIB>`.

The design points that make the numbers trustworthy are baked in: a discarded warm-up run to reach steady cache state, timing that brackets *only* the measured call, `steady_clock` (monotonic — never jumps like `system_clock`), and stddev so run-to-run variance is visible instead of hidden behind a single mean.

```cpp
#ifndef <LIB>_BENCHMARK_H
#define <LIB>_BENCHMARK_H

#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <vector>

namespace <lib>
{

struct BenchmarkResult
{
    double meanSeconds;
    double stddevSeconds;
    double minSeconds;
    std::size_t runs;
};

template <typename T>
inline void doNotOptimize(const T& value)
{
    asm volatile("" : : "g"(value) : "memory");
}

class Benchmark final
{
public:
    explicit Benchmark(std::size_t runs = 30, std::size_t warmupRuns = 1)
        : m_runs(runs)
        , m_warmupRuns(warmupRuns)
    {
    }

    BenchmarkResult measure(const std::function<void()>& target) const
    {
        for (std::size_t i = 0; i < m_warmupRuns; ++i) { target(); }

        std::vector<double> samples;
        samples.reserve(m_runs);
        for (std::size_t i = 0; i < m_runs; ++i)
        {
            const auto start = std::chrono::steady_clock::now();
            target();
            const auto end = std::chrono::steady_clock::now();
            samples.push_back(std::chrono::duration<double>(end - start).count());
        }

        return summarize(samples);
    }

private:
    static BenchmarkResult summarize(const std::vector<double>& samples)
    {
        double sum = 0.0;
        double minimum = samples.front();
        for (double sample : samples)
        {
            sum += sample;
            if (sample < minimum) { minimum = sample; }
        }
        const double mean = sum / samples.size();

        double variance = 0.0;
        for (double sample : samples) { variance += (sample - mean) * (sample - mean); }
        variance /= samples.size();

        return BenchmarkResult{mean, std::sqrt(variance), minimum, samples.size()};
    }

    std::size_t m_runs;
    std::size_t m_warmupRuns;
};

inline double speedup(const BenchmarkResult& sequential, const BenchmarkResult& parallel)
{
    return sequential.meanSeconds / parallel.meanSeconds;
}

inline double efficiency(double measuredSpeedup, std::size_t processors)
{
    return measuredSpeedup / static_cast<double>(processors);
}

} // namespace <lib>

#endif /* <LIB>_BENCHMARK_H */
```

Usage — compare a sequential and a parallel implementation, and **verify they produce the same result** before trusting the speedup (a faster wrong answer is not a speedup):

```cpp
<lib>::Benchmark benchmark(30);

const auto sequentialResult = runSequential(input);
const auto parallelResult   = runParallel(input);
if (sequentialResult != parallelResult) { /* report mismatch, abort — the comparison is meaningless */ }

const auto sequential = benchmark.measure([&] { runSequential(input); });
const auto parallel   = benchmark.measure([&] { runParallel(input); });

const double s = <lib>::speedup(sequential, parallel);
const double e = <lib>::efficiency(s, std::thread::hardware_concurrency());
```

Prevent the optimizer from deleting the work: make the target's result observable, e.g. `benchmark.measure([&]{ doNotOptimize(runParallel(input)); })`, or accumulate into a `volatile` sink. Without this, an `-O2` build can elide the whole computation and report a meaningless near-zero. Always benchmark an optimized build (`-O2`/`-O3`).

If the measured function mutates its input (e.g. an in-place sort), don't reset inside the timed lambda (the reset time would be counted). Use the two-callable form — a `reset` that restores a fresh copy outside the clock, and a `target` that runs inside it:

```cpp
BenchmarkResult measure(const std::function<void()>& reset,
                        const std::function<void()>& target) const
{
    for (std::size_t i = 0; i < m_warmupRuns; ++i) { reset(); target(); }

    std::vector<double> samples;
    samples.reserve(m_runs);
    for (std::size_t i = 0; i < m_runs; ++i)
    {
        reset();
        const auto start = std::chrono::steady_clock::now();
        target();
        const auto end = std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double>(end - start).count());
    }
    return summarize(samples);
}
```
