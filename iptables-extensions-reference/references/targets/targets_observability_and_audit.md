# Targets Observability and Audit

This file covers targets that produce packet-level observations, audit records, and tracing output: AUDIT, IDLETIMER, LED, NFLOG, and TRACE.

---

# Target: AUDIT

## Type
- `target`

## Purpose
Generate audit log records via the Linux Audit subsystem (`auditd`) for accepted, dropped, or rejected packets.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `filter`, `mangle`, `nat`
- Valid chains: any
- Terminating: no (packet continues after audit record is written)

## Basic syntax
```bash
iptables -A INPUT -j AUDIT --type accept
```

## Available options
- `--type {accept|drop|reject}` — audit record type

## Use cases by purpose
- `compliance`
- `audit`
- `security-monitoring`

## Command examples
```bash
# Audit accepted SSH connections
iptables -A INPUT -p tcp --dport 22 -j AUDIT --type accept
iptables -A INPUT -p tcp --dport 22 -j ACCEPT

# Audit dropped packets from untrusted zone
iptables -A INPUT -s 203.0.113.0/24 -j AUDIT --type drop
iptables -A INPUT -s 203.0.113.0/24 -j DROP
```

## Dependencies
- requires `auditd` running and `xt_AUDIT` kernel module
- audit records go to `/var/log/audit/audit.log`

## Limitations and considerations
- non-terminating; must be followed by the actual ACCEPT/DROP rule
- high-volume traffic can saturate the audit daemon

## Relations with other extensions
- complement to: `LOG`, `NFLOG`

## Official reference
- `iptables-extensions(8)`: section `AUDIT`

---

# Target: IDLETIMER

## Type
- `target`

## Purpose
Create or reset a timer labeled with a given identifier. When no matching packet is seen for the timer's timeout, a notification is sent via the Netlink interface — useful for detecting idle connections and triggering power-saving policies.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `filter`, `mangle`
- Valid chains: any
- Terminating: no

## Basic syntax
```bash
iptables -A INPUT -p tcp --dport 80 -j IDLETIMER --timeout 60 --label "http-idle"
```

## Available options
- `--timeout seconds` — idle timeout in seconds
- `--label string` — identifier for the timer
- `--send_nl_msg {0|1}` — send Netlink notification on timeout (default: 1)

## Use cases by purpose
- `power-management`
- `idle-detection`

## Command examples
```bash
# 60-second idle timer for HTTP traffic
iptables -A FORWARD -p tcp --dport 80 -j IDLETIMER --timeout 60 --label "http-idle"
# 300-second idle timer for active sessions
iptables -A FORWARD -m conntrack --ctstate ESTABLISHED -j IDLETIMER --timeout 300 --label "session-idle"
```

## Dependencies
- requires `xt_IDLETIMER` module
- userspace must listen on Netlink for timeout events

## Limitations and considerations
- timer accuracy depends on kernel timer resolution
- primarily useful in embedded/mobile Linux environments

## Official reference
- `iptables-extensions(8)`: section `IDLETIMER`

---

# Target: LED

## Type
- `target`

## Purpose
Trigger a kernel LED indicator (e.g., a hardware LED on an embedded device) each time a matching packet is processed. Useful for visual packet activity indicators.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `filter`, `mangle`
- Valid chains: any
- Terminating: no

## Basic syntax
```bash
iptables -A INPUT -p icmp -j LED --led-trigger-id "ping-led" --led-delay 1000
```

## Available options
- `--led-trigger-id name` — name of the LED trigger (registered in `ledtrig-netfilter`)
- `--led-delay ms` — how long to keep the LED on after a packet (ms)
- `--led-always-blink` — blink even if already on

## Use cases by purpose
- `embedded-ui`
- `diagnostics`

## Command examples
```bash
# Blink for 500 ms on incoming ICMP
iptables -A INPUT -p icmp -j LED --led-trigger-id "ping" --led-delay 500
```

## Dependencies
- requires `ledtrig-netfilter` and `xt_LED` modules
- LED trigger must be registered and available in `/sys/class/leds/`

