# Matches Protocol Specific

This file consolidates matches for protocols beyond TCP/UDP/ICMP: IPsec headers, SCTP, DCCP, and IPv6 extension headers.

---

# Reference: ah

## Type
- `match`

## Purpose
Match IPv6 packets bearing an Authentication Header (AH), used in IPsec authentication-only mode.

## Scope
- Tool: `ip6tables`
- Common protocols: `ah` (IPv6 IPsec AH)

## Basic syntax
```bash
ip6tables -A INPUT -m ah --ahspi 256 -j ACCEPT
```

## Available options / matches
- `--ahspi spi[:spi]`

## Use cases by purpose
- `ipsec-policy`
- `vpn-filtering`

## Command examples
```bash
ip6tables -A INPUT -m ah --ahspi 256 -j ACCEPT
ip6tables -A INPUT -p ah -j LOG --log-prefix "AH-SEEN "
```

## Dependencies
- requires `-p ah` implicit or explicit protocol selection

## Limitations and considerations
- IPv6-specific; for IPv4, AH matching is typically handled via the `policy` match
- SPI values must match the configured IPsec SA

## Relations with other extensions
- related to: `esp`, `policy`

## Official reference
- `iptables-extensions(8)`: section `ah`

---

# Reference: esp

## Type
- `match`

## Purpose
Match IPv6 packets bearing an Encapsulating Security Payload (ESP) header.

## Scope
- Tool: `ip6tables`
- Common protocols: `esp` (IPv6 IPsec ESP)

## Basic syntax
```bash
ip6tables -A INPUT -m esp --espspi 256 -j ACCEPT
```

## Available options / matches
- `--espspi spi[:spi]`

## Use cases by purpose
- `ipsec-policy`
- `vpn-filtering`

## Command examples
```bash
ip6tables -A INPUT -m esp --espspi 256 -j ACCEPT
ip6tables -A INPUT -p esp -j LOG --log-prefix "ESP-SEEN "
```

## Dependencies
- IPv6-specific header match

## Limitations and considerations
- SPI values must align with configured IPsec SA
- for broader IPsec policy matching, use `policy`

## Relations with other extensions
- related to: `ah`, `policy`

## Official reference
- `iptables-extensions(8)`: section `esp`

---

# Reference: dccp

## Type
- `match`

## Purpose
Match DCCP (Datagram Congestion Control Protocol) packets by source/destination port and packet type.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `dccp`

## Basic syntax
```bash
iptables -A INPUT -p dccp --dport 5004 -j ACCEPT
```

## Available options / matches
- `--sport port[:port]`
- `--dport port[:port]`
- `--dccp-types type[,type...]`
- `--dccp-option option`

## Use cases by purpose
- `service-exposure`
- `voip`
- `streaming`

## Command examples
```bash
iptables -A INPUT -p dccp --dport 5004 -j ACCEPT
iptables -A INPUT -p dccp -m dccp --dccp-types REQUEST -j LOG --log-prefix "DCCP-REQ "
```

## Dependencies
- requires `-p dccp`

## Limitations and considerations
- DCCP is uncommon in standard deployments; verify kernel support

## Relations with other extensions
- pairs well with: `conntrack`, `LOG`

## Official reference
- `iptables-extensions(8)`: section `dccp`

---

# Reference: sctp

## Type
- `match`

## Purpose
Match SCTP (Stream Control Transmission Protocol) packets by port, chunk type, or flags.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `sctp`

## Basic syntax
```bash
iptables -A INPUT -p sctp --dport 5060 -j ACCEPT
```

## Available options / matches
- `--sport port[:port]`
- `--dport port[:port]`
- `--chunk-types match type[:flags]`

## Use cases by purpose
- `voip`
- `telecom`
- `service-exposure`

## Command examples
```bash
iptables -A INPUT -p sctp --dport 5060 -j ACCEPT
iptables -A INPUT -p sctp -m sctp --chunk-types any INIT -j LOG --log-prefix "SCTP-INIT "
```

## Dependencies
- requires `-p sctp`

## Limitations and considerations
- SCTP multi-homing may complicate stateful tracking
- verify conntrack module support for SCTP

## Relations with other extensions
- pairs well with: `conntrack`, `LOG`

## Official reference
- `iptables-extensions(8)`: section `sctp`

