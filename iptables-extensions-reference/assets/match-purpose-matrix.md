# Match/target by purpose matrix

| Purpose | Match / Target | Primary use | File |
|---|---|---|---|
| anti-dos | `limit` | Simple global rate limiting | [../references/matches_rate_control.md](../references/matches_rate_control.md) |
| anti-dos | `hashlimit` | Rate limit by source, destination or port | [../references/matches_rate_control.md](../references/matches_rate_control.md) |
| anti-dos | `recent` | Contain repetition and simple brute force | [../references/matches_rate_control.md](../references/matches_rate_control.md) |
| anti-dos | `conntrack` | Filter by connection state | [../references/matches_core.md](../references/matches_core.md) |
| anti-dos | `icmp` | Control `echo-request` and other ICMP types | [../references/matches_core.md](../references/matches_core.md) |
| anti-dos | `LOG` | Log events without ending traversal | [../references/targets_filtering_and_logging.md](../references/targets_filtering_and_logging.md) |
| anti-dos | `DROP` | Silent discard | [../references/targets_filtering_and_logging.md](../references/targets_filtering_and_logging.md) |
| ping | `icmp` | Select specific ICMP types | [../references/matches_core.md](../references/matches_core.md) |
| ping | `limit` | Reduce `echo-request` flood | [../references/matches_rate_control.md](../references/matches_rate_control.md) |
| ping | `LOG` | Log excess or controlled debug | [../references/targets_filtering_and_logging.md](../references/targets_filtering_and_logging.md) |
| ssh | `tcp` | TCP port and flags for SSH | [../references/matches_core.md](../references/matches_core.md) |
| ssh | `conntrack` | Restrict `NEW` and maintain stateful clarity | [../references/matches_core.md](../references/matches_core.md) |
| ssh | `hashlimit` | Rate limit by source | [../references/matches_rate_control.md](../references/matches_rate_control.md) |
| ssh | `recent` | Simple brute force by time window | [../references/matches_rate_control.md](../references/matches_rate_control.md) |
| ssh | `REJECT` | Explicit denial | [../references/targets_filtering_and_logging.md](../references/targets_filtering_and_logging.md) |
| ssh | `DROP` | Silent denial | [../references/targets_filtering_and_logging.md](../references/targets_filtering_and_logging.md) |
| logging | `LOG` | Event logging | [../references/targets_filtering_and_logging.md](../references/targets_filtering_and_logging.md) |
| logging | `limit` | Prevent log flooding | [../references/matches_rate_control.md](../references/matches_rate_control.md) |
| logging | `multiport` | Group ports in one rule | [../references/matches_core.md](../references/matches_core.md) |
| logging | `tcp` | Observability for TCP services | [../references/matches_core.md](../references/matches_core.md) |
| logging | `udp` | Observability for UDP services | [../references/matches_core.md](../references/matches_core.md) |
| nat | `MASQUERADE` | Source NAT with dynamic egress IP | [../references/targets_nat_and_routing.md](../references/targets_nat_and_routing.md) |
| nat | `REDIRECT` | Redirect to local machine | [../references/targets_nat_and_routing.md](../references/targets_nat_and_routing.md) |
| nat | `ACCEPT` | Allow associated flow in filter rules | [../references/targets_nat_and_routing.md](../references/targets_nat_and_routing.md) |
| nat | `set` | Restrict NAT by sets | [../references/matches_policy_and_sets.md](../references/matches_policy_and_sets.md) |
| nat | `time` | Restrict NAT or access time windows | [../references/matches_policy_and_sets.md](../references/matches_policy_and_sets.md) |
| port-forward | `REDIRECT` | Redirect ports to local service | [../references/targets_nat_and_routing.md](../references/targets_nat_and_routing.md) |
| port-forward | `tcp` | Restrict by TCP port | [../references/matches_core.md](../references/matches_core.md) |
| port-forward | `udp` | Restrict by UDP port | [../references/matches_core.md](../references/matches_core.md) |
| port-forward | `multiport` | Group redirect ports | [../references/matches_core.md](../references/matches_core.md) |
| access-control | `time` | Control by time and dates | [../references/matches_policy_and_sets.md](../references/matches_policy_and_sets.md) |
| access-control | `set` | Control by external lists via ipset | [../references/matches_policy_and_sets.md](../references/matches_policy_and_sets.md) |
| inspection | `string` | Simple payload pattern search | [../references/matches_policy_and_sets.md](../references/matches_policy_and_sets.md) |
| legacy-rulesets | `state` | Compatibility with legacy rules | [../references/matches_policy_and_sets.md](../references/matches_policy_and_sets.md) |

## Conventions

- Use `purposes_*` when the starting point is the operational goal.
- Use `matches_*` when the question is technical about the mechanism.
- Use `targets_*` when the question is about the final action of the rule.