# BDD skills — development record

Two skills, built and measured 2026-09-27: `bdd-gherkin-style` (how to word
Gherkin) and `bdd-behave-allure` (the Python machinery under it, plus Allure
reporting). Source material was the hand-written suite at
`~/development/E2E (Firewall Redir_Firewall Input)/features/`.

Everything below is measured, not assumed. Workspaces hold the raw runs:
`bdd-gherkin-style-workspace/` and `bdd-behave-allure-workspace/`, one directory
per iteration, each run with its `outputs/`, `grading.json` and `timing.json`.

---

## Results

Pass rate is the fraction of that eval's assertions a graded run satisfied. Each
cell is one run per configuration — n is small, so read the direction, not the
decimals.

| Iteration | Skill | Evals | With skill | Baseline |
|---|---|---|---|---|
| 1 | bdd-gherkin-style | 2 | 100% | 87.5% |
| 1 | bdd-behave-allure | 2 | 95% | 88.8% |
| 2 | bdd-gherkin-style | 4 | 97.7% (38/39) | 90.7% (35/39) |
| 2 | bdd-behave-allure | 1 | 100% (9/9) | 89% (8/9) |
| 3 | bdd-gherkin-style, lean vs full | 4 | lean 42/43 (97.7%) | full 43/43 (100%) |

Baseline = same prompt, same model, no skill. Iteration 3 changed the question:
both arms had the skill, one lean and one full.

### Cost

Averaged from `timing.json` per run.

| | tokens | vs baseline | seconds | vs baseline |
|---|---|---|---|---|
| bdd-gherkin-style | 86,880 | +13,099 (+17.8%) | 100.4 | +30.1 (+42.9%) |
| bdd-behave-allure | 86,053 | +6,715 (+8.5%) | 115.7 | −4.7 (−3.9%) |

The gherkin skill costs more because it is nearly twice the material (35k chars
vs 19k) and because it makes the model do more work on the output — collapsing
tables, checking placeholder grammar, preserving boundaries.

The behave skill is *faster* than the baseline on debugging tasks (−12s on the
isolation bug, −17s on the Allure setup): the baseline spends that time
exploring wrong hypotheses. It is slower only when building from scratch.

Note: the `tokens` column in the generated `benchmark.json` files is unreliable
(zeros and mismatched values — the aggregator reads a different field). Use
`timing.json`.

---

## What the skills actually changed

Each of these is a difference between the with-skill and baseline output on the
same prompt, script-verified by the grader.

**Outcome as data.** The baseline repeatedly wrote two near-identical Scenario
Outlines — one for the cases that pass, one for the cases that fail — with the
threshold buried in prose ("the key has already used its full quota"). In one
run the actual limits, 60 and 600, appeared **nowhere in the file**. The skill
produces one table with a `result` column where the boundary is two adjacent
rows, which forces the number to be written down.

