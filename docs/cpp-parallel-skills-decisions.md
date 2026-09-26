# C++ parallel/concurrent skills — design & evaluation decisions

Rationale behind the C++ parallel-programming skill set built from the "Parallel and Concurrent Programming with C++" course notes + examples. Records what was decided and why, so the choices aren't re-litigated later.

## The skill set (final)

Five skills + one shared reference doc, split by **mode of use** (not by topic — topic-per-skill would be ~25 trivial, colliding skills):

| Artifact | Type | Mode |
|---|---|---|
| `cpp-concurrency-debug` | skill (diagnostic) | debug a concurrency bug (symptom → cause → fix) |
| `cpp-synchronization` | skill (generator) | write sync code (mutex/atomic/shared_mutex/condvar/semaphore/barrier/latch) |
| `cpp-async-tasks` | skill (generator) | write task-based parallelism (async/future/promise/pool/fork-join) |
| `cpp-parallel-decompose` | skill (advisory) | decide whether/how to parallelize (Amdahl, work/span, granularity) |
| `cpp-parallel-benchmark` | skill (generator) | measure speedup/efficiency with a trustworthy harness |
| `docs/choosing-concurrency-primitives.md` | shared doc | pick the right primitive ("which one?") |

Existing `cpp-producer-consumer` is linked, not duplicated. All skills target **C++17**, follow `docs/cpp-conventions.md`, and route to each other.

## Why `cpp-concurrency-reference` was removed (demoted to a doc)

It originally shipped as a sixth skill — a pure decision map ("mutex vs atomic vs semaphore…"). It was demoted to `docs/choosing-concurrency-primitives.md` because:

1. **It was the biggest source of trigger collision.** "Which primitive?" overlaps with `cpp-concurrency-debug` (diagnose), `cpp-synchronization` (write), and `cpp-parallel-decompose` (worth parallelizing?). A dedicated skill competing for those triggers risked firing in place of the right one.
2. **It was purely advisory — zero code generation.** Its whole job was to route to the other skills. That is reference data, not an action, so it fits a doc that the generator/diagnostic skills link to at their "choose the primitive first" step.
3. **No loss of data.** The full decision map, close-call reasoning (mutex vs atomic, mutex vs shared_mutex, semaphore vs mutex, barrier vs latch), C++17 fallback table, and demo index all live in the doc. `cpp-synchronization` and `cpp-concurrency-debug` link to it.

Trade-off accepted: a pure "should I use a mutex or an atomic?" question with no intent to write code no longer has a dedicated trigger; it will be handled inline or fall to `cpp-synchronization`. Judged worth it to cut collision. The original skill's evaluation is preserved in `docs/choosing-concurrency-primitives-review.md`.

## Benchmark rationale (how the skills were evaluated)

Each skill was validated by running realistic prompts through a subagent with the skill loaded, judged for correctness + project idiom. Two depths:

- **Quick review (5 skills):** 3 with-skill prompts each, judged qualitatively. Recorded in `docs/<skill>-benchmark.md`.
- **Detailed loop (`cpp-parallel-benchmark`):** with-skill vs no-skill baseline per prompt, graded against explicit assertions (steady_clock, warm-up, N-run mean+stddev, correctness check, speedup/efficiency, in-place reset). This surfaced two real gaps a clean baseline had that the skill lacked → fixed in iteration 2 (dead-code-elimination guard, report minimum alongside mean).

### Baseline contamination caveat
Subagents told "no skill" but running in `~/.claude/skills` sometimes **read the skill files anyway**, producing skill-identical output and voiding the comparison.

**An instruction is not isolation.** Merely forbidding the baseline from reading that directory — the first fix, used in the Haiku/Sonnet cross-tier runs — leaves the skill tree sitting in the agent's own working directory, one `Glob` away. A clean baseline requires **structural isolation**: a project root on a path that does not physically contain `~/.claude/skills`, plus a **post-hoc grep** of the output for the skill's own vocabulary (reference-file names, sibling skill names, distinctive helper identifiers) to confirm nothing leaked. Method and worked example: `docs/cpp-parallel-benchmark-benchmark.md`, section "Baseline contamination — root cause and fix".

Consequence for the numbers below: the Opus baselines were re-run 2026-09-25 under structural isolation and verified clean. The Haiku/Sonnet baselines predate that method — they were prompt-forbidden only and never grep-verified. Their results are consistent with the verified Opus run, but count as plausibly clean rather than confirmed clean.

### Cross-tier experiment (the 3 generators, Haiku + Sonnet + Opus)
Clean baselines across model tiers, documented per skill. Consolidated findings:

