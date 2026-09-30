#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace filter
{

using json = nlohmann::json;

struct FilterRule
{
    std::string id;
    std::vector<std::string> blockedHosts;
    std::optional<std::string> comment;

    friend void to_json(json& j, const FilterRule& r);
    friend void from_json(const json& j, FilterRule& r);
};

} // namespace filter
