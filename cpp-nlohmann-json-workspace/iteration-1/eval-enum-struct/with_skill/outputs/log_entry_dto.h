#pragma once

#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

namespace logger
{

using json = nlohmann::json;

enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warn,
    Error,
};

NLOHMANN_JSON_SERIALIZE_ENUM(LogLevel, {
    {LogLevel::Trace, "Trace"},
    {LogLevel::Debug, "Debug"},
    {LogLevel::Info, "Info"},
    {LogLevel::Warn, "Warn"},
    {LogLevel::Error, "Error"},
})

struct LogEntry
{
    LogLevel level{LogLevel::Info};
    std::string message;
    uint64_t timestampMs{0};

    friend void to_json(json& j, const LogEntry& e);
    friend void from_json(const json& j, LogEntry& e);
};

} // namespace logger
