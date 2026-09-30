# Matches Mark and Classification

This file covers matches that test packet and connection marks set by prior rules, enabling multi-table policies and stateful mark propagation.

---

# Reference: mark

## Type
- `match`

## Purpose
Match packets whose Netfilter mark (`nfmark`) equals (or does not equal) a given value/mask. Used to act on marks set by prior `MARK` target rules, implementing multi-stage routing and classification policies.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A FORWARD -m mark --mark 0x1 -j ACCEPT
```

## Available options / matches
- `--mark value[/mask]` — match if `(nfmark & mask) == value`; default mask is `0xFFFFFFFF`
- `[!]` negation supported

## Use cases by purpose
- `policy-routing`
- `traffic-shaping`
- `multi-stage-policy`

## Command examples
```bash
# Route marked traffic through alternate routing table (used with ip rule)
iptables -t mangle -A PREROUTING -m mark --mark 0x2 -j ACCEPT

# Log traffic that passed through a marking phase
iptables -A FORWARD -m mark --mark 0x1/0x1 -j LOG --log-prefix "MARKED-1 "

# Accept traffic marked as trusted
iptables -A INPUT -m mark --mark 0xA -j ACCEPT
iptables -A INPUT -j DROP
```

## Dependencies
- mark must have been set previously via `MARK` target in `mangle` table, or via `CONNMARK` restore

## Limitations and considerations
- packet mark is per-packet and is not persisted; use `CONNMARK` to persist across packets of the same connection
- masking with `value/mask` allows testing specific bits without full equality

## Relations with other extensions
- set by: `MARK` target
- pairs well with: `CONNMARK`, `ip rule`, `tc`

## Official reference
- `iptables-extensions(8)`: section `mark`

---

# Reference: connmark

## Type
- `match`

## Purpose
Match packets based on the connection mark stored in the conntrack entry for the packet's flow. Connection marks persist across all packets of a connection, unlike per-packet marks.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any tracked protocol

## Basic syntax
```bash
iptables -m connmark --mark 0x1 -j ACCEPT
```

## Available options / matches
- `--mark value[/mask]` — match if `(connmark & mask) == value`
- `[!]` negation supported

## Use cases by purpose
- `stateful-policy`
- `policy-routing`
- `multi-stage-policy`

## Command examples
```bash
# Match connections previously marked as trusted
iptables -A FORWARD -m connmark --mark 0x1 -j ACCEPT

# Restore connection mark to packet mark at start of chain
iptables -t mangle -A PREROUTING -j CONNMARK --restore-mark

# Then route based on packet mark
iptables -t mangle -A PREROUTING -m mark --mark 0x1 -j MARK --set-mark 0x1

# Log flows marked as VPN
iptables -A FORWARD -m connmark --mark 0xBEEF -j LOG --log-prefix "VPN-FLOW "
```

## Dependencies
- requires `nf_conntrack` and `xt_connmark`
- connection must already have a mark set via `CONNMARK` target

## Limitations and considerations
- connection mark is shared across original and reply direction packets
- `CONNMARK --restore-mark` is the common idiom to transfer the conn mark to the packet mark at the start of processing

## Relations with other extensions
- set by: `CONNMARK` target
- related to: `mark`, `CONNMARK`, `conntrack`

## Official reference
- `iptables-extensions(8)`: section `connmark`
