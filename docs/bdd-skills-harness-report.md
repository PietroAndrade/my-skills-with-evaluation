# Does `bdd-gherkin-style` make the agent better? — harness report

Technical report on the evaluation harness built for `bdd-gherkin-style`, and
what it measured. Same question as `cpp-skills-harness-report.md`, same answer
shape: **when an agent has this skill, what specifically improves, and what does
not.** Written after the measurements, from the measurements.

One dimension was measured here — output quality. Triggering was not tested, so
nothing in this report says whether the skill fires on the right queries.

| Dimension | Question | Status |
|---|---|---|
| **Output quality** | Given the skill fired, is the feature file better? | measured, below |
| **Triggering** | Does it fire on the right queries, and not instead of `bdd-behave-allure`? | **not measured** |

---

## 1. The harness, and two fixture styles that gave opposite answers

Per prompt, two arms: one subagent with the skill loaded, one baseline with no
skill. Assertions fixed in a file before any run, 10-12 per prompt, graded
binary. Both arms always on the same model (Opus), because comparing a weak-tier
baseline against a strong-tier skill run measures the tier.

The run happened in two phases, and the phases disagreed.

**Phase 1 — artefact-based fixtures (h1-h3).** Hand the model an existing
artefact carrying implementation detail and ask for it to be cleaned: a
mechanism-heavy handover note, a leaked feature file, a public API document.

| | baseline | with skill |
|---|:---:|:---:|
| h1 locker handover | 11/12 → **12/12** clean | 11/12 |
| h2 subtle-leakage rewrite | 9/10 | 10/10 |
| h3 public API, over-scrub trap | 10/10 → 9/10 clean | 10/10 |
| **total** | **30/32** (both contaminated and clean) | **31/32** |

Conclusion drawn at the time: *"say what, not how" is common knowledge; the skill
adds nothing on its headline rule.*

**That conclusion was an artefact of the fixture design.** Cleaning a leaked file
is **translation**: the mechanism is visible, and deleting it is easy. A strong
model does it unaided.

**Phase 2 — user stories.** Give the model a bare user story, nothing to
translate, so it must *generate* the detail.

| story | substrate | baseline | with skill | gap |
|---|---|:---:|:---:|:---:|
| 1 firewall rule for RDP access | adjacent | 7/12 | 11/12 | **+4** |
| 2 device audio config, transport named | adjacent | 7/12 | 11/12 | **+4** |
| 4 SSL inspection exclusion | adjacent | 8/12 | **12/12** | **+4** |
| 3 ETL percentile comparison | distant | 11/12 | 12/12 | +1 |
| 3b server response monitoring | distant | 10/12 | 11/12 | +1 |

Same skill, same model, same grader. The fixture style decided the answer.

### Both conditions are necessary — the 2x2

The two phases differ on two axes at once, and separating them matters because
neither axis alone predicts the gap.

|  | **substrate adjacent** | **substrate distant** |
|---|---|---|
| **input is a user story**<br>(model must generate the detail) | **+4** — stories 1, 2, 4 | +1 — stories 3, 3b |
| **input is plain text / an existing artefact**<br>(model must transform what it was given) | +1 — h1, h2, h3 | not tested |

The plain-text fixtures were not substrate-free. They were the opposite: h1's
handover note names Kafka topics, `locker_slots`, a GPIO line and a cron job;
h2's leaked feature file names tables, columns and a retry worker; h3's document
carries RabbitMQ and Celery. The substrate was maximally present — **handed over
explicitly** — and the gap was still ~1.

So the discriminator is not how much mechanism is nearby. It is **who produces
it**:

- **Given the mechanism, a strong model deletes it.** Removing visible plumbing
  is a transformation with a clear target, and Opus does it unaided.
- **Asked to invent the scenario, a strong model invents the mechanism** — but
  only when a mechanism is lying next to the domain to reach for.

`bdd-gherkin-style` earns its keep on **stories, not on plain text**, and among
stories only on adjacent-substrate domains. That is a narrow claim, and it is the
one the measurements support. The empty cell (plain text, distant substrate) is
untested and would be worth one run to complete the square.

