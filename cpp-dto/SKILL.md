---
name: create-cpp-dto
description: Creates a C++ DTO (Data Transfer Object) struct following project conventions. Use whenever the user asks to create a DTO, a data struct, a value type, or a plain data container in C++ — triggered by "create a DTO", "add a data struct for X", "I need a type to carry Y data", "create the request/response type for Z".
applyTo:
  - "*.h"
  - "*.cpp"
---

## Goal

Create a C++ DTO — a plain data container with no business logic. DTOs use `struct`, not `class`, because their members are public by design. Read `/home/pandrade/.claude/skills/docs/cpp-conventions.md` for naming, parameter passing, and numeric type guidelines.

## Workflow

1. Ask for the DTO name and the lib it belongs to.
2. Ask for each field: name (camelCase, no `m_` prefix), type.
3. Ask if the DTO needs:
   - Static helper / factory methods (e.g., `parse()`, `build()`, `fromBytes()`)
   - JSON serialization (`to_json`/`from_json`) — only add if explicitly requested
   - Move-only semantics (useful for DTOs carrying large or non-copyable resources)
   - `static constexpr` constants (offsets, sizes, flags)
4. Generate `<lib>/include/<lib>/<name>_dto.h`.
5. Generate `<lib>/src/<name>_dto.cpp` only if the DTO has method implementations.
   - Header-only is fine for structs with no methods or only inline-able constants.

## Rules

### Struct, not class
DTOs have all-public data members — `struct` expresses this intent directly and removes the need for accessors. Use `class` only when the DTO needs to enforce invariants or hide state (then it's no longer a plain DTO — reconsider the design).

### Naming
- File: `<name>_dto.h` / `<name>_dto.cpp`
- Struct: `CamelCase` (e.g., `DnsHeader`, `FilterProfileDto`)
- Public fields: `camelCase`, **no `m_` prefix** — these are data, not implementation details
- Constants: `static constexpr`, UPPER_SNAKE_CASE
- Include guard: `#ifndef <LIB>_<NAME>_DTO_H` / `#endif // <LIB>_<NAME>_DTO_H`
- Namespace: matches lib name

### Constructors
- If all fields can be default-initialized (zero/empty), let the compiler generate the default constructor — no need to declare it.
- Declare `= default` explicitly when the intent of a compiler-generated constructor needs to be made clear (Rule of 5 scenarios).
- For move-only DTOs (large payload, non-copyable resource):
  ```cpp
  Dto() = default;
  Dto(Dto&&) noexcept = default;
  Dto& operator=(Dto&&) = default;
  Dto(const Dto&) = delete;
  Dto& operator=(const Dto&) = delete;
  ```

### Static helper methods
Declare in header, implement in `.cpp`. Use for parsing, serialization, or construction from raw data:
```cpp
static DnsHeader parse(const uint8_t* buffer, std::size_t length);
static std::array<uint8_t, SIZE> toBytes(const DnsHeader& header);
```

### JSON serialization (only when requested)
Use nlohmann_json friend functions. Declare in header, implement in `.cpp`. Add `using json = nlohmann::json;` at the top of the `.cpp`:
```cpp
// header
friend void to_json(nlohmann::json& j, const MyDto& d);
friend void from_json(const nlohmann::json& j, MyDto& d);

// .cpp
using json = nlohmann::json;

void to_json(json& j, const MyDto& d) {
    j = json{{"field", d.field}};
}

void from_json(const json& j, MyDto& d) {
    d.field = j.value("field", DefaultType{});
}
```
Prefer `j.value("key", default)` over `j.at("key")` — it's more resilient to missing fields and supports backward compatibility.

## Limitations
- No explanatory comments in generated code
- No inheritance — DTOs are standalone data containers
- No comparison operators unless the user explicitly asks
- No business logic — if a method starts making decisions, it likely belongs in a service class

## Examples

### Header-only DTO (`<lib>/include/<lib>/resolution_dto.h`)

```cpp
#ifndef <LIB>_RESOLUTION_DTO_H
#define <LIB>_RESOLUTION_DTO_H

#include <cstdint>
#include <string>

namespace <lib>
{

struct ResolutionDto
{
    std::string domain;
    std::string resolvedAddress;
    uint32_t    ttl;
    bool        fromCache;
};

} // namespace <lib>

#endif // <LIB>_RESOLUTION_DTO_H
```

### DTO with static helpers and constants

```cpp
#ifndef <LIB>_DNS_HEADER_DTO_H
#define <LIB>_DNS_HEADER_DTO_H

#include <array>
#include <cstdint>
#include <cstddef>

namespace <lib>
{

struct DnsHeaderDto
{
    static constexpr std::size_t SIZE = 12;
    static constexpr uint16_t    QR_RESPONSE = 0x8000;

    uint16_t id;
    uint16_t flags;
    uint16_t questionCount;
    uint16_t answerCount;

    static DnsHeaderDto parse(const uint8_t* buffer, std::size_t length);
    std::array<uint8_t, SIZE> toBytes() const;
};

} // namespace <lib>

#endif // <LIB>_DNS_HEADER_DTO_H
```

### Move-only DTO

```cpp
struct PacketDto
{
    PacketDto() = default;
    PacketDto(PacketDto&&) noexcept = default;
    PacketDto& operator=(PacketDto&&) = default;
    PacketDto(const PacketDto&) = delete;
    PacketDto& operator=(const PacketDto&) = delete;

    std::vector<uint8_t> payload;
    std::string          sourceAddress;
    uint16_t             sourcePort;
};
```

### DTO with JSON (when requested)

```cpp
// header
struct FilterProfileDto
{
    std::string  profileId;
    bool         loggingEnabled;
    bool         redirectEnabled;

    friend void to_json(nlohmann::json& j, const FilterProfileDto& d);
    friend void from_json(const nlohmann::json& j, FilterProfileDto& d);
};

// .cpp
using json = nlohmann::json;

void to_json(json& j, const FilterProfileDto& d) {
    j = json{
        {"profileId",       d.profileId},
        {"loggingEnabled",  d.loggingEnabled},
        {"redirectEnabled", d.redirectEnabled}
    };
}

void from_json(const json& j, FilterProfileDto& d) {
    d.profileId       = j.value("profileId", std::string{});
    d.loggingEnabled  = j.value("loggingEnabled", true);
    d.redirectEnabled = j.value("redirectEnabled", false);
}
```
