# Matches Classification and Routing

This file consolidates matches for address type classification, interface groups, IP ranges, and reverse path filtering.

---

# Reference: addrtype

## Type
- `match`

## Purpose
Match packets based on the type of their source or destination address, as classified by the kernel routing table (e.g., `LOCAL`, `UNICAST`, `BROADCAST`, `MULTICAST`, `UNREACHABLE`).

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A INPUT -m addrtype --dst-type LOCAL -j ACCEPT
```

## Available options / matches
- `--src-type type`
- `--dst-type type`
- `--limit-iface-in`
- `--limit-iface-out`

## Use cases by purpose
- `routing-control`
- `loopback-filtering`
- `anti-spoofing`
- `multicast-policy`

## Command examples
```bash
iptables -A INPUT -m addrtype --dst-type LOCAL -j ACCEPT
iptables -A INPUT -m addrtype --src-type BROADCAST -j DROP
iptables -A FORWARD -m addrtype --dst-type LOCAL -j DROP
iptables -A INPUT -m addrtype --dst-type MULTICAST -j ACCEPT
```

## Dependencies
- depends on the kernel routing table for address classification

## Limitations and considerations
- classification depends on the routing table state at the time of packet evaluation
- `--limit-iface-in` / `--limit-iface-out` restrict evaluation to the incoming or outgoing interface

## Relations with other extensions
- pairs well with: `conntrack`, `LOG`, `rpfilter`

## Official reference
- `iptables-extensions(8)`: section `addrtype`

---

# Reference: devgroup

## Type
- `match`

## Purpose
Match packets based on the device group of the incoming or outgoing network interface.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A INPUT -m devgroup --src-group 1 -j ACCEPT
```

## Available options / matches
- `--src-group value[/mask]`
- `--dst-group value[/mask]`

## Use cases by purpose
- `interface-policy`
- `segmentation`
- `zoning`

## Command examples
```bash
iptables -A INPUT -m devgroup --src-group 1 -j ACCEPT
iptables -A FORWARD -m devgroup --dst-group 2 -j DROP
```

## Dependencies
- interface groups must be configured in the system (via `ip link set dev <iface> group <id>`)

## Limitations and considerations
- requires pre-configuration of device groups
- useful in environments with many interfaces that share a common policy

## Relations with other extensions
- pairs well with: `conntrack`, `LOG`, `addrtype`

## Official reference
- `iptables-extensions(8)`: section `devgroup`

---

# Reference: iprange

## Type
- `match`

## Purpose
Match packets whose source or destination IP address falls within a specified range, without requiring a subnet mask.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A INPUT -m iprange --src-range 192.168.1.10-192.168.1.50 -j ACCEPT
```

## Available options / matches
- `--src-range ip[-ip]`
- `--dst-range ip[-ip]`

## Use cases by purpose
- `access-control`
- `allowlist`
- `blocklist`
- `segmentation`

## Command examples
```bash
iptables -A INPUT -m iprange --src-range 10.0.0.1-10.0.0.100 -p tcp --dport 22 -j ACCEPT
iptables -A INPUT -m iprange --src-range 192.168.1.10-192.168.1.50 -j DROP
iptables -A INPUT -m iprange --dst-range 203.0.113.1-203.0.113.10 -j LOG --log-prefix "DST-RANGE "
```

## Dependencies
- no external dependencies

## Limitations and considerations
- for large or frequently updated ranges, `set` (ipset) is more scalable
- ranges cannot be negated directly with `!` in all versions

## Relations with other extensions
- pairs well with: `tcp`, `udp`, `LOG`
- scalable alternative for large lists: `set`

## Official reference
- `iptables-extensions(8)`: section `iprange`

---

# Reference: rpfilter

## Type
- `match`

## Purpose
Perform a reverse path filter check: matches packets whose source address would be routed back through the same interface they arrived on.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any
- Typical chain: `PREROUTING` (raw table)

## Basic syntax
```bash
iptables -t raw -A PREROUTING -m rpfilter --invert -j DROP
```

## Available options / matches
- `--loose`
- `--validmark`
- `--accept-local`
- `--invert`

## Use cases by purpose
- `anti-spoofing`
- `routing-control`
- `hardening`

## Command examples
```bash
iptables -t raw -A PREROUTING -m rpfilter --invert -j DROP
ip6tables -t raw -A PREROUTING -m rpfilter --invert -j DROP
iptables -t raw -A PREROUTING -m rpfilter --invert -j LOG --log-prefix "RPF-FAIL "
iptables -t raw -A PREROUTING -m rpfilter --invert -j DROP
```

## Dependencies
- requires correct routing table configuration
- should be applied in the `raw` table, `PREROUTING` chain

## Limitations and considerations
- asymmetric routing scenarios may cause legitimate traffic to fail the check
- `--loose` relaxes the check, accepting packets if any route exists for the source
- test carefully before deploying in production

## Relations with other extensions
- pairs well with: `addrtype`, `LOG`

## Official reference
- `iptables-extensions(8)`: section `rpfilter`
