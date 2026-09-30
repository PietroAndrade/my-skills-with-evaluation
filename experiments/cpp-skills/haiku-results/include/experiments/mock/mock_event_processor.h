#pragma once

#include "../interface_event_processor.h"
#include <gmock/gmock.h>
#include <string>
#include <cstdint>

namespace experiments {

class MockEventProcessor : public IEventProcessor {
public:
    MOCK_METHOD(void, process, (const std::string& eventName, const std::string& payload), (override));
    MOCK_METHOD(bool, isReady, (), (const, override));
    MOCK_METHOD(uint32_t, processedCount, (), (const, override));
};

}
