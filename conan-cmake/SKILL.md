---
name: conan-cmake
description: Guides Conan 2.x + CMake integration for C++ projects. Use whenever the user asks about adding a dependency, setting up Conan, writing a conanfile, configuring CMake to find Conan packages, creating build profiles, fixing find_package errors after conan install, or setting up sanitizer builds. Triggered by "add dependency X", "set up conan", "find_package fails", "how do I use conan with cmake", "create a debug/asan/tsan profile", "conan install not finding packages".
---

## Goal

This skill covers the full Conan 2.x + CMake workflow: adding dependencies, wiring them into CMake, and creating build profiles. Read `references/profiles.md` when the task involves creating or debugging a profile.

## How Conan 2.x + CMake work together

Two generators bridge Conan and CMake:

- **CMakeDeps** — generates `<PackageName>Config.cmake` files so CMake's `find_package()` can find Conan-installed packages
- **CMakeToolchain** — generates `conan_toolchain.cmake`, which injects compiler, flags, library paths, and C++ standard into CMake

The build flow is always two steps:
1. `conan install` — resolves dependencies, builds missing ones, generates files into `generators/`
2. `cmake` with `-DCMAKE_TOOLCHAIN_FILE=generators/conan_toolchain.cmake` — CMake finds packages via the generated config files

**Without the toolchain file, `find_package()` will fail** — the generated configs live in `generators/`, which is not in CMake's default search path.

---

## conanfile.py

Minimal structure for a C++ project:

```python
from conan import ConanFile
from conan.tools.cmake import cmake_layout

class MyProject(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("fmt/9.1.0")
        self.requires("nlohmann_json/3.10.5")
        self.requires("gtest/1.10.0")

    def layout(self):
        cmake_layout(self)
```

- `settings` must include all four — Conan uses them to select the right prebuilt binary
- `cmake_layout(self)` organizes build artifacts into `build/<BuildType>/`
- `generators` must always include both `CMakeDeps` and `CMakeToolchain`

### Adding a dependency

1. Add `self.requires("<package>/<version>")` to the `requirements()` method
2. Re-run `conan install` (step below)
3. Add `find_package(<Package> CONFIG REQUIRED)` to the root `CMakeLists.txt`
4. Link against the target in the relevant module: `target_link_libraries(<target> PUBLIC <Package>::<Package>)`

### Version conflicts

When two dependencies require different versions of the same package, force a version with:
```python
self.requires("fmt/9.1.0", override=True)
```
Without `override=True`, Conan fails the resolve if versions conflict.

---

## conan install commands

**Release (no profile):**
```bash
conan install . --build=missing -s compiler.cppstd=11
```

**Debug (with profile):**
```bash
conan install . --profile=./profile/debug.conan --build=missing -s compiler.cppstd=11
```

**Sanitizer builds:**
```bash
conan install . --profile=profile/asan.conan --build=missing
conan install . --profile=profile/tsan.conan --build=missing
```

- `--build=missing` — builds dependencies from source when prebuilt binaries are unavailable; safe to always include
- `-s compiler.cppstd=11` — overrides the C++ standard at the CLI level (takes precedence over conanfile settings)
- Run from the project root — Conan generates files into `generators/` at the project root

---

## CMake integration

### Root CMakeLists.txt

After `conan install`, wire in the packages:

```cmake
find_package(GTest CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
# nlohmann_json is header-only — find_package only needed if using targets
```

`CONFIG REQUIRED` is important — it tells CMake to look for `<Package>Config.cmake` files (the kind Conan generates), not the older Find<Package>.cmake modules.

### cmake invocation

```bash
mkdir -p build/Release && cd build/Release
cmake ../.. \
  -DCMAKE_TOOLCHAIN_FILE=generators/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=11

cmake --build . -j$(nproc)
```

- `CMAKE_TOOLCHAIN_FILE` path is relative to the *build* directory — `generators/` lives at the project root, so from `build/Release` the path is `../../generators/conan_toolchain.cmake`... but Conan's `cmake_layout()` adjusts this automatically when run from the build dir. Use the path that matches where you ran `conan install`.

### Linking against Conan packages

Conan-generated targets use the `<Package>::<Package>` namespace convention:

```cmake
target_link_libraries(my_lib
    PUBLIC
        spdlog::spdlog
        fmt::fmt
        nlohmann_json::nlohmann_json
)

target_link_libraries(my_tests
    PRIVATE
        GTest::gtest
        GTest::gmock_main
)
```

The exact target name is in the Conan package's docs or in the generated `<Package>Config.cmake` file inside `generators/`.

---

## Build profiles

Profiles let you configure compiler, build type, C++ standard, and flags without changing `conanfile.py`.

See `references/profiles.md` for profile templates and when to use each one.

