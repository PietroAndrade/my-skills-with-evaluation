# Purposes Access and Observability

This file consolidates access and observation operational purposes.

---

# Purpose: ping

## Objective
Group extensions used to control diagnostic ICMP traffic.

## Most common matches
- `icmp`
- `limit`

## Common targets
- `LOG`

## Typical strategies
1. allow rate-controlled `echo-request`
2. allow `echo-reply`
3. allow ICMP types useful for operations
4. log excess in a limited way

## Command examples
```bash
iptables -A INPUT -p icmp --icmp-type echo-request -m limit --limit 5/second --limit-burst 10 -j ACCEPT
iptables -A INPUT -p icmp --icmp-type echo-request -j DROP
iptables -A INPUT -p icmp --icmp-type echo-reply -j ACCEPT
iptables -A INPUT -p icmp --icmp-type destination-unreachable -j ACCEPT
iptables -A INPUT -p icmp --icmp-type time-exceeded -j ACCEPT
iptables -A INPUT -p icmp --icmp-type echo-request -m limit --limit 2/minute --limit-burst 2 -j LOG --log-prefix "PING-EXCESS "
```

## Considerations
- blindly blocking ICMP can break diagnostics
- `LOG` rules must be rate-limited

---

# Purpose: ssh

## Objective
Group extensions and common patterns for exposing SSH securely.

## Most common matches
- `tcp`
- `conntrack`
- `hashlimit`
- `recent`
- `multiport`

## Common targets
- `LOG`
- `DROP`
- `REJECT`

## Typical strategies
1. allow SSH only from authorized sources
2. restrict new connections per source
3. contain simple brute force
4. log relevant attempts without flooding

## Command examples
```bash
iptables -A INPUT -p tcp -s 10.10.10.0/24 --dport 22 -m conntrack --ctstate NEW -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -m conntrack --ctstate NEW -m hashlimit --hashlimit-upto 10/minute --hashlimit-burst 10 --hashlimit-mode srcip --hashlimit-name ssh_new -j ACCEPT
iptables -A INPUT -p tcp --dport 22 -j DROP
iptables -A INPUT -p tcp --dport 22 -m recent --name sshbrute --set
iptables -A INPUT -p tcp --dport 22 -m recent --name sshbrute --update --seconds 60 --hitcount 10 -j DROP
iptables -A INPUT -p tcp ! -s 10.10.10.0/24 --dport 22 -m limit --limit 5/minute --limit-burst 10 -j LOG --log-prefix "SSH-OUTSIDE "
iptables -A INPUT -p tcp ! -s 10.10.10.0/24 --dport 22 -j REJECT --reject-with tcp-reset
```

## Considerations
- restricting by source is better than opening SSH globally
- `REJECT` aids troubleshooting; `DROP` is more silent

---

# Purpose: logging

## Objective
Group extensions and patterns for logging relevant firewall events in a controlled manner.

## Most common matches
- `limit`
- `hashlimit`
- `conntrack`
- `icmp`
- `tcp`
- `udp`
- `multiport`

## Common targets
- `LOG`
- `DROP`
- `REJECT`

## Typical strategies
1. log only useful events
2. always rate-limit log rules
3. add clear prefixes
4. position the log before the final action when necessary

## Command examples
```bash
iptables -A INPUT -m limit --limit 3/minute --limit-burst 5 -j LOG --log-prefix "FW-DROP "
iptables -A INPUT -p tcp --syn -m limit --limit 10/minute --limit-burst 20 -j LOG --log-prefix "SYN-SEEN "
iptables -A INPUT -p icmp --icmp-type echo-request -m limit --limit 2/minute --limit-burst 2 -j LOG --log-prefix "PING-EXCESS "
iptables -A INPUT -p tcp -m multiport --dports 22,23,25,3306 -m limit --limit 5/minute --limit-burst 10 -j LOG --log-prefix "ADMIN-PORTS "
iptables -A INPUT -p tcp --dport 23 -m limit --limit 3/minute --limit-burst 5 -j LOG --log-prefix "TELNET-DROP "
iptables -A INPUT -p tcp --dport 23 -j DROP
```

## Considerations
- too many logs hurt more than they help
- `LOG` does not end traversal; it normally needs to be followed by `DROP` or `REJECT`