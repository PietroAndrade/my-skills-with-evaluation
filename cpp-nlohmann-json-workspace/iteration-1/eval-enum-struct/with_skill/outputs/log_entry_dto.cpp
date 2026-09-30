#include <logger/log_entry_dto.h>

namespace logger
{

using json = nlohmann::json;

void to_json(json& j, const LogEntry& e)
{
    j = json{
        {"level", e.level},
        {"message", e.message},
        {"timestampMs", e.timestampMs},
    };
}

void from_json(const json& j, LogEntry& e)
{
    e.level = j.value("level", LogLevel::Info);
    e.message = j.value("message", std::string{});
    e.timestampMs = j.value("timestampMs", uint64_t{0});
}

} // namespace logger
