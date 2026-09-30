#pragma once

#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

namespace logger {

#define LOG_LEVEL_LIST \
    X(Trace) \
    X(Debug) \
    X(Info) \
    X(Warn) \
    X(Error)

enum class LogLevel {
#define X(name) name,
    LOG_LEVEL_LIST
#undef X
};

struct LogEntry {
    LogLevel level;
    std::string message;
    uint64_t timestampMs;

    friend void to_json(nlohmann::json& j, const LogEntry& e);
    friend void from_json(const nlohmann::json& j, LogEntry& e);
};

void to_json(nlohmann::json& j, const LogLevel& level);
void from_json(const nlohmann::json& j, LogLevel& level);

} // namespace logger
