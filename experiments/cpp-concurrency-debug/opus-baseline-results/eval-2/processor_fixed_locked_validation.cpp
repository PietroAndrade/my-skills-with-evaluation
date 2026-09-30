#include "processor.hpp"

void Processor::process(const Job& job)
{
    const std::lock_guard<std::mutex> lock(m_mutex);

    if (!job.isValid())
    {
        return;
    }

    m_queue.push_back(job);
}
