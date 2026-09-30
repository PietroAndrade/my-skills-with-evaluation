# cpp-concurrency-debug — Review

Qualitative review of the `cpp-concurrency-debug` skill (symptom → cause → fix for C++ concurrency bugs). Three realistic bug-report prompts run through a subagent with the skill loaded; outputs judged for correct symptom classification, correct root cause, and correct fix.

**Skill:** `~/.claude/skills/cpp-concurrency-debug/SKILL.md`
**Bundled examples:** `references/examples/{deadlock,livelock,starvation,data_race,race_condition,abandoned_lock}/` (begin = buggy, end = fixed, from the course corpus)
**Baseline C++ standard:** C++17
**Method:** quick review (with-skill only), iteration 1

---

## Test prompts

| # | Symptom given | Correct diagnosis |
|---|----------------|-------------------|
| 0 | Freeze, CPU ~0%, 2 threads sharing 2 mutexes | Deadlock (circular wait) |
| 1 | Mutex added, TSan clean, result still varies run to run | Race condition (not data race) |
| 2 | Freeze after a thread does an early `return` | Abandoned lock |

---

## Results

| Criterion | eval-0 | eval-1 | eval-2 |
|-----------|--------|--------|--------|
| Correct symptom classification | ✅ deadlock | ✅ race condition | ✅ abandoned lock |
| Used the CPU-usage discriminator (0% vs 100%) | ✅ (vs livelock) | n/a | ✅ |
| Correct root cause | ✅ opposite lock order | ✅ ordering, not access | ✅ return skips unlock |
| Correct fix | ✅ scoped_lock + order | ✅ barrier/latch/redesign | ✅ RAII guard |
| Ruled out the wrong fix | ✅ recursive_mutex won't help | ✅ more mutex won't help | ✅ manual lock/unlock = red flag |
| Routed to sibling skill | — | ✅ cpp-synchronization | ✅ (RAII guard) |

## Score

| | with-skill |
|--|-----------|
| Prompts fully correct | **3 / 3** |

## Observations

- The data-race-vs-race-condition distinction — the hardest and most commonly confused — was nailed: eval-1 correctly explained why ThreadSanitizer stays silent (ordering flaw, not access flaw) and that adding more mutex does nothing.
- CPU-usage discriminator (0% = deadlock, 100% = livelock) was applied unprompted.
- Minor: eval-1 proposed `std::barrier` (C++20) without noting the C++17 fallback. The skill defers fallbacks to `cpp-synchronization`, so this is acceptable; a one-word "(C++20)" tag on the barrier/latch row could reinforce it.

**Verdict:** approved, iteration 1. Optional minor tweak noted above.

---

## First baseline (added 2026-09-25) — lift is now measured, and it is zero at Opus tier

The review above was with-skill only: 3/3, but **no lift was measurable**. The missing baseline was run 2026-09-25 on **Opus**, same three symptom reports, under **structural isolation** (scratch project root outside `~/.claude/skills`) and **grep-verified** (`cpp-concurrency-debug`, `cpp-synchronization`, `choosing-concurrency`, `SKILL.md`, `.claude`) — clean on all three.

### Results

| Criterion | eval-0 deadlock | eval-1 race condition | eval-2 abandoned lock |
|---|:---:|:---:|:---:|
| Correct symptom classification | ✅ AB-BA lock-order inversion | ✅ race condition, explicitly "no data race" | ✅ lost unlock on early return |
| Used the CPU-usage discriminator | ✅ ~0% = parked in kernel, not spinning | n/a | ✅ distinguished from lock-order deadlock (the returning thread keeps running) |
| Correct root cause | ✅ opposite lock order in the two functions | ✅ mutex gives exclusion, not ordering | ✅ `return` skips `unlock()`; also wedges if `push_back` throws |
| Correct fix | ✅ `std::scoped_lock(m_accounts, m_ledger)` | ✅ per-thread partials over **static** index ranges, merged in fixed order | ✅ `lock_guard`, validate before locking |
| Ruled out the wrong fixes | ✅ 11 non-fixes with reasons | ✅ 9 non-fixes with reasons | ✅ named `unlock()` before `return` as the wrong fix |
| Empirically reproduced | ✅ buggy hangs (`timeout 5` → 124), fixed runs 200k iters/thread; `/proc/*/wchan` = `futex_do_wait`, `utime=0` | ✅ 3 runs of drifting low bits beside a stable fixed column, **TSan clean** | ✅ buggy killed by timeout, fixed exits 0, TSan clean |
| Named the confirmation tooling | ✅ gdb `thread apply all bt`, helgrind lock-order, TSan | ✅ TSan silence as *evidence*, not false negative | ✅ gdb (all threads in `mutex::lock`, `__owner` = idle tid), helgrind |

