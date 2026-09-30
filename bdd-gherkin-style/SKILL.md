---
name: bdd-gherkin-style
description: House style for writing Gherkin — feature files, scenarios, Scenario Outlines, Background and Examples tables — in business language that hides implementation detail. Use this skill whenever you are about to write, review, refactor or extend a .feature file, add scenarios to an existing suite, translate a bug report or ticket into a BDD scenario, or design step wording for Behave/Cucumber/SpecFlow/pytest-bdd. Trigger it even when the user only says "add a test for X" in a repo that has a features/ directory, or asks to "describe this behaviour as a scenario" — getting the wording right the first time is much cheaper than rewriting a suite later.
---

# Gherkin Style

## Why this style exists

A `.feature` file has two audiences at once: a person deciding whether the
behaviour described is the behaviour they want, and a step definition layer that
makes it executable. The style below optimises for the first audience, because
the second one is infinitely flexible — any sentence can be backed by a step
definition, but only a sentence written in the language of the problem can be
read by someone who does not know the code.

The practical consequence: **a scenario should still be true after a rewrite of
the implementation.** If you change the database, the CLI flags, or the
generated config and the scenario has to change, the scenario was describing the
implementation, not the behaviour.

## The shape of a scenario

```gherkin
  Scenario: Traffic to an unmapped port is denied
    Given a redirection rule for TCP port 80 to 192.168.100.10:8080
    When a connection attempt arrives on the firewall WAN port 9000
    Then access to the protected host is denied
```

Three moves, in order, with one job each:

- **Given** — the context before the action. Rules, configuration,
  existing data, active services. Multiple `Given`/`And` lines are fine; each
  should set up one independent fact.
- **When** — the condition for the trigger, performed by an actor from outside
  the system. One `When` per scenario is the default. A second `When`/`And` is
  justified only when the action genuinely has ordered parts (apply rules, then
  move the clock, then connect) — and even then, keep it to the steps that
  change the world, not the ones that observe it.
- **Then** — the results expected. Start with the primary
  outcome; use `And` for secondary observations that would be a separate
  assertion.

If you find yourself wanting a `When` after a `Then`, you have two scenarios.
Split them.

## Vocabulary discipline

This is the part that pays off most over a suite's lifetime.

**One phrasing per concept, forever.** Once `access to the protected host is
granted` exists, never introduce `the connection succeeds` or `the client can
reach the backend` for the same idea. Before writing a new step line, grep the
existing feature files for the concept and reuse the exact wording. Divergent
phrasings for one concept fragment the step layer and make the suite unreadable
as a whole.

This applies to what a step _renders to_, not just to what you typed. A
`Then the request is <result>` whose rows expand to `accepted` has to match the
standalone `Then the request is accepted` elsewhere in the file, character for
character — if one says `the final request is accepted` and another says `every
request is accepted`, that is three step definitions for one concept even though
each line looked reasonable while you wrote it. Expand your Outlines mentally
against their rows and compare the results against the plain scenarios.

**Name domain objects, not mechanisms.** `a redirection rule that allows only
United States IP addresses` — not `a row in firewall_redirects with geo_active
true`. The reader should not need to know the schema, the config format, or the
binary being invoked.

**Concrete values belong in the scenario; derivation belongs in the steps.**
Writing `172.16.50.1` or `192.168.100.10:8080` inline is good — it makes the
scenario self-contained and reviewable. Writing `the ipset bb_nat_redir_1_src`
is not — that name is an implementation artefact.

**Actors are named.** "a client", "a network admin", "a connection attempt".
Avoid passive constructions that hide who acted ("the request is made").

## Say what, not how

Every smell in this style reduces to one mistake: writing the mechanism instead
of the behaviour. It is an easy mistake because the person writing the scenario
usually just finished building the thing, and the mechanism is what is fresh in
their head.

It helps to be precise about what the two words mean here, because "be less
technical" is vague advice that leads to prose that is merely vaguer:

**The _how_ is the view inside of the system.** Concretely:
names of tables, columns, chains, ipsets, queues, counters, log prefixes, marks,
files, flags, commands, scripts, functions, classes, config keys, environment
variables — anything a reader would have to open the codebase to recognise. Also
the tools used to observe a result (a counter, a table, a log file) and the
mechanics of driving it (a click, a command, an HTTP verb on an internal route).

**The _what_ is the view outside of the system.** One level of abstraction up: the actors, the
domain objects, the rules and the observable effects, stated so they hold for any
implementation that satisfies them.

