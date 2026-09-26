# C++ DTO Skill Benchmark

Comparison between code generated following the `cpp-dto` skill vs. baseline (no skill), same task description.

**Reference files:** `~/.claude/skills/cpp-dto-workspace/iteration-1/`

---

## Eval 0 — Simple DTO (`EventLogDto`)

### skill
```
reporter/include/reporter/event_log_dto.h
```
### baseline
```
event_log_dto.h  (root, no lib path structure)
```

| Criterion | skill | baseline |
|-----------|-------|----------|
| Guard: `#ifndef REPORTER_EVENT_LOG_DTO_H` | ✅ | ❌ (`#pragma once`) |
| Correct lib path (`reporter/include/reporter/`) | ✅ | ❌ (file at root) |
| `struct`, not `class` | ✅ | ✅ |
| Fields: `camelCase`, no `m_` prefix | ✅ | ✅ |
| Namespace `reporter` with closing comment | ✅ | ✅ |
| No `.cpp` generated (header-only) | ✅ | ✅ |

**Score: 6/6 vs 4/6**

---

## Eval 1 — DTO with static helpers (`PacketHeaderDto`)

### skill
- Guard: `#ifndef SOCKET_CLIENT_PACKET_HEADER_DTO_H` ✅
- `static constexpr std::size_t SIZE = 8` (uses `std::size_t`) ✅
- Constants declared before fields ✅
- `parse()` takes `std::size_t` (not bare `size_t`) ✅
- `.cpp` namespace brace on own line ✅
- Field alignment with spaces ✅

### baseline
- Guard format: `#ifndef SOCKET_CLIENT_PACKET_HEADER_DTO_H` ✅
- `static constexpr size_t SIZE = 8` (bare `size_t`, not `std::size_t`) ❌
- Constants declared *after* fields ❌
- `parse()` takes bare `size_t` ❌
- Field alignment missing (no column alignment) ❌

| Criterion | skill | baseline |
|-----------|-------|----------|
| Guard format correct | ✅ | ✅ |
| `std::size_t` (not bare `size_t`) | ✅ | ❌ |
| Constants before fields | ✅ | ❌ |
| `parse()` uses `std::size_t` | ✅ | ❌ |
| Field column alignment | ✅ | ❌ |
| Correct lib path structure | ✅ | ❌ (path: `include/socket_client/` not `socket_client/include/socket_client/`) |

**Score: 6/6 vs 2/6**

---

## Eval 2 — DTO with JSON (`FilterRuleDto`)

### skill
- Guard: `#ifndef CLASSIFIER_FILTER_RULE_DTO_H` ✅
- `friend` declarations in header only — no implementations in header ✅
- `.cpp` generated with `using json = nlohmann::json` ✅
- `j.value("key", default)` in `from_json` — resilient to missing fields ✅
- `using json` scoped to `.cpp`, not polluting header namespace ✅

### baseline
- Guard: `#ifndef FILTER_RULE_DTO_H` (missing lib prefix) ❌
- `using json = nlohmann::json` **inside the header namespace** — pollutes every file that includes this header ❌
- `to_json`/`from_json` **implemented inline in the header** ❌
- `j.at("key")` in `from_json` — throws on missing fields, not resilient ❌
- No `.cpp` generated ❌

| Criterion | skill | baseline |
|-----------|-------|----------|
| Guard includes lib prefix (`CLASSIFIER_`) | ✅ | ❌ |
| `using json` scoped to `.cpp` only | ✅ | ❌ (in header) |
| JSON impl in `.cpp`, not header | ✅ | ❌ (inline in header) |
| `j.value()` with fallback in `from_json` | ✅ | ❌ (`j.at()` — throws on missing key) |
| Correct lib path structure | ✅ | ❌ (file at root) |

**Score: 5/5 vs 0/5**

---

## Overall Score

| Eval | skill | baseline |
|------|-------|----------|
| simple-dto | 6/6 | 4/6 |
| dto-with-helpers | 6/6 | 2/6 |
| dto-with-json | 5/5 | 0/5 |
| **Total** | **17/17** | **6/17** |

---

## Key Observations

**What baseline gets right:** `struct` usage, field naming (`camelCase`, no `m_`), namespace closing comment, correct includes.

**Where skill wins consistently:**
1. **Lib path structure** — baseline drops files at root or uses wrong directory layout; skill always generates `<lib>/include/<lib>/<name>.h`
2. **Include guard format** — baseline uses `#pragma once` or omits lib prefix; skill uses `#ifndef <LIB>_<NAME>_DTO_H`
3. **JSON placement** — baseline inlines `to_json`/`from_json` in the header and puts `using json` in the header namespace (a namespace pollution bug); skill keeps both in `.cpp`
4. **`j.value()` vs `j.at()`** — baseline uses `j.at()` which throws on missing keys, breaking backward compatibility; skill uses `j.value(key, default)` per project convention
5. **`std::size_t` vs bare `size_t`** — baseline uses unqualified `size_t` which is technically UB without `<cstddef>` being explicitly included; skill uses `std::size_t` consistently