---

## Docker builds

Docker builds differ from local builds in two ways: the compiler is unknown until the image runs, and the output directory needs to be explicit.

### Multi-stage Dockerfile pattern

```dockerfile
# syntax=docker/dockerfile:1

FROM debian:bookworm AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip python3-venv \
    build-essential cmake ninja-build pkg-config \
    libsqlite3-dev libpcre3-dev libzmq3-dev \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir --break-system-packages conan

WORKDIR /src
COPY . .

# Auto-detect the container's compiler — replaces a named profile
RUN conan profile detect --force

RUN conan install . \
    -of=/src \
    -s build_type=Release \
    -s compiler.cppstd=11 \
    --build=missing

WORKDIR /src/build/Release
RUN cmake ../.. \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/src/build/Release/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=11

RUN cmake --build . -j

# Runtime image — only what the binary needs at runtime
FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libsqlite3-0 libpcre3 libzmq5 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/Release/src/myapp /usr/local/bin/myapp

ENTRYPOINT ["/usr/local/bin/myapp"]
```

### Key differences from local builds

| | Local | Docker |
|--|-------|--------|
| Profile | `--profile=profile/debug.conan` | `conan profile detect --force` |
| Output dir | default (`generators/` at project root) | `-of=/src` (explicit absolute path) |
| Generator | `make` | `-G Ninja` (faster in containers) |
| Toolchain path | `generators/conan_toolchain.cmake` | `/src/build/<Type>/generators/conan_toolchain.cmake` |
| System libs | managed separately | installed via `apt` in builder stage |

### `conan profile detect --force`

Generates a default Conan profile based on the compiler found in the container. Equivalent to running `conan profile detect` locally — it auto-detects GCC version, `libstdc++11`, architecture, and OS. The `--force` flag overwrites any existing default profile.

Use this instead of a named profile (`--profile=...`) when the compiler version is determined by the base image.

### `-of` flag (output files)

`-of=<path>` tells Conan where to write the generated files (`conan_toolchain.cmake`, package config files). Without it, Conan uses the path determined by `cmake_layout()`, which may not match the absolute path CMake expects inside the container.

```bash
# Local — generators/ ends up at project root relative to CWD
conan install . --build=missing

# Docker — explicit absolute path avoids path resolution issues
conan install . -of=/src -s build_type=Release --build=missing
```

Then reference it with the same absolute path in CMake:
```bash
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=/src/build/Release/generators/conan_toolchain.cmake
```

### System vs Conan dependencies in Docker

Not all dependencies need to come from Conan. Libraries available via `apt` (sqlite3, pcre, zmq) can be installed in the builder stage and linked normally. Only use Conan for libraries not available in the distro's package manager or when you need a specific version.

```dockerfile
# Builder: install system deps + conan deps
RUN apt-get install -y libsqlite3-dev libzmq3-dev   # system
RUN conan install . -of=/src --build=missing          # conan (fmt, spdlog, gtest, etc.)

# Runtime: only runtime libs
FROM debian:bookworm-slim AS runtime
RUN apt-get install -y libsqlite3-0 libzmq5           # runtime counterparts
# Conan-managed static libs are baked into the binary — no runtime install needed
```

### Running a specific profile in Docker

To use a Conan profile (e.g., ASan) inside Docker, copy the profile into the image and pass `--profile`:

```dockerfile
COPY profile/asan.conan /root/.conan2/profiles/asan

RUN conan install . \
    -of=/src \
    --profile=asan \
    --build=missing
```

Or pass settings inline to avoid copying the file:

```dockerfile
RUN conan install . \
    -of=/src \
    -s build_type=Debug \
    -s compiler.cppstd=11 \
    --build=missing
```

## Common problems

### `find_package(X CONFIG REQUIRED)` fails
Cause: `conan install` hasn't been run, or the `CMAKE_TOOLCHAIN_FILE` wasn't passed.
Fix: Run `conan install` first, then pass `-DCMAKE_TOOLCHAIN_FILE=generators/conan_toolchain.cmake` to CMake.

### `ERROR: Invalid setting 'AddressSanitizer' is not a valid 'settings.build_type'`
Cause: Using a custom `build_type` value without registering it in the Conan settings.
Fix: Either use `build_type=Debug` in the profile and pass `-DENABLE_ASAN=ON` to CMake separately, or register the custom value in `~/.conan2/settings.yml`.

### Dependency graph conflict
Cause: Two packages require incompatible versions of a shared dependency.
Fix: Add `self.requires("<dep>/<version>", override=True)` to force the version.

### Binary not found (`--build=missing` not used)
Cause: No prebuilt Conan binary for your compiler/platform combination.
Fix: Add `--build=missing` to the `conan install` command to build from source.