The division of labour follows from this. The scenario owns the _what_; the step
definition owns the _how_ and is the right place for all of it. Every technical
detail you take out of a step's text does not disappear — it moves one layer down
to where it can change freely.

Two questions catch a violation:

1. **Would this line still be true after a reimplementation?** If the team
   swapped the logging sink, the kernel module, the storage engine — does the
   sentence survive? If not, it describes the how.
2. **Would someone who has never seen the code know whether this is the
   behaviour they wanted?** If they'd have to ask what a counter or a table is,
   the sentence is not a specification.

The pairs below are real: the left column is what an engineer wrote from a
handover note about the implementation, the right is the same behaviour stated
as behaviour. Read them as a family, not as individual rules — the same move is
being made every time.

Example 1 — a literal word filter.

Instead of:

```gherkin
  Given a word object "1" with patterns "facebook"
  And profile "1" blocks word object "1"
  When the firewall receives a webfilter request for URI "http://www.facebook.com/home"
  Then the action is "block"
```

Write:

```gherkin
  Given a rule that blocks any URL containing "facebook"
  When the firewall receives a request for "http://www.facebook.com/home"
  Then the action is "block"
```

The object id, the profile and the binding between them are all schema. The
behaviour is one rule blocking one pattern.

Example 2 — the same filter expressed as a regex.

Instead of:

```gherkin
  Scenario: regex match blocks the request
    Given a word object "1" with regex patterns ".*[fF]acebook.*"
    And profile "1" blocks word object "1"
    When the firewall receives a webfilter request for URI "http://www.facebook.com/home"
    Then the action is "block"
```

Write:

```gherkin
  Scenario: regex match blocks the request
    Given a rule that blocks any URL containing "facebook", ignoring capitalisation
    When the firewall receives a request for "http://www.facebook.com/home"
    Then the action is "block"
```

Note what the rewrite has to get right: `.*[fF]acebook.*` matches the word
anywhere in the URL and accepts either case of the first letter. State that, not
"a regex" and not a narrower rule like "ends with" — a wrong paraphrase of the
mechanism is worse than the mechanism, because it reads as a specification and
promises the wrong thing.

### Writing from a user story, not from existing code

A story is a harder input than a leaked feature file. With a leaked file the
mechanism is visible and you delete it. With a story you must *invent* the
detail, and the nearest vocabulary is usually the implementation's. Two failures
show up almost every time, both measured:

**The mechanism becomes the asserted outcome.** A story that says *"I need a rule
enabling this access"* invites a scenario ending in `Then the rule appears in the
rule list as enabled`. That passes while nobody can connect. The rule belongs in
the `Given`; the outcome is the access. Test: **if the scenario would still pass
after the feature stopped working for its user, it asserts the mechanism.**

**A context line naming a transport is describing the _how_.** "configured via
REST API and websocket", "events arrive on a queue", "stored in Postgres" tell
you how the behaviour is carried, not what it is. Mentioning a transport is not
permission to make it the subject. Apply the ownership question below: if the
reader is an integrator building against a published API, the routes and status
codes are the contract and belong; if they are an operator who wants a correct
device, they are plumbing. The story usually does not say — so state which
reading you took instead of defaulting to the transport because it was named.

Worked before/after pairs for both, from real runs:
`references/user-story-examples.md`.

### The one case where the mechanism is the behaviour

If the thing under test _is_ the interface — an HTTP status code in an API
contract, a file format, a wire protocol — then naming it is naming the
behaviour, and hiding it behind prose makes the spec vaguer, not cleaner. A
scenario for a public API may legitimately say `Then the response status is 429`
when 429 is what the contract promises callers.

The test is ownership: is this detail something the consumer depends on, or
something the team chose and could change on a Tuesday? Status codes in a
published API are the former. Chain names, table columns, counters and log
prefixes are the latter.

## Background

Use `Background` only for setup shared by _every_ scenario in the file, and only
when it is genuinely context rather than action — a `Background` full of `When`
steps is a smell.

```gherkin
  Background:
    Given a firewall with WAN and LAN networks
    And the firewall drops all traffic by default
