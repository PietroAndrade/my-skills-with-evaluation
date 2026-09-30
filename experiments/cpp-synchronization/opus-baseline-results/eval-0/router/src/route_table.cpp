#include "router/route_table.h"

namespace router
{

bool RouteTable::lookup(const std::string& name, std::string& destination) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    const auto it = m_routes.find(name);
    if (it == m_routes.end())
    {
        return false;
    }

    destination = it->second;
    return true;
}

bool RouteTable::contains(const std::string& name) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_routes.find(name) != m_routes.end();
}

void RouteTable::upsert(const std::string& name, const std::string& destination)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_routes[name] = destination;
}

bool RouteTable::remove(const std::string& name)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    return m_routes.erase(name) > 0;
}

void RouteTable::clear()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_routes.clear();
}

std::size_t RouteTable::size() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_routes.size();
}

} // namespace router
