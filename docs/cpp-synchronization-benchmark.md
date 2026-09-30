# cpp-synchronization — Review

Qualitative review of the `cpp-synchronization` skill (generates project-idiomatic C++ synchronization code). Three realistic code-generation prompts run through a subagent with the skill + `docs/cpp-conventions.md` loaded; outputs judged for correct primitive, correctness rules, and project idiom.

**Skill:** `~/.claude/skills/cpp-synchronization/SKILL.md`
**Bundled:** `references/examples/*.cpp` (course demos) + `references/cpp17-fallbacks.md` (project-idiomatic semaphore/latch/barrier)
**Baseline C++ standard:** C++17
**Method:** quick review (with-skill only), iteration 1

---

## Test prompts

| # | Task | Correct output |
|---|------|----------------|
| 0 | RouteTable read by many, written rarely (lib `router`) | shared_mutex reader-writer |
| 1 | Cap 4 concurrent downloads, C++17, no permit leak (lib `downloader`) | C++17 semaphore fallback + RAII guard |
| 2 | Threads wait for a one-shot `ready` signal (lib `engine`) | condition_variable + predicate |

---

## Results

| Criterion | eval-0 | eval-1 | eval-2 |
|-----------|--------|--------|--------|
| Correct primitive | ✅ shared_mutex | ✅ counting semaphore fallback | ✅ condition_variable |
| RAII locking only (no manual lock/unlock) | ✅ | ✅ | ✅ |
| `condition_variable::wait` predicate | n/a | ✅ | ✅ |
| `notify` outside lock | n/a | ✅ | ✅ |
| Project idiom (guard/m_/final/namespace/no comments) | ✅ | ✅ | ✅ |
| Used bundled fallback | n/a | ✅ cpp17-fallbacks.md | n/a |
| Correctness extra caught | ✅ copy-under-lock (no dangling ref) | ✅ no permit leak on exception | ✅ flag protected on read too |

## Score

| | with-skill |
|--|-----------|
| Prompts fully correct & idiomatic | **3 / 3** |

## Observations

- Every output followed the concrete-header guard format (`ROUTER_ROUTE_TABLE_H`), `m_` members, `final`, namespace, and emitted **no explanatory comments** — the exact conventions that the earlier cpp-skills benchmark showed a no-skill baseline misses.
- eval-1 pulled the semaphore + `SemaphoreGuard` straight from `references/cpp17-fallbacks.md`, wired the cap via initial count, and explained why the guard prevents a permit leak on exception — the core of the prompt.
- Unprompted correctness catches: eval-0 returned by out-param + copy under lock to avoid a dangling reference after unlock; eval-2 protected the flag on read as well as write.
- All three noted the C++20 migration path where relevant.

**Verdict:** approved, iteration 1. No changes required.

---

## Cross-tier experiment (Haiku, Sonnet)

Same 3 prompts re-run with-skill vs no-skill baseline (baseline forbidden from reading `~/.claude/skills`) on Haiku and Sonnet. To isolate whether *knowing* the conventions equals *following* them, the baselines were **handed the conventions explicitly in the prompt** (namespace=lib, `m_`, guard, `final`, no comments, RAII). This neutralizes the idiom advantage by design, so what remains exposes deeper knowledge gaps.

### Results

| Prompt | with-skill (Haiku/Sonnet) | baseline (Haiku) | baseline (Sonnet) |
|--------|:---:|:---:|:---:|
| RouteTable shared_mutex | ✅ / ✅ | ✅ | ✅ (used C++20 `.contains()`) |
| Semaphore, C++17, 4 permits | ✅ / ✅ (C++17 fallback) | ❌ **C++20 `binary_semaphore`, and `binary_semaphore(4)` is broken (max 1)** | ✅ correct C++17 fallback |
| Ready-gate condvar | ✅ / ✅ | ✅ (namespace `lib`, ignored lib name) | ✅ |

### Findings

