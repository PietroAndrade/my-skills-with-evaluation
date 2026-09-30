#pragma once

#include <gmock/gmock.h>

#include "experiments/interface_event_processor.h"

namespace experiments {

class MockEventProcessor : public IEventProcessor
{
public:
    MOCK_METHOD(void, process, (const std::string& eventName, const std::string& payload), (override));
    MOCK_METHOD(bool, isReady, (), (const, override));
    MOCK_METHOD(uint32_t, processedCount, (), (const, override));
};

} // namespace experiments
