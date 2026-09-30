#include "reporter/event_bus.h"

namespace reporter {

EventBus::EventBus(std::shared_ptr<zmq::context_t> context)
    : context(std::move(context))
{
}

void EventBus::publish(const std::string& topic, const std::string& payload)
{
    zmq::socket_t socket(*context, ZMQ_PUB);
    socket.bind(topic);

    zmq::message_t message(payload.size());
    memcpy(message.data(), payload.data(), payload.size());
    socket.send(message);
}

void EventBus::subscribe(const std::string& topic)
{
    subscribers_by_topic[topic];
}

} // namespace reporter
