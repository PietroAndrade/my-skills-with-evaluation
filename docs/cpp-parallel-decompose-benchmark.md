# cpp-parallel-decompose — Review

Qualitative review of the `cpp-parallel-decompose` skill (advisory: decide whether/how to parallelize — Amdahl gate, domain/functional decomposition, work/span, granularity, mapping). Three realistic planning prompts run through a subagent with the skill loaded; judged for correct go/no-go reasoning, sound decomposition, and useful numbers.

**Skill:** `~/.claude/skills/cpp-parallel-decompose/SKILL.md`
**Bundled:** `references/examples/` (matrix multiply domain decomposition, recursive sum/merge sort)
**Method:** quick review (with-skill only), iteration 1

---

## Test prompts

| # | Task | Correct thrust |
|---|------|----------------|
| 0 | 40% serial disk I/O + 60% CPU — worth it? Speedup at 8 cores? | Amdahl gate: ~2.1×, ceiling 2.5× |
| 1 | Plan a parallel 1000×1000 matrix multiply | domain decomposition by row bands, medium granularity |
| 2 | 16 threads but only 3× — why, and how to find the limit? | inverted Amdahl (P≈0.71), scaling study, profiling |

---

## Results

| Criterion | eval-0 | eval-1 | eval-2 |
|-----------|--------|--------|--------|
| Correct Amdahl reasoning (forward or inverted) | ✅ 2.1× / ceiling 2.5× | ✅ P≈0.99 table | ✅ solved P≈0.71 |
| Sound decomposition choice | ✅ pipeline (functional) | ✅ domain, row bands | n/a (diagnostic) |
| Granularity tied to hardware_concurrency | ✅ ≤4 workers (I/O-bound) | ✅ ~N bands | ✅ agglomerate |
| Named the real bottleneck | ✅ disk | ✅ memory bandwidth/cache | ✅ contention/false sharing/mem-bound |
| Concrete method to verify | ✅ measure overlap | ✅ benchmark | ✅ scaling study + perf/VTune |
| Routed to implementation skill | ✅ producer-consumer + async | ✅ async (no sync needed) | ✅ synchronization |

## Score

| | with-skill |
|--|-----------|
| Prompts fully correct | **3 / 3** |

## Observations

- All three used Amdahl as a decision gate, not a formula recital: eval-0 forward, eval-2 inverted to recover the serial fraction from an observed speedup.
- Answers went past rote: eval-0 spotted that overlapping I/O with CPU (pipeline) beats piling on CPU threads and that >4 cores is wasted; eval-1 flagged memory bandwidth as the true ceiling for dense matmul; eval-2 enumerated contention/false-sharing/HT/memory-bound and gave a curve-shape diagnostic.
- Routing was correct and varied — producer-consumer for the pipeline, async-only for the lock-free matmul, synchronization for contention fixes — showing the decision map's boundaries hold.

**Verdict:** approved, iteration 1. No changes required.

---

## First baseline (added 2026-09-25) — lift is zero, and the baseline contradicts the skill's answer

The review above was with-skill only: 3/3, no measurable lift. The missing baseline was run 2026-09-25 on **Opus**, same three planning prompts, under **structural isolation** (scratch project root outside `~/.claude/skills`) and **grep-verified** (`cpp-parallel-decompose`, `cpp-async-tasks`, `cpp-producer-consumer`, `cpp-synchronization`, `choosing-concurrency`, `SKILL.md`, `.claude`) — clean on all three.

### Results

