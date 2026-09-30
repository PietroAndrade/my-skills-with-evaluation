#include "peer_dto.h"

#include <stdexcept>

namespace net {

void to_json(nlohmann::json& j, const in_addr& addr) {
    char buf[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &addr, buf, sizeof(buf))) {
        throw std::runtime_error("inet_ntop failed");
    }
    j = std::string(buf);
}

void from_json(const nlohmann::json& j, in_addr& addr) {
    const std::string& s = j.get<std::string>();
    if (inet_pton(AF_INET, s.c_str(), &addr) != 1) {
        throw std::runtime_error("inet_pton failed for: " + s);
    }
}

void to_json(nlohmann::json& j, const PeerDto& dto) {
    j["addresses"] = dto.addresses;

    nlohmann::json routes_json = nlohmann::json::object();
    for (const auto& [proto, port] : dto.routes) {
        nlohmann::json proto_key = proto;
        routes_json[proto_key.get<std::string>()] = port;
    }
    j["routes"] = routes_json;
}

void from_json(const nlohmann::json& j, PeerDto& dto) {
    j.at("addresses").get_to(dto.addresses);

    dto.routes.clear();
    for (const auto& [key, val] : j.at("routes").items()) {
        Protocol proto = nlohmann::json(key).get<Protocol>();
        Port port = val.get<Port>();
        dto.routes[proto] = port;
    }
}

} // namespace net
