---
name: create-cpp-concrete-class
description: Creates a concrete C++ class. Use whenever the user asks to create a concrete class, implement an interface, or add a C++ class implementation — triggered by "create a class", "implement interface X", "add a concrete class", "create the implementation of Y".
applyTo:
  - "*.h"
  - "*.cpp"
---

## Goal

Create a well-structured concrete C++ class (C++11 or later): header with declarations only, `.cpp` with all implementations. Read `/home/pandrade/.claude/skills/docs/cpp-conventions.md` for naming, parameter passing, memory management, types, and collections before generating any code. Ask the user which C++ standard the project targets if not evident from context — it affects `std::make_unique`, structured bindings, `if constexpr`, and other features.

## Workflow

1. Ask for the class name and the interface it implements.
   - If no inheritance mentioned, ask which interface it inherits from — concrete classes must inherit from an interface, except patterns like Builder or Value Object.
2. Ask for the lib name to define file paths.
3. For each method: full name (no abbreviations), return type, parameters (type and full name).
   - If a parameter has a long type, ask if `using DescriptiveName = ComplexType;` can be used.
4. For the constructor: parameters and whether to follow Rule of 3 or Rule of 5.
5. Generate both files:
   - Header: `<lib>/include/<lib>/<class_name>.h`
   - Implementation: `<lib>/src/<class_name>.cpp`

## Header rules

- Include guard: `#ifndef <LIB>_<CLASS>_H` / `#endif /* <LIB>_<CLASS>_H */`
- Include interface as: `#include "<lib>/interface_<interface_name>.h"`
- Class declaration: `class <Name> final : public I<Interface>`
- Always `explicit` on single-argument constructors
- Destructor: `~<Name>() = default;` in header
- All method overrides marked `override`
- No method implementations in header
- Private members: `m_` prefix

## `.cpp` rules

- Constants as `constexpr`, not in header
- Member initializer list in constructor
- Two blank lines between each method implementation
- Alias for complex types declared in `.cpp` (not header unless public interface requires it)

## UNIT_TEST guard

When the class needs to expose internals for testing:
```cpp
#ifdef UNIT_TEST
    void setDependency(const std::shared_ptr<IDependency>& dep) {
        m_dependency = dep;
    }
#endif
```

## Limitations
- No explanatory comments in generated code
- One class per file
- Do not use `const` on methods that modify class state
- Do not use `int` or `string` when a more expressive type exists — ask the user

## Examples

### Header

```cpp
#ifndef <LIB>_<CLASS_NAME>_H
#define <LIB>_<CLASS_NAME>_H

#include "<lib>/interface_<interface>.h"

#include <cstdint>
#include <memory>
#include <string>

namespace <lib>
{

class <ClassName> final : public I<Interface>
{
public:
    explicit <ClassName>(std::shared_ptr<IDependency> dependency);
    ~<ClassName>() = default;

    ReturnType someMethod(const ParameterType& parameterName) override;

private:
    ReturnType auxiliaryMethod(const ParameterType& parameterName);

    std::shared_ptr<IDependency> m_dependency;
    std::string                  m_descriptiveName;
    uint32_t                     m_eventCounter;
};

} // namespace <lib>

#endif /* <LIB>_<CLASS_NAME>_H */
```

### Implementation

```cpp
#include "<lib>/<class_name>.h"

namespace <lib>
{

<ClassName>::<ClassName>(std::shared_ptr<IDependency> dependency)
    : m_dependency(dependency)
    , m_descriptiveName("")
    , m_eventCounter(0)
{
}


<ClassName>::~<ClassName>() = default;


ReturnType <ClassName>::someMethod(const ParameterType& parameterName)
{
    return m_dependency->execute(parameterName);
}


ReturnType <ClassName>::auxiliaryMethod(const ParameterType& parameterName)
{
    return ReturnType{};
}

} // namespace <lib>
```
