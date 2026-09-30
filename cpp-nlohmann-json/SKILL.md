---
name: cpp-nlohmann-json
description: Generates nlohmann/json serialization code for C++ types following project conventions — enum<->string mapping, to_json/from_json for structs, and safe parsing. Use whenever the user asks to make a C++ type serializable to/from JSON, map an enum to strings in JSON, write to_json/from_json, parse a JSON payload into a struct, or handle nlohmann json — triggered by "serialize this to JSON", "add JSON to this DTO", "map this enum to JSON strings", "SERIALIZE_ENUM", "parse this JSON into X", "to_json/from_json for Y".
applyTo:
  - "*.h"
  - "*.cpp"
---

## Goal

Generate nlohmann/json serialization for C++ types the way this project already does it. Read `/home/pandrade/.claude/skills/docs/cpp-conventions.md` for naming, parameter passing, and numeric type rules. This skill is the JSON specialist; for creating the DTO struct itself use the `cpp-dto` skill, then come here to make it serializable.

The reference implementation lives in `~/development/dnsproxy/filter_dtos/`. When unsure how a case is handled, read a sibling DTO there before inventing a new style.

## When each tool applies

- **Enum ⟷ JSON string** → `NLOHMANN_JSON_SERIALIZE_ENUM`. Placed in the header, right after the enum, inside the namespace.
- **Struct ⟷ JSON object** → `friend` `to_json`/`from_json` declared in the header, defined in the `.cpp`.
- **Reading untrusted JSON** → `j.value(key, default)` for scalars, `contains` + `is_*` guards for arrays/objects, `try/catch` around enum conversions that can fail.

### Hand-written vs `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE`

Decide per struct:

- **One-liner macro** — use `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Type, field1, field2)` when the struct is a plain bag whose member names *are* the JSON keys and every member already serializes (scalars, strings, `SERIALIZE_ENUM`-mapped enums, other serializable types). No `.cpp` needed; the macro emits both functions. Placed at namespace scope.
- **Hand-written `to_json`/`from_json`** — when you need custom key names, POSIX conversions, lenient parsing that tolerates missing fields, or any per-field logic. The intrusive macros can't express these.

When in doubt, start with the macro; drop to hand-written the moment a key name or a conversion has to differ from the default.

## Enum serialization

Declare the enum, then the macro immediately after it, both inside the namespace:

```cpp
enum class ResolutionStatus
{
    Ok,
    NotFound,
    SystemError,
    InvalidInput
};

NLOHMANN_JSON_SERIALIZE_ENUM(ResolutionStatus, {
    {ResolutionStatus::Ok, "Ok"},
    {ResolutionStatus::NotFound, "NotFound"},
    {ResolutionStatus::SystemError, "SystemError"},
    {ResolutionStatus::InvalidInput, "InvalidInput"},
})
```

Once the macro exists, an enum field serializes like any scalar — `{"status", r.status}` writes the string, and `j.value("status", Default)` reads it back. The **first** pair is the fallback: if the JSON string matches no entry, `from_json` yields that first enum value, so order it so the safest default comes first (here `Ok`, but pick per type).

## Struct serialization

Declare the two friends in the struct; keep the struct a plain data holder:

```cpp
struct ResolutionDto
{
    ResolutionStatus status{ResolutionStatus::SystemError};
    std::string host;
    int systemErrorCode{0};

    friend void to_json(json& j, const ResolutionDto& r);
    friend void from_json(const json& j, ResolutionDto& r);
};
```

Define them in the `.cpp`. `to_json` builds one `json{...}` initializer; `from_json` reads each field defensively so a partial or malformed payload never throws:

```cpp
void to_json(json& j, const ResolutionDto& r)
{
    j = json{
        {"status", r.status},
        {"host", r.host},
        {"systemErrorCode", r.systemErrorCode},
    };
}

void from_json(const json& j, ResolutionDto& r)
{
    r.status = j.value("status", ResolutionStatus::SystemError);
    r.host = j.value("host", std::string{});
    r.systemErrorCode = j.value("systemErrorCode", 0);
}
```

`j.value(key, default)` is the workhorse — it returns the default when the key is absent, giving backward compatibility for free. Reserve `j.at(key)` for fields whose absence is a genuine error you want to throw on.

## Enums and structs nested inside a class

Types declared inside a class (`class Outer { enum class E{...}; struct S{...}; };`) still serialize, but the macros and free functions **cannot live inside the class body** — nlohmann finds `to_json`/`from_json` by ADL, so they must be free functions at namespace scope. Place them **after the closing `};` of the class**, using the fully-qualified name:

