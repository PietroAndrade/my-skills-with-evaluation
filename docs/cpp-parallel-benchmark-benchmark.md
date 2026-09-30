# cpp-parallel-benchmark — Benchmark (detailed)

Full with-skill vs baseline evaluation of the `cpp-parallel-benchmark` skill (generates trustworthy timing harnesses). Three prompts, each run twice — once with the skill loaded, once with no skill — to measure the skill's contribution. This is the one skill in the C++ parallel set given the rigorous loop rather than a quick review.

**Skill:** `~/.claude/skills/cpp-parallel-benchmark/SKILL.md`
**Bundled:** `references/benchmark-harness.md` (reusable `Benchmark`) + `references/examples/*.cpp`
**Baseline C++ standard:** C++17
**Method:** with-skill + no-skill baseline per prompt (6 subagent runs)

---

## Test prompts

| # | Task | Trap under test |
|---|------|-----------------|
| 0 | Harness comparing sequential vs parallel sum, speedup + efficiency (lib `bench`) | full harness correctness |
| 1 | Merge-sort benchmark gives unstable numbers — how to measure reliably | stability: warm-up, N runs, in-place reset |
| 2 | Measure if in-place parallel sort is actually faster (lib `sorter`) | in-place mutation reset |

## Assertions

A1 steady_clock · A2 warm-up discarded · A3 average N runs · A4 stddev reported · A5 verify parallel==sequential · A6 speedup=seq/par · A7 efficiency=speedup/cores · A8 time only the measured call · A9 (eval2) handle in-place reset outside timing · A10 project idiom.

---

## Results — with-skill

| Assertion | eval-0 | eval-1 | eval-2 |
|-----------|--------|--------|--------|
| A1 steady_clock | ✅ | ✅ | ✅ |
| A2 warm-up discarded | ✅ | ✅ | ✅ |
| A3 average N runs | ✅ | ✅ | ✅ |
| A4 stddev | ✅ | ✅ | ✅ |
| A5 correctness check | ✅ | ✅ (vs std::sort) | ✅ |
| A6 speedup | ✅ | (setup for it) | ✅ |
| A7 efficiency | ✅ | n/a | ✅ |
| A8 time only measured call | ✅ | ✅ | ✅ |
| A9 in-place reset outside timing | n/a | ✅ fresh copy | ✅ reset callback |
| A10 idiom | ✅ | ✅ | ✅ |

with-skill: **3/3 fully correct.**

## Results — baseline (no skill)

| Eval | Baseline verdict |
|------|------------------|
| 0 | **Contaminated** — produced code byte-identical to `references/benchmark-harness.md`. The agent, running in `~/.claude/skills`, read the skill files despite no skill path. Comparison void. |
| 2 | **Contaminated** — identical template *and* cited the sibling skill `cpp-parallel-decompose` by name. Comparison void. |
| 1 | **Clean** — genuinely independent approach: reported **min/median/p95** instead of mean/stddev, added a **`DoNotOptimize` dead-code-elimination barrier**, and environment control (`cpupower`, `taskset`, turbo-off). Mentioned Google Benchmark. |

## Observations

- The with-skill runs were uniformly correct, including the two subtle traps this skill exists for: the in-place reset kept outside the timed region (eval-1, eval-2) and the correctness check before reporting speedup (all three).
- **Baseline contamination:** subagents told "no skill" still read the skill files because the working directory *is* the skills tree. Two of three baselines are therefore invalid for measuring lift. A clean measurement would require running baselines from an isolated directory.
- **The one clean baseline earned its keep** — it surfaced two real gaps the skill lacked:
  1. **No dead-code-elimination guard.** At `-O2` the optimizer can delete unobserved measured work and report a fake near-zero. The clean baseline used an `asm volatile` barrier; the skill had nothing.
  2. **Mean-only reporting.** The clean baseline reported min/median, which are more robust to OS-noise outliers than the mean.

## Changes applied (iteration 1 → 2)

- Added measurement rule **#8 Prevent dead-code elimination** (sink / `DoNotOptimize` barrier / optimized build) to `SKILL.md`.
- Extended rule **#1** to report the **minimum alongside mean and stddev**.
- Updated `references/benchmark-harness.md`: `doNotOptimize` helper, `minSeconds` in `BenchmarkResult`, and the two-callable (`reset`/`target`) `measure` overload for in-place targets.
- Extended the "Before returning code" checklist accordingly.

