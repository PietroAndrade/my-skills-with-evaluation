# Matches System and Process Context

This file consolidates matches that bind packet filtering to Linux system-level identity: cgroups, CPU affinity, process ownership, and socket metadata.

---

# Reference: cgroup

## Type
- `match`

## Purpose
Match packets belonging to a specific Linux control group (cgroup), enabling per-container or per-service traffic policies based on the cgroup hierarchy.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A OUTPUT -m cgroup --cgroup 0x10001 -j MARK --set-mark 1
```

## Available options / matches
- `--cgroup classid` — numeric cgroup net_cls classid (major:minor in hex)
- `--path path` — cgroup v2 path

## Use cases by purpose
- `container-qos`
- `service-isolation`
- `policy-by-service`

## Command examples
```bash
# Mark traffic from cgroup with classid 0x10001
iptables -A OUTPUT -m cgroup --cgroup 0x10001 -j MARK --set-mark 1
# Limit outbound rate for a container cgroup
iptables -A OUTPUT -m cgroup --cgroup 0x20002 -j ACCEPT
```

## Dependencies
- requires `net_cls` cgroup subsystem enabled in kernel
- classid must be set via `/sys/fs/cgroup/net_cls/<group>/net_cls.classid`

## Limitations and considerations
- cgroup v1 uses `--cgroup classid`; cgroup v2 path matching requires a newer kernel
- classid 0 (default) matches processes not assigned to any cgroup

## Relations with other extensions
- pairs well with: `MARK`, `CLASSIFY`, `statistic`

## Official reference
- `iptables-extensions(8)`: section `cgroup`

---

# Reference: cpu

## Type
- `match`

## Purpose
Match packets based on the CPU core that is processing them, enabling per-CPU firewall rules for NUMA or load-distribution scenarios.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A INPUT -m cpu --cpu 0 -j ACCEPT
```

## Available options / matches
- `--cpu number` — CPU index (0-based)

## Use cases by purpose
- `load-distribution`
- `observability`

## Command examples
```bash
# Accept on CPU 0 only
iptables -A INPUT -m cpu --cpu 0 -j ACCEPT
# Log traffic arriving on a specific CPU
iptables -A INPUT -m cpu --cpu 3 -j LOG --log-prefix "CPU3-SEEN "
```

## Dependencies
- requires `xt_cpu` kernel module

## Limitations and considerations
- CPU affinity can change with RPS/RFS; rules relying on this may behave unexpectedly if RSS is reconfigured
- primarily a diagnostic and performance-tuning tool, not for security policies

## Relations with other extensions
- pairs well with: `MARK`, `statistic`

## Official reference
- `iptables-extensions(8)`: section `cpu`

---

# Reference: owner

## Type
- `match`

## Purpose
Match outgoing packets based on the UID, GID, process ID, or session ID of the socket owner — enables per-user or per-process outbound policies.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid chains: `OUTPUT` (local origination only)
- Common protocols: any

## Basic syntax
```bash
iptables -A OUTPUT -m owner --uid-owner 1000 -j ACCEPT
```

## Available options / matches
- `--uid-owner uid[:uid]` — match UID or UID range
- `--gid-owner gid[:gid]` — match GID or GID range
- `--socket-exists` — match only if a socket is attached to the packet

## Use cases by purpose
- `user-policy`
- `service-isolation`
- `access-control`

## Command examples
```bash
# Only allow HTTP from the www-data user
iptables -A OUTPUT -m owner --uid-owner www-data -p tcp --dport 80 -j ACCEPT
# Block all outbound from UID 1001
iptables -A OUTPUT -m owner --uid-owner 1001 -j DROP
# Restrict outbound DNS to root only
iptables -A OUTPUT -p udp --dport 53 -m owner --uid-owner 0 -j ACCEPT
iptables -A OUTPUT -p udp --dport 53 -j DROP
```

## Dependencies
- only works in the `OUTPUT` chain
- UID/GID must be resolved at rule creation time (numeric or name)

## Limitations and considerations
- does not work with forwarded traffic (no socket owner for routed packets)
- root-suid processes run as UID 0 even when invoked by other users
- kernel threads have no owner; `--socket-exists` handles this case

## Relations with other extensions
- related to: `cgroup`, `socket`

## Official reference
- `iptables-extensions(8)`: section `owner`

---

# Reference: socket

## Type
- `match`

## Purpose
Match packets if an established socket exists in the kernel that corresponds to the packet's flow — used in transparent proxy and policy-routing scenarios.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid chains: `PREROUTING`, `INPUT`
- Common protocols: `tcp`, `udp`

## Basic syntax
```bash
iptables -t mangle -A PREROUTING -m socket -j MARK --set-mark 1
```

## Available options / matches
- `--transparent` — only match transparent proxied sockets (`IP_TRANSPARENT`)
- `--nowildcard` — do not match wildcard (0.0.0.0) socket binds

## Use cases by purpose
- `transparent-proxy`
- `policy-routing`
- `access-control`

## Command examples
```bash
# Mark packets that belong to an existing socket for policy routing
iptables -t mangle -A PREROUTING -m socket -j MARK --set-mark 1
# Match transparent proxy sockets specifically
iptables -t mangle -A PREROUTING -p tcp -m socket --transparent -j MARK --set-mark 200
```

## Dependencies
- requires `xt_socket` kernel module
- conntrack must be loaded for full socket lookup

## Limitations and considerations
- often used with `ip rule` / `ip route` for policy routing to avoid routing loops
- `--transparent` requires `IP_TRANSPARENT` socket option set on the proxy

## Relations with other extensions
- pairs well with: `MARK`, `conntrack`
- related to: `owner`

## Official reference
- `iptables-extensions(8)`: section `socket`
