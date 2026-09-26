# Do skills actually make the agent better? — harness report

Technical report on the evaluation harness built for the C++ parallel/concurrent skill set, and what it measured. Written to answer one question with evidence rather than intuition: **when an agent has a skill, what specifically improves, and what does not.**

Two independent dimensions were measured, and they need to stay separate:

| Dimension | Question | Where |
|---|---|---|
| **Output quality** | Given the skill fired, is the code better? | `docs/<skill>-benchmark.md`, summarized here |
| **Triggering** | Does the skill fire on the right queries at all? | `docs/cpp-skills-trigger-benchmark.md` |

A skill with perfect output that never fires is worthless; one that fires instead of its sibling is worse than worthless. Both dimensions had to be measured, and each one needed its instrument fixed before the numbers meant anything.

---

## 1. The harness

### Output-quality arm

Per skill, a set of 3 realistic prompts is run twice: once by a subagent **with** the skill loaded, once by a **baseline** subagent with no skill. Outputs are graded against explicit assertions fixed in advance (e.g. for the benchmark skill: `steady_clock`, discarded warm-up, N-run mean+stddev, correctness check before speedup, in-place reset outside the timed region, project idiom). Run across model tiers — Haiku, Sonnet, Opus — to separate "the skill taught the model something" from "the model already knew it".

### Triggering arm

Per skill, an eval set of ~8 positive queries plus ~10 **near-miss negatives** — queries deliberately close to the skill's topic but which belong to a sibling skill or are out of scope. Each query is run through a real `claude -p` session and it is recorded which skill, if any, the model invoked. Near-misses are the whole point: an obvious negative tests nothing.

Runner and raw data: `cpp-skills-workspace/trigger-evals/trigger_probe.py`, `results-2026-09-25/`.

### Both instruments were broken, in the same way

This is the most transferable finding in the report, and it cost two rounds of invalid conclusions.

**Output arm — baseline contamination.** Baseline subagents were told "use no skill" but launched with their working directory set to `~/.claude/skills`. They read the skill's own reference files anyway and emitted output byte-identical to the skill's bundled template. Two of the first three baselines were void. The first fix — instructing the baseline not to read that directory — is not isolation: the skill tree still sits in the agent's own working directory, one glob away. The working fix is **structural**: a project root on a path that does not physically contain the skill tree, plus a **post-hoc grep** of the output for the skill's own vocabulary (bundled-file names, sibling skill names, distinctive helper identifiers) to confirm nothing leaked.

**Triggering arm — the harness never tested the real skill.** `skill-creator/scripts/run_eval.py` writes a throwaway slash command containing a stub of the description, then counts a trigger only when the model invokes *that hashed stub name*. But the real skills are installed in `~/.claude/skills/`, so when a query genuinely calls for one, the model invokes the **real** skill and the name does not match — scored as a miss. It also aborts on any first tool that is not `Skill`/`Read`, while `Bash`-then-`Skill` is a common order. Net effect: it measured "does the model prefer my empty duplicate over the real installed skill?", answer naturally no.

Both failures share a shape: **silent**. No error, no warning, plausible-looking numbers with decimal places. And both produced a *specific* misleading artefact — contaminated baselines inflate the baseline, and the stub harness deflates positives while making negatives pass by construction. In both cases a conclusion had already been written into the docs before the instrument was checked.

**Rule adopted:** validate the instrument against a known-answer case before trusting a single number it produces. For the triggering arm that took one live query; it invoked `Skill(cpp-synchronization)` and the old harness would have logged it as a miss.

---

## 2. Model efficiency — lift by tier

"Efficiency" here = how much the skill adds **on top of what that model already does unaided**. High baseline means the model already knows it; the skill's remaining value is elsewhere.

### `cpp-parallel-benchmark` — the only skill with a complete three-tier matrix

