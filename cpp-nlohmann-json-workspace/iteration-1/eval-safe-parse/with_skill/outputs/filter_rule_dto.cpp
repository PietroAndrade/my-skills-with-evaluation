#include "filter_rule_dto.h"

namespace filter
{

using json = nlohmann::json;

void to_json(json& j, const FilterRule& r)
{
    j = json{
        {"id", r.id},
        {"blockedHosts", r.blockedHosts},
    };
    if (r.comment.has_value())
        j["comment"] = r.comment.value();
}

void from_json(const json& j, FilterRule& r)
{
    r.id = j.value("id", std::string{});
    r.blockedHosts.clear();
    if (j.contains("blockedHosts") && j["blockedHosts"].is_array())
    {
        for (const auto& s : j["blockedHosts"])
        {
            if (!s.is_string()) continue;
            r.blockedHosts.push_back(s.get<std::string>());
        }
    }
    if (j.contains("comment") && j["comment"].is_string())
        r.comment = j["comment"].get<std::string>();
    else
        r.comment = std::nullopt;
}

} // namespace filter
