# Matches Policy and Sets

This file consolidates additional matches for compatibility, time-based policies, payload inspection, and set integration.

---

# Reference: state

## Type
- `match`

## Purpose
Match connection states like `NEW`, `ESTABLISHED`, `RELATED`, and `INVALID` using the classic state matching interface.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any protocol tracked by conntrack

## Basic syntax
```bash
iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT
```

## Available options / matches
- `--state state`

## Use cases by purpose
- `baseline`
- `ssh`
- `anti-dos`
- `legacy-rulesets`

## Command examples
```bash
iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT
iptables -A INPUT -m state --state INVALID -j DROP
iptables -A INPUT -p tcp --dport 22 -m state --state NEW -j ACCEPT
```

## Dependencies
- requires connection tracking
- in newer documentation, `conntrack` is preferred over `state`

## Limitations and considerations
- useful for compatibility with legacy rules
- for new rulesets, prefer `conntrack`
- avoid mixing `state` and `conntrack` unnecessarily within the same example

## Relations with other extensions
- related to: `conntrack`
- may appear in old rules still in production

## Official reference
- `iptables-extensions(8)`: section `state`

---

# Reference: set

## Type
- `match`

## Purpose
Match elements stored in sets managed by `ipset`, enabling more scalable rules by IP, network, port, or combinations.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any protocol, depending on the set type

## Basic syntax
```bash
iptables -A INPUT -m set --match-set admin_hosts src -p tcp --dport 22 -j ACCEPT
```

## Available options / matches
- `--match-set setname flag[,flag...]`
- typical flags indicate which part of the packet to query, such as `src` and `dst`

## Use cases by purpose
- `ssh`
- `anti-dos`
- `allowlist`
- `blocklist`
- `segmentation`

## Command examples
```bash
iptables -A INPUT -m set --match-set admin_hosts src -p tcp --dport 22 -j ACCEPT
iptables -A INPUT -m set --match-set blocked_nets src -j DROP
iptables -A INPUT -p tcp -m set --match-set exposed_ports dst -j ACCEPT
```

## Dependencies
- requires sets previously defined with `ipset(8)`
- set format must be compatible with the fields evaluated in the rule

## Limitations and considerations
- requires a clear lifecycle for creation, update, and persistence of sets
- troubleshooting requires knowing which set was queried and how it is maintained
- documentation must indicate the set name and expected type

## Relations with other extensions
- pairs well with: `tcp`, `udp`, `multiport`, `LOG`
- useful to replace long IP lists in individual rules

## Official reference
- `iptables-extensions(8)`: section `set`

---

# Reference: string

## Type
- `match`

## Purpose
Match text patterns in packet payload, useful for specific simple content inspection scenarios.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: typically protocols with readable payload, such as some TCP or UDP flows

## Basic syntax
```bash
iptables -A INPUT -m string --string "GET " --algo bm -j LOG
```

## Available options / matches
- `--string pattern`
- `--hex-string pattern`
- `--algo {bm|kmp}`
- `--from offset`
- `--to offset`

## Use cases by purpose
- `logging`
- `inspection`
- `policy-enforcement`
- `legacy-filtering`

## Command examples
```bash
iptables -A INPUT -p tcp -m string --string "GET " --algo bm -j LOG --log-prefix "HTTP-GET "
iptables -A INPUT -p tcp -m string --string "User-Agent:" --algo kmp -j LOG --log-prefix "HTTP-UA "
iptables -A INPUT -p tcp -m string --hex-string "|16 03|" --algo bm -j LOG --log-prefix "TLS-SEEN "
```

## Dependencies
- requires algorithm choice: `bm` or `kmp`, as documented in `iptables-extensions(8)`

## Limitations and considerations
- may have relevant performance cost
- does not replace deep inspection tools
- sensitive to fragmentation, encapsulation, and traffic characteristics
- use with caution in high-volume environments

## Relations with other extensions
- pairs well with: `LOG`, `tcp`, `udp`
- generally a niche option, not baseline

## Official reference
- `iptables-extensions(8)`: section `string`

---

# Reference: time

## Type
- `match`

## Purpose
Match packets based on time range, date, weekdays, or month days.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any protocol

## Basic syntax
```bash
iptables -A INPUT -m time --weekdays Mon,Tue,Wed,Thu,Fri --timestart 08:00 --timestop 18:00 -j ACCEPT
```

## Available options / matches
- `--datestart YYYY[-MM[-DD[Thh[:mm[:ss]]]]]`
- `--datestop YYYY[-MM[-DD[Thh[:mm[:ss]]]]]`
- `--timestart hh:mm[:ss]`
- `--timestop hh:mm[:ss]`
- `--monthdays day[,day...]`
- `--weekdays day[,day...]`
- `--contiguous`
- `--kerneltz`

## Use cases by purpose
- `access-control`
- `business-hours`
- `maintenance-window`
- `policy-enforcement`

## Command examples
```bash
iptables -A INPUT -p tcp --dport 22 -m time --weekdays Mon,Tue,Wed,Thu,Fri --timestart 08:00 --timestop 18:00 -j ACCEPT
iptables -A INPUT -m time --weekdays Sa,Su -j LOG --log-prefix "WEEKEND-TRAFFIC "
iptables -A INPUT -m time --datestart 2026-12-24 --datestop 2026-12-27 -j ACCEPT
iptables -A INPUT -m time --timestart 23:00 --timestop 02:00 --contiguous -j LOG --log-prefix "NIGHT-WINDOW "
```

## Dependencies
- times are interpreted in UTC by default
- the manual advises against `--kerneltz` because the kernel timezone may be incorrect or outdated

## Limitations and considerations
- requires attention to UTC vs local time
- windows crossing midnight may need `--contiguous`
- time-based rules must be well documented to avoid operational ambiguity

## Relations with other extensions
- pairs well with: `tcp`, `udp`, `LOG`, `set`
- useful in access policies restricted by time

## Official reference
- `iptables-extensions(8)`: section `time`