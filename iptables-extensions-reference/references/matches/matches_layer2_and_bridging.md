# Matches Layer 2 and Bridging

This file covers matches that operate at Layer 2 / bridge level: MAC address matching, bridge physical device inspection, and packet type classification.

---

# Reference: mac

## Type
- `match`

## Purpose
Match packets by source MAC address, enabling Layer 2 access control when the firewall is also a bridge or when filtering traffic on the same broadcast domain.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid chains: `PREROUTING`, `FORWARD`, `INPUT` (only packets arriving on a local interface)
- Common protocols: any (Ethernet)

## Basic syntax
```bash
iptables -A INPUT -m mac --mac-source 00:11:22:33:44:55 -j ACCEPT
```

## Available options / matches
- `--mac-source address` — source MAC address in `HH:HH:HH:HH:HH:HH` format
- `[!]` negation supported

## Use cases by purpose
- `access-control`
- `device-allowlist`
- `bridge-policy`

## Command examples
```bash
# Allow only specific device by MAC
iptables -A INPUT -m mac --mac-source 00:11:22:33:44:55 -j ACCEPT
iptables -A INPUT -j DROP

# Block a specific device
iptables -A FORWARD -m mac --mac-source DE:AD:BE:EF:00:01 -j DROP

# Log unknown MACs
iptables -A INPUT -m mac ! --mac-source 00:11:22:33:44:55 \
  -j LOG --log-prefix "UNKNOWN-MAC "
```

## Dependencies
- only meaningful for locally received packets before routing; MAC is not preserved across routed hops
- for bridge filtering, also consider `ebtables`

## Limitations and considerations
- MAC addresses are trivially spoofed; use only as a convenience control, not a security boundary
- MAC is stripped by NAT traversal and routing; not available in POST-routing chains

## Relations with other extensions
- pairs well with: `physdev` (for bridge port identification)
- complement: `ebtables` for full L2 policy

## Official reference
- `iptables-extensions(8)`: section `mac`

---

# Reference: physdev

## Type
- `match`

## Purpose
Match packets based on the physical bridge port they arrived on or will leave from, enabling per-port policies when iptables is used on a Linux bridge.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A FORWARD -m physdev --physdev-in eth1 -j ACCEPT
```

## Available options / matches
- `--physdev-in ifname` — physical input bridge port
- `--physdev-out ifname` — physical output bridge port
- `--physdev-is-in` — match if there is a physical input port
- `--physdev-is-out` — match if there is a physical output port
- `--physdev-is-bridged` — match only bridged (not routed) packets
- `[!]` negation for each option

## Use cases by purpose
- `bridge-policy`
- `zone-separation`
- `vlan-enforcement`

## Command examples
```bash
# Isolate two bridge ports from each other
iptables -A FORWARD -m physdev --physdev-in eth1 --physdev-out eth2 -j DROP

# Allow traffic from trusted port only
iptables -A FORWARD -m physdev --physdev-in eth0 -j ACCEPT

# Log bridged traffic for diagnostics
iptables -A FORWARD -m physdev --physdev-is-bridged \
  -j LOG --log-prefix "BRIDGE-FWD "
```

## Dependencies
- requires bridge netfilter (`br_netfilter`) module
- requires `net.bridge.bridge-nf-call-iptables = 1` sysctl

## Limitations and considerations
- only available when packet traverses a Linux bridge
- combining with regular routing rules requires careful chain ordering

## Relations with other extensions
- related to: `mac`

## Official reference
- `iptables-extensions(8)`: section `physdev`

---

# Reference: pkttype

## Type
- `match`

## Purpose
Match packets by their link-layer packet type: unicast, broadcast, or multicast.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any (Ethernet / link-layer aware)

## Basic syntax
```bash
iptables -A INPUT -m pkttype --pkt-type broadcast -j DROP
```

## Available options / matches
- `--pkt-type {unicast|broadcast|multicast}`

## Use cases by purpose
- `hardening`
- `broadcast-control`
- `multicast-policy`

## Command examples
```bash
# Drop all broadcast packets on INPUT
iptables -A INPUT -m pkttype --pkt-type broadcast -j DROP

# Log multicast traffic on FORWARD
iptables -A FORWARD -m pkttype --pkt-type multicast \
  -j LOG --log-prefix "MCAST-FWD "

# Accept only unicast on sensitive services
iptables -A INPUT -p tcp --dport 443 -m pkttype ! --pkt-type unicast -j DROP
```

## Dependencies
- depends on link-layer metadata being available in the socket buffer
- available in `INPUT`, `FORWARD` chains

## Limitations and considerations
- may not behave as expected on virtual interfaces (tun/tap) where link-layer metadata is synthetic

## Relations with other extensions
- related to: `mac`, `physdev`

## Official reference
- `iptables-extensions(8)`: section `pkttype`