---

## 2. Gotchas — every instrument defect found, in the order found

This is the most transferable part of the report. Nine defects; six were mine,
three were the harness's. All of them were silent.

### 2.1 The control group was told the skill's thesis

Claude Code lists every available skill, **with its description**, in every
agent's system prompt — subagents included. For this skill that line reads:
*"House style for writing Gherkin … in business language that hides
implementation detail."*

When the assertion under test is whether the skill teaches the model to hide
implementation detail, the control was handed the answer. A subagent baseline
cannot measure that.

**Fix:** run baselines as a separate headless process,
`claude -p --disable-slash-commands`, which removes the skills listing.

### 2.2 Grep-of-output was mistaken for isolation

Every baseline output was grepped for the skill's vocabulary and came back clean,
and that was reported as "structurally isolated". It proves the skill's *text*
did not leak into the output. It says nothing about what entered the input.

**Rule adopted:** verify what reaches the control's input, not only what leaves
its output.

### 2.3 The working directory was inside the skill tree

Subagents inherit the parent's cwd. Several baselines ran with cwd =
`~/.claude/skills/<skill>` — inside the directory of the skill under test.
Transcript checks confirmed none of them read `SKILL.md`, but "isolation" was
claimed on the strength of a prompt instruction, not a boundary.

### 2.4 The baseline prompt contained hints

*"Complete the task without any style guide"* and *"do not read ~/.claude"* both
tell the model that a style guide exists and is relevant to this task. That is a
hint, landing in the arm that is supposed to have none.

**Fix:** give the baseline the task and nothing else. Isolation is the harness's
job, not the prompt's.

### 2.5 `grep -E` with `\|` — a scan that reported four present items as missing

The leakage scanner used `grep -qiE '2xx\|200'`. In ERE, alternation is `|`; `\|`
is a literal pipe. The scan searched for the string `2xx|200` and reported 2xx,
5xx, HMAC and all five backoff delays as MISSING **in both arms**. All four were
present.

### 2.6 A case-insensitive scan for case-sensitive constants

The same scanner looked for state constants (`EXPIRED`, `RELEASED`) without
`-i` discipline, so ordinary English — "once the lockout has expired", "released
to me" — counted as leaked constants.

Both 2.5 and 2.6 were caught and corrected before any number was recorded,
because the output disagreed with what the agents reported writing.

### 2.7 `pgrep -f` matching its own wrapper — one hour lost

A wait loop polled `pgrep -f "claude -p --model opus"`. That string is inside the
bash command line running the loop, so `pgrep` matched the loop itself. The loop
concluded the processes were alive and span for an hour while the user watched.

The processes had in fact died 30 seconds in.

### 2.8 Headless baselines that could not read their fixtures

Those three processes were launched with cwd set to their *output* directory,
while the fixtures sat one level up. Non-interactive, so no permission prompt
could be granted. They tried `Read`, `cat`, sandboxed `cat` and `python3`,
reported the block, and correctly refused to invent the fixture content.

Nothing surfaced, because the wait loop (2.7) was reporting progress.

**Fix, and the general rule:** a baseline runner must report **exit code and
artefact existence** per run. "No news" is otherwise indistinguishable from
"died on the first call".

```
run() { timeout 900 claude -p ... > "$log" 2>&1
        echo "$name exit=$? file=$([ -f "$out" ] && echo yes || echo NO)"; }
```

### 2.9 Two traps that could be satisfied by restating the feature

- **Threshold trap (story 3).** The story never defined what makes a response
  "bad"; inventing a threshold silently was supposed to fail. Both arms stated
  one concretely as data (20% and 10%). Both passed. The trap measured nothing.
- **Security-tradeoff trap (story 4).** The file had to acknowledge that bypassed
  traffic is no longer examined. The with-skill file says *"the traffic is passed
  through without inspection"* three times, which satisfies the wording without
  ever exploring the consequence.

**The pattern is in the trap-writing, not the model:** if a story's own wording
can satisfy an assertion, the assertion tests nothing. A working trap must
require something the story does not state.

