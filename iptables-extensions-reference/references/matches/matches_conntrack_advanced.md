# Matches Conntrack Advanced

This file extends the core conntrack match with advanced connection tracking features: byte/packet accounting per connection, connection labels, and per-address connection limiting.

---

# Reference: connbytes

## Type
- `match`

## Purpose
Match packets or bytes transferred within a connection direction (original, reply, or both), enabling per-connection volume thresholds for rate policies or QoS.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `tcp`, `udp`, any tracked protocol

## Basic syntax
```bash
iptables -A FORWARD -m connbytes --connbytes 1000000: --connbytes-dir both --connbytes-mode bytes -j DROP
```

## Available options / matches
- `--connbytes from[:to]` — byte or packet range; omit `to` for open-ended
- `--connbytes-dir {original|reply|both}` — direction to count
- `--connbytes-mode {bytes|packets|avgpkt}` — unit of measurement

## Use cases by purpose
- `traffic-shaping`
- `dos-protection`
- `metering`

## Command examples
```bash
# Drop connections that transfer more than 1 MB total
iptables -A FORWARD -m connbytes \
  --connbytes 1000000: \
  --connbytes-dir both \
  --connbytes-mode bytes \
  -j DROP

# Log connections with more than 10000 packets in both directions
iptables -A FORWARD -m connbytes \
  --connbytes 10000: \
  --connbytes-dir both \
  --connbytes-mode packets \
  -j LOG --log-prefix "CONNBYTES-LARGE "

# Mark high-volume flows for tc shaping
iptables -t mangle -A FORWARD -m connbytes \
  --connbytes 500000: \
  --connbytes-dir reply \
  --connbytes-mode bytes \
  -j MARK --set-mark 2
```

## Dependencies
- requires conntrack (`nf_conntrack`)
- conntrack must be enabled and the connection must be in the tracking table

## Limitations and considerations
- counters accumulate for the lifetime of the connection; resetting requires connection teardown
- `avgpkt` is the average packet size (total bytes / total packets)
- high-traffic firewalls may see significant conntrack table pressure

## Relations with other extensions
- requires: `conntrack`
- pairs well with: `MARK`, `LOG`, `REJECT`

## Official reference
- `iptables-extensions(8)`: section `connbytes`

---

# Reference: connlabel

## Type
- `match`

## Purpose
Match packets based on user-defined labels attached to a connection tracking entry. Labels can be set and tested, allowing state to be propagated across rules without additional conntrack entries.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any tracked protocol

## Basic syntax
```bash
iptables -A INPUT -m connlabel --label "vpn-user" -j ACCEPT
```

## Available options / matches
- `--label name` — label name (must exist in `/etc/xtables/connlabel.conf`)
- `--set` — set the label on the connection (also matches)
- `[!]` — negate match

## Use cases by purpose
- `stateful-policy`
- `multi-pass-marking`

## Command examples
```bash
# Set a label on connections from trusted source, then use it later
iptables -A PREROUTING -t mangle -s 10.10.0.0/16 -m connlabel --label "trusted" --set -j ACCEPT
iptables -A FORWARD -m connlabel --label "trusted" -j ACCEPT

# Reject unlabeled connections at a stricter chain
iptables -A FORWARD -m connlabel ! --label "trusted" -j DROP
```

## Dependencies
- requires `nf_conntrack` and `xt_connlabel`
- label names must be pre-configured in `/etc/xtables/connlabel.conf` (one label per line)

## Limitations and considerations
- connlabel is a bitmask; maximum number of labels is kernel-determined (typically 128)
- labels persist for the connection lifetime
- configuration file path varies by distribution

## Relations with other extensions
- requires: `conntrack`
- related to: `connmark`, `MARK`

## Official reference
- `iptables-extensions(8)`: section `connlabel`

---

# Reference: connlimit

## Type
- `match`

## Purpose
Limit the number of simultaneous connections from a source address (or address group), defending against connection exhaustion attacks and enforcing per-client session caps.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `tcp`, `udp`

## Basic syntax
```bash
iptables -A INPUT -p tcp --dport 22 -m connlimit --connlimit-above 3 -j REJECT
```

## Available options / matches
- `--connlimit-above n` — match if connections exceed n
- `--connlimit-upto n` — match if connections are ≤ n
- `--connlimit-mask prefix_length` — group addresses by prefix (default: 32 for IPv4, 128 for IPv6)
- `--connlimit-saddr` — count connections from source address (default)
- `--connlimit-daddr` — count connections to destination address

## Use cases by purpose
- `dos-protection`
- `access-control`
- `service-protection`

## Command examples
```bash
# Allow max 3 simultaneous SSH connections per source IP
iptables -A INPUT -p tcp --dport 22 -m connlimit --connlimit-above 3 -j REJECT --reject-with tcp-reset

# Limit web connections to 100 per /24 subnet
iptables -A INPUT -p tcp --dport 80 -m connlimit \
  --connlimit-above 100 --connlimit-mask 24 \
  -j DROP

# Log when connection count hits threshold
iptables -A INPUT -p tcp --dport 443 -m connlimit --connlimit-above 50 \
  -j LOG --log-prefix "CONNLIMIT-EXCEED "
```

## Dependencies
- requires `nf_conntrack` and `xt_connlimit`

## Limitations and considerations
- counts all connections in the tracking table, including ESTABLISHED and TIME_WAIT
- use `--connlimit-mask` carefully for IPv6 (default 128 means per single address)
- high connection rates can cause conntrack table exhaustion before this match fires

## Relations with other extensions
- requires: `conntrack`
- pairs well with: `REJECT`, `LOG`, `recent`

## Official reference
- `iptables-extensions(8)`: section `connlimit`