| Capability | Haiku baseline | Sonnet baseline | Opus baseline | with-skill |
|---|:---:|:---:|:---:|:---:|
| Fundamentals (`steady_clock`, warm-up, N runs, in-place reset) | inconsistent — 1/3 used `high_resolution_clock`, missed warm-up | solid 3/3 | solid 3/3 | 9/9 |
| stddev reported | 1/3 | 2/3 | 2/3 | 9/9 |
| **Dead-code-elimination barrier** | **0/3** | **1/3** | **1/3** | **9/9** |
| **Interleaved variants (rule 8)** | not graded | not graded | **0/3** | **9/9** |
| **Project idiom** (guard, `m_`, namespace, no comments) | **0/3** | **0/3** | **0/3** | **9/9** |
| Overall band | 3–6/10, high variance | 6–8/10 | 6–9/10 | uniform, at ceiling |

Opus was re-measured on 2026-09-25 under structural isolation and grep-verified. Haiku and Sonnet predate that method: prompt-forbidden only, never grep-verified — **plausibly clean, not confirmed clean.**

### `cpp-synchronization` — Haiku/Sonnet only, and the baselines were *handed the conventions*

This run has a deliberate methodological twist: the baselines were given the project conventions **explicitly in the prompt** (namespace = lib name, `m_`, guard format, `final`, no comments, RAII). That neutralizes the skill's idiom advantage by design, so whatever remains is deeper than style.

| Prompt | with-skill (Haiku/Sonnet) | Haiku baseline | Sonnet baseline |
|---|:---:|:---:|:---:|
| Read-heavy RouteTable | ✅ / ✅ | ✅ | ✅ but used C++20 `.contains()` in a C++17 task |
| Cap 4 concurrent downloads, C++17 | ✅ / ✅ correct C++17 fallback | ❌ **C++20 `binary_semaphore(4)` — functionally broken, maxes at 1** | ✅ correct fallback |
| One-shot ready gate | ✅ / ✅ | ✅ but ignored the lib name | ✅ |

### `cpp-async-tasks` — Haiku/Sonnet only; baselines *not* handed conventions

| Prompt | with-skill (Haiku/Sonnet) | Haiku baseline | Sonnet baseline |
|---|:---:|:---:|:---:|
| 200 I/O downloads, sum bytes | ✅ / ✅ pool + futures | ✅ pool, but summed into a shared atomic | ✅ pool + futures |
| Divide-and-conquer sum | ✅ / ✅ stateless depth | ❌ **data race** — `config.current_depth++` on a shared ref across threads | ✅ clean |
| `std::async` loop is slow — why | ✅ / ✅ | ✅ | ✅ |

### Reading across the tiers

| Tier | What the skill buys |
|---|---|
| **Haiku** | Capability. It gets fundamentals wrong unaided and actively ships broken code: a semaphore that caps at 1 instead of 4, a data race in the parallelizing code itself. Here the skill *teaches*. |
| **Sonnet** | Idiom, edge rules, consistency. Fundamentals are already solid; standard-version slips persist (C++20 API in a C++17 task). |
| **Opus** | Idiom, edge rules, consistency — same as Sonnet, no more. Its best baseline produced CV/MAD, a multi-shape workload sweep and a DCE barrier unprompted, essentially matching the skill on that one prompt; its other two prompts did not. Strength raises the *ceiling*, not the *floor*. |

**The load-bearing pattern: capability lift falls as the model gets stronger, while preference lift stays flat.** Idiom adherence is 0/3 at every tier, Haiku through Opus.

---

## 3. Where skills genuinely improved things

