# CMake Lib Conventions

Patterns for all library modules in this project.

## Directory structure

```
<lib_name>/
├── CMakeLists.txt
├── include/
│   └── <lib_name>/
│       └── mock/
├── src/
└── test/
    └── CMakeLists.txt
```

- `include/<lib_name>/` — all public headers, including interface files
- `include/<lib_name>/mock/` — GMock implementations (picked up automatically by GLOB_RECURSE)
- `src/` — all `.cpp` implementations
- `test/` — GTest unit tests

## `<lib_name>/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.15)
project(<lib_name> CXX)

set(CMAKE_CXX_STANDARD 11)  # adjust to 14/17/20 if project uses a newer standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(<LIB_NAME>_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/include")
set(<LIB_NAME>_SOURCES_DIR "${CMAKE_CURRENT_LIST_DIR}/src")

message("[<lib_name>] path to headers: ${<LIB_NAME>_HEADERS_DIR}")
message("[<lib_name>] path to sources: ${<LIB_NAME>_SOURCES_DIR}")

file(GLOB_RECURSE <LIB_NAME>_HEADERS "${<LIB_NAME>_HEADERS_DIR}/*.h")
file(GLOB_RECURSE <LIB_NAME>_SOURCES "${<LIB_NAME>_SOURCES_DIR}/*.cpp")

add_library(<lib_name>
    STATIC
        ${<LIB_NAME>_SOURCES}
        ${<LIB_NAME>_HEADERS}
)

target_include_directories(<lib_name>
    PUBLIC
        $<BUILD_INTERFACE:${<LIB_NAME>_HEADERS_DIR}>
)

target_link_libraries(<lib_name>
    PUBLIC
        <dependencies>
)

set_target_properties(<lib_name>
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS <lib_name>)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
```

Key requirements (do not omit):
- `GLOB_RECURSE` for both headers and sources — picks up `mock/` automatically
- `$<BUILD_INTERFACE:...>` in `target_include_directories` — avoids install-time path pollution
- `POSITION_INDEPENDENT_CODE ON` — required for all static libs in this project
- Alias `${PROJECT_NAME}::${PROJECT_NAME}` — enables `::` namespaced linking
- `BUILD_TESTING` guard — test subdir is opt-in

## `<lib_name>/test/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.12)
project(<lib_name>_tests CXX)

set(CMAKE_CXX_STANDARD 11)  # match the lib's standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
find_package(GTest REQUIRED)

set(<LIB_NAME>_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/../include")

file(GLOB_RECURSE <LIB_NAME>_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_executable(<lib_name>_tests ${<LIB_NAME>_TEST_SOURCES})

target_compile_definitions(<lib_name>_tests PRIVATE UNIT_TEST)

target_link_libraries(<lib_name>_tests
    PRIVATE
        <lib_name>
        GTest::gtest
        GTest::gmock_main
)

target_include_directories(<lib_name>_tests
    PRIVATE
        ${<LIB_NAME>_HEADERS_DIR}
)

add_test(NAME <lib_name>_tests COMMAND <lib_name>_tests)
```

Key requirements:
- `UNIT_TEST` compile definition — enables `#ifdef UNIT_TEST` blocks in production headers
- Link `GTest::gtest` + `GTest::gmock_main` (not `gtest_main` alone)
- Test executable named `<lib_name>_tests`

## Root CMakeLists.txt

Add `add_subdirectory(<lib_name>)` after all its dependency libs — CMake resolves targets in declaration order.

## Variable naming convention

CMake variable prefix is the lib name in uppercase with underscores:
- `socket_client` → `SOCKET_CLIENT_HEADERS_DIR`
- `filter_dtos` → `FILTER_DTOS_SOURCES_DIR`
