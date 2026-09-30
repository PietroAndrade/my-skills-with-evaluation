# iptables Usage Reference

Table selection (`-t filter/nat/mangle/raw`)

Chain operations (`-A`, `-I`, `-N`, `-F`, `-X`, `-D`)

Match extensions (`-m`): protocol, source, destination, interfaces, ports, time, geoip, connmark, devgroup, multiport, hashlimit, etc.

Target extensions (`-j`): ACCEPT, DROP, REJECT, LOG, NFLOG, CONNMARK, MARK, etc.

An iptables command has this general format:

```
iptables [-t table] {-A|-C|-D} chain rule-specification
iptables [-t table] -I chain [rulenum] rule-specification
iptables [-t table] {-F|-L|-Z} [chain [rulenum]] [options...]
iptables [-t table] -N chain
iptables [-t table] -X [chain]
iptables [-t table] -P chain target
```

Where `rule-specification = [matches...] [-j target]`