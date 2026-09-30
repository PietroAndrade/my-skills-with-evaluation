#ifndef DOWNLOADER_HPP
#define DOWNLOADER_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

struct DownloadResult final
{
    std::string url;
    std::size_t bytes = 0;
    bool succeeded = false;
    std::string error;
};

class SimulatedDownloader final
{
public:
    DownloadResult fetch(const std::string& url) const
    {
        const std::uint64_t seed = std::hash<std::string>{}(url);
        const std::chrono::milliseconds latency(50 + static_cast<long>(seed % 451));
        std::this_thread::sleep_for(latency);

        DownloadResult result;
        result.url = url;

        if (seed % 97 == 0)
        {
            result.succeeded = false;
            result.error = "simulated transport failure";
            return result;
        }

        result.bytes = static_cast<std::size_t>(1024 + (seed >> 8) % 65536);
        result.succeeded = true;
        return result;
    }
};

#endif