```

Keep it to two or three lines. A long `Background` means the reader has to hold
too much in their head before reaching the scenario, and it silently couples all
scenarios in the file. When only some scenarios need a fact, repeat it in those
scenarios instead — duplication is cheaper than hidden coupling here.

## Scenario Outline

Reach for an `Outline` when the _only_ thing changing between cases is data. The
steps must read naturally with every row substituted.

```gherkin
  Scenario Outline: GeoIP allowlist controls access by source country
    Given a redirection rule that allows only <allowed_country> IP addresses
    When a connection attempt comes from a <source_country> IP address
    Then access to the protected host is <result>

    Examples:
      | allowed_country | source_country | result  |
      | United States   | United States  | granted |
      | United States   | Japan          | denied  |
```

**The `result` column is the workhorse of this style.** Parameterising the
outcome collapses the happy path and the rejection path into one readable table,
so a reviewer sees the whole decision boundary at a glance instead of hunting
for the negative case three scenarios down. Use it whenever a rule has a
"passes" and a "blocked" side.

Rules for Examples tables:

- Pad columns so pipes line up. A ragged table is hard to scan and that is the
  table's entire purpose.
- Column names are `snake_case` and read as nouns from the domain.
- Order columns inputs first, outcome last, so each row reads left to right as
  "given this, that happens".
- Keep the table focused: if it grows past ~17 rows, ask whether the extra rows
  are testing anything new or just re-running the same path. A scale test of 16
  countries is legitimate; 16 near-identical port numbers is not.
- When a table is long and uniform, put the interesting row (the one that breaks
  the pattern — the country _not_ in the allowlist) last, so it reads as the
  punchline.

## Collapsing repeated scenarios into one Outline

Most Gherkin bloat is the same sentence written five times with one word
changed. It is worth hunting deliberately, because prose repeated per case is
prose a reviewer has to diff by eye to find the one thing that differs — and
because each copy is a place a future edit can be forgotten.

### The test for collapsibility

Line the candidate scenarios up and ask: **if I replace the differing words with
placeholders, does every scenario become the same text?** If yes, they are one
Outline and the differences are rows. If no, they are genuinely different
scenarios and forcing them together will produce empty cells and half-sentences.

The differing words are almost always nouns (a country, a plan, a port, a
category) or outcomes (granted/denied, accepted/rejected). Both are data. What
must _not_ differ is the shape: same number of steps, same keywords, same order.

### The move that matters most: outcome as data

The commonest missed collapse is splitting by outcome — one scenario (or one
Outline) for the cases that pass and a near-identical twin for the cases that
fail. It looks tidy and it hides the boundary.

**Before** — two Outlines, one rule, the threshold buried in prose:

```gherkin
  Scenario Outline: Requests within the plan allowance are served
    Given an API key on the <plan> plan
    And the key has not reached its allowance this minute
    When the key makes a request
    Then the request is accepted

    Examples:
      | plan |
      | Free |
      | Pro  |

  Scenario Outline: Requests beyond the plan allowance are rejected
    Given an API key on the <plan> plan
    And the key has already used its full allowance this minute
    When the key makes a request
    Then the request is rejected as rate limited

    Examples:
      | plan |
      | Free |
      | Pro  |
```

**After** — one Outline; the limit is a number, the boundary is two adjacent
rows, and a reader sees the whole rule at once:

```gherkin
  Scenario Outline: A plan's allowance decides whether a request is served
    Given an API key on the <plan> plan
    And the key has made <used> requests this minute
    When the key makes one more request
    Then the request is <result>

    Examples:
      | plan       | used  | result                    |
      | Free       | 59    | accepted                  |
      | Free       | 60    | rejected as rate limited  |
      | Pro        | 599   | accepted                  |
      | Pro        | 600   | rejected as rate limited  |
      | Enterprise | 10000 | accepted                  |
