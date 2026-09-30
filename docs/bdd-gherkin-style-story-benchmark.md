# bdd-gherkin-style — user-story benchmark (2026-09-28)

Measures the skill on the input it actually meets in a backlog: a bare user
story, no implementation to translate. Opus both arms, baselines run headless and
isolated (`claude -p --disable-slash-commands` from a neutral cwd — see
`AGENTS.md` step 4). Prompts verbatim, including the stories' original grammar.

## Why this run exists

The earlier run (`swnh-2026-09-28`) handed the model a mechanism-heavy note and
asked it to clean it. Result: 30/32 baseline vs 31/32 with skill — effectively no
lift, which was recorded as "say what, not how is common knowledge".

**That conclusion was an artefact of the fixture design.** Cleaning a leaked file
is *translation*: the mechanism is visible and deleting it is easy, and Opus does
it unaided. A bare story forces the model to *generate* the detail, and unaided it
falls into the nearest implementation vocabulary. Generated leakage is the hard
case, and it is the one that occurs in real work.

## Results

| story | domain | substrate | baseline | with skill | gap |
|---|---|---|:---:|:---:|:---:|
| 1 | firewall rule for external RDP | adjacent | 7/12 | 11/12 | **+4** |
| 2 | device audio config, transport named | adjacent | 7/12 | 11/12 | **+4** |
| 4 | SSL inspection exclusion | adjacent | 8/12 | **12/12** | **+4** |
| 3 | ETL baseline vs new run by percentile | distant | 11/12 | 12/12 | +1 |
| 3b | server response monitoring, fixed + dynamic metrics | distant | 10/12 | 11/12 | +1 |

Three adjacent-substrate stories: +4, +4, +4. Two distant: +1, +1. Clean
separation at n=5. Story 4 produced the only 12/12 in the project.

Story 3 was rewritten by its author after its first version proved too loosely
specified to be usable evidence; 3b is the rewrite. Both are shown because they
land in the same band, which is itself the finding.

All four: Opus both arms, baselines headless and isolated, prompts verbatim
including the authors' original grammar and, for 3b, both language versions the
author wrote.

## Story 1 — "As a network admin, I want to connect in my appliance by rdp, For it I need a rule enabling this external access"

The baseline's central failure, and the one this whole benchmark was built to
catch:

```gherkin
Scenario: Create a firewall rule enabling external RDP access
  When I create a firewall rule with the following attributes:
    | attribute   | value         |
    | name        | Allow-RDP-WAN |
    | direction   | inbound       |
    | source      | any           |
    | ...         | ...           |
  And I apply the firewall configuration
  Then the rule "Allow-RDP-WAN" appears in the firewall rule list as enabled
```

**The asserted outcome is that a form was filled in.** It passes while nobody can
connect. The data table is the rule-creation screen transcribed into the spec.

Also: two assertions on the firewall log; scenario names describing the action
("Create a firewall rule…") instead of the result; two renderings of "refused";
and one scenario with `When/Then/When/Then`, which is two scenarios fused.

With skill, every outcome is the access itself:

```gherkin
Scenario Outline: The source address allowed by the rule decides who may connect
  Given a rule that allows remote desktop access to the appliance from <allowed_source>
  When a network admin opens a remote desktop session to the appliance from <admin_source>
  Then access to the appliance is <result>
```

One cost on the skill side: it dropped port 3389 and said "remote desktop" /
"secure shell". 3389 is a public protocol fact, the same category as the HTTP 410
the skill's own exception section says to keep. The exception did not fire. For a
spec a network admin reviews, the port is arguably the reviewable detail.

## Story 2 — "As a user, I want to know if my device is correctly configurated, so that I can ensure only one audio source was seted"

Context given to both arms: *configuration of OS properties via REST API and
websocket*.

**That context line acted as an invitation.** The baseline treated the transport
as the subject and produced an API test suite:

```gherkin
When I send a GET request to "/api/v1/os/properties/audio"
Then the response status code is 200
And the response field "activeSource" is "hdmi"
```

plus websocket topics, message type names and 5-second timeouts. The story says
*I want to know if my device is correctly configured*. The user wants to learn
something; the baseline answers with HTTP calls.

