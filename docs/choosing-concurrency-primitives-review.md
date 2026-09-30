# choosing-concurrency-primitives — Review

> Note: this content originally shipped as the `cpp-concurrency-reference` skill, later demoted to a shared reference doc (`docs/choosing-concurrency-primitives.md`) to cut trigger collision with the sibling skills. The decision-map data is preserved there and linked from `cpp-synchronization` and `cpp-concurrency-debug`. This review records the original skill evaluation.

Qualitative review of the decision map for choosing a C++ concurrency primitive. Three realistic decision prompts run through a subagent that had the content loaded; outputs judged for correct primitive choice, sound justification, and correct routing to sibling skills.

**Source:** now `~/.claude/skills/docs/choosing-concurrency-primitives.md`
**Baseline C++ standard:** C++17 (barrier/latch/semaphore noted as C++20 with fallback)
**Method:** quick review (with-skill only), iteration 1

---

## Test prompts

| # | Prompt | Correct answer |
|---|--------|----------------|
| 0 | Contador global (`unsigned int`) incrementado por várias threads — mutex ou atomic? | `std::atomic` |
| 1 | Cache de config, muitos leitores + escritor raro — qual primitivo? | `std::shared_mutex` |
| 2 | Limitar pool de 3 conexões simultâneas, preso no C++17 — o quê? | counting semaphore + fallback C++17 |

---

## Results

| Criterion | eval-0 | eval-1 | eval-2 |
|-----------|--------|--------|--------|
| Chose correct primitive | ✅ atomic | ✅ shared_mutex | ✅ counting semaphore |
| Ruled out the wrong alternative with reason | ✅ (vs mutex) | ✅ (vs mutex/atomic) | ✅ (vs mutex) |
| Applied skill's reasoning cue | ✅ "read-decide-write tell" | ✅ "mostly-readers rule" | ✅ "owner vs permit-counter" |
| Named C++17 fallback where needed | n/a | n/a | ✅ mutex+condvar+count |
| Routed to sibling skill for code | — | ✅ cpp-synchronization | ✅ cpp-synchronization |
| Cited corpus demo | — | ✅ 04_09 | — |

## Score

| | with-skill |
|--|-----------|
| Prompts fully correct | **3 / 3** |

## Observations

- All three picked the right primitive and justified it with the exact reasoning cues codified in the decision map, not generic advice.
- eval-1 and eval-2 volunteered extra value: a lock-free `shared_ptr<const Config>` alternative, and an RAII guard note for the semaphore fallback.
- Routing worked — outputs deferred code generation to `cpp-synchronization` rather than inlining half an implementation, matching the skill's intent.
- Subagents could not persist output files (Write/Bash denied in sandbox); answers captured from task results. Does not affect the qualitative verdict.

**Verdict:** approved, iteration 1. No changes required before proceeding to `cpp-concurrency-debug`.
