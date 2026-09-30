#if !defined(INTERFACE_EVENT_BUS_H)
#define INTERFACE_EVENT_BUS_H

#include <string>

namespace reporter {

class IEventBus
{
public:
    virtual ~IEventBus() = default;

    virtual void publish(const std::string& topic, const std::string& payload) = 0;
    virtual void subscribe(const std::string& topic) = 0;
};

} // namespace reporter

#endif // INTERFACE_EVENT_BUS_H
