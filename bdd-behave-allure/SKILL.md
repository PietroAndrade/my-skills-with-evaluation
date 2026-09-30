---
name: bdd-behave-allure
description: How to build and run a Python Behave BDD suite — project layout, environment.py hooks and scenario isolation, step definitions that stay thin over a support library, tag-based execution, and Allure reporting labels. Use this skill whenever you are writing or editing step definitions, touching features/environment.py, wiring fixtures or cleanup between scenarios, debugging a Behave run that leaks state or fails only when run with the whole suite, adding tags for report grouping, or setting up/fixing an Allure report from Behave. Trigger it as soon as a task involves a features/ directory with Python steps, even if the user only says "make this scenario pass" or "the test is flaky".
---

# Behave + Allure

This skill covers the machinery *under* the feature files. For how to word the
Gherkin itself, use the `bdd-gherkin-style` skill — the two are designed to be used
together, and most tasks touching a `.feature` file need both.

## The layered shape

The single decision that determines whether a Behave suite stays maintainable is
where the logic lives. Step definitions are glue, not a place to work.

```
features/
  *.feature              ← business language, no implementation detail
  environment.py         ← hooks: wiring, isolation, cleanup
  steps/
    <feature>_steps.py   ← one file per feature, thin
support/                 ← the real library: domain classes, no behave imports
  <domain>/...
tests/                   ← pytest unit tests for support/, no Behave, no infra
```

A step definition should read as three or four lines: translate the business
words into domain values, call the support layer, store what the `Then` will
need on `context`. When a step grows past ~15 lines, the logic belongs in
`support/` — that is what makes it unit-testable without Docker, a database or a
network.

The `support/` package must not import `behave`. Keeping that boundary is what
lets `tests/` exercise the same code with mocks in milliseconds.

## environment.py

### Module-level wiring, hook-level assignment

Build the expensive objects once at import time with their dependencies injected,
then hand them to `context` in `before_all`. Constructing them per scenario is
wasted time; reaching for globals from step files is what makes a suite
impossible to reason about.

```python
_docker = DockerExec()
_client = TrafficClient(
    NetworkAlias(_docker, "client", CLIENT_INTERFACE),
    TcpProbe(_docker, "client"),
)
_firewall = Firewall(
    iptables=IptablesManager(_docker, "firewall"),
    wan_alias=NetworkAlias(_docker, "firewall", FIREWALL_WAN_INTERFACE),
    docker=_docker,
)


def before_all(context):
    context.firewall = _firewall
    context.country_resolver = DictCountryResolver(COUNTRY_ISO)
```

Use keyword arguments when a constructor takes more than two or three
collaborators — a positional list of five managers is unreadable at the call
site and silently wrong when reordered.

What goes on `context` versus what stays module-level: anything a step file needs
goes on `context` (that is the documented, discoverable channel); anything only
the hooks touch stays a module private. A step file reaching into
`environment._firewall` is a sign the object should have been on `context`.

### The isolation contract

**Every scenario must pass when run alone and when run in any order.** This is
not a nicety — a suite that only passes in file order cannot be bisected, cannot
be run with `--tags`, and turns every real failure into an archaeology session.

Split the work by hook:

- `before_all` — one-time environment setup that no scenario mutates (network
  aliases, shared clients, resolvers).
- `before_scenario` — reset the world to a known baseline: restore the clock,
  clear per-scenario `context` fields to their empty values, flush infrastructure
  state, truncate every domain's test data.
- `after_scenario` — tear down what *this* scenario created, using the record it
  left on `context`.

```python
def before_scenario(context, scenario):
    _clock.restore()
    context.source_ip = None
    context.country_wan_ips = []
    context.firewall.flush()
    with DatabaseConnection() as conn:
        for service in (GeoIpDbService, PortRedirectDbService, IpsetDbService):
            service(conn).clear()


def after_scenario(context, scenario):
    _clock.restore()
    if context.source_ip:
        _client.remove_source_alias(context.source_ip)
    for ip in context.country_wan_ips:
        context.firewall.remove_country_wan_alias(ip)
```

Three habits make this hold up:

**Clean in `before_scenario`, not only in `after_scenario`.** A scenario that
crashes mid-way never reaches its teardown, so the next scenario must not trust
that the world is clean. Treat `after_scenario` as courtesy and
`before_scenario` as the guarantee.

**Initialise every mutable `context` field to empty in `before_scenario`.**
`context.country_wan_ips = []` before each scenario means `after_scenario` can
iterate it unconditionally and a step can append to it without checking whether
it exists.

