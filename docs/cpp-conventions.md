# C++ Conventions

Shared rules for all C++ code in this project (C++11 or later, no external deps beyond what's in method signatures). When the standard is not specified, default to C++11 — ask the user if a newer standard is available before using C++14/17/20 features.

## Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Class | `CamelCase` | `EventBus` |
| Interface class | `I` + `CamelCase` | `IEventBus` |
| Method / variable | `camelCase` | `publishMessage` |
| Private member | `m_` prefix | `m_subscriberMap` |
| Static member | `s_` prefix | `s_instanceCount` |
| Interface file | `interface_<snake_case>.h` | `interface_event_bus.h` |
| Concrete class file | `<snake_case>.h` / `.cpp` | `event_bus.h` |
| Mock file | `mock/mock_<snake_case>.h` | `mock/mock_event_bus.h` |

No abbreviations — names must be fully descriptive (`m_rulesRepository`, not `m_repo`).

## Parameter passing

- Prefer `const T&` for any parameter that is not a primitive or small value type — avoids unnecessary copies
- Pass by value for primitives (`int`, `uint32_t`, `bool`, etc.) and when a copy is intentionally needed (e.g., storing by value in a constructor)
- For class types, strings, and containers, `const T&` is the default — deviate only with good reason
- If ownership transfer is intended, ask before reaching for move semantics (`T&&`)

## Numeric types

- Prefer `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t` from `<cstdint>` when a value cannot be negative
- Use `int` only when negative values are semantically valid
- Use `std::size_t` for indexes and sizes

## Collections

- `std::array<T, N>` when size is known at compile-time (stack-allocated, communicates fixed size)
- `std::vector<T>` when size is dynamic or must grow at runtime
- Never C-style arrays (`T[]` or `T*` as a collection)
- Collection parameters: `const std::array<T,N>&` or `const std::vector<T>&`
- When choice is not obvious from context, ask whether the size is fixed or variable

## Memory management

- Never use `new` directly — use `std::make_shared<T>()` or `std::make_unique<T>()`
- `std::shared_ptr` for shared ownership; `std::unique_ptr` for exclusive ownership
- Members that hold a pointer: `std::shared_ptr<IInterface>`
- `std::make_unique` requires C++14 — if the project is on C++11 only, use `std::unique_ptr<T>(new T(...))` as fallback, or ask the user

## Include guard formats

| File type | Format |
|-----------|--------|
| Interface | `#if !defined(INTERFACE_<NAME>_H)` / `#endif // INTERFACE_<NAME>_H` |
| Concrete class header | `#ifndef <LIB>_<CLASS>_H` / `#endif /* <LIB>_<CLASS>_H */` |
| Mock | `#pragma once` |

## Namespace

One namespace per module, named after the lib: `namespace reporter { ... } // namespace reporter`.
No nested namespace declarations. Sub-namespaces are represented as subdirectories within the same namespace.

## No comments in generated code

Do not add explanatory comments or docstrings. Names and structure communicate intent.