With skill:

```gherkin
When a user asks the device for its audio configuration
Then the device reports the built-in microphone as the active audio source
And the device reports no other active audio source
```

`And the device reports no other active audio source` is reused character for
character across every scenario and Outline expansion — the one-source invariant
gets one step definition instead of five.

Costs on the skill side, both real:
- **It dropped the zero-active-sources case.** The baseline covered it. A device
  with no audio source is as misconfigured as one with two.
- It collapsed the websocket into "a user is watching the device configuration
  for changes". Defensible, but it no longer distinguishes push from polling —
  and if the websocket exists precisely because polling was not good enough, that
  distinction is the behaviour.

## Story 3 — the narrow case, and what it exposed about the eval

Baseline 11/12. The only failure was asserting screen layout
(`the comparison table shows … the absolute delta`, `the trend chart plots …`).

**The designed trap did not discriminate.** The story never defines what makes a
response "bad", so inventing a threshold silently was supposed to fail. Both arms
stated one concretely as data — baseline 20%, with skill 10%. Both passed.

**The deeper problem, raised by the story's author:** a high score here does not
mean the file is right. The baseline invented a 20% threshold, a trend chart, a
"low confidence" rule and a no-baseline case. All plausible, none confirmed. The
file *looks* correct and still costs a round trip to correct. Ambiguous input
produces confident output, and these assertions measure form, not rework.

**Eval defect, recorded not patched:** there is no assertion for "how much of
this must the author send back". A file that invents five unconfirmed rules and a
file that flags them as open questions score identically. The earlier
contaminated run showed the same thing from the other side — the baseline there
marked five gaps `# OPEN QUESTION` and got no credit for it.

Story 3 is being rewritten by its author. This section stands as the record of
why its result is not usable evidence either way.

## Story 3b — the rewrite, and a limit that is not the skill's to fix

> Dado um sistema de monitoramento de resposta de servidores. Eu como usuário,
> quero comparar a resposta baseline e a resposta atual de cada tipo de servidor
> automaticamente, com métricas fixadas como media, moda, mediana, e métricas
> dinâmicas (que posso mudar) como percentil.

Baseline 10/12, with skill 11/12.

### Both arms failed the mode trap

The story asks for the **mode** of response times. The mode of millisecond-precision
latencies is degenerate: with real data almost every value is unique, so the
metric is either undefined or an artefact of tie-breaking. A usable spec has to
bin the values, use a discrete quantity, or say the problem out loud.

Neither arm did. With skill: `| mode | 100 | 140 | worse than baseline |`.
Baseline, worse: it curated the samples so a mode would exist
(`100, 110, 110, 120, 130, 400` -> mode 110) and asserted on it, hiding the
problem behind example data that will never resemble production.

**This is a category limit, not a skill defect.** `bdd-gherkin-style` governs how
behaviour is worded. Whether a requested metric is meaningful for the data it
will run on is a statistics question, and no amount of style guidance reaches it.
Both arms specified an unworkable metric obediently. Covering this would be a
different skill — reviewing a requested metric against the nature of the data —
not another rule in this one.

### The with-skill arm lost coverage, for the third story running

Baseline ~130 lines and 17 scenarios; with skill ~60 lines and 6. Cleaner, and
missing:

- **"Fixed metrics cannot be removed from the report."** The entire point of the
  story's fixed-versus-dynamic split is that the first group is not the user's to
  change. The with-skill file shows the three always appear but never that an
  attempt to remove one is refused: it specifies the consequence, not the rule.
- Percentile range validation (0, -10, 101 rejected).
- An aggregate verdict across metrics, with ties reported as inconclusive.
- A changed dynamic metric applying only to the next comparison, not rewriting a
  report already produced.

Three for three: story 1 asserted `73 hours -> returned to sender` against a
once-daily batch job, story 2 dropped the zero-active-sources case, story 3b
drops the immutability of the fixed metrics. **Collapsing and abstracting is not
free**, and the skill says nothing about checking what the collapse removed.

### Vocabulary, fifth consecutive baseline failure

