#if !defined(INTERFACE_EVENT_PROCESSOR_H)
#define INTERFACE_EVENT_PROCESSOR_H

#include <cstdint>
#include <string>

namespace experiments {

class IEventProcessor
{
public:
    virtual ~IEventProcessor() = default;

    virtual void process(const std::string& eventName, const std::string& payload) = 0;
    virtual bool isReady() const = 0;
    virtual uint32_t processedCount() const = 0;
};

} // namespace experiments

#endif // INTERFACE_EVENT_PROCESSOR_H
