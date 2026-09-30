# Matches QoS and Header Fields

This file consolidates matches that inspect QoS-related IP header fields: DSCP, ECN, IP TTL/HL, and TOS byte.

---

# Reference: dscp

## Type
- `match`

## Purpose
Match packets by their DSCP (Differentiated Services Code Point) field in the IP header, enabling QoS classification and policy enforcement based on traffic priority markings.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A FORWARD -m dscp --dscp 0x28 -j ACCEPT
```

## Available options / matches
- `--dscp value` — numeric DSCP value (6-bit, 0-63)
- `--dscp-class class` — symbolic DSCP class (e.g., EF, AF11, CS0)

## Use cases by purpose
- `qos-policy`
- `traffic-classification`

## Command examples
```bash
# Accept Expedited Forwarding traffic
iptables -A FORWARD -m dscp --dscp-class EF -j ACCEPT

# Log traffic with bulk background DSCP
iptables -A FORWARD -m dscp --dscp-class CS1 -j LOG --log-prefix "DSCP-CS1 "

# Reclassify DSCP after matching
iptables -t mangle -A FORWARD -m dscp --dscp-class AF41 -j DSCP --set-dscp-class EF
```

## Dependencies
- requires `xt_dscp` module

## Limitations and considerations
- DSCP values may be remarked by transit routers; verify end-to-end DSCP trust boundaries
- `--dscp-class` names are kernel-defined; verify available names with the modules

## Relations with other extensions
- set by: `DSCP` target
- related to: `tos`, `CLASSIFY`, `tc`

## Official reference
- `iptables-extensions(8)`: section `dscp`

---

# Reference: ecn

## Type
- `match`

## Purpose
Match packets by their ECN (Explicit Congestion Notification) bits in the IP and TCP headers, enabling policies that inspect or enforce congestion signaling.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `tcp`

## Basic syntax
```bash
iptables -A FORWARD -p tcp -m ecn --ecn-tcp-cwr -j LOG --log-prefix "ECN-CWR "
```

## Available options / matches
- `--ecn-tcp-cwr` — match TCP CWR (Congestion Window Reduced) flag
- `--ecn-tcp-ece` — match TCP ECE (ECN Echo) flag
- `--ecn-ip-ect value` — match IP ECT bits (0-3)

## Use cases by purpose
- `congestion-monitoring`
- `qos-policy`

## Command examples
```bash
# Log CWR events
iptables -A FORWARD -p tcp -m ecn --ecn-tcp-cwr -j LOG --log-prefix "ECN-CWR "
# Log ECN-capable transport packets (ECT bit set)
iptables -A FORWARD -m ecn --ecn-ip-ect 1 -j LOG --log-prefix "ECN-ECT "
```

## Dependencies
- TCP ECN matching requires `-p tcp`
- requires `xt_ecn` module

## Limitations and considerations
- ECN requires cooperation from both endpoints; intermediate nodes should not strip ECN bits
- used mainly for diagnostics, not for common security policies

## Relations with other extensions
- related to: `dscp`, `ECN` target

## Official reference
- `iptables-extensions(8)`: section `ecn`

---

# Reference: hl

## Type
- `match`

## Purpose
Match IPv6 packets by their Hop Limit field value, the IPv6 equivalent of IPv4 TTL.

## Scope
- Tool: `ip6tables`
- Common protocols: any

## Basic syntax
```bash
ip6tables -A INPUT -m hl --hl-eq 1 -j DROP
```

## Available options / matches
- `--hl-eq value` — equal
- `--hl-neq value` — not equal
- `--hl-lt value` — less than
- `--hl-gt value` — greater than

## Use cases by purpose
- `hardening`
- `ttl-normalization`
- `loop-detection`

## Command examples
```bash
# Drop HL=1 packets (would expire on this hop)
ip6tables -A FORWARD -m hl --hl-eq 1 -j DROP
# Log unusually large hop limits
ip6tables -A INPUT -m hl --hl-gt 200 -j LOG --log-prefix "HL-LARGE "
```

## Dependencies
- IPv6 only; for IPv4, use `ttl`

## Relations with other extensions
- IPv4 equivalent: `ttl`
- target: `HL`

## Official reference
- `iptables-extensions(8)`: section `hl`

---

# Reference: tos

## Type
- `match`

## Purpose
Match IPv4 packets by their TOS (Type of Service) byte, the predecessor to DSCP/ECN in the DiffServ model.

## Scope
- Tool: `iptables`
- Common protocols: any

## Basic syntax
```bash
iptables -A FORWARD -m tos --tos 0x10 -j ACCEPT
```

## Available options / matches
- `--tos value[/mask]` — numeric TOS value with optional mask
- `--tos name` — symbolic TOS name (Minimize-Delay, Maximize-Throughput, Maximize-Reliability, Minimize-Cost, Normal-Service)

## Use cases by purpose
- `qos-policy`
- `legacy-tos`

## Command examples
```bash
iptables -A FORWARD -m tos --tos Minimize-Delay -j ACCEPT
iptables -t mangle -A PREROUTING -m tos --tos 0x10 -j MARK --set-mark 1
```

## Dependencies
- IPv4 only; use `dscp` for modern DiffServ

## Limitations and considerations
- TOS is largely superseded by DSCP; existing deployments may still use TOS for legacy equipment
- TOS bits overlap with DSCP in the IP header (upper 6 bits = DSCP, lower 2 bits = ECN)

## Relations with other extensions
- related to: `dscp`, `TOS` target

## Official reference
- `iptables-extensions(8)`: section `tos`

---

# Reference: ttl

## Type
- `match`

## Purpose
Match IPv4 packets by their TTL (Time To Live) field value.

## Scope
- Tool: `iptables`
- Common protocols: any

## Basic syntax
```bash
iptables -A INPUT -m ttl --ttl-eq 1 -j DROP
```

## Available options / matches
- `--ttl-eq value`
- `--ttl-neq value`
- `--ttl-lt value`
- `--ttl-gt value`

## Use cases by purpose
- `hardening`
- `ttl-normalization`
- `traceroute-control`

## Command examples
```bash
# Block traceroute probes (TTL=1 at this hop)
iptables -A FORWARD -m ttl --ttl-eq 1 -j DROP

# Log suspicious low-TTL packets
iptables -A INPUT -m ttl --ttl-lt 5 -j LOG --log-prefix "LOW-TTL "

# Normalize TTL to 64 for outbound traffic
iptables -t mangle -A POSTROUTING -m ttl --ttl-gt 64 -j TTL --ttl-set 64
```

## Dependencies
- IPv4 only; use `hl` for IPv6

## Limitations and considerations
- TTL equalization can mask network topology from traceroute (useful or obfuscating depending on policy)

## Relations with other extensions
- IPv6 equivalent: `hl`
- target: `TTL`

## Official reference
- `iptables-extensions(8)`: section `ttl`
