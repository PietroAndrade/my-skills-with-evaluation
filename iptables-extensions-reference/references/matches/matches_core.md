# Matches Core

This file consolidates protocol, port, and state matches.

---

# Reference: conntrack

## Type
- `match`

## Purpose
Match connection tracking metadata, especially states like `NEW`, `ESTABLISHED`, `RELATED`, and `INVALID`.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any tracked protocol

## Basic syntax
```bash
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
```

## Available options / matches
- `--ctstate state`

## Use cases by purpose
- `anti-dos`
- `baseline`
- `ssh`
- `stateful-firewall`

## Command examples
```bash
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A INPUT -m conntrack --ctstate INVALID -j DROP
iptables -A INPUT -p tcp --dport 22 -m conntrack --ctstate NEW -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -m conntrack --ctstate NEW -m hashlimit --hashlimit-upto 10/minute --hashlimit-burst 10 --hashlimit-mode srcip --hashlimit-name ssh_new -j ACCEPT
```

## Dependencies
- requires conntrack to be active on the system

## Limitations and considerations
- `conntrack` does not replace rate limiting
- `INVALID` and `NEW` rules must consider evaluation order

## Relations with other extensions
- pairs well with: `hashlimit`, `limit`, `tcp`, `udp`, `LOG`

## Official reference
- `iptables-extensions(8)`: section `conntrack`

---

# Reference: icmp

## Type
- `match`

## Purpose
Select specific ICMP types in IPv4, such as `echo-request`, `echo-reply`, `destination-unreachable`, and `time-exceeded`.

## Scope
- Tool: `iptables`
- Common protocols: `icmp` IPv4

## Basic syntax
```bash
iptables -A INPUT -p icmp --icmp-type echo-request -j ACCEPT
```

## Available options / matches
- `--icmp-type {type[/code]|typename}`

## Use cases by purpose
- `ping`
- `anti-dos`
- `network-diagnostics`

## Command examples
```bash
iptables -A INPUT -p icmp --icmp-type echo-request -j ACCEPT
iptables -A INPUT -p icmp --icmp-type echo-reply -j ACCEPT
iptables -A INPUT -p icmp --icmp-type destination-unreachable -j ACCEPT
iptables -A INPUT -p icmp --icmp-type time-exceeded -j ACCEPT
iptables -A INPUT -p icmp --icmp-type echo-request -m limit --limit 5/second --limit-burst 10 -j ACCEPT
iptables -A INPUT -p icmp --icmp-type echo-request -j DROP
```

## Dependencies
- requires `-p icmp`

## Limitations and considerations
- covers IPv4 ICMP only
- blindly blocking ICMP can break diagnostics and operations

## Relations with other extensions
- pairs well with: `limit`, `LOG`

## Official reference
- `iptables-extensions(8)`: section `icmp`

---

# Reference: tcp

## Type
- `match`

## Purpose
Match specific attributes of TCP traffic, such as ports, flags, and options.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `tcp`

## Basic syntax
```bash
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
```

## Available options / matches
- `--sport port[:port]`
- `--dport port[:port]`
- `--tcp-flags mask comp`
- `--syn`
- `--tcp-option number`

## Use cases by purpose
- `ssh`
- `anti-dos`
- `service-exposure`
- `logging`

## Command examples
```bash
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
iptables -A INPUT -p tcp --syn -m limit --limit 30/second --limit-burst 60 -j ACCEPT
iptables -A INPUT -p tcp --syn -j DROP
iptables -A INPUT -p tcp --tcp-flags SYN,ACK,FIN,RST SYN -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -m limit --limit 5/minute --limit-burst 5 -j LOG --log-prefix "TCP22 "
```

## Dependencies
- requires `-p tcp`

## Limitations and considerations
- `--syn` is useful for new connections, but does not replace stateful logic
- rules with TCP flags require care to avoid blocking legitimate traffic

## Relations with other extensions
- pairs well with: `conntrack`, `hashlimit`, `multiport`, `LOG`

## Official reference
- `iptables-extensions(8)`: section `tcp`

---

# Reference: udp

## Type
- `match`

## Purpose
Match UDP traffic attributes, mainly source and destination ports.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `udp`

## Basic syntax
```bash
iptables -A INPUT -p udp --dport 53 -j ACCEPT
```

## Available options / matches
- `--sport port[:port]`
- `--dport port[:port]`

## Use cases by purpose
- `dns`
- `service-exposure`
- `logging`
- `observability`

## Command examples
```bash
iptables -A INPUT -p udp --dport 53 -j ACCEPT
iptables -A INPUT -p udp --dport 123 -j ACCEPT
iptables -A INPUT -p udp -m limit --limit 5/minute --limit-burst 10 -j LOG --log-prefix "UDP-SEEN "
iptables -A INPUT -p udp -s 192.0.2.50 --dport 514 -j ACCEPT
```

## Dependencies
- requires `-p udp`

## Limitations and considerations
- UDP has no session state like TCP
- exposing UDP without source restrictions may increase the attack surface

## Relations with other extensions
- pairs well with: `multiport`, `LOG`, `conntrack`

## Official reference
- `iptables-extensions(8)`: section `udp`

---

# Reference: multiport

## Type
- `match`

## Purpose
Match multiple TCP or UDP ports in a single rule.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `tcp`, `udp`

## Basic syntax
```bash
iptables -A INPUT -p tcp -m multiport --dports 22,80,443 -j ACCEPT
```

## Available options / matches
- `--sports`
- `--dports`
- `--ports`

## Use cases by purpose
- `service-exposure`
- `ssh`
- `web`
- `dns`
- `logging`

## Command examples
```bash
iptables -A INPUT -p tcp -m multiport --dports 22,80,443 -j ACCEPT
iptables -A INPUT -p tcp -m multiport --dports 21,22,23,25 -m limit --limit 5/minute --limit-burst 10 -j LOG --log-prefix "SENSITIVE-PORTS "
iptables -A INPUT -p tcp -s 10.10.10.0/24 -m multiport --dports 22,8443 -j ACCEPT
iptables -A INPUT -p tcp -m multiport --dports 53,80,443 -j ACCEPT
```

## Dependencies
- must be used with a compatible protocol
- supports up to 15 ports per rule

## Limitations and considerations
- good for small groups
- does not replace segmentation by source, state, or rate

## Relations with other extensions
- pairs well with: `tcp`, `udp`, `conntrack`, `LOG`

## Official reference
- `iptables-extensions(8)`: section `multiport`