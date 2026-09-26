---
name: create-cpp-lib
description: Creates a complete C++ library module with full directory structure, CMake configuration, and wiring into the project build. Use whenever the user asks to create a new lib, module, or component — triggered by "create a lib", "add a new module", "create the X module", "I need a new component called Y".
---

## Goal

Scaffold a complete C++ library module (C++11 or later). Ask the user which C++ standard the project targets — use that value for `CMAKE_CXX_STANDARD` in both CMakeLists.txt files. Read `/home/pandrade/.claude/skills/docs/cpp-cmake-conventions.md` for the exact CMakeLists.txt templates and directory structure before generating any files.

## Workflow

1. Ask for the lib name (snake_case).
2. Ask which existing libs this lib depends on.
3. Ask whether to create a starter interface + concrete class, or just the scaffold.
4. Create all files and directories per the CMake conventions doc.
5. Add `add_subdirectory(<lib_name>)` to the root `CMakeLists.txt` after its dependency libs.

## What to generate

```
<lib_name>/
├── CMakeLists.txt          (from template in cmake-conventions doc)
├── include/
│   └── <lib_name>/
│       └── mock/           (empty — GLOB_RECURSE picks it up)
├── src/                    (empty)
└── test/
    └── CMakeLists.txt      (from template in cmake-conventions doc)
```

## CMake variable naming

Lib name uppercased with underscores becomes the CMake variable prefix:
- `socket_client` → `SOCKET_CLIENT`
- `filter_dtos` → `FILTER_DTOS`

## Starter interface + class

If the user wants a starter interface and class, invoke:
1. `create-cpp-interface` skill — creates `interface_<name>.h` + mock inside this lib's `include/` dir
2. `create-cpp-concrete-class` skill — creates the concrete class header + `.cpp` inside this lib

## Limitations
- No comments or explanations in generated CMake or C++ files
- Do not hardcode absolute paths in CMake — always use `${CMAKE_CURRENT_LIST_DIR}`
- Do not put more than one lib's sources in the same directory
