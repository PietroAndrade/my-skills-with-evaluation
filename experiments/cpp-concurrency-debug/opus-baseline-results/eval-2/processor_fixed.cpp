#include "processor.hpp"

void Processor::process(const Job& job)
{
    if (!job.isValid())
    {
        return;
    }

    const std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push_back(job);
}
