# Targets NAT and Address Translation

This file covers all NAT-related targets: DNAT, DNPT, NETMAP, SAME, SNAT, and SNPT.

---

# Target: DNAT

## Type
- `target`

## Purpose
Rewrite the destination IP address (and optionally port) of packets — the primary target for port forwarding, load balancing, and transparent proxying.

## Scope
- Tool: `iptables`
- Valid tables: `nat`
- Valid chains: `PREROUTING`, `OUTPUT`
- Terminating: yes

## Basic syntax
```bash
iptables -t nat -A PREROUTING -p tcp --dport 80 -j DNAT --to-destination 10.0.0.5:8080
```

## Available options
- `--to-destination addr[:port[-port]]` — new destination; can specify port range for load balancing
- `--random` — use random port assignment
- `--persistent` — same destination for a given source (sticky sessions)

## Use cases by purpose
- `port-forwarding`
- `load-balancing`
- `transparent-proxy`

## Command examples
```bash
# Forward external port 80 to internal server
iptables -t nat -A PREROUTING -i eth0 -p tcp --dport 80 -j DNAT --to-destination 192.168.1.10:80

# Load balance across 2 servers (combined with statistic)
iptables -t nat -A PREROUTING -p tcp --dport 80 \
  -m statistic --mode nth --every 2 --packet 0 \
  -j DNAT --to-destination 10.0.0.1
iptables -t nat -A PREROUTING -p tcp --dport 80 \
  -j DNAT --to-destination 10.0.0.2

# Transparent HTTP proxy
iptables -t nat -A PREROUTING -p tcp --dport 80 -j DNAT --to-destination 127.0.0.1:3128
```

## Dependencies
- requires conntrack for reverse translation (return traffic)
- MASQUERADE or SNAT needed for return traffic if source address also needs translation

## Official reference
- `iptables-extensions(8)`: section `DNAT`

---

# Target: DNPT

## Type
- `target`

## Purpose
Perform Destination Network Prefix Translation for IPv6 — translate the destination prefix of an IPv6 packet without altering the interface ID (equivalent to IPv6 stateless DNAT for prefixes).

## Scope
- Tool: `ip6tables`
- Valid tables: `mangle`
- Valid chains: `PREROUTING`, `INPUT`
- Terminating: yes

## Basic syntax
```bash
ip6tables -t mangle -A PREROUTING -d 2001:db8:1::/48 -j DNPT --src-pfx 2001:db8:1::/48 --dst-pfx fd00::/48
```

## Available options
- `--src-pfx prefix/len` — original destination prefix
- `--dst-pfx prefix/len` — translated destination prefix

## Use cases by purpose
- `ipv6-nat`
- `prefix-translation`

## Command examples
```bash
ip6tables -t mangle -A PREROUTING -d 2001:db8:1::/48 \
  -j DNPT --src-pfx 2001:db8:1::/48 --dst-pfx fd00::/48
```

## Relations with other extensions
- IPv6 companion: `SNPT` (source prefix translation)

## Official reference
- `iptables-extensions(8)`: section `DNPT`

---

# Target: NETMAP

## Type
- `target`

## Purpose
Statically map an entire network prefix to another prefix (1:1 address mapping), changing host bits while keeping the interface ID constant. Useful for transparent address reassignment.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `nat`
- Valid chains: `PREROUTING` (DNAT), `POSTROUTING` (SNAT)
- Terminating: yes

## Basic syntax
```bash
iptables -t nat -A PREROUTING -d 1.2.3.0/24 -j NETMAP --to 10.0.0.0/24
```

## Available options
- `--to address/prefix` — the target network prefix

## Use cases by purpose
- `address-reassignment`
- `network-migration`
- `vpn-overlap`

## Command examples
```bash
# Map entire /24 to another /24 (bidirectional with two rules)
iptables -t nat -A PREROUTING -d 1.2.3.0/24 -j NETMAP --to 10.0.0.0/24
iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -j NETMAP --to 1.2.3.0/24
```

