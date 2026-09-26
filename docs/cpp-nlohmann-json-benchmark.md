# C++ nlohmann-json Skill Benchmark

Comparison between code generated following the `cpp-nlohmann-json` skill vs. baseline (no skill), same task description.

**Reference files:** `~/.claude/skills/cpp-nlohmann-json-workspace/iteration-1/`

---

## Eval 0 — Enum + struct (`LogLevel` / `LogEntry`)

### skill
- `NLOHMANN_JSON_SERIALIZE_ENUM` with all 5 values ✅
- `using json = nlohmann::json` inside namespace ✅
- `friend to_json`/`from_json` declared in struct ✅
- `from_json` uses `j.value()` for all fields (no throws on missing) ✅
- `to_json` uses single `json{...}` initializer ✅
- Default-initialized fields (`level{LogLevel::Info}`, `timestampMs{0}`) ✅

### baseline
- Hand-rolled `unordered_map` + `#define X(name)` macro — reinvents what `SERIALIZE_ENUM` does ❌
- `from_json` uses `j.at()` — throws on missing fields ❌
- No namespace for `FilterRule` ❌
- No default field initialization ❌

| Criterion | skill | baseline |
|-----------|-------|----------|
| `NLOHMANN_JSON_SERIALIZE_ENUM` macro | ✅ | ❌ |
| `j.value()` in `from_json` (no throw) | ✅ | ❌ |
| Correct namespace | ✅ | ✅ |
| `friend` declarations in struct | ✅ | ✅ |
| Single `json{...}` initializer in `to_json` | ✅ | ✅ |
| Default-initialized fields | ✅ | ❌ |

**Score: 6/6 vs 3/6**

---

## Eval 1 — Safe parse (`FilterRule`)

### skill
- `j.value()` for scalars (`id`, `comment`) ✅
- `contains + is_array` guard before `blockedHosts` loop ✅
- Per-element `is_string()` check + skip ✅
- `std::nullopt` assigned when `comment` absent ✅
- `to_json` serializes `comment` only when `has_value()` ✅
- Namespace `filter` ✅

### baseline
- `j.value()` for `id` ✅
- `contains + is_array` guard ✅
- Bulk `.get<vector<string>>()` — no per-element type guard ❌
- No `to_json` generated ❌
- No namespace ❌

| Criterion | skill | baseline |
|-----------|-------|----------|
| `j.value()` for scalars | ✅ | ✅ |
| `contains + is_array` guard | ✅ | ✅ |
| Per-element `is_string()` skip | ✅ | ❌ |
| `to_json` generated | ✅ | ❌ |
| Correct namespace | ✅ | ❌ |
| `comment` serialized only when present | ✅ | ❌ |

**Score: 6/6 vs 2/6**

---

## Eval 2 — POSIX + enum-keyed map (`PeerDto`)

### skill
- `SERIALIZE_ENUM` for both `Protocol` and `Port` ✅
- Static helpers on struct (`addrToString`/`stringToAddr`) ✅
- `inet_ntop` + `inet_pton` via static helpers ✅
- Map serialized with `json(kv.first).get<string>()` pattern ✅
- `contains + is_object` guard before map parse ✅
- `try/catch` per-entry skips unknown keys ✅
- `contains + is_array` + per-element guard for addresses ✅

### baseline
- `SERIALIZE_ENUM` for both enums ✅
- `inet_ntop`/`inet_pton` as free `to_json`/`from_json` for `in_addr` — throws on failure ❌
- Map serialize uses structured binding + `json(proto_key).get<string>()` ✅
- `j.at()` in `from_json` — throws on missing keys ❌
- No `try/catch` in map parse — unknown keys throw ❌

| Criterion | skill | baseline |
|-----------|-------|----------|
| `SERIALIZE_ENUM` for both enums | ✅ | ✅ |
| `inet_ntop`/`inet_pton` used | ✅ | ✅ |
| Map key conversion via `json()` | ✅ | ✅ |
| No throw on missing/invalid address | ✅ | ❌ |
| `try/catch` skip for unknown map keys | ✅ | ❌ |
| `contains` guard before map/array parse | ✅ | ❌ |

**Score: 6/6 vs 3/6**

---

## Eval 3 — Nested inside class (`SessionManager::Mode` / `SessionInfo`)

### skill
- Macros placed after class `};`, at namespace scope ✅
- Fully-qualified `SessionManager::Mode` and `SessionManager::SessionInfo` ✅
- `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` for plain-bag `SessionInfo` ✅
- `NLOHMANN_JSON_SERIALIZE_ENUM` for `Mode` ✅
- `ReadOnly` first (safest fallback) ✅
- No `.cpp` needed — macro is header-only ✅

### baseline
- Macros after class `};` ✅
- Fully-qualified names ✅
- Hand-written `inline to_json`/`from_json` instead of `DEFINE_TYPE_NON_INTRUSIVE` ✅ (works, verbose)
- `from_json` uses `j.at()` — throws on missing fields ❌
- No default field initialization in struct ❌

| Criterion | skill | baseline |
|-----------|-------|----------|
| Macros at namespace scope after `};` | ✅ | ✅ |
| Fully-qualified names | ✅ | ✅ |
| `DEFINE_TYPE_NON_INTRUSIVE` for plain bag | ✅ | ❌ (hand-written) |
| `from_json` non-throwing | ✅ | ❌ |
| Default-initialized struct fields | ✅ | ❌ |

**Score: 5/5 vs 2/5**

---

## Summary

| Eval | skill | baseline |
|------|-------|----------|
| 0 — enum + struct | 6/6 | 3/6 |
| 1 — safe parse | 6/6 | 2/6 |
| 2 — POSIX + enum-keyed map | 6/6 | 3/6 |
| 3 — nested in class | 5/5 | 2/5 |
| **Total** | **23/23** | **10/23** |

**Consistent skill advantages:**
- `NLOHMANN_JSON_SERIALIZE_ENUM` vs. hand-rolled maps
- `j.value()` + guards everywhere vs. `j.at()` (throws on missing)
- `try/catch` per-entry in map parsing vs. throw-on-unknown
- `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` for plain bags
- Proper namespace and default field initialization