| Improvement | Evidence | Tier dependence |
|---|---|---|
| **Project idiom / conventions** | 0/9 clean baselines followed them; 9/9 with-skill did | **None — flat across all tiers** |
| **Conventions survive even when handed over in the prompt** | `cpp-synchronization` baselines were *given* the conventions and still used namespace `lib` and `#pragma once` | Both tiers tested |
| **Correctness under an explicit language-standard constraint** | Haiku baseline shipped `binary_semaphore(4)`, broken; with-skill used the C++17 fallback | Strongest on weak models |
| **Preventing a real concurrency bug** | Haiku baseline wrote a data race on a shared depth counter; skill's stateless-recursion pattern is race-free by construction | Strongest on weak models |
| **Edge rules the model drops under load** | DCE barrier: baseline 0/3 Haiku, 1/3 Sonnet, 1/3 Opus → with-skill 9/9. Rule 8 interleaving: 0/3 Opus → 9/9 | Flat |
| **Robust statistics** (min, p50/p99 alongside mean) | Baseline mostly mean-only; with-skill always reports min | Flat |
| **Consistency / low variance** | Baseline swings 3→9/10 by prompt and tier; with-skill uniform | Flat |
| **Sibling discrimination** | Negatives 49/50 — the *correct* sibling skill takes the query | Measured on Opus 5 only |

The variance result deserves emphasis because it is easy to undersell. A baseline that scores 9/10 on one prompt and 3/10 on the next is not "usually fine" — it is unpredictable, and unpredictability is what makes a tool unusable in a pipeline. The skill's contribution is a **floor**, not a peak.

---

## 4. Where skills made no measurable difference

Recording these matters as much as the wins: a rule that a capable model already follows is noise in the skill, and the honest move is to say so.

| Area | Result | Why |
|---|---|---|
| **The famous `std::async` discarded-future trap** | **Zero lift.** 4/4 configurations (Haiku + Sonnet, with-skill + baseline) diagnosed it correctly | Well documented enough that every tier already knows it |
| **Reaching for a thread pool at 200 tasks** | **Zero lift.** Every configuration chose a pool over 200 raw threads | Shared instinct across tiers |
| **Benchmark fundamentals on Sonnet and above** | **Zero lift.** `steady_clock`, warm-up, N runs, in-place reset: Sonnet 3/3, Opus 3/3 unaided | Only Haiku needed teaching |
| **Choosing `shared_mutex` for read-heavy data** | **Zero lift.** Both tiers' baselines picked it unaided | Obvious from the problem statement |
| **Description tuning for triggering** | **Nothing to fix.** Hand-written descriptions scored 89/94; the optimizer loop did not beat them | The dimension the tool exists to fix was already good |
| **`cpp-async-tasks` overall** | **Lowest lift of the three generators** — its headline traps are common knowledge | Still net positive via the race prevention + idiom + bundled pool |

The `cpp-async-tasks` row is the useful negative result: it is a well-written skill whose most-cited rule adds nothing, because that rule is famous. Value came from its *least* glamorous parts — the stateless-recursion pattern and the bundled pool.

---

## 5. What is NOT measured (gaps, stated plainly)

| Gap | Consequence |
|---|---|
| ~~No Opus baseline for `cpp-synchronization` and `cpp-async-tasks`~~ | **CLOSED 2026-09-25** — 6 runs added under structural isolation, grep-verified. All three generators now have a complete three-tier matrix. See §7. |
| Haiku/Sonnet baselines never grep-verified | Plausibly clean, not confirmed clean. Same evidence standard as Opus would require re-running them. |
| ~~`cpp-concurrency-debug` and `cpp-parallel-decompose` have **no baseline at all**~~ | **CLOSED 2026-09-25** — 6 Opus baselines added. Result: **lift is zero at Opus tier for both**, and both skills were shown to contain improvable answers. See §7. |
| Triggering measured on one model, one day | 89/94 describes Opus 5 on 2026-09-25. Weaker tiers may discriminate worse. |
| Triggering used n=1 on passing queries | 4 of 9 failures flipped under 3 runs, so unobserved flakiness among the passes is likely. |
| `service-benchmark` never evaluated at all | Written from a single case (DNS over UDP) and declares itself generic for HTTP/gRPC/brokers. That claim is untested; the exposed parts are the response-correlation table and the assumption of a connectionless transport. |
| Eval sets written by the same authors as the descriptions | Biases toward queries the descriptions already cover. |
| 3 prompts per skill | Small. Fine for detecting a floor, too small for a confidence interval. |

---