### 2.10 No assertion measures rework

Raised by a story's author, not by the grader. A file that invents five
unconfirmed rules and a file that flags them as open questions score identically.
Story 3's baseline scored 11/12 while inventing a 20% threshold, a trend chart, a
"low confidence" rule and a no-baseline case — all plausible, none confirmed, all
costing a round trip to correct.

The inverse appeared too: one baseline marked five gaps inline as
`# OPEN QUESTION` and received no credit for it.

**Unfixed.** The assertions measure form, not how much the author must send back.

---

## 3. Where the skill genuinely improved things

| Improvement | Evidence | Reliability |
|---|---|---|
| **One phrasing per concept** | baseline failed **6 of 6** applicable runs; with-skill **0 of 6** | the only result that repeats everywhere |
| **Outcome as a `result` column** rather than split pass/fail scenarios | g3 baseline split five scenarios into two Outlines, the second with no value column at all; with-skill produced one table | strong |
| **Mechanism kept out of the asserted `Then`** | story 1 baseline: `Then the rule "Allow-RDP-WAN" appears in the firewall rule list as enabled` — passes while nobody can connect | 1 of 3 adjacent stories |
| **Substrate vocabulary kept out of the supporting steps** | 3 of 3 adjacent stories: bypass lists, policy apply, SNI, session logs, endpoints, payload fields | strong on adjacent domains |
| **A named transport in the context line does not become the subject** | story 2 baseline turned "via REST API and websocket" into an API test suite | 1 of 1 |
| **Collapsing duplicated scenarios** | g3 traps (a `@bug` scenario and an extra-condition scenario) correctly left out of the Outline by both arms; the collapse itself only by the skill arm | strong |

The vocabulary row deserves emphasis because it survived every attempt to
discount it. It held under contamination and after the contamination was removed,
on artefact fixtures and on stories, in five different domains. And the skill's
own description says nothing about phrasing reuse, so it was never a candidate
for the leak in 2.1.

---

## 4. Where the skill made no measurable difference, or cost something

| Area | Result |
|---|---|
| **Cleaning an already-leaked file** | 30/32 vs 31/32. A strong model deletes visible mechanism unaided. |
| **Keeping contract detail on a public API** | h3: both arms kept 410, 2xx, 5xx, 401/403, `Idempotency-Key`, `X-Signature-256` and all five backoff delays, and both excluded RabbitMQ/Celery/`webhook_deliveries`. The "one case where the mechanism is the behaviour" section held nothing the model did not already do. |
| **Distant-substrate domains** | +1 on both. Where the story already speaks in verdicts and metrics, there is nothing to drift toward. |
| **Statistical validity of a requested metric** | Story 3b asked for the **mode** of continuous latencies, which is degenerate without binning. Both arms specified it obediently; the baseline curated its samples so a mode would exist. Out of category for a style skill. |
| **Coverage — a real cost** | The with-skill arm lost a case the baseline kept in **3 of 5 stories**: story 1 asserted `73 hours → returned to sender` against a once-daily batch job (a promise the system does not make); story 2 dropped the zero-active-sources case; story 3b dropped the immutability of the fixed metrics, which is the entire point of calling them fixed. |
| **Over-scrubbing a public fact** | Story 1's with-skill file dropped port 3389 for "remote desktop". 3389 is a public protocol fact, the same category as the HTTP 410 the skill's own exception says to keep. The exception did not fire. |

The coverage row is the most important negative result. Collapsing into Outlines
and abstracting away mechanism repeatedly removed a real case, and **the skill
has no rule about checking what a collapse removed.**

---

## 5. The finding: substrate proximity

**Substrate** is the layer of vocabulary that sits underneath a domain: the words
for how the system stores, transports and manipulates the thing, as opposed to
the words for what happens to someone. A firewall rule's substrate is the rule
list, the chain, the apply action and the log line. A refund's substrate is the
payments table and the ledger entry. Every domain has one; what varies is how
close it sits.