1. **with-skill is uniform; baseline swings by prompt and tier.** The durable value is a predictable quality floor, tier-independent.
2. **Fundamentals lift is inversely proportional to model strength.** Haiku needs the skill to get steady_clock/stddev/in-place-reset right; Sonnet and Opus mostly know them (Opus's strongest baseline produced a DCE barrier, CV/MAD and a multi-shape workload sweep unprompted).
3. **Idiom lift is constant across tiers.** No clean baseline at any tier (Haiku, Sonnet or Opus — 0/9 on the benchmark skill) followed the project conventions reliably — even when the conventions were handed over in the prompt (namespace/lib-name, include-guard format). The skills' worked examples anchor idiom better than instructions do. Matches the thesis of `cpp-skills-benchmark.md`.
4. **Edge rules the model forgets under load** are where the skills earn their keep: dead-code-elimination guard (benchmark), the C++17 semaphore/barrier fallback (synchronization — Haiku's baseline reached for C++20 `binary_semaphore` and used it *wrong*, `binary_semaphore(4)` maxes at 1), stateless-recursion depth (async — Haiku's baseline wrote a data race on a shared depth counter).
5. **Honest limits:** where knowledge is common (the famous `std::async` discarded-future trap), skill lift is ~0 — every tier and config already knew it. `cpp-async-tasks` had the lowest lift of the three generators; still net positive (prevents the divide-and-conquer race, enforces futures-over-shared-state + idiom, ships a correct pool).

Per-skill detail: `docs/cpp-concurrency-debug-benchmark.md`, `cpp-synchronization-benchmark.md`, `cpp-async-tasks-benchmark.md`, `cpp-parallel-decompose-benchmark.md`, `cpp-parallel-benchmark-benchmark.md`, `choosing-concurrency-primitives-review.md`.

Note that all of the above measure **output quality given that the skill fired**. Whether each skill *fires on the right queries* is a separate dimension, measured in `docs/cpp-skills-trigger-benchmark.md`.

## Description (triggering) optimization — decided NOT to run

**Decision:** accept the current hand-written descriptions as-is — **confirmed 2026-09-25 by a corrected measurement of all five skills: 89/94 (positives 40/44, negatives 49/50).** Full results, method and caveats: **`docs/cpp-skills-trigger-benchmark.md`**. The eval-sets stay at `cpp-skills-workspace/trigger-evals/*.json`, now alongside the working runner (`trigger_probe.py`) and raw data (`results-2026-09-25/`), to re-run if real-world usage shows a specific skill mis-triggering.

The decision stands; **the original reasoning for it does not.** Superseded record below, kept because the failure mode is worth not repeating.

### Superseded: the 2026-09-24 canary (invalid instrument)

A canary run on `cpp-parallel-benchmark` via the skill-creator's `run_loop.py` (2 iterations, 2 runs/query) originally produced this reasoning:

- ~~**Negatives: 100% pass — zero over-triggering.**~~ Artefact. The harness tests a throwaway stub command, which is never invoked for anything, so every negative passed *by construction*. It was not measuring discrimination. Real figure: 49/50, passing on merit (the correct sibling skill takes the query).
- ~~**Positives under-triggered (≈6/11 train), largely inherent, not a description defect.**~~ Artefact. Real figure for that same skill: **17/18**. The harness scored a real-skill invocation as a miss, because it only counts its own hashed stub name — and it aborted on any first tool that wasn't `Skill`/`Read`, while `Bash`-then-`Skill` is a common order. Genuine under-triggering exists but is small and localized: 4 of 44 positives across all five skills.
- **Cost/ROI:** this part still holds — each loop spawns many recursive `claude -p` runs.

`cpp-skills-workspace/HANDOFF.md:28` had flagged "harness artefact?" as an open question and it was never diagnosed; the decision was taken assuming the numbers were real. The lesson is the same one as the baseline-contamination caveat above: **validate the instrument before trusting — or acting on — what it reports.** Defect details with file:line are in `docs/cpp-skills-trigger-benchmark.md`.

Side note: `run_loop.py` registers transient temp skill variants during a run (they appeared in the skill list); they are `tempfile`-based and clean up on exit — no on-disk residue.

## Completion run 2026-09-25 — what it did to these decisions

12 Opus baselines (3 prompts × `cpp-synchronization`, `cpp-async-tasks`, `cpp-concurrency-debug`, `cpp-parallel-decompose`) closed the two "no baseline" gaps declared in `docs/cpp-skills-harness-report.md` §5. All under structural isolation, all grep-verified clean. Details in each `docs/<skill>-benchmark.md`; consolidated in the harness report §7.

Effect on the decisions recorded above:

1. **The five-skill split still stands** — nothing in the run argued for a different decomposition by mode of use.
2. **The `cpp-concurrency-reference` demotion is retroactively vindicated, and now points at two more candidates.** The reasons for demoting it were trigger collision and *zero code generation*. The completion run shows the second reason bites harder than assumed: the two remaining **advisory** skills, `cpp-concurrency-debug` and `cpp-parallel-decompose`, have **zero measured lift at Opus tier** (baseline 3/3 on both, and better than the skill on 3 of 6 prompts). They generate no code, so they cannot carry project idiom — which the run confirms is the only thing that survives at strong tiers. They are now candidates for the same skill → doc demotion, **pending a weak-tier measurement**, since §6 predicts advisory value concentrates where the model is weak and that has not been tested.
3. **The baseline-contamination caveat above is upgraded for these four skills.** Their Opus baselines are now confirmed clean by the structural-isolation + grep method, not merely prompt-forbidden. The Haiku/Sonnet baselines remain plausibly-clean-only.
4. **Three skill defects were found and are NOT yet applied** — deliberately, so the docs do not drift from the measurement:
   - `cpp-parallel-decompose` eval-0: replace "40% I/O ⇒ Amdahl s=0.40 ⇒ 2.1×" with "serial *today* ≠ inherently serial" and the two-roof bound `T ≥ s + max(compute/C, io/D)` over storage concurrency `D`.
   - `cpp-parallel-decompose` eval-2: teach Karp–Flatt across a thread sweep, read by trend, instead of inverting Amdahl from one data point.
   - `cpp-concurrency-debug` eval-1: the fix for a non-deterministic reduction is per-thread partials over **static** index ranges merged in fixed order — not a barrier/latch, which buys determinism by serializing. Add the FP-associativity mechanism and the "low-bit jitter vs torn value" discriminator.
5. **The decision not to run the description-optimization loop is untouched** — this run measured output quality only.

## Run de tier fraco 2026-09-25 — decisão de demoção resolvida

12 runs adicionais (os dois braços em Haiku, 3 prompts por skill) fecharam o gap aberto acima. Detalhe em `cpp-skills-harness-report.md` §8 e nos dois benchmark docs.

| Skill | baseline Haiku | com-skill Haiku | baseline Opus |
|---|:---:|:---:|:---:|
| `cpp-concurrency-debug` | 2/3 | 3/3 | 3/3 |
| `cpp-parallel-decompose` | 0/3 sólidos | 3/3 | 3/3 |

**Decisão: nenhuma das duas advisory é demovida.**

- `cpp-concurrency-debug` **fica como skill**: no tier fraco impede que o modelo prescreva um fix no-op para um bug real e enuncie uma regra que declara o bug seguro (`result += value` listado como "commutative, safe" — o problema é associatividade, não comutatividade).
- `cpp-parallel-decompose` **fica, mas com os defeitos corrigidos antes**. O lift no tier fraco é real e menor (aritmética e gate quantitativo, não decisão errada), e o conteúdo do eval-0 foi medido em 4 configurações: a única que acertou foi **sem skill, modelo forte**. Ou seja, aquele conteúdo *limita* o output nos dois tiers — os itens 4.1 e 4.2 da lista acima deixam de ser opcionais.

A demoção original de `cpp-concurrency-reference` continua válida pelos motivos dela (colisão de trigger + roteamento puro, zero geração de código), que não são os motivos avaliados aqui.

Gap remanescente: **baseline Sonnet** para as duas advisory. Haiku (lift real) e Opus (lift zero) medidos; o meio está interpolado.

## Iteração 2/3 aplicada — 2026-09-25

Os defeitos listados no item 4 do bloco anterior ("Run de tier fraco") estão corrigidos nos `SKILL.md`:

- `cpp-concurrency-debug` → iteração 2 (ordem dos fixes invertida, mecanismo FP, discriminador, TSan-como-evidência, não-fixes)
- `cpp-parallel-decompose` → iteração 2 (serial-hoje-≠-inerentemente-serial + teto duplo com `D`; Karp–Flatt como diagnóstico prescrito, com sweep)
- `cpp-parallel-benchmark` → iteração 3, herdada (regra 10: page cache cold vs warm, rotear serviço para `service-benchmark`; eficiência baixa exige sweep)

Detalhe por mudança nos três benchmark docs; resumo em `cpp-skills-harness-report.md` §9.

**Condição de manutenção de `cpp-parallel-decompose` satisfeita.** A decisão registrada acima foi "manter, mas corrigir antes" — o "antes" agora está feito.

**Status: não verificado.** As edições saíram dos gaps medidos, não de um run novo. O que um re-run estabeleceria está tabelado em §9; o item mais fácil de esquecer é que toda mudança alongou a skill, e alongar dilui.
