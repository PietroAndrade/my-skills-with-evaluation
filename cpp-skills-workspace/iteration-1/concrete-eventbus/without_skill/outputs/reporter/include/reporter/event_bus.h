#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <zmq.hpp>

#include "reporter/interface_event_bus.h"

namespace reporter {

class EventBus final : public IEventBus
{
public:
    explicit EventBus(std::shared_ptr<zmq::context_t> context);
    ~EventBus() override = default;

    void publish(const std::string& topic, const std::string& payload) override;
    void subscribe(const std::string& topic) override;

private:
    std::shared_ptr<zmq::context_t> context;
    std::map<std::string, std::vector<std::string>> subscribers_by_topic;
};

} // namespace reporter
