# Matches Rate Control

This file consolidates rate limiting and abuse control matches.

---

# Reference: limit

## Type
- `match`

## Purpose
Control match rate using a token bucket.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `icmp`, `tcp`, `udp`

## Basic syntax
```bash
iptables -A INPUT -m limit --limit 10/second --limit-burst 20 -j ACCEPT
```

## Available options / matches
- `--limit rate[/second|/minute|/hour|/day]`
- `--limit-burst number`

## Use cases by purpose
- `anti-dos`
- `ping`
- `logging`

## Command examples
```bash
iptables -A INPUT -p icmp --icmp-type echo-request -m limit --limit 5/second --limit-burst 10 -j ACCEPT
iptables -A INPUT -p icmp --icmp-type echo-request -j DROP
iptables -A INPUT -m limit --limit 3/minute --limit-burst 5 -j LOG --log-prefix "FW-DROP "
iptables -A INPUT -p tcp --syn -m limit --limit 30/second --limit-burst 60 -j ACCEPT
iptables -A INPUT -p tcp --syn -j DROP
```

## Dependencies
- typically combined with `LOG` or a subsequent `DROP` rule

## Limitations and considerations
- controls the rule rate, not per individual host
- for per-source control, `hashlimit` is usually better

## Relations with other extensions
- pairs well with: `icmp`, `LOG`, `tcp`, `udp`
- more granular alternative: `hashlimit`

## Official reference
- `iptables-extensions(8)`: section `limit`

---

# Reference: hashlimit

## Type
- `match`

## Purpose
Perform rate limiting per group using hash buckets.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `tcp`, `udp`, `icmp`

## Basic syntax
```bash
iptables -A INPUT -m hashlimit --hashlimit-upto 100/second --hashlimit-burst 20 --hashlimit-mode srcip --hashlimit-name per_src -j ACCEPT
```

## Available options / matches
- `--hashlimit-upto amount`
- `--hashlimit-above amount`
- `--hashlimit-burst amount`
- `--hashlimit-mode {srcip|srcport|dstip|dstport},...`
- `--hashlimit-name name`
- `--hashlimit-srcmask prefix`
- `--hashlimit-dstmask prefix`

## Use cases by purpose
- `anti-dos`
- `ssh`
- `rate-limit`
- `ping`

## Command examples
```bash
iptables -A INPUT -p tcp --syn -m hashlimit --hashlimit-upto 30/second --hashlimit-burst 60 --hashlimit-mode srcip --hashlimit-name syn_per_src -j ACCEPT
iptables -A INPUT -p tcp --syn -j DROP
iptables -A INPUT -p tcp --dport 22 -m conntrack --ctstate NEW -m hashlimit --hashlimit-upto 10/minute --hashlimit-burst 10 --hashlimit-mode srcip --hashlimit-name ssh_per_src -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -j DROP
iptables -A INPUT -s 10.0.0.0/8 -m hashlimit --hashlimit-upto 10000/minute --hashlimit-mode srcip --hashlimit-srcmask 28 --hashlimit-name subnet_rate -j ACCEPT
```

## Dependencies
- requires `--hashlimit-name`
- requires proper rate and mode configuration

## Limitations and considerations
- more granular than `limit`, but more complex
- wrong `mode` choice can cause unexpected behavior

## Relations with other extensions
- pairs well with: `conntrack`, `tcp`, `udp`, `LOG`
- simpler alternative: `limit`

## Official reference
- `iptables-extensions(8)`: section `hashlimit`

---

# Reference: recent

## Type
- `match`

## Purpose
Maintain a recent address list and allow rules based on repetition, time window, and hit count.

## Scope
- Tool: `iptables` / `ip6tables`
- Common protocols: `tcp`, `udp`, `icmp`

## Basic syntax
```bash
iptables -A INPUT -m recent --name testlist --set
iptables -A INPUT -m recent --name testlist --update --seconds 60 --hitcount 10 -j DROP
```

## Available options / matches
- `--name name`
- `--set`
- `--rcheck`
- `--update`
- `--remove`
- `--seconds value`
- `--hitcount value`
- `--rsource`
- `--rdest`

## Use cases by purpose
- `anti-dos`
- `ssh`
- `brute-force`
- `observability`

## Command examples
```bash
iptables -A INPUT -p tcp --dport 22 -m recent --name sshbrute --set
iptables -A INPUT -p tcp --dport 22 -m recent --name sshbrute --update --seconds 60 --hitcount 10 -j DROP
iptables -A INPUT -p icmp --icmp-type echo-request -m recent --name pingburst --set
iptables -A INPUT -p icmp --icmp-type echo-request -m recent --name pingburst --update --seconds 10 --hitcount 20 -j DROP
iptables -A INPUT -s 192.0.2.10 -m recent --name sshbrute --remove
```

## Dependencies
- uses `xt_recent`
- state can be inspected via `/proc/net/xt_recent/`

## Limitations and considerations
- poorly tuned thresholds can cause false positives
- maintenance can become complex in larger environments

## Relations with other extensions
- pairs well with: `tcp`, `icmp`, `LOG`
- rate/group alternative: `hashlimit`

## Official reference
- `iptables-extensions(8)`: section `recent`