## 6. Conclusion

**A skill transfers *preference* far more reliably than it transfers *capability*, and preference is the part that does not decay as models get stronger.**

The evidence separates cleanly into two mechanisms:

**Teaching** — filling in what the model does not know. Real, and entirely concentrated in the weak tier: Haiku unaided violated a stated C++17 constraint with a semaphore that silently caps at 1, and wrote a data race into the very function meant to parallelize safely. This mechanism **shrinks as models improve**. Sonnet and Opus needed almost none of it, and a skill built only on teaching would age out with the next model release.

**Transferring preference** — making the agent work the way *this* codebase and *this* engineer work: which primitive to reach for, which language standard is in force, the include-guard format, `m_` prefixes, no explanatory comments, reporting a minimum next to a mean, interleaving variants when comparing. This mechanism is **flat across tiers**: 0 of 9 clean baselines followed the project conventions — Haiku, Sonnet and Opus alike — and 9 of 9 with-skill runs did.

The sharpest single result in the harness is the `cpp-synchronization` cross-tier run, because it was designed to falsify exactly this claim. The baselines were **handed the conventions in the prompt** — namespace, `m_`, guard format, `final`, no comments. They still used namespace `lib` instead of the requested library name, and `#pragma once` instead of the project guard. Being *told* the preference did not transfer it; being *shown* worked examples did.

That is the mechanism behind the intuition that skills give the agent the specialities you already have or prefer. The harness supports it, with one refinement worth keeping: a skill is not mainly a way to make the agent *smarter* — on a strong model it adds nearly nothing in raw capability, and where knowledge is common (the `std::async` trap, pooling at 200 tasks) the lift is measurably zero. What it does is make the agent *specific*, and do it **predictably**: an unaided model swings from 3/10 to 9/10 depending on the prompt and the tier, while the with-skill runs sat uniformly at the ceiling. The deliverable is a floor under the agent's output, positioned where you want it rather than where the model's defaults happen to fall.

Two practical corollaries, both earned the hard way:

1. **Encode what the model gets wrong, not what it already does right.** The rules that paid off were idiom, the C++17 fallback, stateless recursion, the DCE barrier and interleaving. The famous trap paid nothing. Skill content that duplicates common knowledge is dead weight that dilutes the parts that work.
2. **Never trust a measurement whose instrument you have not validated.** Both arms of this harness produced confident, wrong numbers before being checked — contaminated baselines that inflated the baseline, and a triggering harness that scored real skill invocations as misses. Both were silent, both had already been written into the docs as conclusions.

### Source documents

- `docs/cpp-parallel-benchmark-benchmark.md` — three-tier matrix, contamination root cause and fix
- `docs/cpp-synchronization-benchmark.md` — the conventions-handed-over experiment
- `docs/cpp-async-tasks-benchmark.md` — the zero-lift result
- `docs/cpp-skills-trigger-benchmark.md` — triggering, 89/94, and the stub-harness defects
- `docs/cpp-parallel-skills-decisions.md` — design decisions and superseded conclusions
- `docs/cpp-skills-benchmark.md` — the earlier interface/concrete/mock comparison (15/15 vs 5/15)

---

## 7. Completion run — 2026-09-25

The report above shipped with three declared gaps in the output-quality arm. Twelve baseline runs closed them: **Opus, 3 prompts each for `cpp-synchronization`, `cpp-async-tasks`, `cpp-concurrency-debug` and `cpp-parallel-decompose`**, every one in a scratch project root that does not contain `~/.claude/skills`, every output grep-verified afterwards against that skill's own vocabulary (skill name, bundled-file names, sibling skill names, distinctive helper identifiers). 12/12 clean — no leakage.

Each skill's protocol was preserved from its original cross-tier run, so the numbers stay comparable: `cpp-synchronization` baselines were **handed the conventions in the prompt**, `cpp-async-tasks` baselines were **not**.

### Result: 12/12 baselines correct

