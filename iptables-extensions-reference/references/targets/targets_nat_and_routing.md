# Targets NAT and Routing

This file consolidates targets for explicit acceptance and NAT/redirect manipulation.

---

# Reference: ACCEPT

## Type
- `target`

## Purpose
Accept the packet and end rule evaluation with a positive decision for that point in the chain.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any protocol

## Basic syntax
```bash
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
```

## Use cases by purpose
- `baseline`
- `ssh`
- `ping`
- `service-exposure`
- `nat`

## Command examples
```bash
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
iptables -A INPUT -p icmp --icmp-type echo-reply -j ACCEPT
```

## Dependencies
- no specific dependency beyond the rule context

## Limitations and considerations
- accepting too early may bypass later controls
- chain order is critical
- documentation must make clear why traffic is being accepted

## Relations with other extensions
- pairs well with virtually all matches
- frequently follows `conntrack`, `tcp`, `icmp`, `time`, and `set`

## Official reference
- standard decision target in `iptables`

---

# Reference: REDIRECT

## Type
- `target`

## Purpose
Redirect the packet to the local machine by altering the destination to a local address.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid table: `nat`
- Valid chains: `PREROUTING`, `OUTPUT` and chains derived from those
- Common protocols: `tcp`, `udp`, `dccp`, `sctp` when used with `--to-ports`

## Basic syntax
```bash
iptables -t nat -A PREROUTING -p tcp --dport 80 -j REDIRECT --to-ports 8080
```

## Available options / targets
- `--to-ports port[-port]`
- `--random`

## Use cases by purpose
- `nat`
- `port-forward`
- `transparent-proxy`
- `local-service-redirection`

## Command examples
```bash
iptables -t nat -A PREROUTING -p tcp --dport 80 -j REDIRECT --to-ports 8080
iptables -t nat -A OUTPUT -p tcp --dport 80 -j REDIRECT --to-ports 3128
iptables -t nat -A PREROUTING -p udp --dport 53 -j REDIRECT --to-ports 5353
```

## Dependencies
- only valid in the `nat` table, in `PREROUTING` and `OUTPUT`, or in chains called exclusively from those
- `--to-ports` requires a compatible protocol such as `tcp` or `udp`

## Limitations and considerations
- redirects to the local machine, not to an arbitrary host
- if the interface does not have the appropriate IP, behavior may not be as expected
- troubleshooting must consider the distinction between `REDIRECT` and `DNAT`

## Relations with other extensions
- pairs well with: `tcp`, `udp`, `multiport`, `LOG`
- related to: `DNAT`, `MASQUERADE`

## Official reference
- `iptables-extensions(8)`: section `REDIRECT`

---

# Reference: MASQUERADE

## Type
- `target`

## Purpose
Apply source NAT automatically using the outgoing interface's IP address, typically in dynamic IP scenarios.

## Scope
- Tool: `iptables`
- Valid table: `nat`
- Valid chain: `POSTROUTING`
- Common protocols: any protocol traversing NAT

## Basic syntax
```bash
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
```

## Available options / targets
- specific options may vary by system and kernel support

## Use cases by purpose
- `nat`
- `internet-sharing`
- `egress-nat`

## Command examples
```bash
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
iptables -t nat -A POSTROUTING -s 192.168.0.0/24 -o ppp0 -j MASQUERADE
iptables -t nat -A POSTROUTING -o wlan0 -j MASQUERADE
```

## Dependencies
- only valid in the `nat` table, `POSTROUTING` chain
- the manual recommends use especially for connections with dynamically assigned IP; with static IP, other approaches such as `SNAT` are usually more appropriate

## Limitations and considerations
- not the best choice for static IP
- troubleshooting must validate outgoing interface and routing
- requires forwarding and associated policies to be correctly configured

## Relations with other extensions
- pairs well with: `FORWARD` rules, `ACCEPT`, source network filters
- related to: `SNAT`, `REDIRECT`

## Official reference
- `iptables-extensions(8)`: section `MASQUERADE`