| Criterion | eval-0 40/60 log job | eval-1 matmul plan | eval-2 16 threads → 3× |
|---|:---:|:---:|:---:|
| Amdahl reasoning | ⚠️ **refused the skill's model** (see below) | ✅ work/span: W=2N³, span=O(N), parallelism ≈2×10⁶ | ✅ inverted to s=0.289, then **replaced it with Karp–Flatt** |
| Sound decomposition | ✅ domain over files + reader/parser pipeline | ✅ domain, row bands of C, i-k-j order | n/a (diagnostic) |
| Granularity tied to hardware | ✅ batch 32–256 files if parse <100 µs | ✅ static 8×125 rows, or 32 chunks for imbalance insurance; ≥100 µs rule | ✅ ≥50–100 µs work items |
| Named the real bottleneck | ✅ storage concurrency `D`, then CPU | ✅ memory/L3 bandwidth, with the roofline numbers | ✅ DRAM bandwidth ranked #1 of 13 causes |
| Concrete verification method | ✅ measure both roofs first; queue-depth sampling | ✅ GFLOP/s beside speedup, sweep 1/2/4/8 | ✅ Karp–Flatt table, roofline, `perf c2c`, `turbostat`, `perf --topdown` |
| Routed to an implementation shape | ✅ bounded queue + thread-local aggregates + single merge (full listing) | ✅ jthread/OpenMP/`for_each(par)`, "a mutex in the loop nest means the decomposition is wrong" | ✅ fuse stages, tile for L2, parallelize across frames |
| Anti-pattern called out | ✅ shared mutex-guarded aggregate map as the usual ~1.5× cause | ✅ never split along k (reduction dim); row bands not column bands (false sharing) | ✅ nested parallelism / oversubscription, SMT vs physical cores |

**Score: baseline 3/3. Measured lift: zero — and on eval-0, negative.**

### eval-0: the baseline rejects the skill's headline number

The with-skill answer applied Amdahl with s = 0.40 and reported **~2.1× at 8 cores, ceiling 2.5×** (while correctly recommending an I/O/CPU pipeline). The clean baseline explicitly names that as **the wrong model**:

> "*Serial today* is not the same as *inherently serial*."

Its argument: the 50,000 files are independent, so the reads parallelize too. What caps the job is hardware — storage concurrency `D` and 8 cores — not an algorithmic dependency. It replaced the single serial fraction with a two-roof bound, `T ≥ s + max(0.6/C, 0.4/D)`, where the genuinely serial part (startup, enumeration, final merge, output) is 1–3%, not 40%:

| `D` | bound by | ceiling |
|---|---|---|
| 1 (HDD, or one reader) | I/O | 2.5× |
| 2 | I/O | 5.0× |
| 4 | I/O | 10.0× |
| ≥6 (NVMe at QD 16+) | CPU | ~13.3× |

Derated: ~5–7× on NVMe, ~4× on SATA SSD, ~2.4× on a single HDD — and the crossover where extra parser threads stop helping is `D ≤ 5.3`. It also flagged the failure mode the skill's answer cannot see: on a spinning disk, 8 readers across 50,000 files turn near-sequential reads into seek thrash, `D` drops **below** 1, and the parallel version is *slower*. Plus the page-cache trap (5 GB working set on a 32 GB box makes the second run a pure CPU problem).

**The skill's 2.1× is only right if the storage refuses concurrent reads.** For the common case (flash), it under-predicts by 2–3× and it under-predicts for a *structural* reason: treating an I/O percentage as Amdahl's `s`.

### eval-2: Karp–Flatt beats inverted Amdahl

The with-skill run inverted Amdahl to recover P ≈ 0.71 from the observed 3×. The baseline did the same arithmetic (s = 0.289) and then said one data point cannot distinguish a genuine serial section from a saturated shared resource from N-growing overhead — so measure the **Karp–Flatt** experimentally-determined serial fraction across N = 1…16 and read its *trend*: constant ⇒ real serial code, rising ⇒ contention/saturation, falling ⇒ granularity. That is strictly more diagnostic than a single inverted number.

### Consequence

`cpp-parallel-decompose` has **no demonstrated lift at Opus tier**, and two of its canonical answers are now known to be weaker than an unaided strong model's:

1. **eval-0 is a content defect, not just a missing lift.** The skill needs the "serial today ≠ inherently serial" distinction and the two-roof `max(compute, I/O-concurrency)` bound, or it will keep telling people a parallelizable I/O phase caps them at 2.5×.
2. **eval-2 should teach Karp–Flatt across a sweep**, not a single inverted Amdahl.

Same shape as the DCE-barrier finding on `cpp-parallel-benchmark`: the clean baseline earns its cost by exposing what the skill lacks. **Action taken (2026-09-25, iteration 2):** applied. See "Iteration 2 — changes applied" at the end of this document.

---

## Haiku tier (added 2026-09-25) — modest lift, and both arms inherit the skill's defect

Both arms run at Haiku tier: 3 baselines (structural isolation, grep-verified clean) and 3 with-skill, same prompts as the Opus run. Both arms were needed — a Haiku baseline against an Opus with-skill run would confound tier with skill.