**Verdict:** approved at iteration 2.

---

## Cross-tier experiment (Haiku, Sonnet, Opus)

To measure the skill's lift free of the baseline-contamination problem above, the 3 prompts were re-run on **Haiku** and **Sonnet**, each with-skill vs a no-skill baseline explicitly forbidden from reading `~/.claude/skills` (clean isolation). Opus baselines were re-run later (below) once isolation moved from a prompt instruction to a real sandboxed working directory.

### with-skill: uniform across tiers
All 9 with-skill runs (3 tiers × 3 prompts) produced the full harness: `steady_clock`, discarded warm-up, N-run mean + **stddev + min**, correctness comparison before speedup, **`doNotOptimize` DCE barrier**, in-place reset outside the timed region, and project idiom (guard/namespace/`m_`/no comments). The iteration-2 additions (min, DCE) propagated to every tier.

### baseline lift by tier

| Capability | Haiku baseline | Sonnet baseline | Opus baseline |
|---|---|---|---|
| Fundamentals (steady_clock, warm-up, N runs, in-place reset) | inconsistent (1/3 used `high_resolution_clock`, missed warm-up) | solid (3/3) | solid (3/3) |
| stddev reported | 1/3 | 2/3 | 2/3 (eval-0 missing) |
| **DCE barrier** | **0/3** | **1/3** | **1/3** (eval-1 only, explicit `asm volatile` `keepAlive()`) |
| Interleaved variants (rule 8) | not observed | not observed | **0/3** — all block sequential-then-parallel |
| **Project idiom** (guard/namespace/`m_`/no comments) | **0/3** | **0/3** | **0/3** — has include guards, but no `m_` prefix, no namespace-close comment, no full `"lib/file.h"` include paths |
| Overall | 3–6/10, high variance | 6–8/10 | 6–9/10 |

### Opus baseline detail (re-run, clean isolation)

Re-run 2026-09-25 using a sandboxed scratch directory outside `~/.claude/skills` as the subagent's project root (see "Baseline contamination — root cause and fix" below), rather than relying on a prompt instruction alone. Grep across all three outputs confirmed no mention of the skill, `benchmark-harness.md`, `cpp-parallel-decompose`, or `doNotOptimize` — genuinely independent.

| Assertion | eval-0 (sum) | eval-1 (merge sort stability) | eval-2 (in-place sort) |
|-----------|--------------|-------------------------------|--------------------------|
| A1 steady_clock | ✅ | ✅ | ✅ |
| A2 warm-up discarded | ✅ | ✅ | ✅ |
| A3 average N runs | ✅ (7 runs) | ✅ (calibrated reps) | ✅ (5 runs) |
| A4 stddev | ❌ (avg/best/worst only) | ✅ (+ CV, MAD, trimming) | ✅ |
| A5 correctness check | ✅ | ✅ (vs `std::sort`) | ✅ (gates the verdict) |
| A6 speedup | ✅ | n/a (setup for it) | ✅ |
| A7 efficiency | ✅ | n/a | ✅ |
| A8 time only measured call | ✅ | ✅ | ✅ |
| A9 in-place reset outside timing | n/a | n/a | ✅ (`scratch.assign` before the clock starts) |
| DCE barrier (rule 9) | ❌ | ✅ explicit `asm volatile` | ❌ (low risk: real memory writes + `is_sorted` check) |
| Rule 8 interleaving | ❌ (block sequential-then-parallel) | n/a | ❌ (block) |
| Idiom | partial (guard yes, no `m_`/namespace-close) | partial | partial |

eval-1 is notably strong on its own: median/mean/sd/CV with symmetric trimming, calibrated inner-repetition count to clear clock resolution, four input shapes (random/sorted/reversed/few-unique), and an explicit DCE barrier — unprompted, matching the one clean baseline finding from the original (contaminated) run below almost point for point. This reproduces the Sonnet-tier finding that a strong model already knows most fundamentals; the skill's durable win stays in idiom and rule 8 (interleaving), which 0/3 Opus baselines applied.

### Findings

