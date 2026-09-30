#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace session
{

using json = nlohmann::json;

class SessionManager
{
public:
    enum class Mode
    {
        ReadOnly,
        ReadWrite,
        Admin,
    };

    struct SessionInfo
    {
        std::string id;
        Mode mode{Mode::ReadOnly};
        int ttlSeconds{0};
    };
};

NLOHMANN_JSON_SERIALIZE_ENUM(SessionManager::Mode, {
    {SessionManager::Mode::ReadOnly,  "read_only"},
    {SessionManager::Mode::ReadWrite, "read_write"},
    {SessionManager::Mode::Admin,     "admin"},
})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SessionManager::SessionInfo, id, mode, ttlSeconds)

} // namespace session