`the report ... contains the metric` beside `the report ... shows the metric with
baseline ... and current`; verdicts asserted once per line and once as a table.
Across every story and both fixture styles, contaminated runs and clean ones, no
with-skill run has failed the one-phrasing rule and no baseline has passed it.

## Story 4 — SSL inspection exclusion, the maximum-proximity case

> As a security admin, I want internet banking sites to skip SSL inspection,
> so that the bank app does not break for users

Written specifically to test the substrate-proximity claim: what the admin asks
for *is* a configuration object, so the pull toward asserting configuration
should be at its strongest. Domain chosen outside the skill's own examples (word
filter, port redirection, GeoIP) so the test measures transfer, not memory.

Baseline 8/12, with skill 12/12 — the only perfect score in the project.

### The prediction was right on the aggregate and wrong on the mechanism

The predicted failure was `Then the exception appears in the bypass list`.
**It did not happen.** The baseline's scenario for adding an uncategorised bank
is structured correctly: the list operation sits in the `When`, and the `Then`
asserts behaviour — `new sessions to "newbank.example.com" are tunneled without
decryption`. H1 passed in both arms.

What the adjacent substrate produced instead was leakage in the supporting steps:

- `the URL category "Internet Banking" is on the SSL inspection bypass list`
- `And the policy is applied` — the apply-script anti-pattern nearly verbatim
- `the firewall log records the server name "ib.examplebank.com" from the TLS SNI`
- three assertions on log contents

**The claim narrows accordingly: an adjacent substrate reliably produces leakage;
whether it also captures the outcome varies.** Story 1 produced both failures,
story 4 only the first.

### The baseline was strong where it mattered most

Its certificate-pinning scenario is the best of any baseline in the project —
`the pinned certificate check passes`, `the app completes login without a TLS
error`. That is the actual reason the app breaks, stated as the app's behaviour.

It also handled the security tradeoff **better than the with-skill arm**: a whole
scenario on what visibility survives a bypass, ending `And no payload content is
recorded for the session`.

### My trap failed to discriminate, for the second time and the same way

H7 asked the file to acknowledge that bypassed traffic is no longer examined. The
with-skill file says `the traffic is passed through without inspection` three
times, which satisfies the wording, and never explores the consequence — while
the baseline specifies exactly what is lost.

This is the second assertion in this benchmark (after the threshold trap in story
3) written loosely enough that restating the feature satisfies it. **The pattern
in the failure is mine, not the model's: if a story's own wording can satisfy an
assertion, the assertion tests nothing.** A working version of H7 would have to
require a consequence the story does not state — for example, that some scenario
show a threat reaching a user through an excluded site.

## The finding: substrate proximity

The gap does not track how well the story is written, how long it is, or how
ambiguous it is. It tracks **how close an implementation vocabulary sits to the
domain the story describes.**

| story | nearest substrate | how far away | gap |
|---|---|---|:---:|
| 1 firewall rule | rule lists, apply actions, log lines | adjacent — the domain *is* configuration | +4 |
| 2 device config | endpoints, payload fields, websocket topics | adjacent, and named in the context line | +4 |
| 4 SSL exclusion | bypass lists, policy apply, SNI, session logs | adjacent — maximum by design | +4 |
| 3 ETL comparison | none obvious; verdicts and numbers all the way down | distant | +1 |
| 3b response monitoring | none obvious; metrics and verdicts | distant | +1 |

### How to classify a story

The question is not "is this domain technical?" — every domain here is technical.
It is: **when the user names the thing they want, are they naming an object the
system stores and displays?**

- *"a rule enabling this access"* — a rule is a row in a list. Adjacent.
- *"only one audio source was set"* — a setting is a field with a value. Adjacent.
- *"banking sites skip SSL inspection"* — an exclusion is an entry. Adjacent.
- *"compare current against baseline by percentile"* — a comparison is not stored
  anywhere; it is a judgement produced from data. Distant.
- *"see if the newer run is worse"* — a verdict is not an object. Distant.

The practical tell: **can you satisfy the story by changing a configuration and
showing the configuration changed?** If yes, the substrate is adjacent and an
unaided model will drift into it. If the story can only be satisfied by producing
a judgement or an effect, there is no object to drift toward.

### Two conditions, not one