| Skill | Baseline score | with-skill | Measured lift at Opus tier |
|---|:---:|:---:|---|
| `cpp-synchronization` | 3/3 | 3/3 | Exact trailing-comment convention only (`#endif /* GUARD */`, `} // namespace <lib>`): 0/3 baseline. Substance already present. |
| `cpp-async-tasks` | 3/3 | 3/3 | Project idiom and layout (0/3 baseline, conventions not in prompt). Correctness lift: **zero**. |
| `cpp-concurrency-debug` | 3/3 | 3/3 | **Zero.** Baseline matched or beat the skill on all three. |
| `cpp-parallel-decompose` | 3/3 | 3/3 | **Zero, and negative on one prompt** — the baseline rejected the skill's headline number. |

### What the completion run changes in this report's conclusions

**1. Confirmed, and now with no untested tiers: capability lift is a weak-model phenomenon.** The two sharpest teaching wins in §3 — the broken `binary_semaphore(4)` and the divide-and-conquer data race — are both **Haiku-only**. The Opus baseline produced the correct C++17 counting semaphore *and* named why `counting_semaphore`/`latch` were unavailable, and it arrived independently at stateless recursion (`remainingDepth - 1u` passed by value), plus a `system_error` fallback the skill does not have. Sonnet was already clean on both. The "Teaching" mechanism is now measured at all three tiers, and it is confined to the bottom one.

**2. Refined, and this is a correction: the "0 of 9 baselines followed the conventions" headline needs its condition stated.** Those 9 were the `cpp-parallel-benchmark` runs, where the conventions were **not** in the prompt. When they *are* in the prompt, tier matters after all: Haiku and Sonnet baselines still slid back to `namespace lib` and `#pragma once`, but the **Opus baseline honored the guard name, `m_`, `final`, the namespace name and the no-comments rule on 3/3 prompts.** What it missed was only the exact trailing-comment form. So the strongest claim in §6 — "being *told* the preference did not transfer it" — holds for Haiku and Sonnet and **fails for Opus**. Idiom lift is flat across tiers only when the conventions are absent from the prompt; hand them over and the top tier complies on substance. The durable case for the skill is that nobody restates the conventions in every prompt — the skill is what makes them always present — not that a strong model cannot follow them when told.

**3. New, and the most useful result of the run: the two advisory skills have no measurable lift, and a clean baseline exposed defects in both.**

- `cpp-parallel-decompose` eval-0: the skill answers the 40%-I/O question with Amdahl at s = 0.40 → **~2.1×, ceiling 2.5×**. The baseline refused that model — *"serial today is not the same as inherently serial"* — since the 50,000 files are independent and flash serves concurrent reads. It replaced it with a two-roof bound `T ≥ s + max(0.6/C, 0.4/D)` over storage concurrency `D`, putting the true serial fraction at 1–3% and the answer at 5–7× on NVMe, ~4× on SATA, ~2.4× only on a spinning disk — where it also warned that 8 readers over 50k files cause seek thrash, `D < 1`, and a parallel version that *loses*. The skill's number is right for one storage class and under-predicts the common case by 2–3×, for a structural reason: treating an I/O percentage as Amdahl's `s`.
- `cpp-parallel-decompose` eval-2: skill inverts Amdahl to P ≈ 0.71 from one data point; baseline computed the same figure, then argued one point cannot separate a serial section from a saturated resource from N-growing overhead, and prescribed **Karp–Flatt across N = 1…16, read by trend** (constant ⇒ serial code, rising ⇒ contention, falling ⇒ granularity).
- `cpp-concurrency-debug` eval-1: skill says "ordering, not access" and proposes `barrier`/`latch`/redesign. Baseline named the mechanism — FP `+` is commutative but not associative, so lock-acquisition order regroups the sum — gave the discriminator (**bounded low-bit jitter = reduction order; torn or wildly wrong = a real race**), demonstrated it with a drifting buggy column beside a stable fixed one under a silent TSan, and pointed out that a barrier or turn-taking token restores determinism **by serializing the program**, whereas per-thread partials over static index ranges get determinism *and* parallelism. The skill's suggested fix is on the wrong side of that trade.

