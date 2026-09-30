# Matches Security and IPsec

This file covers the `policy` match, which tests IPsec policy attributes assigned to a packet by the kernel's Security Policy Database (SPD).

---

# Reference: policy

## Type
- `match`

## Purpose
Match packets based on the IPsec policy that was applied (or should be applied) to them, as determined by the kernel's Security Policy Database (SPD). Used to enforce that traffic arrives or departs using the expected IPsec transform.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid chains: `INPUT`, `OUTPUT`, `FORWARD`, `PREROUTING`, `POSTROUTING`
- Common protocols: any (operates on IPsec-encapsulated traffic)

## Basic syntax
```bash
iptables -A INPUT -m policy --dir in --pol ipsec --proto esp -j ACCEPT
```

## Available options / matches

### General
- `--dir {in|out}` — traffic direction relative to the IPsec policy
- `--pol {none|ipsec}` — match packets with no IPsec policy or with one

### ESP/AH specifics (appended with `--next` for multiple requirements)
- `--proto {ah|esp|ipcomp}` — IPsec protocol
- `--spi value[:value]` — Security Parameter Index range
- `--reqid id` — policy request ID (matches configured SA reqid)
- `--tunnel-src addr[/mask]` — tunnel source
- `--tunnel-dst addr[/mask]` — tunnel destination
- `--strict` — all requirements must match (vs. any)
- `--next` — add another policy requirement (AND chain)

## Use cases by purpose
- `ipsec-policy`
- `vpn-filtering`
- `zone-separation`

## Command examples
```bash
# Accept only traffic that arrived via ESP IPsec (enforces no plaintext bypass)
iptables -A INPUT -m policy --dir in --pol ipsec --proto esp -j ACCEPT
iptables -A INPUT -j DROP

# Require IPsec for a specific subnet
iptables -A FORWARD -d 10.10.0.0/24 \
  -m policy --dir out --pol ipsec --proto esp \
  -j ACCEPT

# Drop unencrypted traffic to protected network
iptables -A FORWARD -d 10.10.0.0/24 \
  -m policy --dir out --pol none \
  -j DROP

# Match specific tunnel source
iptables -A INPUT -m policy --dir in --pol ipsec \
  --proto esp --tunnel-src 203.0.113.1 \
  -j ACCEPT

# Reject decapsulated traffic that bypassed IPsec
iptables -A INPUT -m policy ! --dir in --pol ipsec \
  -d 10.20.0.0/16 -j REJECT
```

## Dependencies
- requires IPsec subsystem (`xfrm`) in the kernel
- IPsec SAs and SPs must be configured (via `ip xfrm` or `strongSwan` / `Libreswan`)

## Limitations and considerations
- `--dir in` matches after decapsulation in `INPUT`; `--dir out` matches before encapsulation in `OUTPUT`
- `--pol none` is useful to drop traffic that *should* have been IPsec but arrived plaintext
- `--next` chaining with `--strict` enforces multiple simultaneous SA requirements
- does not create or modify IPsec SAs; only matches against existing policy

## Relations with other extensions
- related to: `ah`, `esp`
- pairs well with: `ACCEPT`, `DROP`, `LOG`

## Official reference
- `iptables-extensions(8)`: section `policy`
