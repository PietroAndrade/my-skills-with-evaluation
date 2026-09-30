# Conan Build Profiles

Profiles live in `profile/` at the project root. They configure compiler, build type, C++ standard, and compiler/linker flags for a specific build scenario.

## Profile anatomy

```ini
[settings]
os = Linux
arch = x86_64
compiler = gcc
compiler.version = 14
compiler.libcxx = libstdc++11
compiler.cppstd = 11
build_type = Debug

[conf]
tools.build:cxxflags=["-flag1", "-flag2"]
tools.build:cflags=["-flag1", "-flag2"]
tools.build:sharedlinkflags=["-flag1"]
tools.build:exelinkflags=["-flag1"]

[buildenv]
CXXFLAGS=-flag1 -flag2
LDFLAGS=-flag1
```

- `[settings]` — selects the right Conan binary and sets CMakeToolchain variables
- `[conf]` — injects flags via Conan's `tools.build` mechanism into the generated toolchain
- `[buildenv]` — sets environment variables at build time; redundant with `[conf]` but some tools read env vars directly

---

## Standard profiles

### debug.conan — Standard debug build

```ini
[settings]
arch = x86_64
os = Linux
compiler = gcc
compiler.version = 14
compiler.libcxx = libstdc++11
compiler.cppstd = 11
build_type = Debug
```

Use with:
```bash
conan install . --profile=./profile/debug.conan --build=missing -s compiler.cppstd=11
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
```

---

### asan.conan — AddressSanitizer

```ini
[settings]
os = Linux
arch = x86_64
build_type = AddressSanitizer
compiler = gcc
compiler.version = 14
compiler.libcxx = libstdc++11
compiler.cppstd = 11

[conf]
tools.build:cxxflags=["-fsanitize=address", "-fno-omit-frame-pointer", "-g"]
tools.build:cflags=["-fsanitize=address", "-fno-omit-frame-pointer", "-g"]
tools.build:sharedlinkflags=["-fsanitize=address"]
tools.build:exelinkflags=["-fsanitize=address"]

[buildenv]
CXXFLAGS=-fsanitize=address -fno-omit-frame-pointer -g
CFLAGS=-fsanitize=address -fno-omit-frame-pointer -g
LDFLAGS=-fsanitize=address
```

Use with:
```bash
conan install . --profile=profile/asan.conan --build=missing
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build . -j$(nproc)
ASAN_OPTIONS=detect_leaks=1:halt_on_error=0 ./tests/my_tests
```

Note: `build_type=AddressSanitizer` is a custom value — register it in `~/.conan2/settings.yml` under `build_type` if Conan rejects it. Alternatively, use `build_type=Debug` to avoid this.

---

### tsan.conan — ThreadSanitizer

```ini
[settings]
os = Linux
arch = x86_64
build_type = ThreadSanitizer
compiler = gcc
compiler.version = 14
compiler.libcxx = libstdc++11
compiler.cppstd = 11

[conf]
tools.build:cxxflags=["-fsanitize=thread", "-fno-omit-frame-pointer", "-g"]
tools.build:cflags=["-fsanitize=thread", "-fno-omit-frame-pointer", "-g"]
tools.build:sharedlinkflags=["-fsanitize=thread"]
tools.build:exelinkflags=["-fsanitize=thread"]

[buildenv]
CXXFLAGS=-fsanitize=thread -fno-omit-frame-pointer -g
CFLAGS=-fsanitize=thread -fno-omit-frame-pointer -g
LDFLAGS=-fsanitize=thread
```

Use with:
```bash
conan install . --profile=profile/tsan.conan --build=missing
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build . -j$(nproc)
TSAN_OPTIONS=halt_on_error=0:history_size=7 ./tests/my_tests
```

ASan and TSan are mutually exclusive — never enable both at the same time.

---

### perf.conan — Performance profiling

```ini
[settings]
os = Linux
arch = x86_64
build_type = performanceTest
compiler = gcc
compiler.version = 14
compiler.libcxx = libstdc++11
compiler.cppstd = 11

[conf]
tools.build:cxxflags=["-g", "-O2", "-fno-omit-frame-pointer"]
tools.build:cflags=["-g", "-O2", "-fno-omit-frame-pointer"]
```

Use when profiling with `perf`, `valgrind`, or similar tools — optimized but with debug symbols and frame pointers intact.

---

## Creating a new profile

Copy the closest existing profile and adjust:
1. Change `build_type` to a descriptive name
2. Add flags to `[conf]` tools.build entries
3. Mirror the same flags in `[buildenv]` for tools that read env vars
4. Run `conan install . --profile=profile/<new>.conan --build=missing`
