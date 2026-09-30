# Targets Clustering and Distribution

This file covers the CLUSTERIP target, which implements stateless cluster-based load distribution at Layer 3.

---

# Target: CLUSTERIP

## Type
- `target`

## Purpose
Configure a cluster of hosts sharing a single IP and MAC address. Each node uses a consistent hash to determine which packets to handle, enabling stateless load distribution without a dedicated load balancer.

## Scope
- Tool: `iptables`
- Valid tables: `filter`
- Valid chains: `INPUT`
- Terminating: yes

## Basic syntax
```bash
iptables -A INPUT -p tcp --dport 80 -d 192.168.1.100 \
  -j CLUSTERIP --new --hashmode sourceip \
  --clustermac 01:00:5e:00:00:20 \
  --total-nodes 2 --local-node 1
```

## Available options
- `--new` — create a new CLUSTERIP instance (first rule only)
- `--hashmode {sourceip|sourceip-sourceport|sourceip-sourceport-destport}` — hash key
- `--clustermac mac` — multicast MAC address used by all cluster nodes
- `--total-nodes n` — total number of nodes in the cluster
- `--local-node n` — this node's ordinal (1-based)
- `--hash-init seed` — RNG seed for the hash

## Use cases by purpose
- `ha-cluster`
- `load-distribution`

## Command examples
```bash
# Node 1 of 2: accept its share of HTTP traffic
iptables -A INPUT -p tcp --dport 80 -d 192.168.1.100 \
  -j CLUSTERIP --new --hashmode sourceip \
  --clustermac 01:00:5e:00:00:20 \
  --total-nodes 2 --local-node 1

# Node 2 of 2: same rule with --local-node 2 (no --new)
iptables -A INPUT -p tcp --dport 80 -d 192.168.1.100 \
  -j CLUSTERIP --hashmode sourceip \
  --clustermac 01:00:5e:00:00:20 \
  --total-nodes 2 --local-node 2
```

## Dependencies
- all nodes must join the multicast group for the shared MAC
- ARP must be configured to respond on all nodes for the shared IP (`arptables` or similar)

## Limitations and considerations
- stateless: if a node fails, the hash mapping changes and existing connections may be disrupted
- largely superseded by more robust solutions like Keepalived + LVS or Kubernetes services
- requires careful ARP and MAC management across all cluster nodes
- `--new` must appear only on the first matching rule that creates the CLUSTERIP entry

## Relations with other extensions
- related to: `cluster` match, `statistic`

## Official reference
- `iptables-extensions(8)`: section `CLUSTERIP`