## Limitations and considerations
- only changes the network portion; host bits are preserved
- prefix lengths must match between original and target

## Official reference
- `iptables-extensions(8)`: section `NETMAP`

---

# Target: SAME

## Type
- `target`

## Purpose
Map source addresses to a pool of addresses, assigning the same translated address for all connections from the same source IP (deterministic per-source assignment).

## Scope
- Tool: `iptables`
- Valid tables: `nat`
- Valid chains: `POSTROUTING`
- Terminating: yes

## Basic syntax
```bash
iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -j SAME --to 1.2.3.0-1.2.3.3
```

## Available options
- `--to addr[-addr]` — IP range for translation pool
- `--nodst` — do not use destination address in the hash (use only source)

## Use cases by purpose
- `snat-pool`
- `deterministic-nat`

## Command examples
```bash
# Deterministic SNAT pool for /24
iptables -t nat -A POSTROUTING -s 10.0.0.0/24 \
  -j SAME --to 203.0.113.1-203.0.113.4
```

## Limitations and considerations
- largely superseded by SNAT with `--random` or `--persistent`

## Official reference
- `iptables-extensions(8)`: section `SAME`

---

# Target: SNAT

## Type
- `target`

## Purpose
Rewrite the source IP address (and optionally port) of packets leaving the firewall — the primary target for outbound NAT to enable private networks to reach the internet.

## Scope
- Tool: `iptables`
- Valid tables: `nat`
- Valid chains: `POSTROUTING`
- Terminating: yes

## Basic syntax
```bash
iptables -t nat -A POSTROUTING -o eth0 -j SNAT --to-source 203.0.113.1
```

## Available options
- `--to-source addr[-addr][:port[-port]]` — source address (and port range)
- `--random` — randomize port assignment
- `--random-fully` — fully random port assignment
- `--persistent` — same source address per client (sticky)

## Use cases by purpose
- `outbound-nat`
- `masquerade`
- `snat-pool`

## Command examples
```bash
# Basic outbound SNAT for LAN
iptables -t nat -A POSTROUTING -s 192.168.0.0/16 -o eth0 -j SNAT --to-source 203.0.113.1

# SNAT pool for multiple public IPs
iptables -t nat -A POSTROUTING -s 10.0.0.0/8 -o eth0 \
  -j SNAT --to-source 203.0.113.1-203.0.113.4

# Persistent SNAT (same public IP per client)
iptables -t nat -A POSTROUTING -o eth0 -j SNAT --to-source 203.0.113.1 --persistent
```

## Limitations and considerations
- for dynamic public IPs, use `MASQUERADE` instead
- `--persistent` uses a hash of the original source IP; does not survive connection tracking state loss

## Relations with other extensions
- dynamic alternative: `MASQUERADE`
- related to: `DNAT`, `NETMAP`

## Official reference
- `iptables-extensions(8)`: section `SNAT`

---

# Target: SNPT

## Type
- `target`

## Purpose
Perform Source Network Prefix Translation for IPv6 — translate the source prefix of an IPv6 packet while keeping the interface ID intact.

## Scope
- Tool: `ip6tables`
- Valid tables: `mangle`
- Valid chains: `POSTROUTING`, `OUTPUT`
- Terminating: yes

## Basic syntax
```bash
ip6tables -t mangle -A POSTROUTING -s fd00::/48 -j SNPT --src-pfx fd00::/48 --dst-pfx 2001:db8:1::/48
```

## Available options
- `--src-pfx prefix/len` — original source prefix
- `--dst-pfx prefix/len` — translated source prefix

## Use cases by purpose
- `ipv6-nat`
- `prefix-translation`

## Command examples
```bash
ip6tables -t mangle -A POSTROUTING -s fd00::/48 \
  -j SNPT --src-pfx fd00::/48 --dst-pfx 2001:db8:1::/48
```

## Relations with other extensions
- IPv6 companion: `DNPT`

## Official reference
- `iptables-extensions(8)`: section `SNPT`
