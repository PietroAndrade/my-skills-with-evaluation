#pragma once

#include <string>
#include <cstdint>

namespace experiments {

class IEventProcessor {
public:
    virtual ~IEventProcessor() = default;

    virtual void process(const std::string& eventName, const std::string& payload) = 0;
    virtual bool isReady() const = 0;
    virtual uint32_t processedCount() const = 0;
};

}