---

# Reference: dst

## Type
- `match`

## Purpose
Match IPv6 packets that contain a Destination Options extension header and optionally inspect specific options within it.

## Scope
- Tool: `ip6tables`
- Common protocols: IPv6 with Destination Options header

## Basic syntax
```bash
ip6tables -A INPUT -m dst --dst-len 24 -j LOG --log-prefix "DST-OPT "
```

## Available options / matches
- `--dst-len length`
- `--dst-opts type[:length]`

## Use cases by purpose
- `ipv6-inspection`
- `hardening`

## Command examples
```bash
ip6tables -A INPUT -m dst --dst-len 24 -j DROP
ip6tables -A INPUT -m dst --dst-opts 0x05:4 -j LOG --log-prefix "DST-HOMEADDR "
```

## Dependencies
- IPv6-specific

## Limitations and considerations
- Destination Options headers are rare in typical traffic; use for specific hardening policies

## Relations with other extensions
- related to: `hbh`, `rt`

## Official reference
- `iptables-extensions(8)`: section `dst`

---

# Reference: hbh

## Type
- `match`

## Purpose
Match IPv6 packets that contain a Hop-by-Hop Options extension header.

## Scope
- Tool: `ip6tables`
- Common protocols: IPv6 with Hop-by-Hop header

## Basic syntax
```bash
ip6tables -A INPUT -m hbh --hbh-len 8 -j LOG --log-prefix "HBH-OPT "
```

## Available options / matches
- `--hbh-len length`
- `--hbh-opts type[:length]`

## Use cases by purpose
- `ipv6-inspection`
- `hardening`

## Command examples
```bash
ip6tables -A INPUT -m hbh --hbh-len 8 -j DROP
ip6tables -A INPUT -m hbh -j LOG --log-prefix "HBH-SEEN "
```

## Dependencies
- IPv6-specific

## Limitations and considerations
- Hop-by-Hop headers must be processed by every router; filtering them can affect path behavior

## Relations with other extensions
- related to: `dst`, `rt`

## Official reference
- `iptables-extensions(8)`: section `hbh`

---

# Reference: mh

## Type
- `match`

## Purpose
Match Mobile IPv6 (MH) headers for mobility signaling messages.

## Scope
- Tool: `ip6tables`
- Common protocols: IPv6 Mobility Header

## Basic syntax
```bash
ip6tables -A INPUT -m mh --mh-type binding-update -j ACCEPT
```

## Available options / matches
- `--mh-type type[:type]`

## Use cases by purpose
- `ipv6-mobility`
- `hardening`

## Command examples
```bash
ip6tables -A INPUT -m mh --mh-type binding-update -j ACCEPT
ip6tables -A INPUT -p mh -j LOG --log-prefix "MH-SEEN "
```

## Dependencies
- requires Mobile IPv6 kernel support

## Limitations and considerations
- only relevant in Mobile IPv6 deployments

## Official reference
- `iptables-extensions(8)`: section `mh`

---

# Reference: rt

## Type
- `match`

## Purpose
Match IPv6 packets that contain a Routing extension header, including type 0 (deprecated) and type 2 (Mobile IPv6).

## Scope
- Tool: `ip6tables`
- Common protocols: IPv6 with Routing header

## Basic syntax
```bash
ip6tables -A INPUT -m rt --rt-type 0 -j DROP
```

## Available options / matches
- `--rt-type type`
- `--rt-segsleft value[:value]`
- `--rt-len length`
- `--rt-0-res`
- `--rt-0-addrs addr[,addr...]`
- `--rt-0-not-strict`

## Use cases by purpose
- `hardening`
- `ipv6-inspection`

## Command examples
```bash
ip6tables -A INPUT -m rt --rt-type 0 -j DROP
ip6tables -A INPUT -m rt --rt-type 0 -j LOG --log-prefix "RH0-SEEN "
ip6tables -A INPUT -m rt --rt-type 2 -j ACCEPT
```

## Dependencies
- IPv6-specific

## Limitations and considerations
- Routing Header Type 0 is deprecated (RFC 5095) due to amplification attack potential; dropping it is recommended

## Relations with other extensions
- related to: `dst`, `hbh`

## Official reference
- `iptables-extensions(8)`: section `rt`
