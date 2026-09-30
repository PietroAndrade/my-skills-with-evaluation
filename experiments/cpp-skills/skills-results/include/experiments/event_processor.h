#ifndef EXPERIMENTS_EVENT_PROCESSOR_H
#define EXPERIMENTS_EVENT_PROCESSOR_H

#include "experiments/interface_event_processor.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace experiments
{

class IEventSink;

class EventProcessor final : public IEventProcessor
{
public:
    explicit EventProcessor(std::shared_ptr<IEventSink> sink);
    ~EventProcessor() = default;

    void process(const std::string& eventName, const std::string& payload) override;
    bool isReady() const override;
    uint32_t processedCount() const override;

#ifdef UNIT_TEST
    void setSink(const std::shared_ptr<IEventSink>& sink) {
        m_sink = sink;
    }
#endif

private:
    bool hasCapacity() const;

    std::shared_ptr<IEventSink> m_sink;
    std::vector<std::string>    m_eventLog;
    uint32_t                    m_processedCount;
    bool                        m_ready;
};

} // namespace experiments

#endif /* EXPERIMENTS_EVENT_PROCESSOR_H */
