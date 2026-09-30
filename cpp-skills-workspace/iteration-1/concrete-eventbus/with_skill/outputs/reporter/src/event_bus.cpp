#include "reporter/event_bus.h"

namespace reporter
{

EventBus::EventBus(std::shared_ptr<zmq::context_t> context)
    : m_context(context)
{
}


EventBus::~EventBus() = default;


void EventBus::publish(const std::string& topic, const std::string& payload)
{
    zmq::socket_t socket(*m_context, ZMQ_PUB);
    socket.bind(topic);

    zmq::message_t message(payload.size());
    memcpy(message.data(), payload.data(), payload.size());
    socket.send(message);
}


void EventBus::subscribe(const std::string& topic)
{
    m_subscribers[topic].push_back(topic);
}

} // namespace reporter
