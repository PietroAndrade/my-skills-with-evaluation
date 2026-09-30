# Targets Accounting and Rate Estimation

This file covers the RATEEST target, which creates and updates traffic rate estimators for use by the `rateest` match.

---

# Target: RATEEST

## Type
- `target`

## Purpose
Measure and continuously update the traffic rate (bits per second or packets per second) for packets matching a rule, using an Exponential Weighted Moving Average (EWMA). The resulting estimator can be queried by the `rateest` match for adaptive policy decisions.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `mangle`
- Valid chains: any
- Terminating: no

## Basic syntax
```bash
iptables -t mangle -A FORWARD -o eth0 -j RATEEST \
  --rateest-name wan-out \
  --rateest-interval 250ms \
  --rateest-ewma 0.5s
```

## Available options
- `--rateest-name name` — identifier for this estimator (used by `rateest` match)
- `--rateest-interval time` — measurement interval (e.g., `250ms`, `1s`)
- `--rateest-ewma time` — EWMA time constant (smoothing interval, e.g., `0.5s`, `2s`)

## Use cases by purpose
- `bandwidth-monitoring`
- `adaptive-policy`
- `traffic-shaping`

## Command examples
```bash
# Create estimator for outbound traffic on wan interface
iptables -t mangle -A FORWARD -o eth0 -j RATEEST \
  --rateest-name wan-out \
  --rateest-interval 250ms \
  --rateest-ewma 0.5s

# Create estimator for inbound traffic
iptables -t mangle -A FORWARD -i eth0 -j RATEEST \
  --rateest-name wan-in \
  --rateest-interval 250ms \
  --rateest-ewma 0.5s

# Use the estimator in rateest match to mark high-rate flows
iptables -t mangle -A FORWARD \
  -m rateest --rateest-name1 wan-out \
  --rateest-bps1 --rateest-gt --rateest-bps2 50mbit \
  -j MARK --set-mark 2
```

## Dependencies
- requires `xt_RATEEST` module
- each `--rateest-name` creates a global kernel estimator object; names must be unique per netns

## Limitations and considerations
- EWMA smoothing means very short bursts may not register; shorter `--rateest-ewma` = more reactive, more noisy
- `--rateest-interval` should be ≤ `--rateest-ewma` for meaningful results
- estimator objects persist until the rule is deleted; verify with `iptables -t mangle -L`

## Relations with other extensions
- consumed by: `rateest` match
- pairs well with: `MARK`, `CLASSIFY`

## Official reference
- `iptables-extensions(8)`: section `RATEEST`
