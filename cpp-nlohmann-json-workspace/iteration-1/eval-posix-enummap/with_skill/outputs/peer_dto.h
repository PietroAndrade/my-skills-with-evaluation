#pragma once

#include <arpa/inet.h>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

namespace net
{

using json = nlohmann::json;

enum class Protocol
{
    tcp,
    udp,
    icmp,
};

NLOHMANN_JSON_SERIALIZE_ENUM(Protocol, {
    {Protocol::tcp,  "tcp"},
    {Protocol::udp,  "udp"},
    {Protocol::icmp, "icmp"},
})

enum class Port
{
    http,
    https,
};

NLOHMANN_JSON_SERIALIZE_ENUM(Port, {
    {Port::http,  "80"},
    {Port::https, "443"},
})

struct PeerDto
{
    std::vector<in_addr> addresses;
    std::map<Protocol, Port> routes;

    static std::string addrToString(const in_addr& a);
    static bool stringToAddr(const std::string& s, in_addr& out);

    friend void to_json(json& j, const PeerDto& p);
    friend void from_json(const json& j, PeerDto& p);
};

} // namespace net
