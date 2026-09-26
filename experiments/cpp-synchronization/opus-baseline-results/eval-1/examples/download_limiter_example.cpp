#include "downloader/download_limiter.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

std::atomic<int> g_inFlight{0};
std::atomic<int> g_peakInFlight{0};

void recordPeak(int current) {
    int previousPeak = g_peakInFlight.load();
    while (current > previousPeak && !g_peakInFlight.compare_exchange_weak(previousPeak, current)) {
    }
}

void download(int id) {
    const int current = ++g_inFlight;
    recordPeak(current);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    --g_inFlight;
    if (id % 5 == 0) {
        throw std::runtime_error("download failed");
    }
}

void worker(downloader::DownloadLimiter& limiter, int id) {
    downloader::DownloadLimiter::Permit permit(limiter);
    try {
        download(id);
    } catch (const std::exception& error) {
        std::cout << "task " << id << " error: " << error.what() << '\n';
    }
}

}  // namespace

int main() {
    downloader::DownloadLimiter limiter(4);

    std::vector<std::thread> workers;
    workers.reserve(20);
    for (int id = 0; id < 20; ++id) {
        workers.emplace_back(worker, std::ref(limiter), id);
    }

    for (std::thread& thread : workers) {
        thread.join();
    }

    std::cout << "peak concurrent downloads: " << g_peakInFlight.load() << '\n';
    std::cout << "slots available at end: " << limiter.available() << '\n';
    return 0;
}
