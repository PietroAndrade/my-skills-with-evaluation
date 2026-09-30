# Purposes Protection

This file consolidates defensive operational purposes.

---

# Purpose: anti-dos

## Objective
Group extensions commonly used to mitigate rate abuse, bursts, and excess new connections.

## Most common matches
- `limit`
- `hashlimit`
- `recent`
- `conntrack`
- `icmp`

## Common targets
- `LOG`
- `DROP`

## Typical strategies
1. limit simple volumetric traffic
2. limit new connections per source
3. contain abnormal repetition within short windows
4. log excess without generating a log flood
5. discard abusive traffic after control

## Command examples
```bash
iptables -A INPUT -p icmp --icmp-type echo-request -m limit --limit 5/second --limit-burst 10 -j ACCEPT
iptables -A INPUT -p icmp --icmp-type echo-request -j DROP

iptables -A INPUT -p tcp --syn -m hashlimit --hashlimit-upto 30/second --hashlimit-burst 60 --hashlimit-mode srcip --hashlimit-name syn_rate -j ACCEPT
iptables -A INPUT -p tcp --syn -j DROP

iptables -A INPUT -p tcp --dport 22 -m conntrack --ctstate NEW -m hashlimit --hashlimit-upto 10/minute --hashlimit-burst 10 --hashlimit-mode srcip --hashlimit-name ssh_new -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -j DROP

iptables -A INPUT -p tcp --dport 22 -m recent --name sshbrute --set
iptables -A INPUT -p tcp --dport 22 -m recent --name sshbrute --update --seconds 60 --hitcount 10 -j DROP

iptables -A INPUT -m limit --limit 3/minute --limit-burst 5 -j LOG --log-prefix "ANTI-DOS "
```

## Considerations
- thresholds vary widely by environment
- `limit` is simple and global
- `hashlimit` is better for per-host or per-subnet control
- logs without rate limiting can become a problem themselves

## Recommended relations
- use `conntrack` to separate `ESTABLISHED`, `RELATED`, `NEW`, and `INVALID`
- use `limit` for simple cases
- use `hashlimit` for per-source/destination criteria
- use `LOG` sparingly