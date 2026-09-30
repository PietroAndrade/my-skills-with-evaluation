#pragma once

#include <gmock/gmock.h>

#include "reporter/interface_event_bus.h"

namespace reporter {

class MockEventBus : public IEventBus
{
public:
    MOCK_METHOD(void, publish, (const std::string& topic, const std::string& payload), (override));
    MOCK_METHOD(void, subscribe, (const std::string& topic), (override));
};

} // namespace reporter
