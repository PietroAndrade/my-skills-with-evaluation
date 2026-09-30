#include "peer_dto.h"

namespace net
{

using json = nlohmann::json;

std::string PeerDto::addrToString(const in_addr& a)
{
    char buf[INET_ADDRSTRLEN]{};
    const char* p = inet_ntop(AF_INET, &a, buf, sizeof(buf));
    return p ? std::string(p) : std::string();
}

bool PeerDto::stringToAddr(const std::string& s, in_addr& out)
{
    return inet_pton(AF_INET, s.c_str(), &out) == 1;
}

void to_json(json& j, const PeerDto& p)
{
    std::vector<std::string> addrs;
    addrs.reserve(p.addresses.size());
    for (const auto& a : p.addresses) addrs.push_back(PeerDto::addrToString(a));

    json routes = json::object();
    for (const auto& kv : p.routes)
    {
        std::string key = json(kv.first).get<std::string>();
        std::string val = json(kv.second).get<std::string>();
        routes[key] = val;
    }

    j = json{
        {"addresses", addrs},
        {"routes", routes},
    };
}

void from_json(const json& j, PeerDto& p)
{
    p.addresses.clear();
    if (j.contains("addresses") && j["addresses"].is_array())
    {
        for (const auto& s : j["addresses"])
        {
            if (!s.is_string()) continue;
            in_addr a{};
            if (PeerDto::stringToAddr(s.get<std::string>(), a)) p.addresses.push_back(a);
        }
    }

    p.routes.clear();
    if (j.contains("routes") && j["routes"].is_object())
    {
        for (json::const_iterator it = j["routes"].begin(); it != j["routes"].end(); ++it)
        {
            try
            {
                Protocol proto = json(it.key()).get<Protocol>();
                Port port = it.value().get<Port>();
                p.routes.emplace(proto, port);
            }
            catch (const std::exception&)
            {
                continue;
            }
        }
    }
}

} // namespace net
