# Targets Traffic Duplication and Mirroring

This file covers the TEE target, which duplicates packets and routes the copy to a remote monitoring host.

---

# Target: TEE

## Type
- `target`

## Purpose
Clone a packet and route the copy to a specified gateway address (network tap / mirror). The original packet continues along its normal path; the clone is forwarded to the monitor host. Useful for passive traffic monitoring and IDS sensors.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `mangle`
- Valid chains: `PREROUTING`, `INPUT`, `FORWARD`, `POSTROUTING`
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A PREROUTING -j TEE --gateway 192.168.1.200
```

## Available options
- `--gateway addr` — destination IP address for the cloned packet (must be on a directly reachable subnet)

## Use cases by purpose
- `traffic-mirroring`
- `ids-ips-integration`
- `passive-monitoring`
- `forensics`

## Command examples
```bash
# Mirror all inbound traffic to a passive IDS sensor
iptables -t mangle -A PREROUTING -j TEE --gateway 192.168.10.250

# Mirror only HTTP traffic to a web proxy monitor
iptables -t mangle -A PREROUTING -p tcp --dport 80 -j TEE --gateway 10.0.0.99

# Mirror outbound traffic (POSTROUTING)
iptables -t mangle -A POSTROUTING -o eth0 -j TEE --gateway 192.168.10.250

# Mirror specific subnet traffic
iptables -t mangle -A FORWARD -s 10.10.0.0/24 -j TEE --gateway 192.168.1.200
```

## Dependencies
- requires `xt_TEE` module
- `--gateway` must be directly reachable (Layer 2 adjacent); TEE uses the routing table but does not perform full routing on the clone
- monitor host must be able to receive and process the duplicated traffic (e.g., running tcpdump/Wireshark/Snort in promiscuous mode)

## Limitations and considerations
- the clone is sent as-is with the original source/destination addresses; the monitor host must handle the raw traffic
- creating a feedback loop (mirroring to a host that sends traffic back to the firewall) can cause traffic amplification
- high-volume mirroring can saturate the link to the monitor host; use targeted rules rather than blanket mirroring
- does not support spanning across routed hops; the gateway must be directly adjacent
- IPv6: use `ip6tables -t mangle` with the IPv6 address of the monitor host

## Relations with other extensions
- related to: `NFLOG` (log-only alternative), `NFQUEUE` (decision-making alternative)
- pairs well with: `conntrack` (to mirror only NEW connections), `comment`

## Official reference
- `iptables-extensions(8)`: section `TEE`