### Results

| Prompt | baseline Haiku | with-skill Haiku | lift |
|---|:---:|:---:|---|
| eval-0 40% I/O + 60% CPU | ⚠️ 2.1× flat | ⚠️ 2.1× then corrected to 3–5× via pipelining | **partial** |
| eval-1 matmul plan | ⚠️ right plan, unreliable numbers | ✅ 7.9× ceiling, 6.5–7.5× realistic | **real, on the numbers** |
| eval-2 16 threads → 3× | ⚠️ no quantitative gate, 2 fabricated rules | ✅ inverted Amdahl P≈0.71 + decision tree | **real, on the gate** |

**Baseline 0/3 fully sound · with-skill 3/3 sound-but-incomplete.**

### eval-0 — the skill's structural defect survives at both tiers, in both arms

The Haiku **baseline** produced the skill's own answer unaided: `1/(0.40 + 0.60/8) ≈ 2.1×`, plus 1 I/O thread + 7 workers, a bounded queue of 50–100, and thread-local aggregation with a merge. That is the conventional answer, and it is what the skill teaches.

The **with-skill** run went further — it applied the Amdahl gate, then corrected it: *"with pipelining, if I/O time < parsing time / 7, speedup improves to 3–5×"*, landing on realistic 3–4×, best case 4.5–5×, plus a granularity formula (`batch = files / (cores × 4) ≈ 1562`) and routing to `cpp-async-tasks` / `cpp-synchronization`. Better than the baseline.

**But neither reached the insight the Opus baseline had:** the reads themselves parallelize, so the true serial fraction is 1–3%, not 40%, and the cap is storage concurrency `D`. Neither warned that many readers on a spinning disk cause seek thrash and can make the parallel version *slower*. The with-skill run recovers part of the number via a "pipeline factor" rather than by fixing the model.

**Reading:** this is the strongest evidence yet that eval-0 is a **content defect**, not a missing lift. Teaching the right model would have improved *both* arms at *both* tiers.

### eval-1 — the lift is arithmetic reliability

Both arms chose row bands of C, 8 chunks of 125 rows, disjoint writes, barrier-only sync, and named memory bandwidth as the primary limiter. The baseline's numbers, however, do not survive checking:

- Arithmetic intensity computed as **14.7 ops/byte** from 2×10⁹ ops / 136 MB — it assumed B is read once per thread. The non-blocked `ikj` loop re-reads B per row, ~8 GB of traffic, giving **~0.125 FLOP/B**. Off by roughly two orders of magnitude. It still concluded "bandwidth bound", so the verdict survived its own broken arithmetic — which is worse, not better, as a habit.
- "1 core, 1 GHz, 256-bit SIMD" as the peak model; "L3 typically 8–20 MB **per core**" (L3 is shared).

It also missed everything the Opus baseline had: never split along `k`, row bands rather than column bands *because of the straddling cache line*, work/span (`W=2N³`, span `O(N)`, parallelism ~2×10⁶), padding the leading dimension, reporting GFLOP/s beside speedup, and that blocking is worth more than threading.

The with-skill run gave 7.9× ceiling / 6.5–7.5× realistic with a clean go/no-go and correct "no synchronization during compute" reasoning. Its numbers are coarse but not wrong.

### eval-2 — the lift is having a quantitative gate at all

The baseline wrote "81% serial or waiting, 19% useful" — loose, never inverting Amdahl — and invented two rules it presented as fact: *"speedup ∝ √threads ⇒ memory bound"* and *"miss ratio > 25% ⇒ bandwidth saturation"*. It also proposed per-core utilization as the discriminator, which cannot separate its own top suspect (bandwidth-bound code shows 100% on every core). Its tool coverage was broad and useful.

The with-skill run inverted Amdahl to P ≈ 0.71, then gave six prioritized measurements routed by the *shape* of the thread-count curve (peaks-then-declines ⇒ contention; flattens ⇒ structural serial fraction). Correct per the skill — and still short of the Opus baseline's Karp–Flatt sweep.

### Revised verdict for `cpp-parallel-decompose`

| Tier | Lift |
|---|---|
| Haiku | **Real but modest** — supplies the quantitative gate and keeps the arithmetic honest; no prompt where the baseline was outright wrong on the plan |
| Opus | **Zero, negative on eval-0** |

