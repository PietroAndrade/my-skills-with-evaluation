# Matches Packet Shape and Fragmentation

This file covers matches that inspect packet-level structural characteristics: fragmentation headers, packet length, and TCP MSS values.

---

# Reference: frag

## Type
- `match`

## Purpose
Match IPv6 Fragment extension headers, enabling policies for fragmented IPv6 packets. Generally used to drop or log unexpected fragmentation.

## Scope
- Tool: `ip6tables`
- Common protocols: IPv6 fragmented packets

## Basic syntax
```bash
ip6tables -A INPUT -m frag --fragfirst -j DROP
```

## Available options / matches
- `--fragid id[:id]` — match fragment identification value
- `--fraglen length` — match fragment length
- `--fragfirst` — match only the first fragment
- `--fragmore` — match fragments with More Fragments bit set
- `--fraglast` — match only the last fragment

## Use cases by purpose
- `hardening`
- `dos-protection`
- `fragmentation-control`

## Command examples
```bash
# Drop all IPv6 fragments (strict policy)
ip6tables -A INPUT -m frag --fragmore -j DROP
ip6tables -A INPUT -m frag --fraglast -j DROP

# Log first fragment for diagnostics
ip6tables -A FORWARD -m frag --fragfirst -j LOG --log-prefix "FRAG-FIRST "
```

## Dependencies
- IPv6-specific; for IPv4 fragmentation, use the built-in `-f` flag

## Limitations and considerations
- Dropping all fragments may break legitimate IPv6 implementations that rely on Path MTU Discovery
- ensure PMTUD (ICMPv6 Packet Too Big) messages are not blocked

## Relations with other extensions
- related to: `length`, `rt`

## Official reference
- `iptables-extensions(8)`: section `frag`

---

# Reference: length

## Type
- `match`

## Purpose
Match packets by their total IP packet length (including headers), enabling size-based filtering such as blocking oversized packets or matching protocol-specific packet sizes.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A INPUT -m length --length 1500: -j DROP
```

## Available options / matches
- `--length min[:max]` — match packet length in bytes; omit max for open-ended

## Use cases by purpose
- `hardening`
- `dos-protection`
- `traffic-shaping`

## Command examples
```bash
# Drop oversized packets (potential fragmentation attack)
iptables -A INPUT -m length --length 1473: -j DROP

# Match small UDP packets (typical of DNS queries)
iptables -A INPUT -p udp --dport 53 -m length --length :512 -j ACCEPT

# Log zero-length packets
iptables -A INPUT -m length --length 0:20 -j LOG --log-prefix "TINY-PKT "
```

## Dependencies
- none; uses standard Netfilter

## Limitations and considerations
- length is the IP total length field; does not represent payload length alone
- fragmented packets: only the first fragment has the correct total length in the IP header

## Relations with other extensions
- pairs well with: `frag`, `ttl`, `DROP`

## Official reference
- `iptables-extensions(8)`: section `length`

---

# Reference: tcpmss

## Type
- `match`

## Purpose
Match TCP SYN packets by their MSS (Maximum Segment Size) option value, enabling detection of mismatched MSS values that can indicate MTU mismatches or manipulation.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `tcp`

## Basic syntax
```bash
iptables -A FORWARD -p tcp --tcp-flags SYN,RST SYN -m tcpmss --mss 1400:1536 -j TCPMSS --clamp-mss-to-pmtu
```

## Available options / matches
- `--mss min[:max]` — MSS range in bytes

## Use cases by purpose
- `mtu-normalization`
- `vpn`
- `hardening`

## Command examples
```bash
# Clamp MSS to PMTU for VPN traffic
iptables -A FORWARD -p tcp --tcp-flags SYN,RST SYN \
  -m tcpmss --mss 1400:1536 \
  -j TCPMSS --clamp-mss-to-pmtu

# Block connections with suspiciously large MSS
iptables -A INPUT -p tcp --syn \
  -m tcpmss --mss 1461: \
  -j DROP

# Log unusually small MSS
iptables -A INPUT -p tcp --syn \
  -m tcpmss --mss :500 \
  -j LOG --log-prefix "SMALL-MSS "
```

## Dependencies
- requires `-p tcp`
- only evaluates SYN packets (MSS option only present in SYN)

## Limitations and considerations
- only meaningful on SYN/SYN-ACK packets
- MSS manipulation is common in PPPoE, GRE, and VPN tunnels
- use `TCPMSS --clamp-mss-to-pmtu` to auto-fix rather than just match

## Relations with other extensions
- target: `TCPMSS`
- related to: `length`

## Official reference
- `iptables-extensions(8)`: section `tcpmss`