**Vocabulary drift.** A baseline file expressed acceptance three ways ("every
request is served normally", "the request is served normally", "the second key's
request is served normally") — three step definitions where one would do — and
added a redundant negative assertion next to the positive one it duplicated.

**Behaviour loss during refactor.** Asked to rewrite a leaked feature file, the
baseline dropped the granted case entirely: `192.168.100.10:8080` vanished from
the output and the file no longer asserted that traffic reaches a backend at
all. Its rationale described this as a clean split and never mentioned the loss.
In another eval the baseline "fixed" a broken Scenario Outline by deleting it.

**Reframing a seeded defect as a design choice.** Given a Behave suite with
collaborators rebuilt per scenario, the baseline moved only `DockerExec` and
wrote in its diagnosis that "per-scenario objects stay per-scenario" — turning a
planted bug into a justification.

**Reset-forward vs harden-the-teardown.** For the isolation bug, the skill moves
the guarantee into `before_scenario` so a crashed scenario cannot contaminate
the next. The baseline instead wrapped `after_scenario` in try/finally and
justified it with a failure mode that appears nowhere in the inputs.

**Makefile that still reports on failure.** The only assertion separating the
two arms on the Allure task. The baseline restructured `report:` to depend on a
prerequisite target running behave, so any failing scenario aborts before
`allure generate` — no report exactly when one is needed.

---

## Defects the evals found in the skills themselves

**`@wip` on a deliverable (found twice, fixed).** The skill listed `@wip` among
the status tags without saying when not to use it, and the model twice shipped a
feature file tagged `@wip` in response to "write this so QA can review it" —
under the project's own `make tags` convention, a file excluded from normal
runs. The Tags section now explains that status tags are exclusions: a feature
file written *before* the behaviour exists is the specification, not work in
progress, and should run and fail; a file handed over for sign-off must not
carry a tag that stops the suite executing it.

**Allure traps buried in a reference.** They lived only in
`references/allure-reporting.md`, which is read on demand. Promoted to a Traps
section in the body, framed by "every one of these fails quietly — the run
succeeds, the report just comes out wrong". One trap came directly from a
baseline run that registered the formatter in `behave.ini` without adding
`allure-behave` to `requirements.txt`, which breaks *every* `behave` invocation
on a fresh checkout, not just reporting.

**`after_scenario` emptied out.** Both arms failed the same assertion: neither
tore down what the scenario created using state recorded on `context`. The
with-skill run took "before_scenario is the guarantee, after_scenario is
courtesy" literally and reduced teardown to a single line. The wording invites
that reading and has not been fixed yet — open item.

---

## Trimming the skill (iteration 3)

`bdd-gherkin-style` had grown to 27.8k chars. Test: move "Say what, not how"
(keeping two pairs in the body) and all of "Collapsing repeated scenarios" into
reference files, leaving SKILL.md at 18.4k — a 34% cut — and run the same four
evals against both variants.

| eval | lean | full | delta |
|---|---|---|---|
| g1 new spec | 87,358 | 88,258 | −900 |
| g2 refactor | 86,584 | 87,963 | −1,379 |
| g3 outline collapse | 84,278 | 87,300 | **−3,022** |
| g4 what-not-how | 90,102 | 88,857 | **+1,245** |
| mean | 87,080 | 88,094 | −1,014 (−1.2%) |

**Cutting a third of the body bought 1.2% of tokens.** The content gets read
either way; only the number of calls changes. The lean variant opened two or
three reference files where the full one opened a single one.

The per-case pattern is the real finding:

- **g3 was the only genuine win** (−3,022). The lean variant read only
  `collapsing-outlines.md`. The full variant loaded its whole body *plus*
  `worked-examples.md`, which had nothing to do with the task.
- **g4 was the only loss** (+1,245). The lean variant opened all three
  references and read more than the full body would have cost.

So splitting pays only when a task is narrow enough to need one reference. Real
`.feature` files usually mix technical leakage *and* duplication, which makes g4
the representative case, not g3.

### Quality: the split cost one real rule

Lean scored 42/43, full 43/43. Three of the four evals saturated — g2, g3 and g4
came out 30/30 against 30/30 with near-indistinguishable outputs, which confirms
that moving the collapsing material and the extra pairs out of the body cost
nothing measurable there.

The single failure is the interesting part. On g1 (greenfield write) the lean
variant produced **three** renderings of one outcome — `the final request is
accepted`, `the request is accepted`, `every request is accepted` — where full
produced `the request is accepted` ten times. Five step definitions for two
concepts.

It traces straight to the cut. The rule that two scenarios with the same
observable outcome must share an *identical* `Then` was body text in the full
variant and a reference in the lean one, and that reference's pointer is
conditioned on "a file that reads as technical" — g1 is a greenfield write, so
the trigger never fired and the rule never applied.

Two smaller lean-side regressions no assertion caught:

- g2: the two port-redirection scenarios were tagged `@geoip`, which will
  mislead anyone running `--tags=@geoip`. Full tagged them `@port-redirection`.
- g1: full put the boundary at 59/60 (last allowed vs first refused) where lean
  used 60/61 (at the limit vs over), and full's `/internal` exemption exhausts
  the key first, which actually proves exemption; lean's volume-based version
  proves it only for Free.

The lean variant was *better* on one thing: full's g4 Outline used a single
`outcome` column as both the stimulus and the asserted result (`Given a <outcome>
payment notification` … `Then it is recorded as <outcome>`), so the row cannot
fail on the mapping it exists to pin, and it renders as "Given a succeeded
payment notification". Lean separated input from result properly.

**Conclusion: the lean split is sound except for short rules that apply to every
task.** Those are cheap to keep inline and expensive to gate behind a
conditional pointer. Acting on this, the live skill's `## Vocabulary discipline`
section was sharpened to say that one-phrasing-per-concept applies to what a step
*renders to* — an Outline row expanding to `accepted` must match the standalone
`the request is accepted` character for character. That is the rule the lean
variant dropped, and it belongs in the body of either variant.

Timing differences in this iteration (−6.7% mean) are not trustworthy: one full
run took 310s against the lean run's 146s on the same prompt, and another went
the opposite way. n=1 per cell.

---

## Lessons about writing the evals

These cost more to learn than the skill content did.

**An assertion that passes in both arms measures nothing.** In iteration 2, 34
of 39 assertions passed on both sides. "Examples tables are pipe-aligned" and
"every scenario name states an outcome" passed in every run of every iteration —
pure freebies.

**Assertions must check preservation, not just form.** Iteration 1's g2 scored
9/9 for both arms while the baseline silently deleted a case. Adding one
BEHAVIOUR PRESERVATION assertion ("the file still asserts traffic reaches
192.168.100.10:8080") turned the same eval into the most valuable one in the
suite. Any assertion phrased as "X is fixed" should be phrased as "X is repaired
and still present" — the baseline passed a fix-assertion by deletion.

**Do not name the causes in the symptom fixture.** The Allure eval's
`symptoms.md` said "allure-commandline needs a JRE" and "the layer label is
ignored". That tests reading, not knowledge: eight of nine assertions became
derivable from the fixture. Describe only what is observed — "the report shows 4
tests, the suite has 2".

**Baselines are contaminated by the repo they run in.** Subagents inherit the
working directory, so the baseline reads the project's `CLAUDE.md` — which
documents the Allure traps verbatim. The baseline cited the `feature` label
collision without having the skill. Every measured delta here is therefore a
*lower* bound.

**Build traps into fixtures.** The outline-collapse fixture included two
scenarios that must *not* be merged (one with an extra condition, one tagged
`@bug`, since a tag cannot vary per row). Both arms handled them correctly,
which is itself the finding — without the traps, a blind "merge everything"
would have scored identically to careful work.

**Use a fresh domain when testing transfer.** The "say what, not how" pairs were
derived from a Haiku-written version of `monitoring.feature`. Testing on that
same file would have measured memory of the pairs. The eval uses a payment
webhook file instead, so a pass means the principle transferred. A word-boundary
scan for Kafka, topic, partition, offset, redis, SETNX, TTL, gauge, counter,
upsert and friends returned zero hits in both arms — the vocabulary genuinely
changed rather than being renamed.

**The grader should be asked to critique the evals.** Every significant assertion
improvement here came from the grader's `eval_feedback`, not from me reviewing my
own assertions.

---

## Method

Both skills were produced from an existing hand-written suite rather than from a
blank page: read the real `.feature` files, extract the implicit conventions,
state them with reasons. The "say what, not how" pairs were generated by giving
Haiku an engineer's handover note describing the implementation (iptables
chains, NFLOG prefixes, DB columns) and asking for a feature file. It produced
exactly the leaks the hand-written file avoids, and each leak became a
Do/Don't pair against the real file's wording.

That trick generalises: to find a skill's teaching material, have a model do the
task without the skill from implementation-flavoured input, then diff against
the known-good artefact.

## Open items

- `after_scenario` guidance in `bdd-behave-allure` invites emptying teardown
  entirely; both arms failed that assertion.
- Decide whether to ship the lean variant. Quality is now equivalent given the
  vocabulary fix above, but the measured saving is 1.2% of tokens, which does not
  obviously pay for maintaining two more files. Leaning towards keeping the full
  body and treating `variant-lean/` as a rejected experiment.
- The lean variant still has the `@geoip` tag-axis slip; if it ships, the tag
  discipline rule needs to move inline too.
- The `symptoms.md` fixture for the Allure eval should be rewritten to state
  observations without causes, then re-run.
- Iteration 3 used n=1 per cell; timing conclusions need repeats to mean
  anything.
- g3 is now saturated (12/12 both arms). A harder seed would restore its value:
  rows already split into two Examples blocks by outcome, or per-row tags that
  tempt accumulation.
- g4's ordering assertion reads as banning the business-level sequencing the
  intent document itself requires; narrow it to broker-level ordering.

---

## Follow-up: user-story benchmark (2026-09-28)

The evals above all feed the model an existing artefact — a leaked feature file,
a spec document, a symptoms note. A separate run tested the input a backlog
actually produces: a bare user story, nothing to translate.

Headline: the first conclusion drawn from the artefact-based evals ("say what,
not how has no lift, Opus already does it") was **an artefact of fixture design**.
Cleaning a leaked file is translation and Opus does it unaided. Writing from a
story forces the model to generate the detail, and in some domains it generates
the mechanism.

Which domains is the finding: **substrate proximity**. Full technical report, including every instrument defect found along the way: `docs/bdd-skills-harness-report.md`. The gap tracks how close
an implementation vocabulary sits to the domain, not how well the story is
written. Full account, four stories, in
`docs/bdd-gherkin-style-story-benchmark.md`.

Two limitations recorded there that belong here too:

- **The with-skill arm lost coverage in three of four stories.** Collapsing into
  Outlines and abstracting away mechanism repeatedly dropped a real case the
  baseline kept. The skill has no rule about checking what a collapse removed.
- **No assertion measures rework.** A file that invents five unconfirmed rules
  and a file that flags them as open questions score identically. This was raised
  by a story's author, not by the grader, and it is the clearest gap in the eval
  design so far.
