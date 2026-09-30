#include "log_entry_dto.h"

#include <stdexcept>
#include <unordered_map>

namespace logger {

static const std::unordered_map<LogLevel, std::string> kLogLevelToString = {
#define X(name) {LogLevel::name, #name},
    LOG_LEVEL_LIST
#undef X
};

static const std::unordered_map<std::string, LogLevel> kStringToLogLevel = {
#define X(name) {#name, LogLevel::name},
    LOG_LEVEL_LIST
#undef X
};

void to_json(nlohmann::json& j, const LogLevel& level) {
    auto it = kLogLevelToString.find(level);
    if (it == kLogLevelToString.end()) {
        throw std::invalid_argument("Unknown LogLevel value");
    }
    j = it->second;
}

void from_json(const nlohmann::json& j, LogLevel& level) {
    const auto& str = j.get<std::string>();
    auto it = kStringToLogLevel.find(str);
    if (it == kStringToLogLevel.end()) {
        throw std::invalid_argument("Unknown LogLevel string: " + str);
    }
    level = it->second;
}

void to_json(nlohmann::json& j, const LogEntry& e) {
    j = nlohmann::json{
        {"level", e.level},
        {"message", e.message},
        {"timestampMs", e.timestampMs}
    };
}

void from_json(const nlohmann::json& j, LogEntry& e) {
    j.at("level").get_to(e.level);
    j.at("message").get_to(e.message);
    j.at("timestampMs").get_to(e.timestampMs);
}

} // namespace logger
