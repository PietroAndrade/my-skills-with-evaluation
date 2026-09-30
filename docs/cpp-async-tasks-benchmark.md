# cpp-async-tasks — Review

Qualitative review of the `cpp-async-tasks` skill (task-based parallelism: async/future/promise/packaged_task, fan-out, fork-join, thread pool). Three realistic prompts run through a subagent with the skill + `docs/cpp-conventions.md` loaded; judged for correct task mechanism, avoidance of the `std::async` traps, and project idiom.

**Skill:** `~/.claude/skills/cpp-async-tasks/SKILL.md`
**Bundled:** `references/examples/*.cpp` (course demos) + `references/thread-pool.md` (C++17 pool)
**Baseline C++ standard:** C++17
**Method:** quick review (with-skill only), iteration 1

---

## Test prompts

| # | Task | Correct output |
|---|------|----------------|
| 0 | 200 I/O-bound downloads, sum bytes, C++17 | thread pool (200 > "dozens"), launch-all-then-gather |
| 1 | Parallel sum of billions via divide-and-conquer (lib `compute`) | depth-limited fork-join |
| 2 | `std::async` in a loop, no speedup — why? | discarded-future-blocks + default launch policy |

---

## Results

| Criterion | eval-0 | eval-1 | eval-2 |
|-----------|--------|--------|--------|
| Correct mechanism | ✅ thread pool | ✅ fork-join | ✅ (diagnosis) |
| Passed `std::launch::async` explicitly | ✅ | ✅ | ✅ (fix) |
| launch-all-then-gather (no future discarded mid-loop) | ✅ | ✅ right half inline | ✅ (the fix) |
| Depth/size threshold on recursion | n/a | ✅ log2(hw) + size cutoff | n/a |
| Results via futures, no shared mutable state | ✅ | ✅ disjoint slices | ✅ |
| Project idiom (guard/m_/namespace/no comments) | ✅ | ✅ | ✅ |
| Routed to pool/benchmark where relevant | ✅ | ✅ pool alternative | ✅ pool |

## Score

| | with-skill |
|--|-----------|
| Prompts fully correct | **3 / 3** |

## Observations

- Both `std::async` footguns — the destructor of a discarded future blocking (accidental serialization) and the default policy possibly running `deferred` — were caught and explained in eval-2, and pre-empted in evals 0/1.
- eval-1 applied the depth threshold (`log2(hardware_concurrency)`) *and* a size cutoff, ran the right half inline to avoid doubling thread count, and added an unprompted `int64_t` overflow note.
- eval-0 correctly judged 200 items as beyond `std::async` fan-out territory and reached for the bundled thread pool, sizing workers for I/O-bound work (`hw*8`).
- All three kept result flow in futures with no mutex, exactly the skill's "prefer futures over shared state" stance.

**Verdict:** approved, iteration 1. No changes required.

---

## Cross-tier experiment (Haiku, Sonnet)

Same 3 prompts re-run with-skill vs no-skill baseline (baseline forbidden from reading `~/.claude/skills`) on Haiku and Sonnet. Note: unlike the `cpp-synchronization` cross-tier run, the async baselines were **not** handed the project conventions, so idiom differences here are expected and not the focus — correctness and pattern choice are.

### Results

| Prompt | with-skill (Haiku/Sonnet) | baseline (Haiku) | baseline (Sonnet) |
|--------|:---:|:---:|:---:|
| 200 I/O downloads | ✅ / ✅ pool + futures/gather | ✅ pool + atomic counter | ✅ pool + futures |
| Divide-and-conquer sum | ✅ / ✅ depth=log2(hw), stateless | ❌ **data race**: shared `config` by ref, `current_depth++` across threads | ✅ clean (stateless depth) |
| `std::async` loop is slow | ✅ / ✅ both traps | ✅ both traps | ✅ both traps |

### Findings

