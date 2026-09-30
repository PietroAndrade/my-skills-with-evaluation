# Targets Security and IPsec

This file covers targets that apply security context labels to connections and packets for use with SELinux and IPsec: CONNSECMARK and SECMARK.

---

# Target: SECMARK

## Type
- `target`

## Purpose
Apply an SELinux security context label to individual packets, enabling SELinux policy to control network traffic at the packet level in the `mangle` table.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `mangle`
- Valid chains: any
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A INPUT -p tcp --dport 80 -j SECMARK --selctx system_u:object_r:http_packet_t:s0
```

## Available options
- `--selctx context` — SELinux security context string

## Use cases by purpose
- `selinux-policy`
- `mandatory-access-control`

## Command examples
```bash
# Label HTTP packets with HTTP type
iptables -t mangle -A INPUT -p tcp --dport 80 \
  -j SECMARK --selctx system_u:object_r:http_packet_t:s0

# Label SSH packets
iptables -t mangle -A INPUT -p tcp --dport 22 \
  -j SECMARK --selctx system_u:object_r:ssh_packet_t:s0
```

## Dependencies
- requires SELinux enabled and `xt_SECMARK` module
- SELinux policy must define the referenced type/context

## Limitations and considerations
- only meaningful on SELinux-enabled systems (RHEL, CentOS, Fedora, etc.)
- labels are applied per packet; use `CONNSECMARK` to propagate labels across all packets of a connection

## Relations with other extensions
- pairs with: `CONNSECMARK`

## Official reference
- `iptables-extensions(8)`: section `SECMARK`

---

# Target: CONNSECMARK

## Type
- `target`

## Purpose
Copy the SELinux security context label between a packet and its connection tracking entry, propagating the label across all packets of the same connection without re-matching each one.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `mangle`
- Valid chains: any
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A INPUT -j CONNSECMARK --save
iptables -t mangle -A INPUT -j CONNSECMARK --restore
```

## Available options
- `--save` — copy packet security label to connection tracking entry
- `--restore` — copy connection tracking entry label to packet

## Use cases by purpose
- `selinux-policy`
- `mandatory-access-control`

## Command examples
```bash
# Standard pattern: label new packets, propagate to connection, restore on subsequent packets
iptables -t mangle -A INPUT -m conntrack --ctstate NEW -j SECMARK --selctx system_u:object_r:http_packet_t:s0
iptables -t mangle -A INPUT -m conntrack --ctstate NEW -j CONNSECMARK --save
iptables -t mangle -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j CONNSECMARK --restore
```

## Dependencies
- requires SELinux enabled, `nf_conntrack`, and `xt_CONNSECMARK`

## Limitations and considerations
- must follow `SECMARK` labeling for new connections
- the save/restore pattern is idiomatic and required for correct SELinux connection labeling

## Relations with other extensions
- requires: `SECMARK`, `conntrack`

## Official reference
- `iptables-extensions(8)`: section `CONNSECMARK`
