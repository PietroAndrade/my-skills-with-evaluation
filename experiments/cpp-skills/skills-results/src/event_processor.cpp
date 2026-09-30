#include "experiments/event_processor.h"

namespace experiments
{

EventProcessor::EventProcessor(std::shared_ptr<IEventSink> sink)
    : m_sink(sink)
    , m_processedCount(0)
    , m_ready(true)
{
}


EventProcessor::~EventProcessor() = default;


void EventProcessor::process(const std::string& eventName, const std::string& payload)
{
    if (!hasCapacity())
    {
        return;
    }

    m_eventLog.push_back(eventName);
    m_sink->emit(eventName, payload);
    m_processedCount++;
}


bool EventProcessor::isReady() const
{
    return m_ready;
}


uint32_t EventProcessor::processedCount() const
{
    return m_processedCount;
}


bool EventProcessor::hasCapacity() const
{
    return m_ready && m_sink != nullptr;
}

} // namespace experiments