**Score: baseline 3/3 — identical to with-skill. Measured lift: zero on all three prompts.**

### Where the baseline went *beyond* the with-skill run

- **eval-1 supplied the mechanism the with-skill run only gestured at.** The skill run said "ordering, not access" and proposed `std::barrier`/latch/redesign. The baseline named the actual cause — floating-point `+` is commutative but **not associative**, so schedule-dependent grouping moves the low bits — produced a repro whose buggy column drifts while TSan stays silent, and identified the *shape* of the fingerprint (bounded low-bit jitter = reduction order; torn or wildly wrong = a real race). It also rejected the tempting `std::atomic<double>`/`seq_cst` fix with the right reason: memory ordering is visibility, not a predetermined order.
- **eval-1 also caught a trap the skill's answer walks into:** a condition_variable or token that forces threads to take turns *does* restore determinism, but serializes the program. Per-thread partials over static ranges get the same determinism with real parallelism. The skill's "barrier/latch" suggestion is on the wrong side of that line.
- **eval-0 enumerated 11 non-fixes** (recursive_mutex, yield/sleep, atomics/volatile, condvar, shared_lock, naive `try_lock` retry, `timed_mutex`, fixing the order in only one function, …), each with the reason it fails — a superset of the skill run's three.

### Consequence

`cpp-concurrency-debug` has **no demonstrated lift at Opus tier.** Its correctness is confirmed (3/3), but an unaided Opus matched or beat it on every prompt. Two honest readings, both worth recording:

1. The skill may still hold value at weaker tiers (untested here) and as a floor guarantee — the baseline's quality was high but it is one sample per prompt.
2. Two of its answers are now known to be **improvable**: eval-1's fix should be per-thread partials over static ranges (deterministic *and* parallel) rather than a barrier, and the FP-associativity mechanism plus the "low-bit jitter vs torn value" discriminator belong in the skill. That is the same pattern as the DCE-barrier finding on `cpp-parallel-benchmark`: a clean baseline paying for itself by exposing a gap in the skill.

**Action taken (2026-09-25, iteration 2):** applied. See "Iteration 2 — changes applied" at the end of this document.

---

## Haiku tier (added 2026-09-25) — the lift the Opus run could not see

The Opus run above found zero lift and raised the question the §6 thesis predicts: does the value show up on a weak model? Answered by running **both arms at Haiku tier** — 3 baselines (structural isolation, grep-verified clean) and 3 with-skill, same prompts. Running both arms matters: comparing a Haiku baseline against an Opus with-skill run would confound tier with skill.

### Results

| Prompt | baseline Haiku | with-skill Haiku | lift |
|---|:---:|:---:|---|
| eval-0 deadlock | ✅ correct | ✅ correct | **small** — see below |
| eval-1 race condition | ❌ **wrong** | ✅ correct | **real** |
| eval-2 abandoned lock | ✅ correct | ✅ correct | **zero** |

**Baseline 2/3 · with-skill 3/3.**

### eval-1 — the one real correctness lift found anywhere in this skill

The Haiku **baseline** led with the wrong answer and ranked the right one below it:

> "**race condition on the final read** … you're almost certainly reading the final result *without* holding the lock" — cause #1, "most likely".

That is a **data race**, which contradicts the evidence handed to it (TSan silent). Its headline fix — lock the final read — changes nothing. Worse, it then certified the bug as safe:

