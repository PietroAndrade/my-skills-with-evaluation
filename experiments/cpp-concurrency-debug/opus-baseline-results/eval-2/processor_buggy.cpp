#include "processor.hpp"

void Processor::process(const Job& job)
{
    m_mutex.lock();
    if (!job.isValid())
    {
        return;
    }
    m_queue.push_back(job);
    m_mutex.unlock();
}