## Limitations and considerations
- primarily for embedded / router hardware with physical LEDs
- no security function; purely diagnostic/cosmetic

## Official reference
- `iptables-extensions(8)`: section `LED`

---

# Target: NFLOG

## Type
- `target`

## Purpose
Pass a copy of the packet to a userspace logging daemon via the Netlink `NFLOG` interface. Supports grouping, thresholds, and prefixes. More flexible than `LOG` for high-speed packet capture and protocol analysis.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `filter`, `mangle`, `nat`, `raw`
- Valid chains: any
- Terminating: no

## Basic syntax
```bash
iptables -A INPUT -p tcp --dport 22 -j NFLOG --nflog-group 1 --nflog-prefix "SSH-IN "
```

## Available options
- `--nflog-group num` — Netlink multicast group number (default: 0)
- `--nflog-prefix string` — log prefix (up to 64 bytes)
- `--nflog-range bytes` — copy only the first N bytes of the packet
- `--nflog-threshold num` — buffer N packets before flushing to userspace (default: 1)

## Use cases by purpose
- `observability`
- `security-monitoring`
- `intrusion-detection`
- `packet-capture`

## Command examples
```bash
# Send SSH packets to NFLOG group 1 for external capture
iptables -A INPUT -p tcp --dport 22 -j NFLOG --nflog-group 1 --nflog-prefix "SSH-IN "

# Log dropped packets to NFLOG group 0 with 128-byte capture
iptables -A INPUT -j NFLOG --nflog-group 0 --nflog-prefix "DROP " --nflog-range 128
iptables -A INPUT -j DROP

# Enable session log for conntrack monitoring (NGFW pattern)
iptables -A FORWARD -m conntrack --ctstate NEW -j NFLOG --nflog-group 2 --nflog-prefix "NEW-CONN "
```

## Dependencies
- requires `xt_NFLOG` module
- userspace must consume from the Netlink NFLOG socket (e.g., `ulogd`, `nflog-listener`)

## Limitations and considerations
- non-terminating; packet continues after being queued to NFLOG
- if userspace consumer is slow, packets may be lost (no back-pressure)
- `--nflog-threshold` trades latency for throughput; increase for high-speed logging

## Relations with other extensions
- alternative to: `LOG` (kernel syslog)
- complement: `AUDIT`

## Official reference
- `iptables-extensions(8)`: section `NFLOG`

---

# Target: TRACE

## Type
- `target`

## Purpose
Enable per-packet kernel tracing through all Netfilter tables and chains, logging each verdict decision. Indispensable for diagnosing complex ruleset issues.

## Scope
- Tool: `iptables` / `ip6tables`
- Valid tables: `raw`
- Valid chains: `PREROUTING`, `OUTPUT`
- Terminating: no

## Basic syntax
```bash
iptables -t raw -A PREROUTING -s 192.168.1.100 -j TRACE
```

## Available options
- none (no options beyond the target itself)

## Use cases by purpose
- `diagnostics`
- `troubleshooting`

## Command examples
```bash
# Trace all packets from a specific host
iptables -t raw -A PREROUTING -s 192.168.1.100 -j TRACE

# Trace only SSH packets
iptables -t raw -A PREROUTING -p tcp --dport 22 -j TRACE

# Trace outbound ICMP
iptables -t raw -A OUTPUT -p icmp -j TRACE
```

## Dependencies
- requires `nf_log_ipv4` (or `nf_log_ipv6`) and `ipt_LOG` (for legacy kernel logging)
- trace output appears in `dmesg` or `/proc/net/nf_log` depending on kernel version
- on newer kernels, use `xtables-monitor --trace` to read trace events

## Limitations and considerations
- significant performance overhead; never use in production with broad match criteria
- must be in `raw` table to execute before conntrack and other hooks
- generates extremely verbose log output; limit scope aggressively

## Relations with other extensions
- complement: `LOG`, `NFLOG` for production logging
- TRACE is exclusively a debugging tool

## Official reference
- `iptables-extensions(8)`: section `TRACE`
