#include <cstdio>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t kThreadCount = 8;
constexpr std::size_t kItemCount = 200000;

std::vector<double> makeInput()
{
    std::vector<double> input(kItemCount);
    for (std::size_t i = 0; i < kItemCount; ++i)
        input[i] = 1.0 / static_cast<double>(i + 1);
    return input;
}

double buggySum(const std::vector<double>& input)
{
    std::mutex mutex;
    double total = 0.0;

    std::vector<std::thread> threads;
    for (std::size_t t = 0; t < kThreadCount; ++t)
    {
        threads.emplace_back([&, t] {
            for (std::size_t i = t; i < input.size(); i += kThreadCount)
            {
                std::scoped_lock lock(mutex);
                total += input[i];
            }
        });
    }
    for (auto& thread : threads)
        thread.join();

    return total;
}

double fixedSum(const std::vector<double>& input)
{
    std::vector<double> partials(kThreadCount, 0.0);

    std::vector<std::thread> threads;
    for (std::size_t t = 0; t < kThreadCount; ++t)
    {
        threads.emplace_back([&input, &partials, t] {
            const std::size_t begin = input.size() * t / kThreadCount;
            const std::size_t end = input.size() * (t + 1) / kThreadCount;
            double local = 0.0;
            for (std::size_t i = begin; i < end; ++i)
                local += input[i];
            partials[t] = local;
        });
    }
    for (auto& thread : threads)
        thread.join();

    return std::accumulate(partials.begin(), partials.end(), 0.0);
}

} // namespace

int main()
{
    const std::vector<double> input = makeInput();

    for (int run = 0; run < 5; ++run)
        std::printf("run %d  buggy = %.17g   fixed = %.17g\n", run, buggySum(input), fixedSum(input));
}