**Proximity** is that distance. It is not the same as "the domain is technical" —
all five domains measured here are technical. It is whether the substrate is
reachable from the words the user used. A story about a rule puts the rule list
one synonym away. A story about whether one run is slower than another puts
nothing in reach, because the thing asked for is a judgement and judgements are
not stored objects.

The term is borrowed in the ordinary sense: the surface a thing grows on. The
domain is what the user talks about; the substrate is what it is implemented on.
An unaided model, asked to invent a scenario, tends to sink to the substrate,
because that is the concrete layer and the domain layer is the abstract one.

The gap does not track how well the story is written, how long it is, or how
ambiguous it is. It tracks **how close the substrate sits to the domain the story
describes.**

Classification test: *when the user names the thing they want, are they naming an
object the system stores and displays?* Equivalently: **can the story be
satisfied by changing a configuration and showing the configuration changed?**

- "a rule enabling this access" → a row in a list. Adjacent.
- "only one audio source was set" → a field with a value. Adjacent.
- "banking sites skip SSL inspection" → an entry. Adjacent.
- "compare current against baseline by percentile" → a judgement, stored nowhere. Distant.

Three adjacent stories: +4, +4, +4. Two distant: +1, +1.

### The gradient: proximity predicts how technically the unaided model writes

The binary adjacent/distant split is the version the assertions scored, but it
flattens something the outputs show clearly. **The closer the substrate, the more
technically the unaided model writes the scenario** — and it degrades in stages,
not all at once.

Three levels appear in the five runs, ordered by how much technical vocabulary
the baseline produced:

| level | condition | what the baseline did | example |
|---|---|---|---|
| **3 — substrate named in the input** | the story or its context names a transport or store | the named layer becomes the *subject*; the file turns into a test of that layer | story 2: "via REST API and websocket" produced `When I send a GET request to "/api/v1/os/properties/audio"`, status codes, JSON field names, websocket topics and frame types |
| **2 — substrate adjacent, unnamed** | the thing requested is a stored object, but no mechanism is named | the mechanism fills the supporting steps, and sometimes captures the `Then` | story 4: `the URL category "Internet Banking" is on the SSL inspection bypass list`, `And the policy is applied`, `from the TLS SNI`, three log assertions. Story 1 additionally captured the outcome: `Then the rule "Allow-RDP-WAN" appears in the firewall rule list as enabled` |
| **1 — substrate distant** | the thing requested is a judgement or an effect | almost nothing technical leaks; what does is presentation, not plumbing | stories 3 and 3b: `the comparison table shows … the absolute delta`, `the trend chart plots percentile "p95"` |

Note what level 1 still produces. Even with no mechanism in reach, the baseline
reached for the nearest concrete thing it could find — a screen. That is the same
move as level 3, with a weaker target.

**Why the gradient runs this direction.** A model asked to invent a scenario
needs something concrete to anchor on. The domain layer is the abstract one; the
substrate is the concrete one. When the substrate is named in the input it is the
most concrete thing available and it wins outright. When it is merely adjacent it
is still the most concrete thing in reach, so it fills whatever slots the outcome
does not occupy. When nothing is in reach, the model invents a surface — a table,
a chart — to have something solid to assert about.

The practical prediction: **naming a transport or a store anywhere in the input,
including in a line of context meant as background, is the strongest single
predictor that an unaided file will come out technical.** Story 2 is the only run
where the substrate was named, and it is the only run where the substrate became
the subject rather than the scenery.

**Caveat, and it matters.** The three-level ranking is a post-hoc reading of the
outputs, not something the assertions scored. The assertions only counted
pass/fail per leak; "how technical" was assessed by reading the files after the
numbers existed. That is exactly the kind of ordering that can be constructed
from noise, and n is 5. It is recorded as a hypothesis with a mechanism, not as a
result.

**What proximity causes, narrowed after story 4.** Story 4 was built to maximise
proximity and predicted to produce the mechanism-as-outcome failure. It did not —
the baseline kept its outcomes behavioural and leaked in the surrounding steps
instead. So:

| effect | across 3 adjacent stories |
|---|---|
| leakage in `Given`/`And` steps, log and config assertions | **3 of 3** |
| the mechanism becoming the asserted `Then` | **1 of 3** |