**Let steps record what they created.** A step that adds a source IP sets
`context.source_ip`; teardown removes exactly that. This beats a blanket "remove
everything" sweep because it degrades gracefully when the scenario never got
that far.

**Give every service a uniform `clear()`.** When each domain service exposes the
same reset method, `before_scenario` is a loop instead of a growing pile of
bespoke cleanup, and adding a twelfth domain is one line.

## Step definitions

### One file per feature, shared vocabulary across files

Name the step file after the feature (`ipset.feature` → `steps/ipset_steps.py`).
Behave loads every file in `steps/` into one global registry, so a `Then` written
for one feature is automatically available to all of them — and **reuse is the
goal**, not an accident. Before writing a new `Then`, search the whole `steps/`
directory for the phrase. Two step definitions matching the same sentence is an
`AmbiguousStep` error; two *near-identical* phrasings is worse, because it
silently splits one concept in half.

A typical feature's step file therefore holds only its `Given`s plus whatever
`When`/`Then` is genuinely new, and imports nothing from its siblings — the
registry does the sharing.

### Module-level collaborators, parameterised steps

```python
docker = DockerExec()
_inspector = ChainInspector(docker)
atp = AtpInspector(REDIR_ID, _inspector)


@given("source {ip} is in the {category} threat list")
def step_add_threat_ip(context, ip, category):
    context.firewall.add_atp_ip(category, ip)


@given("a multi-page ATP redirection rule for {category} with pages {p1:d} and {p2:d}")
def step_atp_multi_page(context, category, p1, p2):
    with DatabaseConnection() as conn:
        AtpDbService(conn).setup_threat(category)
    for page in (p1, p2):
        context.firewall.create_atp_ipset(category, page=page)
    context.firewall.apply_redir_rules()
```

Prefer Behave's default `parse` matcher (`{name}`, `{n:d}`) over regex. It keeps
the step string readable as the sentence it is; a regex step is a sentence you
have to decode. Use `:d` and friends so the step body receives a real `int` and
does not litter itself with `int(...)`.

### Mapping tables for business words

When a scenario says `a trojan payload` and the machinery needs
`TROJAN_PAYLOAD`, put the translation in a module-level dict, not in branching
code:

```python
THREAT_PAYLOADS = {
    "trojan": "TROJAN_PAYLOAD",
    "web attack": "WEBATTACK_PAYLOAD",
}

CATEGORY_SIDS = {
    "trojan-activity": 1000001,
    "web-application-attack": 1000002,
}
```

This is the seam that lets the `.feature` stay in business language while the
system under test keeps its own identifiers. Adding a case is a row, and the
table doubles as documentation of the vocabulary the steps accept.

### Assertions that explain themselves

A failing `Then` is read by someone who did not write it, often in CI output
with no debugger. Assert one thing at a time and put the observed context into
the message:

```python
@then("traffic was dropped by ATP for {category}")
def step_atp_drop(context, category):
    assert atp.has_atp_drop_rule(category), (
        f"No ATP DROP rule for {category} in mangle chain"
    )
    assert atp.atp_drop_was_hit(category), (
        f"ATP DROP rule for {category} has no packet hits"
    )
```

The two assertions distinguish "the rule was never created" from "the rule exists
but nothing matched it" — two completely different bugs that a single combined
assertion would blur. When a domain has a recurring diagnosis need, give the
support object a `print_diagnostics()` and call it on failure rather than
inlining dumps in the step.

### Where the `When` stores its result

The `When` performs the action and leaves everything the `Then`s need on
`context` — the result code, the packet capture, the source IP. `Then` steps only
read. A `Then` that performs an action is a scenario that cannot be reordered or
reused.

## Running the suite

```bash
behave features/                       # everything
behave features/geoip.feature          # one feature
behave --tags=@bug features/           # one slice
behave --tags='@geoip and not @bug'    # expression
behave --no-capture features/x.feature # let print()/stdout through
```

`--no-capture` is the first thing to reach for when debugging: Behave swallows
stdout by default, so diagnostics printed from a step or a support object are
invisible until you pass it.

Wrap the common invocations in a `Makefile` so nobody has to remember the
virtualenv dance, and so CI and humans run the same thing:

```makefile
VENV = source venv/bin/activate

features:
	$(VENV) && behave features/

feature:
	$(VENV) && behave features/$(name).feature

tags:
	$(VENV) && behave --tags=$(tag) features/
```

A sequential per-file target is worth having too: running each feature in its own
Behave process isolates suites that share heavy infrastructure, and `|| true`
keeps the loop going so one broken feature does not hide the rest.

