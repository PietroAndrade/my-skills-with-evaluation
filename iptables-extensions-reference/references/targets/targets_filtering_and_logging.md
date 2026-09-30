# Targets Filtering and Logging

This file consolidates final action and observability targets.

---

# Reference: LOG

## Type
- `target`

## Purpose
Record information about matching packets in the kernel log.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any protocol

## Basic syntax
```bash
iptables -A INPUT -j LOG --log-prefix "FW "
```

## Available options / targets
- `--log-level level`
- `--log-prefix prefix`
- `--log-tcp-sequence`
- `--log-tcp-options`
- `--log-ip-options`

## Use cases by purpose
- `anti-dos`
- `logging`
- `debug`
- `incident-response`

## Command examples
```bash
iptables -A INPUT -p tcp --dport 22 -j LOG --log-prefix "SSH-DROP "
iptables -A INPUT -m limit --limit 3/minute --limit-burst 5 -j LOG --log-prefix "FW-DROP "
iptables -A INPUT -p tcp --syn -j LOG --log-prefix "SYN-SEEN " --log-level info
```

## Dependencies
- depends on the kernel logging subsystem

## Limitations and considerations
- `LOG` is non-terminating; processing continues to the next rule
- excessive logging can degrade observability

## Relations with other extensions
- pairs well with: `limit`, `hashlimit`, `conntrack`, `icmp`, `tcp`
- typically used before: `DROP`, `REJECT`

## Official reference
- `iptables-extensions(8)`: section `LOG`

---

# Reference: DROP

## Type
- `target`

## Purpose
Silently discard the packet without sending a response to the sender.

## Basic syntax
```bash
iptables -A INPUT -j DROP
```

## Use cases by purpose
- `anti-dos`
- `hardening`
- `default-deny`

## Command examples
```bash
iptables -A INPUT -m conntrack --ctstate INVALID -j DROP
iptables -A INPUT -p icmp --icmp-type echo-request -j DROP
iptables -A INPUT -p tcp --dport 23 -j DROP
```

## Dependencies
- typically used as the final filtering destination

## Limitations and considerations
- remote troubleshooting may be less transparent
- for an explicit response, `REJECT` may be preferable

## Relations with other extensions
- pairs well with: `LOG`, `conntrack`, `limit`, `hashlimit`

## Official reference
- standard filtering target; see context in `iptables(8)` and related extensions

---

# Reference: REJECT

## Type
- `target`

## Purpose
Deny the packet by sending an appropriate response to the sender.

## Basic syntax
```bash
iptables -A INPUT -p tcp --dport 23 -j REJECT --reject-with tcp-reset
```

## Available options / targets
- `--reject-with type`

## Use cases by purpose
- `service-deny`
- `hardening`
- `user-feedback`

## Command examples
```bash
iptables -A INPUT -p tcp --dport 23 -j REJECT --reject-with tcp-reset
iptables -A INPUT -p udp --dport 161 -j REJECT
iptables -A INPUT -p tcp ! -s 10.10.10.0/24 --dport 22 -j REJECT --reject-with tcp-reset
```

## Dependencies
- type depends on the protocol and compatible options

## Limitations and considerations
- more explicit than `DROP`, but also more visible to the sender

## Relations with other extensions
- pairs well with: `LOG`, `multiport`, `tcp`, `udp`
- silent alternative: `DROP`

## Official reference
- `iptables-extensions(8)`: section `REJECT`