---
name: cpp-design-patterns-fowler
version: 1.0.0
description: "Guidance on Martin Fowler's Patterns of Enterprise Application Architecture. Use when: (1) designing data access layers, (2) organizing domain logic, (3) structuring web presentation, (4) handling distribution/remoting, (5) choosing between Active Record vs Data Mapper, (6) avoiding N+1 queries, (7) managing transactions and identity."
license: MIT
metadata:
  domains: [enterprise-architecture, data-access, domain-model, web-presentation, distribution, c++]
  source: https://martinfowler.com/eaaCatalog/
---

# Enterprise Application Patterns (Fowler EAA)

Expert guidance on applying Martin Fowler's **Patterns of Enterprise Application Architecture** (P of EAA, 2002) to solve common structural problems in enterprise software. This skill covers how to organize domain logic, connect to databases, present data on the web, and communicate across process boundaries — with C++ examples throughout.

## Triggers

- `how to organize domain logic` / `transaction script vs domain model` — Domain logic pattern selection
- `how to connect to database` / `active record vs data mapper` — Data source pattern selection
- `unit of work` / `identity map` / `lazy loading` — Object-relational pattern guidance
- `mvc` / `front controller` / `page controller` — Web presentation pattern selection
- `dto` / `remote facade` / `rpc design` — Distribution pattern guidance
- `value object` / `null object` / `special case` — Base pattern guidance
- `repository pattern` / `query object` — Data access abstraction
- `layer supertype` / `gateway` / `service stub` — Infrastructure base patterns

## Quick Reference

| Input | Output | Duration |
|-------|--------|----------|
| Code architecture problem | Pattern recommendation + C++ example | 2-5 min |
| "We need to access the DB from domain objects" | Active Record vs Data Mapper trade-off | 2-3 min |
| "Null checks everywhere" | Special Case / Null Object guidance | 1-2 min |
| Pattern name | Implementation example + context | 1-2 min |
| Layer design question | Decision tree + reference file | 3-5 min |

## Agent Behavior Contract

1. **Understand the layer first** — Ask which layer (domain, data access, presentation, distribution) before recommending
2. **Complexity match** — Transaction Script for simple logic; Domain Model for complex rules
3. **Show trade-offs** — Every pattern has costs; state them explicitly
4. **Use C++ examples** — All code examples should be idiomatic modern C++ (C++14/17)
5. **Prefer simplicity** — Don't recommend Data Mapper if Active Record solves the problem
6. **No premature abstraction** — Repository over raw SQL only when testability or domain isolation matters
7. **Pair patterns** — Mention natural companions (Data Mapper + Unit of Work + Identity Map)

## Pattern Selection Decision Tree

### Domain Logic Layer

```
How complex is the business logic?
│
├─ Simple, few interactions between operations?
│  └─→ Transaction Script
│
├─ Moderate, some shared state but mostly table-centric?
│  └─→ Table Module
│
├─ Complex, many interacting concepts and rules?
│  └─→ Domain Model
│
└─ Multiple clients (web, API, batch) need the same operations?
   └─→ Service Layer (wraps any of the above)
```

### Data Source / Persistence Layer

```
How closely do domain objects map to DB tables?
│
├─ Very closely, simple CRUD, no complex rules?
│  ├─ One class per table, return record sets?  → Table Data Gateway
│  ├─ One object per row, handle own SQL?       → Row Data Gateway
│  └─ Domain object + own persistence?          → Active Record
│
└─ Domain model complex or must be testable without DB?
   └─→ Data Mapper
       ├─ + collection-like query API?          → Repository
       ├─ + complex dynamic queries?            → Query Object
       ├─ + batch DB writes?                    → Unit of Work
       └─ + prevent duplicate loads?            → Identity Map
```

### Object-Relational Behavior

```
Problem with loading?
│
├─ Loading too much (eager loads unused associations)?
│  └─→ Lazy Load
│
├─ Same DB row loaded twice → duplicate objects?
│  └─→ Identity Map
│
├─ Multiple DB writes per business transaction?
│  └─→ Unit of Work
│
└─ Business logic queries are tangled SQL strings?
   └─→ Query Object
```

### Web Presentation

```
How is the web layer structured?
│
├─ Few pages, each with unique logic?
│  └─→ Page Controller
│
├─ Many pages, shared auth/logging/routing needed?
│  └─→ Front Controller
│
├─ Need to separate data, display, and user interaction?
│  └─→ MVC (Model-View-Controller)
│
└─ HTML templates with dynamic data substitution?
   └─→ Template View
```

### Distribution / Remoting

```
Are you exposing domain logic remotely?
│
├─ Fine-grained calls → too many round-trips?
│  └─→ Remote Facade (coarse-grained wrapper)
│
└─ Need to carry data across process boundaries?
   └─→ Data Transfer Object (DTO)
```

### Base / Infrastructure Patterns

```
Problem type?
│
├─ Wrapping external system (payment, email, DB)?
│  └─→ Gateway
│
├─ Translating between two layers without coupling them?
│  └─→ Mapper
│
├─ Small immutable concept (Money, Date, Color)?
│  └─→ Value Object
│
├─ Well-known service accessible application-wide?
│  └─→ Registry
│
├─ Null/missing entity causes pervasive null checks?
│  └─→ Special Case (Null Object)
│
├─ External service breaks tests (slow, expensive, side-effects)?
│  └─→ Service Stub
│
└─ All objects in a layer share common infrastructure?
   └─→ Layer Supertype
```

## Process

### Phase 1: Identify the Architectural Layer

Before recommending a pattern, identify which layer the problem lives in.