1. **The famous `std::async` trap has near-zero skill lift.** All four configurations (Haiku + Sonnet, with-skill + baseline) correctly diagnosed the discarded-future-blocks + default-launch-policy bug and applied launch-all-then-gather. This footgun is well-documented enough that models already know it — an honest result: the skill adds little on its most-cited rule.
2. **Divide-and-conquer surfaced a real concurrency bug in the weakest baseline.** Haiku's no-skill version threaded a mutable `ParallelConfig&` through recursive `std::async` calls and did `config.current_depth++` from multiple threads — a data race in the very code meant to parallelize safely. The with-skill runs (and Sonnet's baseline) pass depth as a by-value parameter, which is race-free by construction. The skill's stateless-recursion pattern prevents a bug an unguided weak model actively introduced.
3. **Fan-out instinct is shared across tiers:** every configuration reached for a thread pool at 200 items rather than 200 raw threads. The skill's contribution here is the bundled pool + project idiom + futures-over-shared-state (some baselines summed into a shared `std::atomic` instead of gathering futures).

**Overall:** skill value on `cpp-async-tasks` is narrower than on the other generators — the headline traps are common knowledge — but it still (a) prevents the divide-and-conquer data race on weaker models, (b) enforces futures-over-shared-state and project idiom, and (c) ships a correct reusable pool. Lowest-lift of the three generators; still net positive.

---

## Opus baseline (added 2026-09-25) — three-tier matrix now complete

Missing Opus baseline run 2026-09-25 under **structural isolation** (scratch project root outside `~/.claude/skills`) and **grep-verified** (`cpp-async-tasks`, `thread-pool.md`, `cpp-parallel`, `SKILL.md`, `.claude`, `cpp-conventions`) — clean on all three. Same protocol as the Haiku/Sonnet baselines: conventions **not** handed over, so idiom gaps are expected and not the point.

### Results

| Criterion | eval-0 200 downloads | eval-1 divide-and-conquer | eval-2 `std::async` loop |
|---|:---:|:---:|:---:|
| Correct mechanism | ✅ bounded pool + futures | ✅ depth-limited fork-join | ✅ diagnosis |
| `std::launch::async` explicit | ✅ | ✅ (named `deferred` as the reason) | ✅ (the fix) |
| launch-all-then-gather | ✅ | ✅ right half inline | ✅ |
| **Depth bound on recursion** | n/a | ✅ `ceil(log2(hw))`, `remainingDepth` **passed by value** | n/a |
| Size cutoff | n/a | ✅ 65536 | n/a |
| Results via futures, no shared mutable state | ✅ (sum in the gather loop) | ✅ disjoint raw ranges | ✅ |
| Both `std::async` traps named | ✅ pre-empted | ✅ | ✅ + `[[nodiscard]]` |
| Project idiom | ❌ `.hpp`, guard `THREAD_POOL_HPP`, no namespace, no lib dir (has `m_`) | ❌ `.hpp`, no `m_` (free functions), no project guard | ❌ flat `.cpp` files |
| Unprompted extra | pool oversubscribed `max(32, hw*4)` for blocking I/O; per-task exception at `get()`; measured 899 ms vs ~55 s | `std::system_error` fallback sums inline so thread exhaustion costs speed not correctness; int64 wraparound regroups identically | 4 programs incl. a reusable `parallel::transform` with per-item error capture; measured 1604 ms → 201 ms |

**Score: 3/3 correct.**

### What this changes

1. **The divide-and-conquer data race is Haiku-only.** This was the skill's single strongest correctness win. Haiku's baseline threaded a mutable `ParallelConfig&` and did `config.current_depth++` across threads; the Opus baseline independently arrived at the stateless form — `remainingDepth - 1u` passed **by value** down the recursion — which is race-free by construction, and additionally handled `std::async` throwing `system_error`. Sonnet's baseline was already clean. So this skill's correctness lift is now measured as **weak-tier only, 0 at Sonnet and Opus.**
2. **Zero lift on the headline traps is confirmed at the top tier.** 6/6 configurations across all three tiers diagnosed the discarded-future + default-policy pair. The Opus baseline went further and cited `[[nodiscard]]`.
3. **Idiom remains the whole lift at Opus tier, and here it is large** — because, unlike the `cpp-synchronization` run, the conventions were *not* in the prompt: 0/3 on file suffix, guard format, namespace and lib directory layout.

**Revised verdict:** `cpp-async-tasks` is the lowest-lift skill of the three generators, and the Opus data narrows it further. Remaining value at strong tiers: project idiom and layout, plus the bundled pool as a ready artifact. Its correctness rules are floor insurance for weak models, not capability transfer for strong ones.