This reproduces the single most valuable behavior of the harness, now three times over: **the clean baseline pays for itself not by losing, but by beating the skill somewhere specific.** The DCE barrier and min-reporting came from exactly this in iteration 1; the three findings above are the same mechanism, and they are logged as pending iteration-2 changes in the respective benchmark docs rather than silently applied.

**4. Sharpened, not overturned, for the generators.** `cpp-synchronization` and `cpp-async-tasks` keep a real Opus-tier lift, but it is now precisely bounded: **file layout, guard and comment form, `m_`, namespace, `.h` over `.hpp`, lib directory structure — plus consistency.** Zero correctness lift at that tier. For `cpp-async-tasks` the narrowing is severe enough to restate the verdict: its value at strong tiers is the bundled pool as an artifact and the conventions, not its rules.

### Remaining gaps after this run

| Gap | Status |
|---|---|
| Haiku/Sonnet baselines never grep-verified | **Still open.** Plausibly clean, not confirmed. Closing it means re-running ~15 baselines under structural isolation. |
| ~~No Haiku baseline for `cpp-concurrency-debug` / `cpp-parallel-decompose`~~ | **CLOSED 2026-09-25** — 12 further runs (both arms at Haiku tier, baselines grep-verified). **§6's prediction confirmed:** lift appears at the weak tier. `cpp-concurrency-debug` baseline 2/3 vs with-skill 3/3; `cpp-parallel-decompose` 0/3 fully sound vs 3/3. Sonnet still untested. See §8. |
| Triggering measured on one model, one day, n=1 on passes | **Still open.** |
| `service-benchmark` never evaluated | **Still open.** |
| 3 prompts per skill, eval sets written by the description authors | **Still open.** |
| Skill defects found in this run not yet applied to the `SKILL.md` files | **New.** 3 pending iteration-2 changes (decompose eval-0 and eval-2, debug eval-1). |

### Standing conclusion after the completion run

Nothing in §6 reverses, but the load-bearing sentence gets narrower and more defensible:

> On a strong model, these skills do not add capability at all — 12/12 clean Opus baselines were correct, and on 4 of 12 prompts the baseline was *better* than the skill. What survives at every tier is that the output is **specific to this project and uniform across runs** without anyone restating the conventions. On weak models, and only there, the skills also prevent shipped defects: a semaphore that caps at 1 instead of 4, and a data race in the function written to parallelize safely.

The corollary from §6 — *encode what the model gets wrong, not what it already does right* — now cuts against two of the five skills. `cpp-concurrency-debug` and `cpp-parallel-decompose` generate no code, so they cannot carry idiom, which is the one thing that survives at strong tiers; and their reasoning content was matched or beaten by an unaided Opus on 3 of 6 prompts. On the evidence, they are candidates for the same demotion `cpp-concurrency-reference` received (skill → doc, see `docs/cpp-parallel-skills-decisions.md`), pending a weak-tier measurement that would show whether their floor still earns a trigger.

---

## 8. Weak-tier run for the two advisory skills — 2026-09-25

§7 closed the "no baseline" gaps and found **zero lift at Opus tier** for `cpp-concurrency-debug` and `cpp-parallel-decompose`, then flagged the obvious objection: §6 predicts advisory value concentrates where the model is weak, and that had not been tested. It has now.

**Method:** 12 runs — **both arms at Haiku tier**, 3 prompts per skill. Running both arms was not optional: comparing a Haiku baseline against the existing Opus with-skill runs would confound tier with skill, which is the same class of instrument error §1 documents. The 6 baselines used structural isolation and were grep-verified clean.

### Result: the prediction holds

| Skill | baseline Haiku | with-skill Haiku | baseline Opus | with-skill Opus |
|---|:---:|:---:|:---:|:---:|
| `cpp-concurrency-debug` | **2/3** | 3/3 | 3/3 | 3/3 |
| `cpp-parallel-decompose` | **0/3 fully sound** | 3/3 | 3/3 | 3/3 |

