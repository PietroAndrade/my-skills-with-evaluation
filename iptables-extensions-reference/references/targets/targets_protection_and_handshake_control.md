# Targets Protection and Handshake Control

This file covers the SYNPROXY target, which provides kernel-level SYN cookie protection for TCP connection flood attacks.

---

# Target: SYNPROXY

## Type
- `target`

## Purpose
Intercept TCP SYN packets and perform the three-way handshake with the client in the kernel on behalf of the protected server. Only when the handshake completes successfully is the connection forwarded to the server — protecting against SYN flood DDoS attacks without exposing the server.

## Scope
- Tool: `iptables`
- Valid tables: `raw`, `filter`
- Valid chains: `PREROUTING` (raw), `FORWARD` / `INPUT` (filter)
- Terminating: yes

## Basic syntax
```bash
iptables -t raw -A PREROUTING -p tcp --dport 80 --syn -j SYNPROXY --sack-perm --timestamp --wscale 7 --mss 1460
```

## Available options
- `--sack-perm` — enable Selective ACK (SACK) permission in proxied SYN-ACK
- `--timestamp` — enable TCP timestamps
- `--wscale value` — window scale factor (0–14)
- `--mss value` — MSS to advertise to client
- `--ecn` — enable ECN in proxied SYN-ACK

## Use cases by purpose
- `dos-protection`
- `syn-flood-mitigation`
- `hardening`

## Command examples
```bash
# Full SYNPROXY setup for HTTP on port 80
# Step 1: Drop invalid packets in raw
iptables -t raw -A PREROUTING -p tcp -m tcp --syn -j CT --notrack
# Step 2: Allow untracked new connections through
iptables -A INPUT -p tcp -m tcp --dport 80 -m conntrack --ctstate INVALID,UNTRACKED \
  -j SYNPROXY --sack-perm --timestamp --wscale 7 --mss 1460

# Step 3: Drop remaining invalid
iptables -A INPUT -m conntrack --ctstate INVALID -j DROP

# Simple inline SYNPROXY for FORWARD chain
iptables -t raw -A PREROUTING -i eth0 -p tcp --dport 80 --syn \
  -j SYNPROXY --sack-perm --timestamp --wscale 7 --mss 1460
iptables -t raw -A PREROUTING -i eth0 -p tcp --dport 80 \
  -m conntrack --ctstate INVALID -j DROP
```

## Dependencies
- requires `nf_synproxy_core` and `xt_SYNPROXY` modules
- for `INVALID,UNTRACKED` matching in filter, conntrack must be active
- works best combined with `CT --notrack` in raw and `INVALID` dropping

## Limitations and considerations
- TCP options advertised by SYNPROXY must match what the protected server will accept; mismatch can cause connection failures
- the `--timestamp` option enables PAWS (Protection Against Wrapped Sequence Numbers) — ensure the real server also supports timestamps if enabling
- SYNPROXY absorbs the three-way handshake; the kernel creates a new TCP sequence number for the server-side connection
- high-volume deployments should tune `net.netfilter.nf_conntrack_tcp_timeout_syn_recv` and conntrack table size

## Relations with other extensions
- pairs with: `CT --notrack`, `conntrack` (INVALID state)
- complement: `recent`, `hashlimit`, `connlimit` for rate-based pre-filtering

## Official reference
- `iptables-extensions(8)`: section `SYNPROXY`
