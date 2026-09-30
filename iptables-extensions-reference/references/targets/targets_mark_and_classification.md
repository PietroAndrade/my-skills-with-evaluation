# Targets Mark and Classification

This file covers targets that set or manipulate Netfilter marks and tc classids for policy routing and QoS: CLASSIFY, CONNMARK, HMARK, and MARK.

---

# Target: CLASSIFY

## Type
- `target`

## Purpose
Set the tc (traffic control) `skb->priority` field, which maps directly to a `tc` class handle. Allows iptables rules to directly classify packets for `tc` queuing disciplines without going through `MARK` + `ip rule`.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `mangle`
- Valid chains: `POSTROUTING`
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A POSTROUTING -p tcp --dport 80 -j CLASSIFY --set-class 1:10
```

## Available options
- `--set-class major:minor` — tc class handle in `major:minor` hex format

## Use cases by purpose
- `qos-policy`
- `traffic-shaping`

## Command examples
```bash
# Classify HTTP to tc class 1:10
iptables -t mangle -A POSTROUTING -o eth0 -p tcp --dport 80 -j CLASSIFY --set-class 1:10

# Classify VoIP RTP to high-priority class
iptables -t mangle -A POSTROUTING -o eth0 -p udp --dport 5004:5005 -j CLASSIFY --set-class 1:1
```

## Dependencies
- requires a corresponding `tc qdisc` / `tc class` configured on the interface

## Limitations and considerations
- class handles must match the `tc` configuration
- only applies to POSTROUTING (egress direction)

## Relations with other extensions
- pairs well with: `MARK`, `DSCP`

## Official reference
- `iptables-extensions(8)`: section `CLASSIFY`

---

# Target: CONNMARK

## Type
- `target`

## Purpose
Save or restore marks between the per-packet mark (`nfmark`) and the per-connection mark stored in the conntrack entry (`ctmark`). Enables mark persistence across all packets of a connection.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `mangle`
- Valid chains: any
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A PREROUTING -j CONNMARK --restore-mark
```

## Available options
- `--set-mark value[/mask]` — set the connection mark to value
- `--save-mark [--mask mask]` — copy packet mark to connection mark
- `--restore-mark [--mask mask]` — copy connection mark to packet mark

## Use cases by purpose
- `stateful-policy`
- `policy-routing`
- `multi-stage-policy`

## Command examples
```bash
# Restore connection mark to packet at the start of each packet processing
iptables -t mangle -A PREROUTING -j CONNMARK --restore-mark

# Save packet mark (set by MARK target) back to connection mark
iptables -t mangle -A POSTROUTING -j CONNMARK --save-mark

# Set connection mark directly (for new connections only)
iptables -t mangle -A PREROUTING -m conntrack --ctstate NEW \
  -p tcp --dport 443 \
  -j CONNMARK --set-mark 0x5

# Use mask to set only specific bits
iptables -t mangle -A PREROUTING -j CONNMARK --restore-mark --mask 0xFF
```

## Dependencies
- requires `nf_conntrack` and `xt_connmark`

## Limitations and considerations
- the save/restore pattern is the canonical idiom for persistent per-flow marking
- masking allows preserving independently managed bit groups in the same mark field

## Relations with other extensions
- `connmark` match
- pairs well with: `MARK`, `ip rule`

## Official reference
- `iptables-extensions(8)`: section `CONNMARK`

---

# Target: HMARK

## Type
- `target`

## Purpose
Set the packet mark based on a hash of configurable header fields (source/destination IP, port, protocol, etc.), enabling consistent packet distribution and multipath hashing.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `mangle`
- Valid chains: any
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A PREROUTING -j HMARK --hmark-tuple src,dst,sport,dport,proto --hmark-mod 2 --hmark-offset 10
```

## Available options
- `--hmark-tuple field[,field...]` — fields to hash: `src`, `dst`, `sport`, `dport`, `proto`, `ct-src`, `ct-dst`, `ct-sport`, `ct-dport`, `ct-proto`
- `--hmark-mod value` — modulus for hash (number of buckets)
- `--hmark-offset value` — added to the hash result to form the mark
- `--hmark-src-prefix length` — apply prefix mask to source IP
- `--hmark-dst-prefix length` — apply prefix mask to destination IP
- `--hmark-sport-mask mask`, `--hmark-dport-mask mask` — port masks
- `--hmark-proto-mask mask` — protocol mask
- `--hmark-rnd seed` — random seed for hash

## Use cases by purpose
- `load-balancing`
- `multipath-routing`
- `traffic-distribution`

## Command examples
```bash
# Distribute flows across 4 mark values (10-13) based on 5-tuple hash
iptables -t mangle -A PREROUTING -j HMARK \
  --hmark-tuple src,dst,sport,dport,proto \
  --hmark-mod 4 \
  --hmark-offset 10

# Then route per mark via ip rule
# ip rule add fwmark 10 table 10
# ip rule add fwmark 11 table 11
```

## Dependencies
- requires `xt_HMARK` module

## Limitations and considerations
- hash is deterministic for the same tuple but not cryptographically secure
- `--hmark-rnd` changes the hash seed; use a consistent value across all nodes for symmetric routing

## Relations with other extensions
- related to: `MARK`, `statistic`, `cluster`

## Official reference
- `iptables-extensions(8)`: section `HMARK`

---

# Target: MARK

## Type
- `target`

## Purpose
Set, AND, OR, or XOR the Netfilter packet mark (`nfmark`), used for policy routing, QoS classification, and multi-stage rule evaluation.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `mangle`
- Valid chains: any
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A PREROUTING -p tcp --dport 80 -j MARK --set-mark 0x1
```

## Available options
- `--set-mark value[/mask]` — set mark bits
- `--and-mark bits` — bitwise AND
- `--or-mark bits` — bitwise OR
- `--xor-mark bits` — bitwise XOR

## Use cases by purpose
- `policy-routing`
- `traffic-shaping`
- `multi-stage-policy`

## Command examples
```bash
# Mark HTTP traffic for alternate routing table
iptables -t mangle -A PREROUTING -p tcp --dport 80 -j MARK --set-mark 0x1

# Set specific bits without clearing others
iptables -t mangle -A PREROUTING -p tcp --dport 443 -j MARK --or-mark 0x2

# Use mark in consecutive rule evaluation
iptables -t mangle -A PREROUTING -m mark --mark 0x1 -j ACCEPT
```

## Dependencies
- mark is per-packet (in-flight only); combine with `CONNMARK` for persistence

## Limitations and considerations
- packet mark is cleared when a packet leaves the kernel (does not persist to network)
- masking with `value/mask` only sets the specified bits, preserving others

## Relations with other extensions
- `mark` match
- pairs well with: `CONNMARK`, `CLASSIFY`, `ip rule`

## Official reference
- `iptables-extensions(8)`: section `MARK`