1. **The C++17-fallback knowledge is the differentiator, and it matters most on weaker models.** Haiku's no-skill baseline reached for `std::binary_semaphore` (C++20, violating the stated C++17 constraint) *and* used it incorrectly — `binary_semaphore` maxes at 1, not 4, so the download limiter was functionally broken. The with-skill run produced the correct `mutex`+`condition_variable` counting-semaphore fallback. Sonnet's baseline got the fallback right on its own.
2. **Standard slips persist even at Sonnet:** the RouteTable baseline used `m_routes.contains()` (C++20) in a C++17 task.
3. **Idiom adherence is stronger with the skill even when conventions are handed over:** multiple baselines used namespace `lib` instead of the requested lib name (`downloader`/`engine`) and `#pragma once`/`.HPP` instead of the project guard format — despite the conventions being in the prompt. The skill's worked examples anchor the naming more reliably than an instruction does.
4. Minor skill gap: a few with-skill runs omitted `final` on the class — worth reinforcing in the skill's examples.

**Overall:** skill value on `cpp-synchronization` = correct C++17 fallback (biggest on Haiku) + reliable idiom/lib-naming across tiers.

---

## Opus baseline (added 2026-09-25) — three-tier matrix now complete

The cross-tier run above stopped at Haiku and Sonnet. The missing Opus baseline was run on 2026-09-25 under **structural isolation** (project root in a scratch directory that does not contain `~/.claude/skills`, per `docs/cpp-parallel-benchmark-benchmark.md`), then **grep-verified** against the skill's own vocabulary (`cpp-synchronization`, `cpp17-fallbacks`, `SemaphoreGuard`, `choosing-concurrency`, `SKILL.md`, `.claude`) — no hits on any of the three outputs.

Same protocol as the Haiku/Sonnet baselines: **the conventions were handed to the baseline in the prompt** (namespace = lib, `m_`, guard format, `final`, no comments, RAII), so this measures what remains after the idiom advantage is neutralized by design.

### Results

| Criterion | eval-0 RouteTable | eval-1 DownloadLimiter | eval-2 ReadyGate |
|---|:---:|:---:|:---:|
| Correct primitive | ✅ `shared_mutex` | ✅ C++17 counting-semaphore fallback | ✅ condvar + sticky flag |
| Respected the C++17 constraint | ✅ | ✅ (rejected `counting_semaphore` as C++20) | ✅ (rejected `std::latch` as C++20) |
| RAII locking only | ✅ | ✅ | ✅ |
| `wait` with predicate | n/a | ✅ | ✅ |
| `notify` outside the lock | n/a | ✅ | ✅ |
| Permit/wakeup leak prevented | n/a | ✅ nested RAII `Permit`, dtor releases on unwind | ✅ sticky state, idempotent signal |
| Guard name + `m_` + `final` + namespace + no comments | ✅ | ✅ | ✅ |
| Exact `#endif /* GUARD */` form | ❌ used `// GUARD` | ❌ used `// GUARD` | ❌ bare `#endif`, bare `}` |
| Unprompted correctness extra | ✅ out-param copy under lock (no dangling ref); copy+move deleted | ✅ `tryAcquire`, `system_error`-free design, verified peak=4 with 4 throwing downloads | ✅ `mutable` mutex for `isReady() const`; late waiter never blocks |

**Score: 3/3 correct — same as with-skill.**

### What this changes

1. **The C++17-fallback lift is Haiku-only.** Haiku's baseline shipped a functionally broken `binary_semaphore(4)`; Sonnet's and now Opus's baselines both produced a correct `mutex`+`condition_variable` counting semaphore unaided, and Opus named *why* `counting_semaphore` and `latch` were off the table. The capability lift on this skill is concentrated entirely in the weak tier.
2. **Idiom lift at Opus tier is much smaller than the report's headline suggests — when the conventions are in the prompt.** Where Haiku and Sonnet baselines slid back to `namespace lib` and `#pragma once` despite being told otherwise, the Opus baseline honored the guard name, `m_`, `final`, the namespace name and the no-comments rule on all three prompts. What it missed was only the *exact* trailing-comment form (`#endif /* GUARD */`, `} // namespace <lib>`): 0/3, and eval-2 dropped both trailing comments entirely.
3. **Opus's baseline also volunteered a `tryAcquire`, a peak-concurrency assertion and a 20-worker/4-exception test run** — beyond the prompt. Consistent with the benchmark-skill finding that a strong model's *ceiling* rises while its floor does not.

**Honest conclusion for this skill at Opus tier:** lift is limited to the exact trailing-comment convention plus the guarantee of consistency. Substance — right primitive, right standard, right leak-freedom — the Opus baseline already had.