**Demotion question:** the evidence is weaker than for `cpp-concurrency-debug`. No Haiku baseline produced a *wrong plan* — they produced right plans with unreliable numbers. That is a real but smaller harm than prescribing a no-op fix. Recommendation: **keep as a skill, but fix eval-0 and eval-2 first** — the pending iteration-2 changes are now load-bearing, since eval-0's defect was shown to propagate into the with-skill output at both tiers.

---

## Iteration 2 — changes applied (2026-09-25)

Applied to `cpp-parallel-decompose/SKILL.md`. Both changes come from the clean Opus baseline beating the skill (`docs/cpp-skills-harness-report.md` §7), and both were shown by the Haiku run (§8) to propagate into the with-skill output at *both* tiers — so these are corrections to content that was capping the skill's own answers, not additions.

### 1. Step 0 — "serial *today* is not the same as inherently serial"

New subsection before the contention discussion:

- States the failure directly: a profile saying "40% of wall time is reading files" reports **how the program is written now**, not a dependency. If the items are independent, the reads parallelize too and `1-P = 0.40` is false.
- Defines what actually counts as serial — startup, enumerating the work, the final merge, writing the output — typically **1–3%** for a batch job over independent items.
- Adds the **two-roof bound** for a phase that is hardware-bound rather than dependency-bound:
  `T_parallel ≥ s + max(f_cpu/C, f_io/D)`, with `D` = effective device concurrency, plus the worked table for the 40/60 job (D=1 ⇒ 2.5×, D=2 ⇒ 5×, D=4 ⇒ 10×, D≥6 ⇒ ~13.3×; derated ~2.4× / ~4× / ~5–7×) and the crossover `D > 5.3`.
- States plainly: **plugging 0.40 into Amdahl gives 2.1×, which is only correct for `D = 1`.**
- Adds the two consequences the skill previously could not reach: `D` is measurable in an afternoon (a read-and-discard sweep at 1/2/4/8 readers *is* the `D` curve, measured before writing threads), and **more readers can be slower** — HDD seek thrash drops `D` below 1, so keep one reader and parallelize only the CPU phase.
- Adds the page-cache check (5 GB working set on a 32 GB box makes run 2 a CPU-bound problem; cold and warm are different questions).
- Generalizes it: the same reasoning covers memory bandwidth, a network link, a connection pool — *ask which resource is already at 100%, not what the serial fraction is*.

### 2. Step 0 — Karp–Flatt across a sweep, never one point

The skill already contained the inversion `s = (N/S - 1)/(N - 1)`, but only as a footnote under "when the model stops applying", so with-skill runs used it at a single data point. Now:

- Named as the **Karp–Flatt metric**, promoted to its own subsection as the prescribed answer to "I got `S`× on `N` threads, why so little?".
- States that one `N` is ambiguous — 3× on 16 threads gives `e = 0.289`, equally consistent with three different problems.
- Adds the trend table: **constant** ⇒ genuine serial section; **rising** ⇒ contention/false sharing/allocator/saturating resource (route to `cpp-concurrency-debug`); **falling** ⇒ granularity, agglomerate (Step 3).
- Instruction: sweep N = 1, 2, 3, 4, 6, 8, 12, 16…, tabulate `e(N)`, report the trend. *One data point is a value; the curve is a diagnosis.*

### 3. Output section

- Item 5 now asks for a **range keyed to the binding resource** rather than a single Amdahl number whenever a shared device is in play ("5–7× on NVMe, ~4× on SATA, ~2.4× on a spinning disk — measure `D` first").
- New **"Before returning the plan" checklist**: counted only non-overlappable work as serial; gave a two-roof bound and named `D` where a shared device is involved; named the case where parallelizing *loses*; prescribed a Karp–Flatt sweep rather than a single inverted point; expressed the expectation as a falsifiable range.

**Not changed:** Steps 1–4 (partition, task graph, agglomeration, mapping). Every baseline at both tiers produced a sound decomposition unaided; that content is not load-bearing.

**Not re-measured.** Written from the measured gaps, not validated by a fresh run. The obvious verification is re-running eval-0 and eval-2 at both tiers and checking whether the with-skill answer now names `D` and the Karp–Flatt trend — in particular whether the Opus with-skill run stops reporting a 2.5× ceiling. Until then: "iteration 2, unverified".