Substrate proximity is half of it. The other half is **who produces the
mechanism**, and it is the axis the earlier artefact-based run sat on:

|  | **substrate adjacent** | **substrate distant** |
|---|---|---|
| **user story** (model generates the scenario) | **+4** — stories 1, 2, 4 | +1 — stories 3, 3b |
| **plain text / existing artefact** (model transforms it) | +1 — h1, h2, h3 | not tested |

The plain-text fixtures were not substrate-free — they were substrate-saturated.
The handover note in h1 names Kafka topics, `locker_slots`, a GPIO line and a
cron job; the leaked file in h2 names tables, columns and a retry worker. All of
it handed over, and the gap was still ~1, because **deleting visible plumbing is
a transformation a strong model performs unaided.**

The skill answers **stories, not plain text**, and among stories only where a
mechanism lies next to the domain. Both conditions must hold.

Call it **substrate proximity** for the column, and generate-versus-transform for
the row.

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

When the substrate sits right next to the domain, an unaided model falls into
that vocabulary and starts asserting the mechanism: the rule appears in the rule list, the response field
equals "hdmi", the trend chart plots p95. When the domain already speaks in
outcomes — a metric is better or worse than its baseline — there is nothing
nearby to fall into, and a strong model stays where it started.

Three consequences worth keeping:

1. **A user story does not automatically expose the skill.** Stories 3 and 3b
   were bare stories too, and both baselines scored 10-11/12. The earlier
   conclusion in this document — that story-shaped input is what reveals the
   skill — was half right. What reveals it is story-shaped input *in a domain
   with a nearby substrate*.

2. **Better specification raises the baseline; it does not create a leak.**
   Story 3b was written to be much tighter than story 3: fixed metrics named,
   dynamic metrics described as configurable, grouping and verdict direction all
   stated. The baseline used that material well and scored 10/12. The gap stayed
   at +1. More detail gives the unaided model more to work from; it does not put
   a substrate where there wasn't one.

3. **The context line is part of the story.** Story 2's "configuration of OS
   properties via REST API and websocket" was intended as background. The
   baseline treated it as the subject and produced an API test suite. Naming a
   transport in the context is enough to pull the whole file toward it, which
   makes it the cheapest known way to reproduce this failure.

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

### What proximity actually causes — narrowed after story 4

Story 4 was written to maximise proximity and predicted to produce the
mechanism-as-outcome failure. It did not. The baseline kept its outcomes
behavioural and leaked in the surrounding steps instead: bypass lists, a policy
apply, SNI, session logs.

So the effect splits in two, and only the first half is reliable:

| effect | across 3 adjacent stories |
|---|---|
| **Leakage** — substrate vocabulary in `Given`/`And` steps, log and config assertions | 3 of 3 |
| **Outcome capture** — the mechanism becomes the asserted `Then` | 1 of 3 (story 1 only) |

The practical reading: proximity makes a model *speak* the substrate reliably,
and makes it *assert* the substrate sometimes. The first costs readability and
couples the spec to the configuration schema. The second is the one that produces
a spec which passes while the feature is broken, and it is rarer than expected.

**Still not tested as a cause.** Five stories that differ in many ways at once
give a clean correlation, not a mechanism. The single-variable test — one story,
run with and without a context line naming a transport — has not been run. Story
2 is the strongest hint that the context line alone is enough, since its
transport was named in the context and nowhere in the story itself.

## Method notes

- Both arms got byte-identical prompts, the stories' original grammar included
  ("configurated", "seted", "newers", "percentil"). Correcting them would have
  been the grader doing part of the work under test, and imperfect input is the
  real input.
- 12 assertions per story, pre-registered in `ASSERTIONS.md` before any run.
- Artefacts: `bdd-gherkin-style-workspace/story-2026-09-28/`.

## Consequence for the skill

Both losing stories failed on the same move: **the mechanism became the asserted
outcome.** That rule is short and applies to every story, so by the finding in
`bdd-skills-findings.md` (short rules that apply everywhere are cheap inline and
expensive behind a conditional pointer) it belongs in the body. The two worked
pairs are long, so they belong in a reference. Both were added after this
measurement and are therefore **unverified** — a re-run of stories 1 and 2 would
show whether the addition helps, does nothing, or dilutes.

