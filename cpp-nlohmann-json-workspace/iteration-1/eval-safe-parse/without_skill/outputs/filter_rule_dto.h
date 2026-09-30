#pragma once

#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct FilterRule {
    std::string id;
    std::vector<std::string> blockedHosts;
    std::optional<std::string> comment;
};

void from_json(const nlohmann::json& j, FilterRule& f);