Proximity makes a model *speak* the substrate reliably and *assert* it sometimes.
The second is the failure that ships a spec passing while the feature is broken,
and it is rarer than predicted.

**Correlation, not cause.** Five stories differing in many ways at once. The
single-variable test — one story, run with and without a context line naming a
transport — has not been run. Story 2 is the strongest hint that the context line
alone suffices: its transport appeared only in the context, never in the story.

---

## 6. What is NOT measured

| Gap | Consequence |
|---|---|
| **Triggering, entirely** | Never tested. `bdd-gherkin-style` and `bdd-behave-allure` both claim a repo with a `features/` directory. Collision is plausible and unmeasured. |
| **Any tier but Opus** | Every run here is Opus both arms. The cpp harness found capability lift concentrated in the weak tier; if that holds, Haiku numbers would be larger and differently shaped. |
| **n = 1 per cell** | No repeats. Nothing here has a confidence interval. |
| **Rework** | See 2.10. The dimension the story authors care about most is the one with no assertion. |
| **The post-measurement additions** | The user-story rule and `references/user-story-examples.md` were added *after* these runs. The skill grew ~2k chars and has not been re-measured; dilution is a real possibility given the cpp harness finding that redundant content weakens what works. |
| **Whether examples came from the same domain as the skill's own** | Deliberately avoided (word filter, port redirection, GeoIP excluded from story 4), but only for that one story. |

---

## 7. Conclusion

**The fixture decides what you measure.** The same skill, the same model and the
same grader produced "no measurable lift" on artefact-based fixtures and a
consistent +4 on adjacent-substrate stories. The first conclusion was published
in this project's docs before the second fixture style existed, which is the
third time an instrument here produced a confident, wrong number.

**The skill answers stories, not plain text.** Given a document that already
contains the mechanism, a strong model strips it unaided — the plain-text
fixtures carried Kafka topics, table names, GPIO lines and cron jobs, and the gap
was ~1. Given a story, the model must produce the scenario itself, and in a
domain with a mechanism lying next to it, that is what it produces. Both
conditions have to hold: **story-shaped input AND an adjacent substrate.** Either
one alone is worth about a point.

**The skill transfers a house preference, and preference is what survives.** Its
one universal result is phrasing reuse — 6 of 6 baselines failed it, 0 of 6
with-skill runs did — and phrasing reuse is a convention, not knowledge. It has
no bearing on whether the model *understands* Gherkin. It determines whether a
suite reads as one document and whether five step definitions exist where two
would do. Nobody restates that rule in every prompt; the skill is what makes it
always present.

**Its capability contribution is bounded by the domain.** Where an implementation
vocabulary sits next to the domain, it keeps the substrate out of the file —
worth 4 points on a 12-point scale, three times out of three. Where the domain
already speaks in outcomes, it is worth about 1.

**It costs coverage.** In 3 of 5 stories the with-skill file was more readable and
specified less, dropping a case the unaided file kept. That is a real trade and
the skill does not warn about it. The most useful single change available would
be a rule to check what a collapse removed — which is a rule this measurement
produced and that has not been written, let alone tested.

Two corollaries, both earned here:

1. **Choose the fixture that matches the input you actually get.** A backlog
   produces stories, not leaked feature files. Measuring on the artefact you wish
   you had gives a number about a task nobody performs.
2. **A trap that the story's own wording can satisfy is not a trap.** Both of the
   traps written for this benchmark failed this way, and both times the failure
   was invisible until the two arms scored identically on the assertion that was
   supposed to separate them.

### Source documents

- `bdd-gherkin-style-story-benchmark.md` — the five stories, per-assertion detail
- `bdd-skills-findings.md` — the earlier artefact-based evals and their lessons
- `cpp-skills-harness-report.md` §10 — the isolation defect, found here, applied there
- `AGENTS.md` §4 — the corrected baseline recipe
- Raw runs: `bdd-gherkin-style-workspace/{bench,swnh,story}-2026-09-28/`
