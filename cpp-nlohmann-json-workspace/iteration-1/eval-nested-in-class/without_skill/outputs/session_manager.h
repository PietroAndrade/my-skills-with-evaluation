#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace session {

class SessionManager {
public:
    enum class Mode {
        ReadOnly,
        ReadWrite,
        Admin
    };

    struct SessionInfo {
        std::string id;
        Mode mode;
        int ttlSeconds;
    };
};

NLOHMANN_JSON_SERIALIZE_ENUM(SessionManager::Mode, {
    {SessionManager::Mode::ReadOnly,  "read_only"},
    {SessionManager::Mode::ReadWrite, "read_write"},
    {SessionManager::Mode::Admin,     "admin"},
})

inline void to_json(nlohmann::json& j, const SessionManager::SessionInfo& s) {
    j = nlohmann::json{
        {"id",         s.id},
        {"mode",       s.mode},
        {"ttlSeconds", s.ttlSeconds}
    };
}

inline void from_json(const nlohmann::json& j, SessionManager::SessionInfo& s) {
    j.at("id").get_to(s.id);
    j.at("mode").get_to(s.mode);
    j.at("ttlSeconds").get_to(s.ttlSeconds);
}

} // namespace session
