#include "downloader/download_limiter.h"

namespace downloader {

DownloadLimiter::DownloadLimiter(std::size_t maxConcurrent)
    : m_availableSlots(maxConcurrent) {
}

void DownloadLimiter::acquire() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_slotFreed.wait(lock, [this] { return m_availableSlots > 0; });
    --m_availableSlots;
}

bool DownloadLimiter::tryAcquire() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_availableSlots == 0) {
        return false;
    }
    --m_availableSlots;
    return true;
}

void DownloadLimiter::release() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_availableSlots;
    }
    m_slotFreed.notify_one();
}

std::size_t DownloadLimiter::available() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_availableSlots;
}

DownloadLimiter::Permit::Permit(DownloadLimiter& limiter)
    : m_limiter(limiter) {
    m_limiter.acquire();
}

DownloadLimiter::Permit::~Permit() {
    m_limiter.release();
}

}  // namespace downloader