```

Note what the collapse bought beyond brevity: the "has not reached its
allowance" hand-wave became an actual number, the off-by-one boundary is now
stated, and the Enterprise case slots in as one more row instead of a third
scenario.

### How to do the collapse

1. Put the candidate scenarios side by side and mark every word that differs.
2. Check the shape matches — same steps, same order. If one scenario has an
   extra `And`, that extra fact is either a column (if it is data) or the signal
   to leave it as its own scenario (if it is a different condition).
3. Name a column per differing word, using the domain's noun for it. If you
   cannot name a column without a slash or the word "type", the scenarios are
   probably not the same case.
4. Replace the words with `<placeholder>` and read the step lines back. Every
   line must be a grammatical sentence for every row — watch for `a`/`an`
   agreement and singular/plural.
5. Write the rows with inputs first and outcome last, aligned, interesting row
   last.
6. Give the Outline a name stating the _rule_ rather than one case's outcome —
   "A plan's allowance decides whether a request is served", not "Requests are
   accepted".

### When to leave them apart

Collapsing is not always the win. Keep separate scenarios when:

- **The wording would have to change.** An Outline whose steps only parse for
  some rows is worse than repetition.
- **A column would be empty for some rows.** An empty cell means that row is a
  different case wearing the same costume. This is the most common broken
  Outline in the wild.
- **A column holds prose fragments** that get glued into a step line
  (`And <extra_assertion>`). The step must be a sentence in the file, not
  assembled at runtime; otherwise the file no longer reads as a specification.
- **The cases need different annotations.** Whatever your runner uses to mark
  or select a single case applies to the whole `Outline`, never to one row. If
  one case has to carry a marker the others must not, it cannot be a row. Split
  it out.
- **There are only two rows and the reader learns more from prose.** Two
  scenarios with vivid names sometimes beat a two-row table; judge by which one
  a stakeholder would rather read.

A note on `Examples` blocks: Gherkin allows several under one Outline, and they
are useful for grouping rows that share a theme. Do not use them to separate the
passing rows from the failing ones — that reintroduces the split the `result`
column exists to remove.

## Naming

**Feature name**: the capability, as a noun phrase. `Single port redirection`,
`SSL Inspection`, `Time and date scheduling for port redirection`.

**Feature description** (optional, 1–4 lines under the `Feature:` line): either a
short prose paragraph explaining the capability and its constraints, or the
classic role/benefit form when the "who wants this and why" is not obvious:

```gherkin
Feature: Single port redirection
  To route clients to a specific backend
  As a network admin
  I want traffic arriving on a single WAN port to be redirected to a specific backend host and port
```

Skip the description entirely when the feature name already says everything.
A description that only restates the title is noise.

**Scenario name**: a full sentence stating the _outcome_, not the steps.

- Good: `Established connection persists after schedule window closes`
- Good: `Allowlisted IP overrides GeoIP country block`
- Bad: `Test allowlist` — no outcome, no subject.
- Bad: `Add rule and connect` — describes the mechanics, not the claim.

For an `Outline`, the name may carry a placeholder so each example reads
distinctly: `Scenario Outline: Malicious IP blocked by <category> threat category`.

## Anti-patterns

| Smell                                                                        | Why it hurts                                                                 | Instead                                              |
| ---------------------------------------------------------------------------- | ---------------------------------------------------------------------------- | ---------------------------------------------------- |
| `Given I run the apply script and the exit code is 0`                        | Couples the spec to the invocation; breaks on refactor                       | `Given the firewall rules are applied`               |
| Assertions about generated artefacts (SQL rows, iptables lines, config text) | Reader needs implementation knowledge; test passes while behaviour is broken | Assert what an outside observer sees                 |
| `Then it works`                                                              | Unfalsifiable                                                                | Name the observable outcome                          |
| Scenario that depends on a previous scenario's leftovers                     | Order-dependent, un-runnable in isolation                                    | Make each scenario self-contained; reset in hooks    |
| Same concept phrased three ways across files                                 | Fragments the step layer, suite stops reading as one document                | Grep first, reuse exact wording                      |
| UI/CLI mechanics in steps (`clicks`, `types`, `--flag`)                      | Rewrites whenever the interface changes                                      | Describe intent, not interaction                     |
| `Outline` with a column used by only one row                                 | Table stops being a comparison                                               | Split into two scenarios                             |
| Long `Background` used as a dumping ground                                   | Hidden coupling; reader loses the thread                                     | Trim to shared context; push the rest into scenarios |

## Before you call a feature file done

- Every scenario name states an outcome a stakeholder would recognise.
- No step mentions a table, column, chain, counter, log prefix, file path, flag
  or function name — unless that detail is part of the contract consumers
  depend on.
- Every step line's wording already exists elsewhere in the suite, or is a
  deliberate new addition to the vocabulary.
- Each scenario runs alone, in any order.
- No scenario would still pass if the feature stopped working for its user.
- Negative cases exist — usually as a `result` column rather than separate
  scenarios.
- No two scenarios in the file are the same prose with one word changed; those
  have been collapsed into one Outline.
- Examples tables are aligned.

## References

- `references/user-story-examples.md` — before/after pairs for writing a feature
  file from a bare user story, taken from measured runs. Read it when the input
  is a story or a ticket rather than existing code.
