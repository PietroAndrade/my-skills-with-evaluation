#ifndef DOWNLOADER_DOWNLOAD_LIMITER_H
#define DOWNLOADER_DOWNLOAD_LIMITER_H

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace downloader {

class DownloadLimiter final {
public:
    explicit DownloadLimiter(std::size_t maxConcurrent = 4);

    DownloadLimiter(const DownloadLimiter&) = delete;
    DownloadLimiter& operator=(const DownloadLimiter&) = delete;

    void acquire();
    bool tryAcquire();
    void release();

    std::size_t available() const;

    class Permit final {
    public:
        explicit Permit(DownloadLimiter& limiter);
        ~Permit();

        Permit(const Permit&) = delete;
        Permit& operator=(const Permit&) = delete;

    private:
        DownloadLimiter& m_limiter;
    };

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_slotFreed;
    std::size_t m_availableSlots;
};

}  // namespace downloader

#endif  // DOWNLOADER_DOWNLOAD_LIMITER_H
