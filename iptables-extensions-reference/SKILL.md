---
name: iptables-extensions-reference
description: Structured reference for agents on iptables-extensions matches and targets, organized by operational purpose and by extension.
reference: https://man7.org/linux/man-pages/man8/iptables-extensions.8.html
---

# Iptables Extensions Reference Skill

## When to invoke this skill

Invoke this skill whenever you are:

- **writing a new iptables rule** and need to choose the right match (`-m`) or target (`-j`)
- **debugging an existing rule** — understanding side effects, dependencies, or why a rule is not matching as expected
- **choosing between alternatives** — e.g., `MASQUERADE` vs `SNAT`, `LOG` vs `NFLOG`, `conntrack` vs `state`
- **designing a policy** around a specific purpose such as anti-DDoS, VPN enforcement, transparent proxy, or QoS
- **validating a rule** — checking table/chain compatibility, required kernel modules, and caveats before applying
- **generating iptables rules** from data (e.g., from a database-driven firewall builder) and need to match the correct syntax

## How to use this skill

### Decision workflow

1. **Start from the goal (recommended)**
   → Go to a [purpose file](#by-purpose) that matches your scenario (e.g., "I need rate limiting" → `protection`).
   → The purpose file lists the recommended matches and targets with ready-to-use examples.

2. **Start from a known extension name**
   → Jump directly to the appropriate [match](#by-match) or [target](#by-target) file.
   → Each entry documents: syntax, all options, use-case tags, examples, dependencies, limitations, and relations.

3. **Start from a use-case tag**
   → Tags like `dos-protection`, `policy-routing`, `transparent-proxy`, `vpn-filtering` appear in every entry.
   → Search the references for the tag to find all relevant extensions.

### Command anatomy

See the [usage reference](references/usage.md) for the general command format. In summary:

```
iptables [-t table] -A chain [matches...] -j target
```

| Component | Flag | Example |
|---|---|---|
| Table | `-t` | `-t mangle` (default: `filter`) |
| Chain operation | `-A`, `-I`, `-D` | `-A FORWARD` |
| Match extension | `-m name [options]` | `-m conntrack --ctstate NEW` |
| Target | `-j TARGET [options]` | `-j LOG --log-prefix "DROP "` |

### Table × Chain quick reference

| Table | Chains available | Typical use |
|---|---|---|
| `filter` | INPUT, FORWARD, OUTPUT | Allow / drop / reject |
| `nat` | PREROUTING, POSTROUTING, OUTPUT | Address/port translation |
| `mangle` | All 5 chains | Mark, TTL, TOS, DSCP, CONNMARK |
| `raw` | PREROUTING, OUTPUT | Conntrack bypass (NOTRACK, CT), TRACE |

> **Rule of thumb for targets:**
> - `DNAT` / `REDIRECT` → `nat PREROUTING`
> - `SNAT` / `MASQUERADE` → `nat POSTROUTING`
> - `MARK` / `CONNMARK` / `DSCP` / `TTL` → `mangle`
> - `NOTRACK` / `CT` / `TRACE` → `raw PREROUTING`
> - `LOG` / `DROP` / `ACCEPT` / `REJECT` → `filter` (any chain)

### Common patterns

```bash
# Stateful allow (most common filter pattern)
iptables -A FORWARD -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
iptables -A FORWARD -m conntrack --ctstate NEW -s <trusted> -j ACCEPT
iptables -A FORWARD -j DROP

# Port forwarding (DNAT + MASQUERADE)
iptables -t nat -A PREROUTING -i eth0 -p tcp --dport 80 -j DNAT --to-destination 10.0.0.5:80
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE

# Rate limiting (SYN flood protection)
iptables -A INPUT -p tcp --syn -m hashlimit \
  --hashlimit-above 20/sec --hashlimit-mode srcip \
  --hashlimit-name syn-flood -j DROP

# Policy routing via mark
iptables -t mangle -A PREROUTING -p tcp --dport 443 -j MARK --set-mark 0x2
iptables -t mangle -A PREROUTING -j CONNMARK --save-mark
# (then: ip rule add fwmark 0x2 table 200)
```

## How to navigate

### By purpose
- [protection](references/purposes_protection.md)
- [access and observability](references/purposes_acess_and_observability.md)
- [nat and forwarding](references/purposes_nat_and_forwarding.md)

### By match
- [core matches](references/matches_core.md)
- [rate control matches](references/matches_rate_control.md)
- [policy and sets matches](references/matches_policy_and_sets.md)
- [classification and routing](references/matches_classification_and_routing.md)
- [protocol specific](references/matches_protocol_specific.md)
- [advanced inspection](references/matches_advanced_inspection.md)
- [system and process context](references/matches_system_and_process_context.md)
- [clustering and distribution](references/matches_clustering_and_distribution.md)
- [metadata and maintainability](references/matches_metadata_and_maintainability.md)
- [conntrack advanced](references/matches_conntrack_advanced.md)
- [mark and classification matches](references/matches_mark_and_classification.md)
- [qos and header fields](references/matches_qos_and_header_fields.md)
- [packet shape and fragmentation](references/matches_packet_shape_and_fragmentation.md)
- [layer2 and bridging](references/matches_layer2_and_bridging.md)
- [accounting and rate estimation matches](references/matches_accounting_and_rate_estimation.md)
- [security and ipsec matches](references/matches_security_and_ipsec.md)

### By target
- [filtering and logging targets](references/targets_filtering_and_logging.md)
- [nat and routing targets](references/targets_nat_and_routing.md)
- [observability and audit](references/targets_observability_and_audit.md)
- [packet rewrite and normalization](references/targets_packet_rewrite_and_normalization.md)
- [mark and classification targets](references/targets_mark_and_classification.md)
- [clustering and distribution targets](references/targets_clustering_and_distribution.md)
- [security and ipsec targets](references/targets_security_and_ipsec.md)
- [conntrack control](references/targets_conntrack_control.md)
- [nat and address translation](references/targets_nat_and_address_translation.md)
- [userspace handoff](references/targets_userspace_handoff.md)
- [accounting and rate estimation targets](references/targets_accounting_and_rate_estimation.md)
- [set and dynamic lists](references/targets_set_and_dynamic_lists.md)
- [protection and handshake control](references/targets_protection_and_handshake_control.md)
- [traffic duplication and mirroring](references/targets_traffic_duplication_and_mirroring.md)

## Current scope of this skill

### Covered matches
- `conntrack`, `icmp`, `tcp`, `udp`, `multiport`
- `limit`, `hashlimit`, `recent`
- `state`, `set`, `string`, `time`
- `addrtype`, `devgroup`, `iprange`, `rpfilter`
- `ah`, `dccp`, `dst`, `esp`, `hbh`, `mh`, `rt`, `sctp`
- `bpf`, `osf`, `u32`
- `cgroup`, `cpu`, `owner`, `socket`
- `cluster`, `ipvs`
- `comment`
- `connbytes`, `connlabel`, `connlimit`
- `connmark`, `mark`
- `dscp`, `ecn`, `hl`, `tos`, `ttl`
- `frag`, `length`, `tcpmss`
- `mac`, `physdev`, `pkttype`
- `nfacct`, `quota`, `rateest`, `statistic`
- `policy`

### Covered targets
- `LOG`, `DROP`, `REJECT`, `ACCEPT`, `REDIRECT`, `MASQUERADE`
- `AUDIT`, `IDLETIMER`, `LED`, `NFLOG`, `TRACE`
- `CHECKSUM`, `ECN`, `HL`, `TCPMSS`, `TCPOPTSTRIP`, `TOS`, `TTL`
- `CLASSIFY`, `CONNMARK`, `HMARK`, `MARK`
- `CLUSTERIP`
- `CONNSECMARK`, `SECMARK`
- `CT`, `NOTRACK`
- `DNAT`, `DNPT`, `NETMAP`, `SAME`, `SNAT`, `SNPT`
- `NFQUEUE`
- `RATEEST`
- `SET`
- `SYNPROXY`
- `TEE`

### Covered purposes
- `anti-dos`, `ping`, `ssh`, `logging`, `nat`, `port-forward`
