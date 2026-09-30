# Targets Userspace Handoff

This file covers the NFQUEUE target, which hands packets off to userspace processes for policy decision.

---

# Target: NFQUEUE

## Type
- `target`

## Purpose
Transfer a packet to a userspace process via the Netfilter queue mechanism (`libnetfilter_queue`). The userspace process inspects the packet and returns a verdict (ACCEPT or DROP), enabling application-layer firewall decisions and deep packet inspection.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `filter`, `mangle`, `nat`, `raw`
- Valid chains: any
- Terminating: yes (packet is suspended until userspace verdict)

## Basic syntax
```bash
iptables -A INPUT -p tcp --dport 80 -j NFQUEUE --queue-num 0
```

## Available options
- `--queue-num num` — queue number to send packet to (default: 0)
- `--queue-balance range` — spread packets across a range of queues (e.g., `0:3`)
- `--queue-bypass` — if no userspace process is listening, accept the packet (instead of dropping)
- `--queue-cpu-fanout` — use CPU ID to determine target queue (for per-CPU scaling)

## Use cases by purpose
- `deep-inspection`
- `application-layer-gateway`
- `ids-ips-integration`
- `custom-policy`

## Command examples
```bash
# Queue all HTTP requests for deep inspection
iptables -A FORWARD -p tcp --dport 80 -j NFQUEUE --queue-num 0

# Balance DPI load across 4 queues (for multi-threaded apps)
iptables -A FORWARD -p tcp --dport 443 -j NFQUEUE --queue-balance 0:3

# Fail open if no consumer (safe production mode)
iptables -A INPUT -j NFQUEUE --queue-num 1 --queue-bypass

# Per-CPU fanout for performance
iptables -A FORWARD -j NFQUEUE --queue-balance 0:7 --queue-cpu-fanout
```

## Dependencies
- requires `nfnetlink_queue` module
- userspace application must open and listen on the queue using `libnetfilter_queue`

## Limitations and considerations
- if the queue is full (no consumer or consumer is too slow), new packets to that queue are dropped by default; use `--queue-bypass` to fail open
- introduces latency proportional to userspace processing time
- each queued packet consumes kernel memory; configure queue depth carefully
- without `--queue-bypass`, a crashed consumer causes all matching traffic to be dropped

## Relations with other extensions
- alternative approach: `NFLOG` (log-only, no verdict)
- complement: `CT`, `TRACE` for diagnostics

## Official reference
- `iptables-extensions(8)`: section `NFQUEUE`