Technical report covering this benchmark, the earlier artefact-based one, and
the instrument defects found in both: `docs/bdd-skills-harness-report.md`.

## Cross-tier check (2026-09-30): Haiku and Sonnet

**Caveat first.** The original workspace (`bdd-gherkin-style-workspace/`) and
`ASSERTIONS.md` from the 2026-09-28 run no longer exist on disk — only this
document survived. This re-run reconstructs the four usable stories (1, 2, 3b,
4 — story 3 was already discredited above) verbatim from the doc text, but the
12-point pre-registered rubric could not be recovered. Grading below is
qualitative, against the same failure mode the Opus run centered on:
**mechanism-as-asserted-outcome and substrate vocabulary leaking into
`Given`/`Then`.** It is a different, looser instrument than the Opus scores
above — treat the two as not directly comparable in magnitude, only in
direction.

Same harness shape as the Opus run: baseline headless and isolated
(`claude -p --model <tier> --disable-slash-commands`, neutral cwd), skill arm
via a fresh subagent following `bdd-gherkin-style/SKILL.md`. Both tiers ran the
same 4 prompts as Opus.

| story | substrate | haiku baseline leaks | haiku+skill leaks | sonnet baseline leaks | sonnet+skill leaks | opus gap (prior) |
|---|---|:---:|:---:|:---:|:---:|:---:|
| 1 firewall RDP rule | adjacent | 2 (port asserted, "rule prevents") | 0 | 2 (config data table, rule-in-list) | 0 | +4 |
| 2 device audio config | adjacent, transport named | 3 (REST/websocket as subject) | 0 | 4 (REST/websocket as subject, worse) | 0 | +4 |
| 3b response comparison | distant | 0 | 0 | 0 | 0 | +1 |
| 4 SSL exclusion | adjacent | 2 (exclusion list, cert-error wording) | 0 | 3 (bypass, "without decryption") | 0 | +4 |

Leak count is mine, read from the files, not a rubric score — see caveat.

### Rubric used for this section

The original `ASSERTIONS.md` (12 pre-registered assertions per story) did not
survive — see caveat above. This section's numbers come from a **new,
self-defined rubric**, not a reproduction of the old one. It is 12
yes/no checks, applied identically to all 16 files (both tiers, both arms),
read directly off the generated `.feature` text:

1. Outcome asserts real behavior, not config/mechanism state
2. No "entity appears in list/config" pattern in `Then`
3. Negative or boundary case present
4. Actor(s) named explicitly in steps
5. No transport/implementation vocabulary in `Given`/`When` (API, websocket, DB, log)
6. No log/audit assertions
7. One consistent phrasing per concept, not two renderings of the same idea
8. Scenario/Outline name states an outcome, not an action
9. No raw config-screen data table transcribed into the spec
10. Retains a concrete domain fact a reviewer needs (a protocol, a metric, a category) rather than over-abstracting
11. Self-contained — readable without implementation knowledge
12. Appropriately compact — Outline where data varies, no near-duplicate scenarios

This rubric does **not** score whether the file found the story's real
underlying mechanism (see "insight" flag below) — that is a separate,
non-numeric judgment call, deliberately kept out of the /12 so a vague-but-safe
file and an insightful file aren't conflated.

| story | substrate | Haiku baseline | Haiku+skill | Sonnet baseline | Sonnet+skill | Opus baseline† | Opus+skill† |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|
| 1 RDP rule | adjacent | 10/12 | 11/12 | 5/12 | 11/12 | 7/12 | 11/12 |
| 2 audio config | adjacent, transport named | 5/12 | 11/12 | 6/12 | **12/12** | 7/12 | 11/12 |
| 3b response comparison | distant | 7/12 | 10/12 | 9/12 | 11/12 | 10/12 | 11/12 |
| 4 SSL exclusion | adjacent | 7/12 | 10/12 | 7/12 | **12/12** | 8/12 | 12/12 |

### Rubric audit: is Haiku+skill's 10/12 on story 4 legitimate?

No individual criterion was mis-scored, but the score overstates quality.
Haiku+skill's story-4 file is one scenario, four lines:

```gherkin
Scenario: Banking website connections work when exempt from SSL inspection
  Given internet banking sites are exempt from SSL inspection
  When a user connects to a banking website
  Then the banking website responds successfully
```

Breaking its 10 points down by whether the point was **earned** or **free
because there was too little content to fail the check**:

**Earned (5 pts)** — real properties of the file: outcome-level `Then` (#1),
no list-as-outcome (#2), no transport vocabulary (#5), no log assertion (#6),
outcome-shaped scenario name (#8).

**Free (4 pts)** — criteria that are vacuously satisfied by thinness, not
quality: #7 ("one consistent phrasing") is unfalsifiable with a single
`Then`; #9 ("no raw config table") passes because there is no table of any
kind; #11 ("self-contained/readable") is trivial at four lines; #12
("appropriately compact, no near-duplicates") can't be violated by a file
this short — the word "appropriately" is doing no work, since nothing checks
whether it is compact because the coverage is complete or compact because
coverage is missing.

**Correctly penalized (0 pts, stayed 0):** #3 (no negative/comparison case)
and #10 (no concrete domain fact — no site, no certificate, nothing) both
still fired, so the rubric did catch that the file is thin. It just wasn't
enough to bring the score down to where "thin" should land it.

This is the same instrument defect the original Opus run already named for
story 3 (a file that invents five rules and a file that flags five open
questions score identically — no assertion for completeness) in inverse form:
**a file too sparse to commit an error also can't fail a same-idea-rendered-
twice or a coverage-shape check, so it profits from having done less.** Four
of twelve checks (#7, #9, #11, #12) only fire against files that are trying
to do more than one thing. The "insight" flag above already surfaces the real
gap in this specific case (Haiku: not captured), but that flag exists
precisely because the /12 alone does not — it is a patch, not a fix to the
rubric itself.

**Consequence: this rubric needs a redesign, not just a patch, before another
tier or story is scored against it.** At minimum: a coverage/completeness
axis (does the file address every clause of the story — fixed vs dynamic
metrics, exempt vs not-exempt, granted vs denied — not just the clauses it
chose to write about) so a file can't earn credit for silence, and a way to
make #7/#9/#11/#12 scale with how much the file is attempting rather than
pass-by-default at low word count. The 16 files already generated remain
usable evidence once a fixed rubric exists; what needs re-running is the
**scoring**, and possibly the **skill-arm generation** if the redesigned
rubric surfaces coverage gaps this one couldn't see (story 4's missing
exempt-vs-not comparison in the Haiku file is the leading candidate — a
tighter rubric might have been what should have driven the prompt/skill
toward producing it in the first place, which a rescoring of the existing
file can't test for).

† Opus numbers are the **original** scores from the "## Results" table above,
scored against the lost `ASSERTIONS.md`, not this new rubric. They are shown
for direction, not for exact numeric comparison — different instrument.

**Insight flag** (not part of the /12 — whether the file located the story's
actual underlying mechanism, e.g. story 4's certificate-pinning reason the
bank app breaks):

| story | Haiku+skill | Sonnet+skill | Opus+skill (original) |
|---|:---:|:---:|:---:|
| 4 SSL exclusion | not captured | captured | captured |

Haiku's skill-arm file for story 4 scored 10/12 — it violates none of the 12
checks — and still produced one generic scenario
(`the banking website responds successfully`) with no certificate reasoning at
all. That is the rubric's blind spot: avoiding every listed trap is not the
same as finding the thing the story is actually about.

### What "substrate" means, in full

This is the term the original 2026-09-28 benchmark doc coined for itself (see
"## The finding: substrate proximity" further up this file) — it is not
standard BDD terminology and was not sourced from outside this project. It is
reused here only to stay consistent with that earlier analysis.

- **Domain** — the words the user's story actually uses: what happens *to
  someone* ("I want to connect by RDP", "I want to know if my device is
  configured right").
- **Substrate** — the layer of vocabulary underneath the domain: the words for
  how the system *stores, transports, or manipulates* the thing. A firewall
  rule's substrate is the rule list, the chain, the apply action, the log
  line. A device setting's substrate is the API endpoint, the JSON field, the
  websocket topic.
- **Proximity** — the distance between what the user asked for and that
  substrate. Not the same as "is this domain technical" (every domain
  measured here is technical). It is whether the substrate is reachable from
  the words the user used.
  - **Adjacent**: what the user names *is* a stored object — a rule, a
    setting, an exclusion entry. The practical test: *can the story be
    satisfied by changing a configuration and showing the configuration
    changed?* If yes, adjacent. Stories 1 (RDP rule), 2 (audio config,
    transport even named in the context line), 4 (SSL exclusion) are all
    adjacent — an unaided model falls into that vocabulary and starts
    asserting the mechanism instead of the outcome.
  - **Distant**: what the user asked for is a judgment or an effect, not a
    stored object — "is this run slower than the baseline?" has no table
    called "verdict" to point at. Story 3b (response-time comparison) is
    distant.
- Why it matters here: the skill's gap between baseline and with-skill is
  large exactly where the substrate is adjacent (+4-class gaps, all three
  tiers) and small where it's distant (+1-class gap, all three tiers). The
  skill's job — stop asserting the mechanism, stay at the outcome — only has
  something to correct when a nearby mechanism exists to fall into.

### What held from the Opus finding

- **Substrate proximity predicted the gap at both tiers too.** Stories 1, 2, 4
  (adjacent) leaked in every baseline; story 3b (distant) leaked in neither
  baseline. Same split as Opus.
- **The skill arm hit zero leaks in all 8 runs**, both tiers. Neither Haiku nor
  Sonnet needed Opus-level capability to *stop* leaking once the skill file was
  in context — this looks like a following-instructions effect, not a
  reasoning-tier effect.
- Story 2 reproduced the transport-as-subject failure at both tiers, matching
  the "context line is an invitation" finding: Haiku baseline produced REST/
  websocket assertions unprompted by anything except the context line, same as
  Opus's baseline did.

### Where tiers diverged — quality of the skill arm, not leak count

Leak count alone hides a real gap between Haiku and Sonnet on the *positive*
content of the skill arm:

- **Story 4 (SSL exclusion).** Sonnet+skill independently derived the same
  insight Opus's did — the app breaks because of certificate pinning, not
  because inspection ran — and asserted `the client receives <certificate>`
  per exemption state. Haiku+skill produced one thin scenario
  (`the banking website responds successfully`) with no certificate reasoning
  at all: zero leaks, but it never located the actual mechanism the story is
  about, so the file is safe rather than useful.
- **Story 2 (audio config).** Both tiers' skill arms covered the zero-active-
  sources case that Opus's own with-skill run *dropped* — an improvement on
  the prior Opus result, at both lower tiers.
- **Story 3b (distant substrate).** Sonnet+skill covered metric removal and
  cross-server-type coverage that Opus's with-skill run was criticized for
  missing (the "fixed metrics can't be removed" gap). Haiku+skill covered less
  — concrete example values but no removal/range-validation cases — closer to
  what Opus's own run produced.

### Reading

The skill's core mechanism-suppression effect (stop asserting the mechanism,
stop leaking substrate vocabulary) held at Haiku and Sonnet, not just Opus —
consistent with the earlier `swnh-2026-09-28` framing that idiom/vocabulary
rules survive at weak tiers while advisory judgment calls don't. What varies by
tier is whether the *resulting* scenario captures the story's real insight
(story 4's cert-pinning) or just avoids the trap safely (Haiku's thin version).
Sonnet tracked Opus's quality closely; Haiku matched Opus on leak-avoidance but
fell short on insight capture in exactly the story built to be hardest
(maximum substrate proximity).

**Not re-verified:** the 12-point rubric, the negative-case and
range-validation checks it scored, and whether these differences would survive
a formal reproduction with `ASSERTIONS.md` pre-registered. This section is a
directional check, not a replacement for the Opus numbers above.

Artefacts: `/tmp/claude-1000/-home-pandrade--claude-skills/de6bd613-e80d-40a8-aec6-55aa3d7ed3ef/scratchpad/gherkin-hs-bench/{baseline,skill}/`
(scratchpad — not preserved across sessions; copy out if needed).
