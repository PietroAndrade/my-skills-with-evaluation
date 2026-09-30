# Claude Skills

Personal skills for generating code following project conventions. Each skill encodes patterns that a model without guidance consistently misses.

## C++ Skills

| Skill | Triggers on |
|-------|-------------|
| `cpp-interface` | "create interface for X", "define the contract of Y" |
| `cpp-concrete-class` | "create a class", "implement interface X" |
| `cpp-lib` | "create a new lib", "add a new module called X" |
| `cpp-dto` | "create a DTO", "add a data struct for X" |
| `cpp-wrapping-c-pure-posix` | "wrap a socket", "POSIX integration", "file descriptor", "errno handling", "event loop", "custom deleter" |
| `cpp-producer-consumer` | "producer consumer", "worker threads", "blocking queue", "IO thread", "graceful shutdown threads" |
| `cpp-design-patterns-gof` | "which pattern to use", "refactor", "code smell", "decouple", "extensible" |
| `cpp-design-patterns-fowler` | "data access layer", "domain logic", "Active Record vs Data Mapper", "N+1", "transaction" |

| `cpp-modernize` | "modernize this code", "replace #define", "NULL to nullptr", "replace malloc", "fix singleton", "int return bool" |

## BDD / Testing Skills

| Skill | Triggers on | Status |
|-------|-------------|--------|
| `bdd-gherkin` | "write a feature file", "write a scenario", "Gherkin", "Given When Then" | **planned** |
| `bdd-best-practices` | "BDD best practices", "how to structure scenarios", "acceptance test design" | **planned** |
| `bdd-behave` | "behave framework", "steps.py", "environment.py", "behave hooks" | **planned** |

## Shared references

| File | Used by |
|------|---------|
| `docs/cpp-conventions.md` | cpp-interface, cpp-concrete-class |
| `docs/cpp-cmake-conventions.md` | cpp-lib |

## Adding a new skill

See [AGENTS.md](AGENTS.md) for the full workflow.

## Benchmarks

| Skill | Score (skill vs baseline) | Doc |
|-------|--------------------------|-----|
| cpp-interface, cpp-concrete-class, cpp-lib | 15/15 vs 5/15 | [cpp-skills-benchmark.md](docs/cpp-skills-benchmark.md) |
| cpp-dto | 17/17 vs 6/17 | [cpp-dto-benchmark.md](docs/cpp-dto-benchmark.md) |
| cpp-producer-consumer | **pending** | — |

## Roadmap

### In progress / next

| Item | Type | Notes |
|------|------|-------|
| Benchmark `cpp-producer-consumer` | benchmark | Concurrency rules are non-obvious — validate skill prevents wrong notify placement, wrong shutdown order |
| `cpp-concurrent` | new skill | Patterns for concurrent programming problems: deadlock avoidance, race conditions, lock granularity, atomic vs mutex decision |
| `bdd-gherkin` | new skill | Gherkin syntax, Given/When/Then structure, scenario outline, tags — explore `features/` in dnsproxy for real examples |
| `bdd-best-practices` | new skill | Scenario design: one behaviour per scenario, ubiquitous language, avoiding implementation details in steps |
| `bdd-behave` | new skill | Behave-specific: `environment.py` hooks, step libraries, fixtures, Docker Compose orchestration pattern from dnsproxy |

### Exempt from benchmarks

| Skill | Reason |
|-------|--------|
| `conan-cmake` | Reference/workflow doc, no generative output to grade |
| `cpp-design-patterns-gof` | Advisory — pattern selection is subjective |
| `cpp-design-patterns-fowler` | Same |
| `cpp-wrapping-c-pure-posix` | Similar topology to cpp-interface/concrete already benchmarked |