**Advisory skills do have lift — at the bottom tier only.** The shape matches the generators exactly: capability lift is a weak-model phenomenon. What differs is that advisory skills have *nothing else*, because they emit no artifact and therefore cannot carry idiom, which is what survives at strong tiers.

### The one correctness lift, and it is the textbook case

`cpp-concurrency-debug` eval-1 (mutex added, TSan clean, result still varies). The Haiku **baseline** led with *"race condition on the final read — you're almost certainly reading the final result without holding the lock"* as cause #1. That is a **data race**, contradicting the evidence it was handed, and its headline fix is a no-op. It then listed `result += value;` under **"COMMUTATIVE (safe, order doesn't matter)"** — for floating point that is inverted, since the issue is that `+` is not **associative**. Its own stated rule certifies the bug as safe.

The **with-skill** run opened with the right classification — *"race condition, not a data race … the bug is in the logic, not the synchronization"* — and offered compute-locally-then-merge as a fix. That is the skill's data-race vs race-condition table doing precisely the job it was written for, on the tier that needs it.

### Where advisory lift is weaker than it looks

`cpp-parallel-decompose` produced no prompt where the Haiku baseline got the **plan** wrong. It got plans right with unreliable numbers:

- eval-1: computed arithmetic intensity as **14.7 ops/byte** (assuming B is read once per thread) where the non-blocked loop gives **~0.125 FLOP/B** — two orders of magnitude off, yet it still concluded "bandwidth bound". A verdict that survives its own broken arithmetic is a worse habit than a wrong verdict.
- eval-2: invented two rules and stated them as fact ("speedup ∝ √threads ⇒ memory bound"; "miss ratio > 25% ⇒ bandwidth saturation"), and proposed per-core utilization as the discriminator — which cannot separate its own top suspect, since bandwidth-bound code shows 100% on every core.

So the lift is **arithmetic reliability and having a quantitative gate at all**, not correcting a wrong decision.

### The finding that matters most: a skill defect propagates into its own output

`cpp-parallel-decompose` eval-0 is now measured in four configurations:

| Config | Answer |
|---|---|
| Haiku baseline | 2.1×, flat |
| Haiku with-skill | 2.1×, then corrected to 3–5× via a "pipeline factor" |
| Opus with-skill | 2.1×, ceiling 2.5× |
| **Opus baseline** | **rejects the model** — reads parallelize, serial fraction is 1–3%, 5–7× on NVMe, and many readers on a spinning disk can make it *slower* |

The only configuration that got it right is the one with **no skill and a strong model**. Teaching the correct model — *serial today ≠ inherently serial*, and the two-roof bound over storage concurrency `D` — would have improved both arms at both tiers. **This is not a missing lift; it is content that actively caps the output.** It is the clearest instance in the whole harness of the §6 corollary: a rule that duplicates conventional wisdom is not neutral, it *pins* the answer to conventional wisdom.

### Demotion decision for the two advisory skills

§7 listed both as candidates for the skill → doc demotion that `cpp-concurrency-reference` received. The weak-tier evidence resolves it differently for each:

| Skill | Decision | Why |
|---|---|---|
| `cpp-concurrency-debug` | **Keep as a skill** | On the tier that needs it, it stops a weak model from prescribing a no-op fix to a real bug *and* from stating a rule that blesses the bug. A floor that specific is worth a trigger. |
| `cpp-parallel-decompose` | **Keep, but fix first** | Weak-tier lift is real but smaller (numbers, not decisions), and its eval-0 content demonstrably caps its own output at both tiers. The pending iteration-2 changes are now load-bearing rather than optional. |

Neither is demoted. The `cpp-concurrency-reference` demotion remains correct on its own grounds — trigger collision plus pure routing — which are different grounds from these.

### Remaining gaps after this run

