#ifndef ROUTER_ROUTE_TABLE_H
#define ROUTER_ROUTE_TABLE_H

#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace router
{

class RouteTable final
{
public:
    RouteTable() = default;
    ~RouteTable() = default;

    RouteTable(const RouteTable&) = delete;
    RouteTable& operator=(const RouteTable&) = delete;
    RouteTable(RouteTable&&) = delete;
    RouteTable& operator=(RouteTable&&) = delete;

    bool lookup(const std::string& name, std::string& destination) const;

    bool contains(const std::string& name) const;

    void upsert(const std::string& name, const std::string& destination);

    bool remove(const std::string& name);

    void clear();

    std::size_t size() const;

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, std::string> m_routes;
};

} // namespace router

#endif // ROUTER_ROUTE_TABLE_H
