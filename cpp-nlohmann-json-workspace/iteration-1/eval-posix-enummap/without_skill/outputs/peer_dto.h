#pragma once

#include <map>
#include <vector>
#include <arpa/inet.h>
#include <nlohmann/json.hpp>

namespace net {

enum class Protocol {
    tcp,
    udp,
    icmp
};

enum class Port {
    http,
    https
};

struct PeerDto {
    std::vector<in_addr> addresses;
    std::map<Protocol, Port> routes;
};

NLOHMANN_JSON_SERIALIZE_ENUM(Protocol, {
    {Protocol::tcp,  "tcp"},
    {Protocol::udp,  "udp"},
    {Protocol::icmp, "icmp"}
})

NLOHMANN_JSON_SERIALIZE_ENUM(Port, {
    {Port::http,  "80"},
    {Port::https, "443"}
})

void to_json(nlohmann::json& j, const in_addr& addr);
void from_json(const nlohmann::json& j, in_addr& addr);
void to_json(nlohmann::json& j, const PeerDto& dto);
void from_json(const nlohmann::json& j, PeerDto& dto);

} // namespace net
