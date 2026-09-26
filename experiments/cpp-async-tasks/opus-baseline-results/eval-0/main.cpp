#include "downloader.hpp"
#include "thread_pool.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{

constexpr std::size_t kUrlCount = 200;
constexpr std::size_t kMaxConcurrentDownloads = 32;

std::vector<std::string> buildUrlList()
{
    std::vector<std::string> urls;
    urls.reserve(kUrlCount);
    for (std::size_t i = 0; i < kUrlCount; ++i)
    {
        urls.push_back("https://example.com/resource/" + std::to_string(i));
    }
    return urls;
}

std::size_t chooseThreadCount(std::size_t taskCount)
{
    const std::size_t hardwareHint = std::max<std::size_t>(std::thread::hardware_concurrency(), 1);
    const std::size_t ioOriented = std::max(kMaxConcurrentDownloads, hardwareHint * 4);
    return std::min(ioOriented, taskCount);
}

} // namespace

int main()
{
    const std::vector<std::string> urls = buildUrlList();
    const SimulatedDownloader downloader;

    const auto started = std::chrono::steady_clock::now();

    ThreadPool pool(chooseThreadCount(urls.size()));

    std::vector<std::future<DownloadResult>> pending;
    pending.reserve(urls.size());

    for (const std::string& url : urls)
    {
        pending.push_back(pool.submit([&downloader, url] { return downloader.fetch(url); }));
    }

    std::size_t totalBytes = 0;
    std::size_t successCount = 0;
    std::size_t failureCount = 0;

    for (std::future<DownloadResult>& future : pending)
    {
        try
        {
            const DownloadResult result = future.get();
            if (result.succeeded)
            {
                totalBytes += result.bytes;
                ++successCount;
            }
            else
            {
                ++failureCount;
                std::cerr << "failed: " << result.url << " (" << result.error << ")\n";
            }
        }
        catch (const std::exception& error)
        {
            ++failureCount;
            std::cerr << "threw: " << error.what() << "\n";
        }
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);

    std::cout << "workers:      " << pool.threadCount() << "\n"
              << "downloads ok: " << successCount << "\n"
              << "downloads ko: " << failureCount << "\n"
              << "total bytes:  " << totalBytes << "\n"
              << "elapsed ms:   " << elapsed.count() << "\n";

    return 0;
}
