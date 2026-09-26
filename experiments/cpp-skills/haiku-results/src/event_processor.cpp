#include "experiments/event_processor.h"

namespace experiments {

EventProcessor::EventProcessor(std::shared_ptr<IEventSink> sink)
    : sink_(sink), processedCount_(0), ready_(true) {}

void EventProcessor::process(const std::string& eventName, const std::string& payload) {
    if (!isReady() || !hasCapacity()) {
        return;
    }

    processedEvents_.push_back(eventName);
    ++processedCount_;

    if (sink_) {
        sink_->onEvent(eventName);
    }
}

bool EventProcessor::isReady() const {
    return ready_;
}

uint32_t EventProcessor::processedCount() const {
    return processedCount_;
}

bool EventProcessor::hasCapacity() const {
    const uint32_t maxCapacity = 1000;
    return processedCount_ < maxCapacity;
}

}