1. **Domain Logic** — Where do business rules live? Who coordinates them?
2. **Data Source** — How are domain objects stored and retrieved?
3. **Object-Relational** — How are in-memory objects kept in sync with DB state?
4. **Web Presentation** — How are HTTP requests handled and responses rendered?
5. **Distribution** — Is any logic exposed across process or network boundaries?
6. **Base/Infrastructure** — What cross-cutting concerns exist (nulls, value types, gateways)?

**Verification**: You can name the specific layer and state the concrete problem.

### Phase 2: Match to Pattern

1. **Use the decision trees above** to narrow candidates
2. **Consult reference files** for detailed descriptions and C++ examples:
   - `references/domain-logic.md` — Transaction Script, Domain Model, Table Module, Service Layer
   - `references/data-source.md` — Table Data Gateway, Row Data Gateway, Active Record, Data Mapper
   - `references/object-relational.md` — Unit of Work, Identity Map, Lazy Load, Repository, Query Object
   - `references/web-presentation.md` — MVC, Front Controller, Page Controller, Template View
   - `references/distribution.md` — Remote Facade, Data Transfer Object
   - `references/base-patterns.md` — Gateway, Mapper, Value Object, Registry, Special Case, Service Stub, Layer Supertype
3. **State the trade-offs** — confirm the chosen pattern fits complexity and team skill level
4. **Mention companions** — Data Mapper almost always pairs with Unit of Work + Identity Map

**Verification**: Pattern recommendation is justified by the concrete problem, not pattern preference.

### Phase 3: Implement

1. **Show key participants** — interfaces, classes, and their relationships
2. **Provide C++ example** — idiomatic modern C++ (unique_ptr, shared_ptr, override)
3. **Walk through the flow** — how data moves between participants
4. **Highlight integration points** — how this pattern connects to adjacent layers

**Verification**: Implementation solves the original problem and integrates cleanly with the surrounding architecture.

## Common Scenarios → Patterns

| Scenario | Primary Pattern | Companion Patterns |
|----------|-----------------|--------------------|
| CRUD app, each table = one class, simple rules | Active Record | — |
| Complex billing/inventory with interacting rules | Domain Model | Data Mapper, Unit of Work |
| Web app: centralized auth, routing, logging | Front Controller | MVC, Template View |
| Avoid SQL strings scattered across domain logic | Repository | Query Object, Data Mapper |
| Load parent lazily (don't fetch children until needed) | Lazy Load | Identity Map |
| REST API exposes internal fine-grained services | Remote Facade | DTO, Mapper |
| Two layers must exchange data without coupling | Mapper / DTO | Remote Facade |
| `nullptr` checks everywhere for missing customer | Special Case | Domain Model |
| Unit tests can't depend on real payment gateway | Service Stub | Gateway |
| Money, Color, DateRange — small immutable concepts | Value Object | Domain Model |
| All domain objects need timestamps + dirty flag | Layer Supertype | Unit of Work, Data Mapper |
| Multiple clients (web, CLI, batch) need same ops | Service Layer | Domain Model, Repository |
| Dynamic query building (filter + sort + paginate) | Query Object | Repository |

## Anti-Patterns

| Avoid | Why | Instead |
|-------|-----|---------|
| Transaction Script for complex domain | Duplicated logic, hard to maintain | Domain Model |
| Active Record when domain ≠ schema | Tight coupling, hard to test | Data Mapper + Repository |
| Remote Facade with fine-grained methods | Defeats the purpose — N round-trips remain | Coarse-grained facade operations |
| Fat Service Layer (business logic in service) | Service Layer should coordinate, not implement | Move logic to Domain Model |
| Global Identity Map across requests | Memory leak, stale data | Scope Identity Map to one transaction/request |
| Lazy Load in a loop | N+1 query problem | Eager load collections before iterating |
| DTO used internally as domain object | Anemic domain model | Keep DTOs at the boundary only |
| Registry as substitute for DI everywhere | Hidden dependencies, hard to test | Prefer explicit constructor injection |
| Null Object without defined "null behavior" | Wrong defaults hide bugs | Define the null behavior explicitly |

## Verification

After applying a pattern:

- [ ] The chosen pattern matches the complexity of the problem
- [ ] DB access is isolated from domain logic (or intentionally colocated via Active Record)
- [ ] No N+1 query bugs (check all loops that access lazy-loaded associations)
- [ ] Unit of Work scope is per-request/transaction, not global
- [ ] Identity Map is scoped to a single business transaction
- [ ] DTOs are used only at system boundaries, not inside the domain
- [ ] Service Stubs are injected via DI / Registry — production code never references them
- [ ] Value Objects are immutable; equality is by value, not identity
- [ ] Special Case behavior is explicitly defined and tested

## References

Always use Context7 MCP (`/websites/martinfowler`) to search Fowler's site for additional context.

- [Domain Logic Patterns](references/domain-logic.md) — Transaction Script, Domain Model, Table Module, Service Layer
- [Data Source Patterns](references/data-source.md) — Table Data Gateway, Row Data Gateway, Active Record, Data Mapper
- [Object-Relational Patterns](references/object-relational.md) — Unit of Work, Identity Map, Lazy Load, Repository, Query Object
- [Web Presentation Patterns](references/web-presentation.md) — MVC, Front Controller, Page Controller, Template View
- [Distribution Patterns](references/distribution.md) — Remote Facade, Data Transfer Object
- [Base Patterns](references/base-patterns.md) — Gateway, Mapper, Value Object, Registry, Special Case, Service Stub, Layer Supertype

---

**Note**: Pattern selection requires judgment. The same problem can often be solved by different patterns at different complexity levels. Start simple (Transaction Script, Active Record) and refactor toward richer patterns (Domain Model, Data Mapper) only when complexity demands it.