### `@bug` and `@wip` as workflow

`@bug` marks a scenario that is *correctly specified* but fails because of a known
defect in the system under test. Keeping it in the suite (rather than deleting it
or weakening its assertions) means `--tags=@bug` is the live list of what is
broken, and the day the defect is fixed the scenario turns green on its own.
Document each `@bug` in the project's docs with the actual root cause — the tag
says "known", the docs say "why".

## Allure reporting

Register the formatter once in `behave.ini`:

```ini
[behave.formatters]
allure = allure_behave.formatter:AllureFormatter
```

then run with both formatters so the terminal stays readable while results are
collected:

```makefile
BEHAVE_ALLURE = -f pretty -o - -f allure -o allure-results

report:
	rm -rf allure-results allure-report
	$(VENV) && behave $(BEHAVE_ALLURE) features/ || true
	allure generate -c .allurerc.mjs -o allure-report
	allure open allure-report
```

The `|| true` matters: a failing suite still has results worth reporting, and
without it `make report` stops before generating anything.

### Labels as tags

Allure's tree groups by exactly three labels. Put them on a **second tag line**
below the behavioural tags, so a reader scanning behaviour can ignore the
reporting row:

```gherkin
@allure.label.parentSuite:GeoIP
Feature: Access redirection by country

  @allowlist @multi-country @single-wan
  @allure.label.suite:Allowlist @allure.label.subSuite:MultiCountry-SingleWan
  Scenario Outline: ...
```

| Label | Scope | Holds |
|---|---|---|
| `parentSuite` | Feature line | Domain / epic — `GeoIP`, `SNAT` |
| `suite` | Scenario | Rule type or functional area — `Allowlist`, `SinglePort` |
| `subSuite` | Scenario | Categories, hyphen-joined — `SingleCountry-MultiWan` |

Keep the mapping mechanical: every category tag on a scenario appears as a
PascalCase segment of its `subSuite`, in tag order. Result tags (`@granted`,
`@denied`) are outcomes, not categories — leave them out unless the report needs
the two sides separated, in which case append `-Granted` / `-Denied` last and do
it consistently across the file.

### Traps

Every one of these fails quietly — the run succeeds, the report just comes out
wrong, so they cost an afternoon each if you meet them cold:

- **`epic` / `feature` / `story` labels collide.** `allure-behave` already
  derives a `feature` label from the `Feature:` line; adding your own gives the
  scenario two, the tree renders both as sibling nodes and the test count
  doubles. This is the whole reason the convention uses
  `parentSuite` / `suite` / `subSuite`.
- **Repeated `subSuite` tags do not nest.** Two `@allure.label.subSuite:` tags
  on one scenario produce siblings, not a path. Hyphenate into one value
  instead: `MultiCountry-MultiWan-AppUsage`.
- **Three levels is the ceiling.** A fourth `groupBy` entry (`{ label: "layer" }`)
  is stored in the result JSON and ignored by the tree widget. Design within
  three.
- **`allure generate` without `-c .allurerc.mjs` ignores the config** and gives
  you the default Suites view — which looks exactly like the tags failed.
- **The npm package `allure-commandline` is Allure 2 and needs Java**, as does
  the Arch AUR package of the same name. The Allure 3 CLI is `npm install -g
  allure`.
- **`import { defineConfig } from "allure"` in `.allurerc.mjs` breaks** under a
  global npm install. Use a plain default export.
- **No `allure-behave` in `requirements.txt` breaks every `behave` run**, not
  just reporting, once the formatter is registered in `behave.ini` — the import
  fails at startup. Pin it alongside `behave`.

When the tree still looks wrong, inspect what was actually written rather than
re-reading the feature file: `references/allure-reporting.md` has the scripts
for dumping a test result's labels and the generated tree, plus the
`.allurerc.mjs` and CLI details.

## Checklist

- Each scenario passes alone and under `--tags`.
- `before_scenario` resets everything; nothing depends on the previous
  scenario's teardown having run.
- Every mutable `context` field is initialised in `before_scenario`.
- Step bodies are a handful of lines; the work lives in `support/`.
- `support/` does not import `behave`, and has unit tests.
- No duplicated step phrasing across `steps/`.
- Assertions carry messages naming what was expected and what was observed.
- `@bug` scenarios have a documented root cause.
- Tags carry domain, category and result; Allure labels on their own line.
- `allure-behave` is pinned in requirements.txt if `behave.ini` registers the
  formatter, and no `epic`/`feature`/`story` labels are used.
