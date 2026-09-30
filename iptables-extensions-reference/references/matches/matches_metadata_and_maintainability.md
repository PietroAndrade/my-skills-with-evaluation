# Matches Metadata and Maintainability

This file covers matches that add documentation, metadata, or human-readable annotations to iptables rules without affecting packet filtering logic.

---

# Reference: comment

## Type
- `match`

## Purpose
Attach a human-readable comment to an iptables rule. The comment is stored in the kernel with the rule and is visible in `iptables -L -v` output, aiding maintainability of complex rulesets.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: any

## Basic syntax
```bash
iptables -A INPUT -s 10.0.0.0/8 -m comment --comment "Allow internal LAN" -j ACCEPT
```

## Available options / matches
- `--comment text` — string up to 256 characters

## Use cases by purpose
- `maintainability`
- `documentation`
- `audit`

## Command examples
```bash
# Document a rule with its origin ticket
iptables -A INPUT -p tcp --dport 22 -m comment --comment "TICKET-1234: Allow SSH from ops" -j ACCEPT

# Tag a firewall zone rule for parsing automation
iptables -A FORWARD -s 192.168.1.0/24 -m comment --comment "zone=trusted src=lan" -j ACCEPT

# Annotate a DROP with reason
iptables -A INPUT -s 192.0.2.0/24 -m comment --comment "Blocked: known scanner range" -j DROP
```

## Dependencies
- none; the match always evaluates to true (it is a no-op filter)

## Limitations and considerations
- the comment does not affect packet matching; it is purely informational
- maximum 256 characters; longer strings are silently truncated or rejected
- `iptables-restore` respects comments; `iptables-save` preserves them
- avoid using characters that conflict with shell quoting in scripts

## Relations with other extensions
- pairs well with: any rule that benefits from documentation

## Official reference
- `iptables-extensions(8)`: section `comment`
