#ifndef REPORTER_EVENT_BUS_H
#define REPORTER_EVENT_BUS_H

#include "reporter/interface_event_bus.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <zmq.hpp>

namespace reporter
{

class EventBus final : public IEventBus
{
public:
    explicit EventBus(std::shared_ptr<zmq::context_t> context);
    ~EventBus() = default;

    void publish(const std::string& topic, const std::string& payload) override;
    void subscribe(const std::string& topic) override;

private:
    std::shared_ptr<zmq::context_t>                  m_context;
    std::map<std::string, std::vector<std::string>>  m_subscribers;
};

} // namespace reporter

#endif /* REPORTER_EVENT_BUS_H */
