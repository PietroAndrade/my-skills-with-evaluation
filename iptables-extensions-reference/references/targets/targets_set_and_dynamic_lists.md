# Targets Set and Dynamic Lists

This file covers the SET target, which enables dynamic modification of ipset members from within firewall rules.

---

# Target: SET

## Type
- `target`

## Purpose
Add, delete, or test membership of packet header fields in an `ipset` set from within iptables rules, enabling dynamic block/allow lists that can be updated without reloading the full ruleset.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `filter`, `mangle`, `nat`
- Valid chains: any
- Terminating: no

## Basic syntax
```bash
iptables -A FORWARD -p tcp --dport 80 -j SET --add-set http-visitors src
```

## Available options
- `--add-set setname flag[,flag...]` — add the packet's src/dst/src,dst to the set
- `--del-set setname flag[,flag...]` — remove packet's field from the set
- `--map-set setname flag[,flag...]` — map set value to mark/dscp/priority
- `--exist` — if adding, do not fail if entry already exists
- `--timeout seconds` — timeout for the added entry (0 = permanent)

### Flag values
- `src` — source IP
- `dst` — destination IP
- `src,dst` — source/destination pair (for hash types)

## Use cases by purpose
- `dynamic-blacklist`
- `dynamic-allowlist`
- `rate-limiting`
- `honeypot`

## Command examples
```bash
# Add source IP to a blacklist set when it triggers a DROP rule
iptables -A INPUT -p tcp --dport 22 -m recent --name ssh-scan --rcheck --seconds 30 --hitcount 10 \
  -j SET --add-set ssh-blacklist src

# Dynamic blocking: drop packets from the blacklist
iptables -A INPUT -m set --match-set ssh-blacklist src -j DROP

# Track active HTTP visitors
iptables -A FORWARD -p tcp --dport 80 -j SET --add-set http-visitors src --exist

# Remove entry from set when connection closes
iptables -A INPUT -m conntrack --ctstate ESTABLISHED \
  -j SET --del-set http-visitors src

# Add with timeout (auto-expire after 60 seconds)
iptables -A INPUT -p tcp --dport 23 -j SET --add-set telnet-attempts src --timeout 60
```

## Dependencies
- requires `ipset` package and `xt_SET` module
- the set must be created with `ipset create` before use
- set type must be compatible with the add/del flags used

## Limitations and considerations
- `--add-set` is non-terminating; pair with `DROP`/`LOG` as needed
- `--exist` prevents failures when adding duplicate entries to hash sets
- sets are volatile by default; use `ipset save`/`ipset restore` for persistence
- set type (`hash:ip`, `hash:net`, `hash:ip,port`, etc.) must match the elements being added

## Relations with other extensions
- complement: `set` match
- pairs well with: `recent`, `conntrack`, `LOG`

## Official reference
- `iptables-extensions(8)`: section `SET`