> "`result += value;  // Addition is commutative`" listed under **COMMUTATIVE (safe, order doesn't matter)**.

For floating point that is exactly inverted: `+` is commutative, and the problem is that it is **not associative**. Its own rule blesses the broken code. It also made a false claim about the instrument ("TSan has zero way to know if you're forgetting to lock a read that happens sequentially after all writes") — if the writes are joined there is no race to find; if they are not, TSan does flag it.

The **with-skill** run classified it correctly on the first line — "race condition, not a data race … the bug is in the logic, not the synchronization" — explained why extra mutexes cannot help, and offered compute-locally-then-merge-single-threaded as a fix, which is the same shape as the Opus baseline's answer.

**This is the data-race vs race-condition table in the skill doing exactly the job it was written for, on the tier that needs it.**

### eval-0 and eval-2 — small and zero

Both configurations diagnosed eval-0 (AB-BA) and eval-2 (abandoned lock) correctly. Differences on eval-0 were real but not correctness:

- with-skill reached for `std::scoped_lock(m_accounts, m_ledger)`; the baseline offered the pre-C++17 `std::lock` + `defer_lock` form.
- with-skill produced the skill's explicit rule-out table (livelock burns CPU / starvation shows uneven progress / abandoned lock affects only dependents / data race gives wrong numbers, not hangs).
- with-skill named the naive `try_lock` retry as **livelock at 100% CPU**; the baseline listed timeouts and sleeps but not that trap.

Both used the 0%-CPU discriminator unaided. On eval-2 the answers are equivalent.

### Revised verdict for `cpp-concurrency-debug`

| Tier | Lift |
|---|---|
| Haiku | **Real, 1 of 3 prompts** — prevents a wrong diagnosis whose recommended fix is a no-op, and whose stated rule certifies the bug as safe |
| Opus | **Zero** — baseline matched or beat the skill on all three |

So the skill earns its keep, but only as a **floor for weak models**, and only on the one prompt where the classification is genuinely confusable. The demotion question from `docs/cpp-parallel-skills-decisions.md` now has an answer: **keep it as a skill.** A rule that stops a weak model from prescribing a no-op fix to a real bug is worth a trigger.

The three pending iteration-2 changes still apply, and eval-1 sharpens one of them: with the skill, Haiku offered a barrier as fix option 1 and local-then-merge only as option 2. The correct ordering is the reverse, since a barrier buys determinism by serializing the program.

---

## Iteration 2 — changes applied (2026-09-25)

Applied to `cpp-concurrency-debug/SKILL.md` from the Opus-baseline finding on eval-1 (`docs/cpp-skills-harness-report.md` §7) and the Haiku-tier run (§8).

| # | Change | Motivated by |
|---|---|---|
| 1 | **Fix order reversed in the Race condition section.** Per-thread partials over *static* index ranges, merged in fixed order after `join()`, is now the preferred fix; barrier/latch is demoted to "only when the phases genuinely must be ordered", with the cost stated: it buys determinism **by serializing**. | Opus baseline: a barrier or turn-taking token restores determinism at the price of the parallelism the user came for. The skill's answer was on the wrong side of that trade. |
| 2 | **Added the mechanism: FP `+` is commutative but not associative.** Lock-acquisition order picks the grouping, so the sum reassociates each run. Explicit warning not to "verify" a shared accumulator by calling `+` commutative, and the note that integer/fixed-point accumulation genuinely *is* order-independent. | Opus baseline named it; the Haiku baseline listed `result += value;` under "COMMUTATIVE (safe, order doesn't matter)" — its own rule certifying the bug as safe. |
| 3 | **Added the discriminator table.** Bounded jitter in the low bits ⇒ reduction order, no race. Torn/impossible values or dropped counts ⇒ a real data race. | Opus baseline; separates the two causes without a debugger. |
| 4 | **Added "a silent ThreadSanitizer is evidence, not a blind spot"** to the data-race vs race-condition preamble, with the instruction not to hypothesize an unlocked access when TSan is clean. | Haiku baseline's top-ranked answer was "you're probably reading the result without the lock" — a data race, contradicting the evidence it was given, and a no-op as a fix. |
| 5 | **Added the non-fix list** (more/finer mutexes; `atomic<double>` `fetch_add` / CAS / `seq_cst` — a total order is not a *predetermined* order; `-ffp-contract=off` / `-fno-fast-math` / `-O0` / `long double`; more sanitizer runs; pinning, priorities, sorting the output). | Opus baseline; the `seq_cst` one is the subtlest and was absent. |
| 6 | **Added the audit list of other order-sensitive combines** — shared `push_back`, `min`/`max` ties, `unordered_map` insertion order, first-writer-wins flags, thread-id/clock seeding, work stealing. | Opus baseline. |
| 7 | **Added the cheap detection test** for this case: 20+ runs watching whether variation stays in the low bits, diffed against a single-threaded run. | Opus baseline reproduced exactly this way. |
| 8 | **Added two correctness caveats on the preferred fix:** fix the *partial count* rather than using `hardware_concurrency()` (or the result changes between machines), and note that a work queue or atomic cursor **reintroduces** the nondeterminism. Plus the distinction between repeatability (partials) and exactness (Kahan/Neumaier or fixed point). | Opus baseline. |
| 9 | **Quick-fixes table row rewritten** to match the new ordering. | Consistency. |

**Not changed:** the deadlock, livelock, starvation, data-race and abandoned-lock sections. Both tiers, both arms, diagnosed those correctly — by the harness's own rule, content a model already gets right is dead weight.

**Not re-measured.** These changes were written from the measured gaps, not validated by a fresh run. A re-run of eval-1 at Haiku and Opus would confirm the reordering took; until then the skill is at "iteration 2, unverified".
