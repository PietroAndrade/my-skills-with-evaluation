# Purposes NAT and Forwarding

This file consolidates purposes related to NAT, local redirection, and simple forwarding.

---

# Purpose: nat

## Objective
Group extensions and patterns for address translation and traffic adaptation between networks, especially egress to the Internet and local redirections.

## Most common matches
- `tcp`
- `udp`
- `multiport`
- `set`
- `time`

## Common targets
- `ACCEPT`
- `REDIRECT`
- `MASQUERADE`

## Typical strategies
1. apply source NAT on egress to Internet
2. redirect ports to local services
3. restrict NAT rules by protocol, port, source, or time
4. clearly document the table and chain used

## Command examples
```bash
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
iptables -t nat -A PREROUTING -p tcp --dport 80 -j REDIRECT --to-ports 8080
iptables -t nat -A OUTPUT -p tcp --dport 80 -j REDIRECT --to-ports 3128
iptables -A FORWARD -p tcp --dport 80 -j ACCEPT
```

## Considerations
- `MASQUERADE` is more appropriate for dynamic IP
- `REDIRECT` does not forward to another host; it redirects to the local machine
- NAT without coherent filter rules typically complicates troubleshooting
- always specify table, chain, and interface

---

# Purpose: port-forward

## Objective
Group patterns for intercepting traffic destined for a port and redirecting it to a local service, or preparing rules related to service forwarding/acceptance.

## Most common matches
- `tcp`
- `udp`
- `multiport`
- `set`
- `time`

## Common targets
- `REDIRECT`
- `ACCEPT`

## Typical strategies
1. intercept connections in `PREROUTING`
2. redirect to an alternate local port
3. keep clear documentation of the real service listening on the destination port
4. optionally combine with source or time filters

## Command examples
```bash
iptables -t nat -A PREROUTING -p tcp --dport 80 -j REDIRECT --to-ports 8080
iptables -t nat -A PREROUTING -p udp --dport 53 -j REDIRECT --to-ports 5353
iptables -t nat -A PREROUTING -p tcp -m multiport --dports 80,443 -j REDIRECT --to-ports 8443
iptables -A INPUT -p tcp --dport 8080 -j ACCEPT
```

## Considerations
- differentiate local redirection from DNAT to another host
- validate that the real service is listening on the destination port
- document whether the rule is for transparent proxy, captive portal, or local service
- troubleshooting must validate `nat` table, correct chain, and active local service