1. **Fundamentals lift is inversely proportional to model strength.** Haiku needs the skill to get `steady_clock`/stddev/in-place-reset right; Sonnet and Opus mostly know them (Opus eval-1 even produced a DCE barrier, CV/MAD, and a 4-shape workload sweep, unprompted).
2. **Idiom lift is constant across tiers, Opus included.** 0 of 9 clean baselines (Haiku + Sonnet + Opus, all prompts) followed the project conventions — no `m_` prefix, no namespace-close comment, no full `"lib/file.h"` include paths. All 9 with-skill runs did. This is the skill's durable, tier-independent value and matches the thesis of `cpp-skills-benchmark.md` (models capture semantics, miss conventions).
3. **DCE is an edge rule even strong models forget inconsistently:** baseline Haiku 0/3, Sonnet 1/3, Opus 1/3; with-skill 9/9. The skill guarantees edge rules the model drops under load, regardless of tier.
4. **Rule 8 (interleaving) has zero baseline adoption at any tier measured so far (0/3 Opus, not observed in Haiku/Sonnet runs either)** — it is not something models reach for spontaneously, unlike warm-up or steady_clock.
5. **Consistency:** baseline quality swings by prompt and tier (3→9/10); with-skill is uniform. The skill acts as a predictable quality floor — the property that matters for deployment.

**Overall:** the skill adds value at every tier, for reasons that shift — teaching the rules on Haiku, enforcing idiom + edge rules (DCE/min/interleaving) + consistency on Sonnet/Opus.

Remaining follow-up (optional): extend the same clean cross-tier experiment to the other two generator skills (`cpp-synchronization`, `cpp-async-tasks`).

---

## Baseline contamination — root cause and fix

The original eval-0/eval-2 Opus baselines (and the very first no-skill runs, above) were contaminated: told "no skill" but launched with cwd = `~/.claude/skills`, the subagent read the skill's own reference files anyway (nothing stopped it — the instruction was a prompt, not an enforced boundary) and produced near-identical output. A prompt telling a model not to look somewhere is not isolation.

**Fix applied for the 2026-09-25 Opus re-run:** each baseline ran as a fresh subagent with no shared context, given a scratch directory *outside* `~/.claude/skills` (under the session's own scratchpad, a different filesystem path entirely) as its explicit project root, plus an explicit instruction not to read anything under `~/.claude/skills`. Because the working directory itself no longer contains the skill tree, there is nothing to accidentally `Glob`/`Read` into — the isolation is structural, not just requested. Post-hoc, outputs were grepped for the skill's own vocabulary (`doNotOptimize`, `benchmark-harness`, sibling skill names) to confirm no leakage; all three came back clean.

This generalizes: any future baseline run must use a project root that does not physically contain `~/.claude/skills`, not merely an instruction to avoid it.

---

## Iteration 3 — changes applied (2026-09-25)

Two additions carried over from the `cpp-parallel-decompose` baselines (`docs/cpp-skills-harness-report.md` §7–8), not from a new benchmark-skill run. Both are cases where the existing rules were silent and the silence produces a plausible-looking wrong number — the same failure shape as the DCE trap fixed in iteration 2.

| # | Change | Why |
|---|---|---|
| 1 | **New rule 10 — control the page cache when the workload touches files.** First run reads from disk, later runs from RAM; uncontrolled, this fakes a speedup when the parallel variant happens to run second, or hides a real one. The rule forces an explicit choice between a **cold** measurement (drop caches every run, or a working set several times RAM) and a **warm** one (pre-load, discard run 1, report steady state), and requires the output to say which. Extended to any external state the timed region warms — database, connection pool, JIT, allocator free lists. | The Opus `cpp-parallel-decompose` baseline flagged it as a first-order trap for file workloads ("otherwise run 2 is 5× faster for reasons unrelated to your code and you'll confirm a speedup you didn't get"). Rule 8 does **not** cover it: interleaving cancels drift, not a step change in where the data lives. |
| 2 | **Rule 10 also routes request/response services to `service-benchmark`.** | Closed-loop vs open-loop load generation changes the harness shape enough that this skill's advice quietly produces wrong numbers for a server. |
| 3 | **"Efficiency low" interpretation now demands a sweep.** A single efficiency figure cannot separate a serial section from contention from bad granularity; the output should hand back the **Karp–Flatt trend** across N = 1, 2, 4, 8, 16… (reading per `cpp-parallel-decompose`). | Opus decompose baseline. *One data point is a value; the curve is a diagnosis.* |
| 4 | **Checklist item added** for the cold/warm declaration. | Consistency with rule 10. |

**Not re-measured.** The three-tier matrix above still describes iteration 2. Re-running the three prompts would show whether rule 10 fires on a file-touching workload; none of the current prompts has one, so a fourth prompt would be needed to test it at all.
