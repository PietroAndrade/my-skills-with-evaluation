#pragma once

#include "interface_event_processor.h"
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

namespace experiments {

class IEventSink {
public:
    virtual ~IEventSink() = default;
    virtual void onEvent(const std::string& event) = 0;
};

class EventProcessor : public IEventProcessor {
public:
    explicit EventProcessor(std::shared_ptr<IEventSink> sink);

    void process(const std::string& eventName, const std::string& payload) override;
    bool isReady() const override;
    uint32_t processedCount() const override;

private:
    bool hasCapacity() const;

    std::shared_ptr<IEventSink> sink_;
    std::vector<std::string> processedEvents_;
    uint32_t processedCount_;
    bool ready_;
};

}
