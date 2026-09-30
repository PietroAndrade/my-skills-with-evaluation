# Matches Clustering and Distribution

This file covers matches used in high-availability clustering and load-balancing scenarios: IPVS integration and CLUSTERIP/cluster packet distribution.

---

# Reference: cluster

## Type
- `match`

## Purpose
Select one node from a cluster to handle a packet by hashing source/destination address and port, enabling stateless load balancing without a dedicated load balancer.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `tcp`, `udp`

## Basic syntax
```bash
iptables -A INPUT -m cluster --cluster-total-nodes 2 --cluster-local-node 1 --cluster-hash-seed 0xdeadbeef -j ACCEPT
```

## Available options / matches
- `--cluster-total-nodes number`
- `--cluster-local-node node_number`
- `--cluster-hash-seed value`
- `--cluster-src-mac` — hash on source MAC instead of source IP

## Use cases by purpose
- `ha-cluster`
- `load-distribution`

## Command examples
```bash
# Node 1 of 2 accepts its share of traffic
iptables -A INPUT -m cluster \
  --cluster-total-nodes 2 \
  --cluster-local-node 1 \
  --cluster-hash-seed 0xdeadbeef \
  -j ACCEPT
iptables -A INPUT -j DROP

# Node 2 of 2
iptables -A INPUT -m cluster \
  --cluster-total-nodes 2 \
  --cluster-local-node 2 \
  --cluster-hash-seed 0xdeadbeef \
  -j ACCEPT
iptables -A INPUT -j DROP
```

## Dependencies
- all cluster nodes must use the same `--cluster-hash-seed`
- requires multicast or shared IP for cluster VIP

## Limitations and considerations
- stateless; if a node fails, its traffic is lost until re-hash or reconfiguration
- superseded by `IPVS` in many modern deployments
- `--cluster-src-mac` requires packets to stay on the same Layer 2 segment

## Relations with other extensions
- related to: `statistic` (for simpler round-robin distribution)

## Official reference
- `iptables-extensions(8)`: section `cluster`

---

# Reference: ipvs

## Type
- `match`

## Purpose
Match packets that have been processed by the Linux Virtual Server (IPVS) subsystem, allowing filtering based on IPVS routing decisions such as virtual service, real server, scheduler, or connection state.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `tcp`, `udp`

## Basic syntax
```bash
iptables -A INPUT -m ipvs --ipvs -j ACCEPT
```

## Available options / matches
- `--ipvs` — match any IPVS packet
- `--vaddr addr[/mask]` — match virtual service address
- `--vport port` — match virtual service port
- `--vproto proto` — match virtual service protocol
- `--vdir {ORIGINAL|REPLY}` — match traffic direction
- `--vmethod {GATE|IPIP|MASQ}` — match IPVS forwarding method
- `--vportctl port` — match virtual service control port

## Use cases by purpose
- `ha-cluster`
- `load-distribution`
- `policy-by-service`

## Command examples
```bash
# Accept all IPVS-scheduled packets
iptables -A INPUT -m ipvs --ipvs -j ACCEPT

# Log IPVS masquerade traffic for a specific service
iptables -A FORWARD -m ipvs --vaddr 10.0.0.1 --vport 80 --vmethod MASQ \
  -j LOG --log-prefix "IPVS-HTTP "

# Accept IPVS direct routing traffic
iptables -A INPUT -m ipvs --vmethod GATE -j ACCEPT
```

## Dependencies
- requires `ip_vs` kernel module and at least one active virtual service
- IPVS must have already made a routing decision before this match is evaluated

## Limitations and considerations
- must be used after IPVS has processed the packet (typically in `FORWARD` or `INPUT` after PREROUTING IPVS hook)
- direction flags (`ORIGINAL`/`REPLY`) are relative to the IPVS connection table

## Relations with other extensions
- pairs well with: `conntrack`, `MARK`
- related to: `cluster`

## Official reference
- `iptables-extensions(8)`: section `ipvs`