| Gap | Status |
|---|---|
| Sonnet baseline for the two advisory skills | **Open.** Lift now known at Haiku (real) and Opus (zero); the middle is interpolated, not measured. |
| Haiku/Sonnet generator baselines never grep-verified | **Still open.** |
| ~~3 skill defects found, not applied to `SKILL.md`~~ | **CLOSED 2026-09-25** — applied to `cpp-concurrency-debug` (iteration 2), `cpp-parallel-decompose` (iteration 2) and, carried over, `cpp-parallel-benchmark` (iteration 3). Per-change detail in each benchmark doc. **Unverified:** written from the measured gaps, not validated by a fresh run — see §9. |
| Triggering measured on one model, one day, n=1 on passes | **Still open.** |
| `service-benchmark` never evaluated | **Still open.** |

### Standing conclusion, final form

> **Capability lift is a weak-model phenomenon; preference lift needs an artifact to ride on.** Generator skills have both: they teach Haiku and they carry this project's conventions at every tier. Advisory skills have only the first, so their value decays with each model release — measured, not assumed: 2/3→3/3 and 0/3→3/3 at Haiku, 3/3→3/3 at Opus. And content that merely restates conventional wisdom is not free: on one prompt it held every configuration except the unaided strong model down to an answer that is wrong for the common case.

---

## 9. Fixes applied — 2026-09-25

The three defects §7 and §8 found in the advisory skills are now in the `SKILL.md` files, plus one carried over to the benchmark skill. Per-change tables live in the respective benchmark docs; the summary and the caveat:

| Skill | Iteration | Change |
|---|---|---|
| `cpp-concurrency-debug` | 2 | Race-condition fix order **reversed** — per-thread partials over static ranges first, barrier/latch demoted with its cost stated (determinism by serializing). Added the FP-associativity mechanism, the low-bit-jitter vs torn-value discriminator, "a silent TSan is evidence, not a blind spot", a non-fix list, the audit list of other order-sensitive combines, and the 20-run detection test. |
| `cpp-parallel-decompose` | 2 | "Serial *today* ≠ inherently serial" + the two-roof bound `T ≥ s + max(f_cpu/C, f_io/D)` with the worked `D` table and the HDD-seek-thrash warning. Karp–Flatt promoted from footnote to the prescribed diagnostic, with the trend table and an explicit sweep. Output now asks for a range keyed to the binding resource, plus a pre-return checklist. |
| `cpp-parallel-benchmark` | 3 | New rule 10 (control the page cache; declare cold vs warm; route services to `service-benchmark`) and "efficiency low" now demands a Karp–Flatt sweep rather than one figure. |

In every case the *untouched* parts are as informative as the changed ones: the deadlock/livelock/starvation/data-race/abandoned-lock sections and decompose Steps 1–4 were left alone because every baseline at every tier got them right unaided. Editing them would violate the corollary this harness exists to enforce.

### These fixes are unverified, and that matters

They were written **from the measured gaps, not validated by a new run.** The harness's own second corollary — *never trust a measurement whose instrument you have not validated* — has a sibling that applies here: never assume an edit had the effect you intended. The specific things a re-run would establish:

| Question | Test |
|---|---|
| Does the decompose skill now stop capping eval-0 at 2.5×? | Re-run eval-0 with-skill at Haiku and Opus; check whether the answer names `D` and gives a storage-keyed range |
| Did the fix reordering take on debug eval-1? | Re-run with-skill at Haiku; check whether per-thread partials is offered *before* the barrier |
| Does rule 10 fire at all? | No current benchmark prompt touches files — a fourth prompt is needed to test it |
| Did the additions cost anything? | Re-run the prompts the skills already passed; added length can crowd out rules that were working |

That last row is the one most easily forgotten. Every change here made a skill **longer**, and the harness's first corollary is that skill content which duplicates common knowledge dilutes the parts that work. The additions were chosen because a clean baseline beat the skill on exactly those points, which is the best available evidence that they are not dilution — but it is an argument, not a measurement.

**Status: iteration 2 (debug, decompose) and iteration 3 (benchmark), unverified.**
