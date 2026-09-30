# Targets Conntrack Control

This file covers targets that directly manipulate connection tracking behavior: CT (connection tracking configuration per flow) and NOTRACK (bypass conntrack entirely).

---

# Target: CT

## Type
- `target`

## Purpose
Set per-connection conntrack parameters such as timeout, helper, expected network, or zone assignment directly in the `raw` or `rawpost` table, before the connection is fully tracked.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `raw`, `rawpost`
- Valid chains: `PREROUTING`, `OUTPUT`
- Terminating: no

## Basic syntax
```bash
iptables -t raw -A PREROUTING -p tcp --dport 21 -j CT --helper ftp
```

## Available options
- `--notrack` — equivalent to NOTRACK (do not track this connection)
- `--helper name` — assign a conntrack helper (e.g., `ftp`, `irc`, `sip`)
- `--ctevents event[,event...]` — limit conntrack events emitted
- `--expevents event[,event...]` — limit expectation events
- `--zone-orig id` — original direction conntrack zone
- `--zone-reply id` — reply direction conntrack zone
- `--zone id` — both-direction conntrack zone
- `--timeout name` — use a named conntrack timeout policy

## Use cases by purpose
- `application-layer-gateway`
- `multi-zone`
- `conntrack-tuning`

## Command examples
```bash
# Assign FTP helper to FTP control connection
iptables -t raw -A PREROUTING -p tcp --dport 21 -j CT --helper ftp

# Assign SIP helper
iptables -t raw -A PREROUTING -p udp --dport 5060 -j CT --helper sip

# Place traffic in conntrack zone 1
iptables -t raw -A PREROUTING -i eth1 -j CT --zone 1

# Use a named timeout policy for long-lived UDP
iptables -t raw -A PREROUTING -p udp --dport 5000 -j CT --timeout udp-long
```

## Dependencies
- requires `nf_conntrack` and `xt_CT`
- helpers must be loaded (`nf_conntrack_ftp`, `nf_conntrack_sip`, etc.)
- named timeouts must be created via `conntrack` tools or `nft`

## Limitations and considerations
- `CT` must be placed in `raw` PREROUTING to act before full conntrack processing
- zone assignment enables asymmetric routing scenarios where the same tuple appears on different interfaces
- `--notrack` in CT is functionally equivalent to NOTRACK

## Relations with other extensions
- related to: `NOTRACK`, `conntrack` match

## Official reference
- `iptables-extensions(8)`: section `CT`

---

# Target: NOTRACK

## Type
- `target`

## Purpose
Exempt a packet and its reverse flow from connection tracking, preventing conntrack state entries from being created. Reduces conntrack table pressure for high-volume or well-known safe flows.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `raw`
- Valid chains: `PREROUTING`, `OUTPUT`
- Terminating: yes (within the `raw` table)

## Basic syntax
```bash
iptables -t raw -A PREROUTING -p udp --dport 53 -j NOTRACK
```

## Available options
- none

## Use cases by purpose
- `performance`
- `conntrack-bypass`
- `dos-protection`

## Command examples
```bash
# Do not track DNS (high-volume, stateless)
iptables -t raw -A PREROUTING -p udp --dport 53 -j NOTRACK
iptables -t raw -A OUTPUT -p udp --sport 53 -j NOTRACK

# Skip conntrack for established bulk transfer
iptables -t raw -A PREROUTING -s 10.0.0.0/8 -p tcp --dport 3000:3100 -j NOTRACK

# Prevent conntrack table overflow during DDoS
iptables -t raw -A PREROUTING -p udp -j NOTRACK
```

## Dependencies
- must be in `raw` table

## Limitations and considerations
- untracked connections cannot use stateful rules (`-m conntrack --ctstate ESTABLISHED`) — those rules will NOT match untracked packets
- NAT cannot be applied to untracked packets
- `RELATED` state cannot be used when helpers are bypassed via NOTRACK

## Relations with other extensions
- related to: `CT --notrack`
- complement: `conntrack` match (cannot be combined with NOTRACK flows for stateful work)

## Official reference
- `iptables-extensions(8)`: section `NOTRACK`
