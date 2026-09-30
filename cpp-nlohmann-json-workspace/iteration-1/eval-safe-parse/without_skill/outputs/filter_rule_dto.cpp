#include "filter_rule_dto.h"

void from_json(const nlohmann::json& j, FilterRule& f) {
    f.id = j.value("id", std::string{});

    if (j.contains("blockedHosts") && j["blockedHosts"].is_array()) {
        f.blockedHosts = j["blockedHosts"].get<std::vector<std::string>>();
    }

    if (j.contains("comment") && j["comment"].is_string()) {
        f.comment = j["comment"].get<std::string>();
    }
}
