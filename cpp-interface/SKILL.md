---
name: create-cpp-interface
description: Creates a pure virtual C++ interface following project conventions. Use whenever the user asks to create an interface, abstract class, or module contract — triggered by "create interface for X", "I need an interface for Y", "define the contract of Z".
applyTo:
  - "*.h"
---

## Goal

Create the interface header and mock for a C++ module (C++11 or later). Read `/home/pandrade/.claude/skills/docs/cpp-conventions.md` for naming, parameter passing, guard formats, and namespace rules before generating any code.

## Workflow

1. Ask for the interface name and the lib it belongs to.
2. Ask for each method: name, return type, parameters (type and full name).
   - If a parameter has a long or complex type, ask if `using DescriptiveName = ComplexType;` can be used.
3. Generate the interface file at `<lib>/include/<lib>/interface_<snake_case_name>.h`.
4. Generate the mock file at `<lib>/include/<lib>/mock/mock_<snake_case_name>.h`.

## Interface rules

- File: `interface_<snake_case_name>.h`
- Class: `I<CamelCaseName>`
- Include guard: `#if !defined(INTERFACE_<NAME>_H)` / `#endif // INTERFACE_<NAME>_H`
- Virtual default destructor required
- All methods `virtual ... = 0`
- No implementations, no private attributes
- Namespace matches lib name
- If C++11 only: no `= delete`, no `noexcept` unless confirmed available

## Mock rules

- File: `mock/mock_<snake_case_name>.h`
- `#pragma once`
- Inherits from the interface
- GMock 2.x syntax: `MOCK_METHOD(ReturnType, methodName, (Args), (override))`
- Same namespace as the interface

## Examples

### Interface (`<lib>/include/<lib>/interface_<name>.h`)

```cpp
#if !defined(INTERFACE_<NAME>_H)
#define INTERFACE_<NAME>_H

#include <string>

namespace <lib> {

class I<Name>
{
public:
    virtual ~I<Name>() = default;

    virtual void someMethod(const std::string& parameter) = 0;
};

} // namespace <lib>

#endif // INTERFACE_<NAME>_H
```

### Mock (`<lib>/include/<lib>/mock/mock_<name>.h`)

```cpp
#pragma once

#include <gmock/gmock.h>

#include "<lib>/interface_<name>.h"

namespace <lib> {

class Mock<Name> : public I<Name>
{
public:
    MOCK_METHOD(void, someMethod, (const std::string& parameter), (override));
};

} // namespace <lib>
```
