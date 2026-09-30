# Matches Accounting and Rate Estimation

This file covers matches used for traffic accounting, quota enforcement, statistical sampling, and real-time rate estimation: nfacct, quota, rateest, and statistic.

---

# Reference: nfacct

## Type
- `match`

## Purpose
Increment named packet and byte counters stored in the kernel `nfacct` (Netfilter accounting) subsystem and optionally match when a quota threshold is crossed.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A FORWARD -m nfacct --nfacct-name http-bytes -j ACCEPT
```

## Available options / matches
- `--nfacct-name name` — name of the nfacct counter object (must be pre-created with `nfacct add`)

## Use cases by purpose
- `metering`
- `quota`
- `per-service-accounting`

## Command examples
```bash
# Create an nfacct counter
nfacct add http-bytes

# Count HTTP forwarded traffic
iptables -A FORWARD -p tcp --dport 80 -m nfacct --nfacct-name http-bytes -j ACCEPT

# Read counter values
nfacct get http-bytes
```

## Dependencies
- requires `nf_conntrack` + `xt_nfacct` module
- counter objects must be created before rule insertion: `nfacct add <name>`

## Limitations and considerations
- counters persist as long as the iptables rule is active; reset with `nfacct reset <name>`
- quota feature (matching when counter exceeds threshold) requires `nfnetlink_acct` with quota type

## Relations with other extensions
- related to: `quota`, `connbytes`

## Official reference
- `iptables-extensions(8)`: section `nfacct`

---

# Reference: quota

## Type
- `match`

## Purpose
Match packets until a total byte quota is exhausted, then stop matching (quota depleted). Enables soft data caps per firewall rule.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A FORWARD -m quota --quota 10000000 -j ACCEPT
```

## Available options / matches
- `--quota bytes` — byte quota; the match succeeds until the quota is fully consumed

## Use cases by purpose
- `data-cap`
- `metering`

## Command examples
```bash
# Allow up to 10 MB, then drop
iptables -A FORWARD -m quota --quota 10000000 -j ACCEPT
iptables -A FORWARD -j DROP

# Count and cap outbound HTTP at 50 MB
iptables -A OUTPUT -p tcp --dport 80 -m quota --quota 52428800 -j ACCEPT
iptables -A OUTPUT -p tcp --dport 80 -j REJECT
```

## Dependencies
- quota is global to the rule; it is not per-IP or per-connection
- quota state is lost on `iptables -F` or rule removal

## Limitations and considerations
- no per-source IP quota; use `nfacct` with netlink for more granular accounting
- quota state is not persistent across reboots

## Relations with other extensions
- related to: `nfacct`, `connbytes`

## Official reference
- `iptables-extensions(8)`: section `quota`

---

# Reference: rateest

## Type
- `match`

## Purpose
Match packets based on traffic rate estimates computed by the `RATEEST` target, comparing estimated rates between two collectors or against a fixed threshold.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A FORWARD -m rateest --rateest-name1 wan-out --rateest-bps1 --rateest-lt --rateest-bps2 100mbit -j ACCEPT
```

## Available options / matches
- `--rateest-name1 name` — first rate estimator
- `--rateest-name2 name` — second rate estimator (for comparison)
- `--rateest-bps1` / `--rateest-pps1` — use bits-per-second or packets-per-second for estimator 1
- `--rateest-bps2` / `--rateest-pps2` — same for estimator 2
- `--rateest-gt`, `--rateest-lt`, `--rateest-eq` — comparison operator

## Use cases by purpose
- `traffic-shaping`
- `adaptive-policy`
- `bandwidth-monitoring`

## Command examples
```bash
# Rate estimator must be created first with RATEEST target
iptables -t mangle -A FORWARD -j RATEEST --rateest-name wan-out --rateest-interval 250ms --rateest-ewma 0.5s

# Match when estimated outbound rate exceeds 10 Mbps
iptables -A FORWARD -m rateest --rateest-name1 wan-out \
  --rateest-bps1 --rateest-gt --rateest-bps2 10mbit \
  -j MARK --set-mark 2
```

## Dependencies
- requires `xt_RATEEST` and `xt_rateest` modules
- rate estimator must be created via `RATEEST` target before this match can reference it

## Limitations and considerations
- EWMA-based estimation introduces lag; very short bursts may not be captured
- name1 and name2 must reference pre-created estimators

## Relations with other extensions
- created by: `RATEEST` target
- pairs well with: `MARK`, `tc`

## Official reference
- `iptables-extensions(8)`: section `rateest`

---

# Reference: statistic

## Type
- `match`

## Purpose
Match every N-th packet (mode: `nth`) or match randomly with configurable probability (mode: `random`), enabling load distribution, traffic sampling, and probabilistic policies.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A PREROUTING -t nat -m statistic --mode nth --every 2 --packet 0 -j DNAT --to-destination 10.0.0.1
```

## Available options / matches
- `--mode {nth|random}` — distribution mode
- `--every n` — (nth) match one out of every n packets
- `--packet p` — (nth) offset within the n-packet cycle (0 ≤ p < n)
- `--probability p` — (random) probability 0.0–1.0

## Use cases by purpose
- `load-balancing`
- `traffic-sampling`
- `probabilistic-drop`

## Command examples
```bash
# Round-robin 2 backends
iptables -t nat -A PREROUTING -p tcp --dport 80 \
  -m statistic --mode nth --every 2 --packet 0 \
  -j DNAT --to-destination 10.0.0.1:80

iptables -t nat -A PREROUTING -p tcp --dport 80 \
  -m statistic --mode nth --every 2 --packet 1 \
  -j DNAT --to-destination 10.0.0.2:80

# Sample 1% of traffic for logging
iptables -A FORWARD -m statistic --mode random --probability 0.01 \
  -j LOG --log-prefix "SAMPLED "

# Simulate 5% packet loss
iptables -A FORWARD -m statistic --mode random --probability 0.05 -j DROP
```

## Dependencies
- none; uses standard Netfilter counters

## Limitations and considerations
- `nth` mode uses a per-rule global counter; in multi-core environments, this can be non-strictly round-robin
- `random` uses kernel PRNG; not cryptographically random

## Relations with other extensions
- related to: `cluster`, `ipvs`
- pairs well with: `DNAT`, `MARK`

## Official reference
- `iptables-extensions(8)`: section `statistic`
