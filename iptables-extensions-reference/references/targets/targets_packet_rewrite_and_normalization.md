# Targets Packet Rewrite and Normalization

This file covers targets that rewrite or normalize IP/TCP header fields: CHECKSUM, ECN, HL, TCPMSS, TCPOPTSTRIP, TOS, and TTL.

---

# Target: CHECKSUM

## Type
- `target`

## Purpose
Recompute the UDP checksum for packets where it was zero (common in virtualization environments like QEMU/KVM where guests offload checksum computation). Prevents broken datagrams from reaching the guest.

## Scope
- Tool: `iptables`
- Valid tables: `mangle`
- Valid chains: `POSTROUTING`
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A POSTROUTING -p udp --dport 68 -j CHECKSUM --checksum-fill
```

## Available options
- `--checksum-fill` — fill in the UDP checksum

## Use cases by purpose
- `virtualization`
- `dhcp-relay`

## Command examples
```bash
# Fix DHCP checksum for QEMU guests
iptables -t mangle -A POSTROUTING -p udp --dport 68 -j CHECKSUM --checksum-fill
```

## Limitations and considerations
- primarily a workaround for DHCP and other UDP services in virtualized environments
- not needed in bare-metal deployments where NIC handles checksum offload correctly

## Official reference
- `iptables-extensions(8)`: section `CHECKSUM`

---

# Target: ECN

## Type
- `target`

## Purpose
Remove ECN (Explicit Congestion Notification) bits from TCP SYN packets to fix compatibility with broken middleboxes that drop ECN-capable packets.

## Scope
- Tool: `iptables`
- Valid tables: `mangle`
- Valid chains: `FORWARD`
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN -j ECN --ecn-tcp-remove
```

## Available options
- `--ecn-tcp-remove` — strip ECN bits from TCP options and flags in SYN packets

## Use cases by purpose
- `compatibility`
- `mtu-normalization`

## Command examples
```bash
# Remove ECN for packets going to broken ISP
iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN -o eth0 -j ECN --ecn-tcp-remove
```

## Limitations and considerations
- only affects SYN packets; ECN negotiation happens at connection setup
- use only when middlebox incompatibility is confirmed

## Relations with other extensions
- related to: `ecn` match
- complement: `TCPMSS` for MSS normalization

## Official reference
- `iptables-extensions(8)`: section `ECN`

---

# Target: HL

## Type
- `target`

## Purpose
Set, increment, or decrement the IPv6 Hop Limit field.

## Scope
- Tool: `ip6tables`
- Valid tables: `mangle`
- Valid chains: `PREROUTING`, `POSTROUTING`, `FORWARD`, `INPUT`, `OUTPUT`
- Terminating: no

## Basic syntax
```bash
ip6tables -t mangle -A PREROUTING -j HL --hl-set 64
```

## Available options
- `--hl-set value` — set HL to value
- `--hl-inc value` — increment HL by value
- `--hl-dec value` — decrement HL by value (min 1)

## Use cases by purpose
- `ttl-normalization`
- `topology-hiding`

## Command examples
```bash
# Normalize IPv6 Hop Limit to 64
ip6tables -t mangle -A PREROUTING -j HL --hl-set 64
# Decrement for NAT traversal equalization
ip6tables -t mangle -A FORWARD -j HL --hl-dec 1
```

## Limitations and considerations
- IPv6 only; for IPv4 use `TTL`
- incrementing HL can produce routing loops if misused

## Relations with other extensions
- IPv4 equivalent: `TTL` target
- `hl` match

## Official reference
- `iptables-extensions(8)`: section `HL`

---

# Target: TCPMSS

## Type
- `target`

## Purpose
Alter the MSS (Maximum Segment Size) option in TCP SYN/SYN-ACK packets to match the Path MTU, resolving issues caused by PMTUD black holes in VPN tunnels and PPPoE links.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `filter`, `mangle`
- Valid chains: `FORWARD`, `OUTPUT`, `POSTROUTING`
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu
```

## Available options
- `--set-mss value` — set MSS to a specific value
- `--clamp-mss-to-pmtu` — automatically calculate and set MSS based on outgoing interface MTU

## Use cases by purpose
- `vpn`
- `pppoe`
- `mtu-normalization`

## Command examples
```bash
# Clamp MSS for VPN traffic
iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu

# Set fixed MSS for PPPoE (MTU 1492 → MSS 1452)
iptables -t mangle -A POSTROUTING -p tcp --tcp-flags SYN,RST SYN -o ppp0 -j TCPMSS --set-mss 1452
```

## Dependencies
- applies only to TCP SYN and SYN-ACK packets

## Relations with other extensions
- `tcpmss` match
- complement: `ECN`, `TTL`

## Official reference
- `iptables-extensions(8)`: section `TCPMSS`

---

# Target: TCPOPTSTRIP

## Type
- `target`

## Purpose
Strip specified TCP options from packets, removing options that cause compatibility problems or that may reveal information about the sender.

## Scope
- Tool: `iptables`
- Valid tables: `mangle`
- Valid chains: any
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A FORWARD -p tcp -j TCPOPTSTRIP --strip-options timestamp
```

## Available options
- `--strip-options option[,option...]` — comma-separated list of TCP option names or numbers to strip (e.g., `timestamp`, `sack`, `mss`)

## Use cases by purpose
- `compatibility`
- `fingerprint-reduction`
- `hardening`

## Command examples
```bash
# Remove TCP timestamps (can reveal host uptime)
iptables -t mangle -A FORWARD -p tcp -j TCPOPTSTRIP --strip-options timestamp

# Remove SACK for compatibility with older equipment
iptables -t mangle -A FORWARD -p tcp -j TCPOPTSTRIP --strip-options sack
```

## Limitations and considerations
- stripping MSS may cause fragmentation; use carefully
- stripping timestamps disables PAWS (Protection Against Wrapped Sequence Numbers)

## Official reference
- `iptables-extensions(8)`: section `TCPOPTSTRIP`

---

# Target: TOS

## Type
- `target`

## Purpose
Set the TOS (Type of Service) byte in the IPv4 header, used for QoS classification in legacy or hybrid DiffServ environments.

## Scope
- Tool: `iptables`
- Valid tables: `mangle`
- Valid chains: `PREROUTING`, `FORWARD`, `POSTROUTING`, `INPUT`, `OUTPUT`
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A PREROUTING -p tcp --dport 22 -j TOS --set-tos Minimize-Delay
```

## Available options
- `--set-tos value[/mask]` — numeric TOS value with optional mask
- `--set-tos name` — symbolic name: `Minimize-Delay`, `Maximize-Throughput`, `Maximize-Reliability`, `Minimize-Cost`, `Normal-Service`
- `--and-tos bits`, `--or-tos bits`, `--xor-tos bits` — bitwise operations on TOS

## Use cases by purpose
- `qos-policy`
- `legacy-tos`

## Command examples
```bash
# Minimize delay for SSH
iptables -t mangle -A PREROUTING -p tcp --dport 22 -j TOS --set-tos Minimize-Delay
# Maximize throughput for bulk transfers
iptables -t mangle -A PREROUTING -p tcp --dport 21 -j TOS --set-tos Maximize-Throughput
```

## Relations with other extensions
- `tos` match
- modern replacement: `DSCP` target

## Official reference
- `iptables-extensions(8)`: section `TOS`

---

# Target: TTL

## Type
- `target`

## Purpose
Set, increment, or decrement the IPv4 TTL field, used for topology hiding, TTL normalization, and bypass of certain ISP throttling techniques.

## Scope
- Tool: `iptables`
- Valid tables: `mangle`
- Valid chains: `PREROUTING`, `POSTROUTING`, `INPUT`, `OUTPUT`, `FORWARD`
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A PREROUTING -j TTL --ttl-set 64
```

## Available options
- `--ttl-set value` — set TTL to value
- `--ttl-inc value` — increment TTL by value
- `--ttl-dec value` — decrement TTL by value

## Use cases by purpose
- `topology-hiding`
- `ttl-normalization`

## Command examples
```bash
# Normalize TTL to 64 for outbound traffic
iptables -t mangle -A POSTROUTING -j TTL --ttl-set 64

# Increment TTL to handle ISP blocking of TTL=1 packets
iptables -t mangle -A PREROUTING -i ppp0 -j TTL --ttl-inc 1
```

## Limitations and considerations
- incrementing TTL can produce routing loops if misused
- setting TTL = 1 on FORWARD will cause the packet to be discarded by the next hop

## Relations with other extensions
- IPv6 equivalent: `HL` target
- `ttl` match

## Official reference
- `iptables-extensions(8)`: section `TTL`