```cpp
class EventsCommandMessage final : public IMessage
{
public:
    enum class CommandState { BundleCreated, Processing, Succeeded, Failed };

    struct CommandStatus
    {
        std::string message;
        CommandState state{CommandState::Processing};
    };
    // ...
};

NLOHMANN_JSON_SERIALIZE_ENUM(EventsCommandMessage::CommandState,
    {{EventsCommandMessage::CommandState::Processing, "processing"},
     {EventsCommandMessage::CommandState::Succeeded, "succeeded"},
     {EventsCommandMessage::CommandState::BundleCreated, "support-bundle-created"},
     {EventsCommandMessage::CommandState::Failed, "failed"}})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EventsCommandMessage::CommandStatus, message, state)
```

`CommandStatus` is a plain two-field bag whose members map straight to keys, so the one-liner macro is the right call — no `.cpp`. The enum it contains serializes because its own `SERIALIZE_ENUM` sits just above. Both macros go at namespace scope, still inside `namespace ipc::messages { ... }`.

At call sites, a nested type round-trips like any other: `body["commandStatus"] = status;` to write, and `body["commandStatus"].get<EventsCommandMessage::CommandStatus>()` (or `.at("state").get<CommandState>()` to reach one field) to read.

## Safe parsing of arrays and nested types

Arrays and objects have no `value()` default, so guard with `contains` + `is_array`/`is_object`, and skip elements of the wrong type instead of throwing:

```cpp
r.ipv4.clear();
if (j.contains("ipv4") && j["ipv4"].is_array())
{
    for (const auto& s : j["ipv4"])
    {
        if (!s.is_string()) continue;
        in_addr a{};
        if (ResolutionDto::stringToIpv4(s.get<std::string>(), a)) r.ipv4.push_back(a);
    }
}
```

Nested custom types serialize automatically once they have their own `to_json`/`from_json` — a `std::vector<SafeSearchEntry>` field just works.

## Types nlohmann can't handle directly

POSIX and other opaque types (`in_addr`, `in6_addr`) have no built-in conversion. Serialize them through a `std::string` representation with static helpers on the DTO, and parse with the matching guard:

```cpp
// to_json: convert each to its text form
for (const auto& a : r.ipv4) v4.push_back(ResolutionDto::ipv4ToString(a));
// from_json: inet_pton returns 1 on success — drop anything that fails
if (ResolutionDto::stringToIpv4(s.get<std::string>(), a)) r.ipv4.push_back(a);
```

## Enum-keyed maps

JSON object keys are always strings, so a `std::map<EnumA, EnumB>` serializes by converting each key/value enum through `json`. Wrap the parse in `try/catch` and skip unknown keys — the config stays valid even if one mapping is stale:

```cpp
void to_json(json& j, const SafeSearchEntry& e)
{
    j = json::object();
    for (const auto& kv : e.url_map)
    {
        std::string key = json(kv.first).get<std::string>();
        std::string val = json(kv.second).get<std::string>();
        j[key] = val;
    }
}

void from_json(const json& j, SafeSearchEntry& e)
{
    e.url_map.clear();
    if (!j.is_object()) return;
    for (json::const_iterator it = j.begin(); it != j.end(); ++it)
    {
        try
        {
            Host h = json(it.key()).get<Host>();
            SafeUrl s = it.value().get<SafeUrl>();
            e.url_map.emplace(h, s);
        }
        catch (const std::exception&)
        {
            continue;
        }
    }
}
```

## Placement and build wiring

- Enum + `SERIALIZE_ENUM` and the struct + friend declarations go in `<lib>/include/<lib>/<name>_dto.h`.
- `to_json`/`from_json` definitions go in `<lib>/src/<name>_dto.cpp`, with `using json = nlohmann::json;` at the top.
- The header includes `<nlohmann/json.hpp>` and declares `using json = nlohmann::json;` inside the namespace.
- Build: the lib's `CMakeLists.txt` needs `find_package(nlohmann_json REQUIRED)` and links `nlohmann_json::nlohmann_json`; `conanfile.py` requires `nlohmann_json/3.10.5`. Add these only if the lib doesn't already have them.

## Rules

- No explanatory comments in generated code (see cpp-conventions).
- `to_json`/`from_json` and `SERIALIZE_ENUM` all live inside the type's namespace — nlohmann finds them by ADL, so never put them in the global scope.
- Keys are the JSON contract. Match existing payloads exactly; don't rename a key to "clean it up" unless asked.
- `from_json` must not throw on missing or extra fields. Use `value()`/guards; let it throw only where absence is truly an error.
